#include <string.h>

#include "buffer.h"

#include "deserialize.h"
#include "utils.h"
#include "types.h"
#include "../constants.h"
#include "../bcs/init.h"
#include "../bcs/decoder.h"

// Forward declarations for functions defined later in this file
static parser_status_e skip_type_tag(buffer_t *buf, uint8_t depth);
static parser_status_e skip_all_type_args(buffer_t *buf);
static generic_arg_type_t infer_arg_type(uint32_t len);
parser_status_e multisig_vote_deserialize(buffer_t *buf, transaction_t *tx);
parser_status_e multisig_create_with_owners_deserialize(buffer_t *buf, transaction_t *tx);

parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx) {
    if (buf->size > MAX_TRANSACTION_LEN) {
        return WRONG_LENGTH_ERROR;
    }
    transaction_init(tx);

    parser_status_e tx_variant_parsing_status = tx_variant_deserialize(buf, tx);
    if (tx_variant_parsing_status != PARSING_OK) {
        return tx_variant_parsing_status;
    }
    switch (tx->tx_variant) {
        case TX_RAW:
            return tx_raw_deserialize(buf, tx);
        case TX_RAW_WITH_DATA:
            break;
        case TX_RAW_MESSAGE:
            break;  // Since the raw message is processed before display without direct transaction
                    // buffer reads, null-termination concerns are mitigated.
        case TX_MESSAGE:
            // To make sure the message is a null-terminated string
            if (buf->size == MAX_TRANSACTION_LEN && buf->ptr[MAX_TRANSACTION_LEN - 1] != 0) {
                return WRONG_LENGTH_ERROR;
            }

            __attribute__((fallthrough));
        default:
            break;
    }

    return PARSING_OK;
}

parser_status_e tx_raw_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->tx_variant != TX_RAW) {
        return TX_VARIANT_UNDEFINED_ERROR;
    }

    // read sender address
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &tx->sender, ADDRESS_LEN)) {
        return SENDER_READ_ERROR;
    }
    // read sequence
    if (!bcs_read_u64(buf, &tx->sequence)) {
        return SEQUENCE_READ_ERROR;
    }

    const size_t buf_footer_begin = buf->size - TX_FOOTER_LEN;
    buffer_t buf_footer = {.ptr = buf->ptr, .size = buf->size, .offset = buf_footer_begin};
    // read max_gas_amount
    if (!bcs_read_u64(&buf_footer, &tx->max_gas_amount)) {
        return MAX_GAS_READ_ERROR;
    }
    // read gas_unit_price
    if (!bcs_read_u64(&buf_footer, &tx->gas_unit_price)) {
        return GAS_UNIT_PRICE_READ_ERROR;
    }
    // read expiration_timestamp_secs
    if (!bcs_read_u64(&buf_footer, &tx->expiration_timestamp_secs)) {
        return EXPIRATION_READ_ERROR;
    }
    // read chain_id
    if (!bcs_read_u8(&buf_footer, &tx->chain_id)) {
        return CHAIN_ID_READ_ERROR;
    }

    // read payload_variant
    uint32_t payload_variant = PAYLOAD_UNDEFINED;
    if (!bcs_read_u32_from_uleb128(buf, &payload_variant)) {
        return PAYLOAD_VARIANT_READ_ERROR;
    }
    if (payload_variant != PAYLOAD_ENTRY_FUNCTION && payload_variant != PAYLOAD_SCRIPT &&
        payload_variant != PAYLOAD_MULTISIG) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    tx->payload_variant = payload_variant;

    parser_status_e payload_parsing_status = 0;
    switch (tx->payload_variant) {
        case PAYLOAD_ENTRY_FUNCTION:
            payload_parsing_status = entry_function_payload_deserialize(buf, tx);
            if (payload_parsing_status != PARSING_OK) {
                return payload_parsing_status;
            }
            if (tx->payload.entry_function.known_type == FUNC_APTOS_ACCOUNT_TRANSFER) {
                return (buf->offset == buf_footer_begin) ? PARSING_OK : WRONG_LENGTH_ERROR;
            }
            return PARSING_OK;
        case PAYLOAD_SCRIPT:
            return script_payload_deserialize(buf, tx);
        case PAYLOAD_MULTISIG:
            return multisig_payload_deserialize(buf, tx);
        default:
            return PAYLOAD_UNDEFINED_ERROR;
    }

    return PARSING_OK;
}

parser_status_e tx_variant_deserialize(buffer_t *buf, transaction_t *tx) {
    if (buf->offset != 0) {
        return TX_VARIANT_READ_ERROR;
    }

    tx->tx_variant = TX_UNDEFINED;

    uint8_t *prefix;
    // read hashed prefix bytes
    if (bcs_read_ptr_to_fixed_bytes(buf, &prefix, TX_HASHED_PREFIX_LEN)) {
        if (memcmp(prefix, PREFIX_RAW_TX_WITH_DATA_HASHED, TX_HASHED_PREFIX_LEN) == 0) {
            tx->tx_variant = TX_RAW_WITH_DATA;
            return PARSING_OK;
        }

        if (memcmp(prefix, PREFIX_RAW_TX_HASHED, TX_HASHED_PREFIX_LEN) == 0) {
            tx->tx_variant = TX_RAW;
            return PARSING_OK;
        }
    }

    // Not a transaction prefix, so we reset the offer to consider the full message
    buf->offset = 0;

    // Try to display the message as UTF8 if possible
    tx->tx_variant =
        transaction_utils_check_encoding(buf->ptr, buf->size) ? TX_MESSAGE : TX_RAW_MESSAGE;

    return PARSING_OK;
}

parser_status_e entry_function_payload_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    entry_function_payload_init(payload);

    // read module id address field
    if (!bcs_read_fixed_bytes(buf,
                              (uint8_t *) payload->module_id.address,
                              sizeof payload->module_id.address)) {
        return MODULE_ID_ADDR_READ_ERROR;
    }
    // read module_id name len field
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->module_id.name.len)) {
        return MODULE_ID_NAME_LEN_READ_ERROR;
    }
    //  read module_id name bytes field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &payload->module_id.name.bytes,
                                     payload->module_id.name.len)) {
        return MODULE_ID_NAME_BYTES_READ_ERROR;
    }
    // read function_name len field
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->function_name.len)) {
        return FUNCTION_NAME_LEN_READ_ERROR;
    }
    // read function_name bytes field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &payload->function_name.bytes,
                                     payload->function_name.len)) {
        return FUNCTION_NAME_BYTES_READ_ERROR;
    }

    payload->known_type = determine_function_type(tx);
    switch (payload->known_type) {
        case FUNC_APTOS_ACCOUNT_TRANSFER:
            return aptos_account_transfer_function_deserialize(buf, tx);
        case FUNC_COIN_TRANSFER:
        case FUNC_APTOS_ACCOUNT_TRANSFER_COINS:
            return coin_transfer_function_deserialize(buf, tx);
        case FUNC_FUNGIBLE_STORE_TRANSFER:
            return fa_transfer_function_deserialize(buf, tx);
        case FUNC_ADD_STAKE:
        case FUNC_UNLOCK_STAKE:
        case FUNC_REACTIVATE_STAKE:
        case FUNC_WITHDRAW_STAKE:
            return delegation_pool_deserialize(buf, tx);
        case FUNC_MULTISIG_CREATE_TRANSACTION:
            return multisig_create_transaction_deserialize(buf, tx);
        case FUNC_MULTISIG_CREATE_WITH_HASH:
            return multisig_create_hash_deserialize(buf, tx);
        case FUNC_MULTISIG_APPROVE:
        case FUNC_MULTISIG_REJECT:
        case FUNC_MULTISIG_VOTE:
            return multisig_vote_deserialize(buf, tx);
        case FUNC_MULTISIG_CREATE_WITH_OWNERS:
            return multisig_create_with_owners_deserialize(buf, tx);
        default:
            return generic_entry_function_deserialize(buf, tx);
    }

    return PARSING_OK;
}

parser_status_e aptos_account_transfer_function_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    if (payload->known_type != FUNC_APTOS_ACCOUNT_TRANSFER) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // read type args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }
    // read args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 2) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }
    uint32_t receiver_len;
    // read receiver address len
    if (!bcs_read_u32_from_uleb128(buf, &receiver_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (receiver_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    // read receiver address field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &payload->args.transfer.receiver, ADDRESS_LEN)) {
        return RECEIVER_ADDR_READ_ERROR;
    }
    uint32_t amount_len;
    // read amount len
    if (!bcs_read_u32_from_uleb128(buf, &amount_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }
    if (amount_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    // read amount field
    if (!bcs_read_u64(buf, &payload->args.transfer.amount)) {
        return AMOUNT_READ_ERROR;
    }

    return PARSING_OK;
}

parser_status_e coin_transfer_function_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    if (payload->known_type != FUNC_COIN_TRANSFER &&
        payload->known_type != FUNC_APTOS_ACCOUNT_TRANSFER_COINS) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // read type args size field
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 1) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    uint32_t ty_arg_variant = TYPE_TAG_UNDEFINED;
    // read type tag variant
    if (!bcs_read_u32_from_uleb128(buf, &ty_arg_variant)) {
        return TYPE_TAG_READ_ERROR;
    }
    if (ty_arg_variant != TYPE_TAG_STRUCT) {
        return TYPE_TAG_UNEXPECTED_ERROR;
    }

    args_coin_transfer_t *coin_transfer = &payload->args.coin_transfer;
    // read coin struct address field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &coin_transfer->ty_coin.address, ADDRESS_LEN)) {
        return STRUCT_ADDRESS_READ_ERROR;
    }
    // read coin struct module name len
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &coin_transfer->ty_coin.module_name.len)) {
        return STRUCT_MODULE_LEN_READ_ERROR;
    }
    // read coin struct module name field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &coin_transfer->ty_coin.module_name.bytes,
                                     coin_transfer->ty_coin.module_name.len)) {
        return STRUCT_MODULE_BYTES_READ_ERROR;
    }
    // read coin struct name len
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &coin_transfer->ty_coin.name.len)) {
        return STRUCT_NAME_LEN_READ_ERROR;
    }
    // read coin struct name field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &coin_transfer->ty_coin.name.bytes,
                                     coin_transfer->ty_coin.name.len)) {
        return STRUCT_NAME_BYTES_READ_ERROR;
    }
    // read coin struct args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &coin_transfer->ty_coin.type_args_size)) {
        return STRUCT_TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (coin_transfer->ty_coin.type_args_size != 0) {
        return STRUCT_TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // read args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 2) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }
    uint32_t receiver_len;
    // read receiver address len
    if (!bcs_read_u32_from_uleb128(buf, &receiver_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (receiver_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    // read receiver address field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &payload->args.transfer.receiver, ADDRESS_LEN)) {
        return RECEIVER_ADDR_READ_ERROR;
    }
    uint32_t amount_len;
    // read amount len
    if (!bcs_read_u32_from_uleb128(buf, &amount_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }
    if (amount_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    // read amount field
    if (!bcs_read_u64(buf, &payload->args.transfer.amount)) {
        return AMOUNT_READ_ERROR;
    }

    return PARSING_OK;
}

parser_status_e fa_transfer_function_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    if (payload->known_type != FUNC_FUNGIBLE_STORE_TRANSFER) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // read type args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }

    if (payload->args.ty_size != 1) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    uint32_t ty_arg_variant = TYPE_TAG_UNDEFINED;
    // read type tag variant
    if (!bcs_read_u32_from_uleb128(buf, &ty_arg_variant)) {
        return TYPE_TAG_READ_ERROR;
    }
    if (ty_arg_variant != TYPE_TAG_STRUCT) {
        return TYPE_TAG_UNEXPECTED_ERROR;
    }

    // READ type Arguments
    args_fungible_asset_transfer_t *fa_transfer = &payload->args.fa_transfer;
    // read coin struct address field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &fa_transfer->ty_args.address, ADDRESS_LEN)) {
        return STRUCT_ADDRESS_READ_ERROR;
    }
    // read coin struct module name len
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &fa_transfer->ty_args.module_name.len)) {
        return STRUCT_MODULE_LEN_READ_ERROR;
    }
    // read coin struct module name field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &fa_transfer->ty_args.module_name.bytes,
                                     fa_transfer->ty_args.module_name.len)) {
        return STRUCT_MODULE_BYTES_READ_ERROR;
    }
    // read coin struct name len
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &fa_transfer->ty_args.name.len)) {
        return STRUCT_NAME_LEN_READ_ERROR;
    }
    // read coin struct name field
    if (!bcs_read_ptr_to_fixed_bytes(buf,
                                     &fa_transfer->ty_args.name.bytes,
                                     fa_transfer->ty_args.name.len)) {
        return STRUCT_NAME_BYTES_READ_ERROR;
    }

    // read coin struct args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &fa_transfer->ty_args.type_args_size)) {
        return STRUCT_TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (fa_transfer->ty_args.type_args_size != 0) {
        return STRUCT_TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // READ function arguments
    // read args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 3) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }

    uint32_t fa_store_addr_len;
    // read fungible stor address len
    if (!bcs_read_u32_from_uleb128(buf, &fa_store_addr_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (fa_store_addr_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }

    // add fungible store address
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &fa_transfer->fungible_asset.address, ADDRESS_LEN)) {
        return STRUCT_ADDRESS_READ_ERROR;
    }

    uint32_t receiver_len;
    // read receiver address len
    if (!bcs_read_u32_from_uleb128(buf, &receiver_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (receiver_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    // read receiver address field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &payload->args.fa_transfer.receiver, ADDRESS_LEN)) {
        return RECEIVER_ADDR_READ_ERROR;
    }
    uint32_t amount_len;
    // read amount len
    if (!bcs_read_u32_from_uleb128(buf, &amount_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }
    if (amount_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    // read amount field
    if (!bcs_read_u64(buf, &payload->args.fa_transfer.amount)) {
        return AMOUNT_READ_ERROR;
    }

    return PARSING_OK;
}

parser_status_e delegation_pool_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    if (payload->known_type != FUNC_ADD_STAKE && payload->known_type != FUNC_UNLOCK_STAKE &&
        payload->known_type != FUNC_REACTIVATE_STAKE &&
        payload->known_type != FUNC_WITHDRAW_STAKE) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // read type args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }

    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // read args size
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 2) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // read args size
    uint32_t pool_len;
    if (!bcs_read_u32_from_uleb128(buf, &pool_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }

    if (pool_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    //  read receiver pool field
    if (!bcs_read_fixed_bytes(buf, (uint8_t *) &payload->args.delegation.pool, ADDRESS_LEN)) {
        return RECEIVER_ADDR_READ_ERROR;
    }

    uint32_t amount_len;
    // read amount len
    if (!bcs_read_u32_from_uleb128(buf, &amount_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }

    if (amount_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    //  read amount field
    if (!bcs_read_u64(buf, &payload->args.delegation.amount)) {
        return AMOUNT_READ_ERROR;
    }

    return PARSING_OK;
}

static parser_status_e skip_type_tag(buffer_t *buf, uint8_t depth) {
    if (depth > MAX_TYPE_TAG_DEPTH) {
        return TYPE_TAG_SKIP_ERROR;
    }

    uint32_t variant = 0;
    if (!bcs_read_u32_from_uleb128(buf, &variant)) {
        return TYPE_TAG_SKIP_ERROR;
    }

    switch (variant) {
        case TYPE_TAG_BOOL:
        case TYPE_TAG_U8:
        case TYPE_TAG_U16:
        case TYPE_TAG_U32:
        case TYPE_TAG_U64:
        case TYPE_TAG_U128:
        case TYPE_TAG_U256:
        case TYPE_TAG_ADDRESS:
        case TYPE_TAG_SIGNER:
            // Primitive type tags have no additional data
            return PARSING_OK;
        case TYPE_TAG_VECTOR:
            // Vector contains one inner type tag
            return skip_type_tag(buf, depth + 1);
        case TYPE_TAG_STRUCT: {
            // Struct: address (32 bytes) + module_name (string) + name (string) + type_args
            if (!buffer_can_read(buf, ADDRESS_LEN)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            if (!buffer_seek_cur(buf, ADDRESS_LEN)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            // Skip module name (ULEB128 length + bytes)
            uint32_t module_len = 0;
            if (!bcs_read_u32_from_uleb128(buf, &module_len)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            if (!buffer_seek_cur(buf, module_len)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            // Skip struct name (ULEB128 length + bytes)
            uint32_t name_len = 0;
            if (!bcs_read_u32_from_uleb128(buf, &name_len)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            if (!buffer_seek_cur(buf, name_len)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            // Skip nested type args
            uint32_t num_type_args = 0;
            if (!bcs_read_u32_from_uleb128(buf, &num_type_args)) {
                return TYPE_TAG_SKIP_ERROR;
            }
            for (uint32_t i = 0; i < num_type_args; i++) {
                parser_status_e status = skip_type_tag(buf, depth + 1);
                if (status != PARSING_OK) {
                    return status;
                }
            }
            return PARSING_OK;
        }
        default:
            return TYPE_TAG_SKIP_ERROR;
    }
}

static parser_status_e skip_all_type_args(buffer_t *buf) {
    uint32_t num_type_args = 0;
    if (!bcs_read_u32_from_uleb128(buf, &num_type_args)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    for (uint32_t i = 0; i < num_type_args; i++) {
        parser_status_e status = skip_type_tag(buf, 0);
        if (status != PARSING_OK) {
            return status;
        }
    }
    return PARSING_OK;
}

static generic_arg_type_t infer_arg_type(uint32_t len) {
    switch (len) {
        case 1:
            return ARG_TYPE_U8;
        case 2:
            return ARG_TYPE_U16;
        case 4:
            return ARG_TYPE_U32;
        case 8:
            return ARG_TYPE_U64;
        case 16:
            return ARG_TYPE_U128;
        case 32:
            return ARG_TYPE_ADDRESS;
        default:
            return ARG_TYPE_BYTES;
    }
}

parser_status_e generic_entry_function_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    args_generic_t *generic = &payload->args.generic;

    // Skip type arguments (we don't need them for display)
    parser_status_e status = skip_all_type_args(buf);
    if (status != PARSING_OK) {
        return status;
    }

    // Read number of function arguments
    uint32_t num_args = 0;
    if (!bcs_read_u32_from_uleb128(buf, &num_args)) {
        return ARGS_SIZE_READ_ERROR;
    }
    generic->num_args = num_args;
    generic->num_parsed = (num_args < MAX_GENERIC_ARGS) ? num_args : MAX_GENERIC_ARGS;

    // Parse up to MAX_GENERIC_ARGS arguments
    for (size_t i = 0; i < generic->num_parsed; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        generic->args[i].raw_len = arg_len;
        if (arg_len > 0) {
            if (!bcs_read_ptr_to_fixed_bytes(buf, &generic->args[i].raw_ptr, arg_len)) {
                return GENERIC_ARG_BYTES_READ_ERROR;
            }
        } else {
            generic->args[i].raw_ptr = NULL;
        }
        generic->args[i].type = infer_arg_type(arg_len);
    }

    // Skip remaining arguments beyond MAX_GENERIC_ARGS
    for (size_t i = generic->num_parsed; i < num_args; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        if (arg_len > 0 && !buffer_seek_cur(buf, arg_len)) {
            return GENERIC_ARG_BYTES_READ_ERROR;
        }
    }

    return PARSING_OK;
}

parser_status_e script_payload_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_SCRIPT) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    script_payload_parsed_t *script = &tx->payload.script_parsed;
    memset(script, 0, sizeof(*script));

    // Read and skip script bytecode
    uint32_t code_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &code_len)) {
        return SCRIPT_CODE_LEN_READ_ERROR;
    }
    script->code_len = code_len;
    if (code_len > 0 && !buffer_seek_cur(buf, code_len)) {
        return SCRIPT_CODE_READ_ERROR;
    }

    // Skip type arguments
    parser_status_e status = skip_all_type_args(buf);
    if (status != PARSING_OK) {
        return status;
    }

    // Read number of script arguments (TransactionArgument enum, self-typed)
    uint32_t num_args = 0;
    if (!bcs_read_u32_from_uleb128(buf, &num_args)) {
        return ARGS_SIZE_READ_ERROR;
    }
    script->num_args = num_args;
    script->num_parsed = (num_args < MAX_GENERIC_ARGS) ? num_args : MAX_GENERIC_ARGS;

    for (size_t i = 0; i < num_args; i++) {
        uint32_t variant = 0;
        if (!bcs_read_u32_from_uleb128(buf, &variant)) {
            return SCRIPT_ARG_VARIANT_READ_ERROR;
        }

        uint32_t arg_len = 0;
        switch (variant) {
            case SCRIPT_ARG_BOOL:
            case SCRIPT_ARG_U8:
                arg_len = 1;
                break;
            case SCRIPT_ARG_U16:
                arg_len = 2;
                break;
            case SCRIPT_ARG_U32:
                arg_len = 4;
                break;
            case SCRIPT_ARG_U64:
                arg_len = 8;
                break;
            case SCRIPT_ARG_U128:
                arg_len = 16;
                break;
            case SCRIPT_ARG_ADDRESS:
                arg_len = ADDRESS_LEN;
                break;
            case SCRIPT_ARG_U256:
                arg_len = 32;
                break;
            case SCRIPT_ARG_U8_VECTOR: {
                uint32_t vec_len = 0;
                if (!bcs_read_u32_from_uleb128(buf, &vec_len)) {
                    return SCRIPT_ARG_READ_ERROR;
                }
                arg_len = vec_len;
                break;
            }
            default:
                return SCRIPT_ARG_VARIANT_READ_ERROR;
        }

        if (i < script->num_parsed) {
            script->args[i].variant = (script_arg_variant_t) variant;
            script->args[i].raw_len = arg_len;
            if (arg_len > 0) {
                if (!bcs_read_ptr_to_fixed_bytes(buf, &script->args[i].raw_ptr, arg_len)) {
                    return SCRIPT_ARG_READ_ERROR;
                }
            } else {
                script->args[i].raw_ptr = NULL;
            }
        } else {
            // Skip remaining args
            if (arg_len > 0 && !buffer_seek_cur(buf, arg_len)) {
                return SCRIPT_ARG_READ_ERROR;
            }
        }
    }

    return PARSING_OK;
}

parser_status_e multisig_payload_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_MULTISIG) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // Read multisig address
    if (!bcs_read_fixed_bytes(buf, tx->multisig_meta.multisig_address, ADDRESS_LEN)) {
        return MULTISIG_ADDRESS_READ_ERROR;
    }

    // Read option tag for inner entry function
    bool has_inner = false;
    if (!bcs_read_bool(buf, &has_inner)) {
        return MULTISIG_OPTION_READ_ERROR;
    }
    tx->multisig_meta.has_inner_entry_function = has_inner;

    if (has_inner) {
        // Read MultisigTransactionPayload enum discriminant
        // MultisigTransactionPayload::EntryFunction is variant 0 (not the outer TransactionPayload
        // variant)
        uint32_t inner_variant = 0;
        if (!bcs_read_u32_from_uleb128(buf, &inner_variant)) {
            return PAYLOAD_VARIANT_READ_ERROR;
        }
        if (inner_variant != 0) {
            return PAYLOAD_UNDEFINED_ERROR;
        }
        // Temporarily set payload_variant so inner deserializer works
        tx->payload_variant = PAYLOAD_ENTRY_FUNCTION;
        parser_status_e status = entry_function_payload_deserialize(buf, tx);
        // Restore multisig variant
        tx->payload_variant = PAYLOAD_MULTISIG;
        return status;
    }

    return PARSING_OK;
}

parser_status_e multisig_create_transaction_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    args_multisig_create_t *create = &payload->args.multisig_create;

    // Read type args size (should be 0)
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // Read args count (should be 2: multisig_address + payload_bytes)
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 2) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // Arg 1: multisig address
    uint32_t addr_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &addr_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (addr_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    if (!bcs_read_fixed_bytes(buf, create->multisig_address, ADDRESS_LEN)) {
        return MULTISIG_ADDRESS_READ_ERROR;
    }

    // Arg 2: BCS-encoded MultisigTransactionPayload
    uint32_t payload_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &payload_len)) {
        return GENERIC_ARG_LEN_READ_ERROR;
    }

    // Get pointer to the raw inner payload bytes
    uint8_t *inner_ptr = NULL;
    if (!bcs_read_ptr_to_fixed_bytes(buf, &inner_ptr, payload_len)) {
        return GENERIC_ARG_BYTES_READ_ERROR;
    }

    // Parse the inner payload: MultisigTransactionPayload enum
    buffer_t inner_buf = {.ptr = inner_ptr, .size = payload_len, .offset = 0};

    // Read MultisigTransactionPayload variant (0 = EntryFunction)
    uint32_t inner_variant = 0;
    if (!bcs_read_u32_from_uleb128(&inner_buf, &inner_variant)) {
        return PAYLOAD_VARIANT_READ_ERROR;
    }
    if (inner_variant != 0) {
        return PAYLOAD_UNDEFINED_ERROR;
    }

    // Parse inner entry function: module_id + function_name
    if (!bcs_read_fixed_bytes(&inner_buf, create->inner_module_address, ADDRESS_LEN)) {
        return MODULE_ID_ADDR_READ_ERROR;
    }
    if (!bcs_read_u32_from_uleb128(&inner_buf, (uint32_t *) &create->inner_module_name.len)) {
        return MODULE_ID_NAME_LEN_READ_ERROR;
    }
    if (!bcs_read_ptr_to_fixed_bytes(&inner_buf,
                                     &create->inner_module_name.bytes,
                                     create->inner_module_name.len)) {
        return MODULE_ID_NAME_BYTES_READ_ERROR;
    }
    if (!bcs_read_u32_from_uleb128(&inner_buf, (uint32_t *) &create->inner_function_name.len)) {
        return FUNCTION_NAME_LEN_READ_ERROR;
    }
    if (!bcs_read_ptr_to_fixed_bytes(&inner_buf,
                                     &create->inner_function_name.bytes,
                                     create->inner_function_name.len)) {
        return FUNCTION_NAME_BYTES_READ_ERROR;
    }

    // Skip inner type args
    parser_status_e status = skip_all_type_args(&inner_buf);
    if (status != PARSING_OK) {
        return status;
    }

    // Parse inner function args generically
    uint32_t num_inner_args = 0;
    if (!bcs_read_u32_from_uleb128(&inner_buf, &num_inner_args)) {
        return ARGS_SIZE_READ_ERROR;
    }
    create->inner_args.num_args = num_inner_args;
    create->inner_args.num_parsed =
        (num_inner_args < MAX_GENERIC_ARGS) ? num_inner_args : MAX_GENERIC_ARGS;

    for (size_t i = 0; i < create->inner_args.num_parsed; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(&inner_buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        create->inner_args.args[i].raw_len = arg_len;
        if (arg_len > 0) {
            if (!bcs_read_ptr_to_fixed_bytes(&inner_buf,
                                             &create->inner_args.args[i].raw_ptr,
                                             arg_len)) {
                return GENERIC_ARG_BYTES_READ_ERROR;
            }
        } else {
            create->inner_args.args[i].raw_ptr = NULL;
        }
        create->inner_args.args[i].type = infer_arg_type(arg_len);
    }

    // Skip remaining inner args
    for (size_t i = create->inner_args.num_parsed; i < num_inner_args; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(&inner_buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        if (arg_len > 0) {
            if (!buffer_seek_cur(&inner_buf, arg_len)) {
                return GENERIC_ARG_BYTES_READ_ERROR;
            }
        }
    }

    return PARSING_OK;
}

parser_status_e multisig_create_hash_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    args_multisig_create_hash_t *create = &payload->args.multisig_create_hash;

    // Read type args (should be 0)
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // Read args count (should be 2)
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.args_size != 2) {
        return ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // Arg 1: multisig address
    uint32_t addr_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &addr_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (addr_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    if (!bcs_read_fixed_bytes(buf, create->multisig_address, ADDRESS_LEN)) {
        return MULTISIG_ADDRESS_READ_ERROR;
    }

    // Arg 2: payload hash (variable-length bytes)
    if (!bcs_read_u32_from_uleb128(buf, &create->payload_hash_len)) {
        return GENERIC_ARG_LEN_READ_ERROR;
    }
    if (create->payload_hash_len > 0) {
        if (!bcs_read_ptr_to_fixed_bytes(buf, &create->payload_hash, create->payload_hash_len)) {
            return GENERIC_ARG_BYTES_READ_ERROR;
        }
    } else {
        create->payload_hash = NULL;
    }

    return PARSING_OK;
}

parser_status_e multisig_vote_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    args_multisig_vote_t *vote = &payload->args.multisig_vote;

    // 0 type args
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // 2 or 3 args (approve/reject=2, vote=3 with bool)
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }

    // Arg 1: multisig address
    uint32_t addr_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &addr_len)) {
        return RECEIVER_ADDR_LEN_READ_ERROR;
    }
    if (addr_len != ADDRESS_LEN) {
        return WRONG_ADDRESS_LEN_ERROR;
    }
    if (!bcs_read_fixed_bytes(buf, vote->multisig_address, ADDRESS_LEN)) {
        return MULTISIG_ADDRESS_READ_ERROR;
    }

    // Arg 2: sequence number (u64)
    uint32_t seq_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &seq_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }
    if (seq_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    if (!bcs_read_u64(buf, &vote->sequence_number)) {
        return AMOUNT_READ_ERROR;
    }

    // Skip remaining args (vote_transaction has a bool arg3)
    for (size_t i = 2; i < payload->args.args_size; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        if (arg_len > 0 && !buffer_seek_cur(buf, arg_len)) {
            return GENERIC_ARG_BYTES_READ_ERROR;
        }
    }

    return PARSING_OK;
}

parser_status_e multisig_create_with_owners_deserialize(buffer_t *buf, transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return PAYLOAD_UNDEFINED_ERROR;
    }
    entry_function_payload_t *payload = &tx->payload.entry_function;
    args_multisig_create_with_owners_t *owners = &payload->args.multisig_owners;

    // 0 type args
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.ty_size)) {
        return TYPE_ARGS_SIZE_READ_ERROR;
    }
    if (payload->args.ty_size != 0) {
        return TYPE_ARGS_SIZE_UNEXPECTED_ERROR;
    }

    // 4 args: vector<address>, u64, vector<String>, vector<vector<u8>>
    if (!bcs_read_u32_from_uleb128(buf, (uint32_t *) &payload->args.args_size)) {
        return ARGS_SIZE_READ_ERROR;
    }

    // Arg 1: vector<address> (BCS: ULEB128 length prefix for the arg, then ULEB128 count, then
    // addresses)
    uint32_t owners_arg_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &owners_arg_len)) {
        return GENERIC_ARG_LEN_READ_ERROR;
    }
    // Save position to handle the arg as a sub-buffer
    uint8_t *owners_arg_ptr = NULL;
    if (!bcs_read_ptr_to_fixed_bytes(buf, &owners_arg_ptr, owners_arg_len)) {
        return GENERIC_ARG_BYTES_READ_ERROR;
    }
    // Parse the owners vector from the sub-buffer
    buffer_t owners_buf = {.ptr = owners_arg_ptr, .size = owners_arg_len, .offset = 0};
    uint32_t num_owners = 0;
    if (!bcs_read_u32_from_uleb128(&owners_buf, &num_owners)) {
        return ARGS_SIZE_READ_ERROR;
    }
    owners->num_owners = num_owners;
    owners->num_owners_displayed =
        (num_owners < MAX_MULTISIG_OWNERS) ? num_owners : MAX_MULTISIG_OWNERS;
    for (size_t i = 0; i < owners->num_owners_displayed; i++) {
        if (!bcs_read_fixed_bytes(&owners_buf, owners->owners[i], ADDRESS_LEN)) {
            return RECEIVER_ADDR_READ_ERROR;
        }
    }

    // Arg 2: num_signatures_required (u64)
    uint32_t sig_len = 0;
    if (!bcs_read_u32_from_uleb128(buf, &sig_len)) {
        return AMOUNT_LEN_READ_ERROR;
    }
    if (sig_len != sizeof(uint64_t)) {
        return WRONG_AMOUNT_LEN_ERROR;
    }
    if (!bcs_read_u64(buf, &owners->num_signatures_required)) {
        return AMOUNT_READ_ERROR;
    }

    // Skip remaining args (metadata vectors)
    for (size_t i = 2; i < payload->args.args_size; i++) {
        uint32_t arg_len = 0;
        if (!bcs_read_u32_from_uleb128(buf, &arg_len)) {
            return GENERIC_ARG_LEN_READ_ERROR;
        }
        if (arg_len > 0 && !buffer_seek_cur(buf, arg_len)) {
            return GENERIC_ARG_BYTES_READ_ERROR;
        }
    }

    return PARSING_OK;
}

entry_function_known_type_t determine_function_type(transaction_t *tx) {
    if (tx->payload_variant != PAYLOAD_ENTRY_FUNCTION) {
        return FUNC_UNKNOWN;
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "aptos_account", 13) &&
        bcs_cmp_bytes(&tx->payload.entry_function.function_name, "transfer", 8)) {
        return FUNC_APTOS_ACCOUNT_TRANSFER;
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "coin", 4) &&
        bcs_cmp_bytes(&tx->payload.entry_function.function_name, "transfer", 8)) {
        return FUNC_COIN_TRANSFER;
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "aptos_account", 13) &&
        bcs_cmp_bytes(&tx->payload.entry_function.function_name, "transfer_coins", 14)) {
        return FUNC_APTOS_ACCOUNT_TRANSFER_COINS;
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "primary_fungible_store", 22) &&
        bcs_cmp_bytes(&tx->payload.entry_function.function_name, "transfer", 8)) {
        return FUNC_FUNGIBLE_STORE_TRANSFER;
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "multisig_account", 16)) {
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "create_transaction",
                          18)) {
            return FUNC_MULTISIG_CREATE_TRANSACTION;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "create_transaction_with_hash",
                          27)) {
            return FUNC_MULTISIG_CREATE_WITH_HASH;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "approve_transaction",
                          19)) {
            return FUNC_MULTISIG_APPROVE;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "reject_transaction",
                          18)) {
            return FUNC_MULTISIG_REJECT;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "vote_transaction",
                          16) ||
            bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "vote_transanction",
                          17)) {
            return FUNC_MULTISIG_VOTE;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "create_with_owners",
                          18) ||
            bcs_cmp_bytes(&tx->payload.entry_function.function_name,
                          "create_with_owners_then_remove_bootstrapper",
                          43)) {
            return FUNC_MULTISIG_CREATE_WITH_OWNERS;
        }
    }

    if (tx->payload.entry_function.module_id.address[ADDRESS_LEN - 1] == 0x01 &&
        bcs_cmp_bytes(&tx->payload.entry_function.module_id.name, "delegation_pool", 15)) {
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name, "add_stake", 9)) {
            return FUNC_ADD_STAKE;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name, "unlock", 6)) {
            return FUNC_UNLOCK_STAKE;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name, "reactivate_stake", 16)) {
            return FUNC_REACTIVATE_STAKE;
        }
        if (bcs_cmp_bytes(&tx->payload.entry_function.function_name, "withdraw", 8)) {
            return FUNC_WITHDRAW_STAKE;
        }
    }

    return FUNC_UNKNOWN;
}
