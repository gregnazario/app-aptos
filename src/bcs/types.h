#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Maximum length allowed for sequence (vectors, bytes, strings) and maps
#define MAX_SEQUENCE_LENGTH ((1ull << 31) - 1)
// Maximum number of nested structs and enum variants
#define MAX_CONTAINER_DEPTH 500
// Address size
#define ADDRESS_LEN 32
// default coin module
#define APTOS_COIN "0x1::aptos_coin::AptosCoin"
// prefix for RawTransaction
#define RAW_TRANSACTION_SALT "APTOS::RawTransaction"
// prefix for MultiAgentRawTransaction
#define RAW_TRANSACTION_WITH_DATA_SALT "APTOS::RawTransactionWithData"
// size of hashed prefix
#define TX_HASHED_PREFIX_LEN 32
// size in bytes of data (max_gas_amount + gas_unit_price + expiration_timestamp_secs + chain_id)
// at the end of the transaction
#define TX_FOOTER_LEN 25
// sha3-256 hash of the RAW_TRANSACTION_SALT
static const uint8_t PREFIX_RAW_TX_HASHED[] = {
    181, 233, 125, 176, 127, 160, 189, 14,  85,  152, 170, 54, 67,  169, 188, 111,
    102, 147, 189, 220, 26,  159, 236, 158, 103, 74,  70,  30, 170, 0,   177, 147};
// sha3-256 hash of the RAW_TRANSACTION_WITH_DATA_SALT
static const uint8_t PREFIX_RAW_TX_WITH_DATA_HASHED[] = {
    94, 250, 60, 79,  2,   248, 58, 15,  75,  45,  105, 252, 149, 198, 7,   204,
    2,  130, 92, 196, 231, 190, 83, 110, 240, 153, 45,  240, 80,  217, 230, 124};

typedef struct {
    uint64_t high;
    uint64_t low;
} uint128_t;

typedef struct {
    int64_t high;
    uint64_t low;
} int128_t;

typedef struct {
    uint8_t *bytes;
    size_t len;
} fixed_bytes_t;

typedef enum {
    TYPE_TAG_BOOL = 0,
    TYPE_TAG_U8 = 1,
    TYPE_TAG_U64 = 2,
    TYPE_TAG_U128 = 3,
    TYPE_TAG_ADDRESS = 4,
    TYPE_TAG_SIGNER = 5,
    TYPE_TAG_VECTOR = 6,
    TYPE_TAG_STRUCT = 7,
    TYPE_TAG_U16 = 8,
    TYPE_TAG_U32 = 9,
    TYPE_TAG_U256 = 10,
    TYPE_TAG_UNDEFINED = 1000
} type_tag_variant_t;

typedef struct {
    type_tag_variant_t type_tag;
    size_t size;
    void *value;
} type_tag_t;

typedef struct {
    uint8_t address[ADDRESS_LEN];
    fixed_bytes_t module_name;
    fixed_bytes_t name;
    size_t type_args_size;
    type_tag_t *type_args;
} type_tag_struct_t;

typedef struct {
    uint8_t address[ADDRESS_LEN];
    fixed_bytes_t name;
} module_id_t;

typedef struct {
    uint8_t address[ADDRESS_LEN];
} fungible_asset_store_t;

// Maximum number of generic args to display for unknown entry functions and scripts
#define MAX_GENERIC_ARGS 6
// Maximum recursion depth for skipping type tags
#define MAX_TYPE_TAG_DEPTH 8

// Inferred type for generic entry function arguments (based on BCS byte length)
typedef enum {
    ARG_TYPE_BOOL = 0,
    ARG_TYPE_U8,
    ARG_TYPE_U16,
    ARG_TYPE_U32,
    ARG_TYPE_U64,
    ARG_TYPE_U128,
    ARG_TYPE_ADDRESS,
    ARG_TYPE_BYTES,
} generic_arg_type_t;

typedef struct {
    generic_arg_type_t type;  // inferred from raw_len
    uint32_t raw_len;         // BCS length-prefix value
    uint8_t *raw_ptr;         // zero-copy pointer into raw_tx
} generic_arg_t;

typedef struct {
    size_t num_args;    // total args in the function
    size_t num_parsed;  // min(num_args, MAX_GENERIC_ARGS)
    generic_arg_t args[MAX_GENERIC_ARGS];
} args_generic_t;

// Self-typed script argument variants (TransactionArgument enum in Aptos)
typedef enum {
    SCRIPT_ARG_U8 = 0,
    SCRIPT_ARG_U64 = 1,
    SCRIPT_ARG_U128 = 2,
    SCRIPT_ARG_ADDRESS = 3,
    SCRIPT_ARG_U8_VECTOR = 4,
    SCRIPT_ARG_BOOL = 5,
    SCRIPT_ARG_U16 = 6,
    SCRIPT_ARG_U32 = 7,
    SCRIPT_ARG_U256 = 8,
} script_arg_variant_t;

typedef struct {
    script_arg_variant_t variant;
    uint8_t *raw_ptr;
    uint32_t raw_len;
} script_arg_t;

typedef struct {
    uint32_t code_len;
    size_t num_args;
    size_t num_parsed;
    script_arg_t args[MAX_GENERIC_ARGS];
} script_payload_parsed_t;

// Multisig payload metadata (stored outside payload union)
typedef struct {
    uint8_t multisig_address[ADDRESS_LEN];
    bool has_inner_entry_function;
} multisig_metadata_t;

typedef enum {
    FUNC_UNKNOWN = 0,
    FUNC_APTOS_ACCOUNT_TRANSFER = 1,
    FUNC_COIN_TRANSFER = 2,
    FUNC_APTOS_ACCOUNT_TRANSFER_COINS = 3,
    FUNC_FUNGIBLE_STORE_TRANSFER = 4,
    FUNC_ADD_STAKE = 5,
    FUNC_UNLOCK_STAKE = 6,
    FUNC_REACTIVATE_STAKE = 7,
    FUNC_WITHDRAW_STAKE = 8,
    FUNC_MULTISIG_CREATE_TRANSACTION = 9,
    FUNC_MULTISIG_CREATE_WITH_HASH = 10,
    FUNC_MULTISIG_APPROVE = 11,
    FUNC_MULTISIG_REJECT = 12,
    FUNC_MULTISIG_CREATE_WITH_OWNERS = 13,
    FUNC_MULTISIG_VOTE = 14,
} entry_function_known_type_t;

typedef struct {
    type_tag_t *ty_args;
    fixed_bytes_t *args;
} args_raw_t;

typedef struct {
    uint8_t receiver[ADDRESS_LEN];
    uint64_t amount;
} args_aptos_account_transfer_t;

typedef struct {
    uint8_t receiver[ADDRESS_LEN];
    uint64_t amount;
    type_tag_struct_t ty_coin;
} args_coin_transfer_t;

typedef struct {
    uint8_t receiver[ADDRESS_LEN];
    uint64_t amount;
    type_tag_struct_t ty_args;
    fungible_asset_store_t fungible_asset;
} args_fungible_asset_transfer_t;

typedef struct {
    uint8_t pool[ADDRESS_LEN];
    uint64_t amount;
} args_delegation_pool_transfer_t;

// Args for 0x1::multisig_account::create_transaction
// Decodes the inner entry function from arg2's BCS blob
typedef struct {
    uint8_t multisig_address[ADDRESS_LEN];
    // Decoded inner entry function from arg2
    uint8_t inner_module_address[ADDRESS_LEN];
    fixed_bytes_t inner_module_name;
    fixed_bytes_t inner_function_name;
    args_generic_t inner_args;
} args_multisig_create_t;

// Args for 0x1::multisig_account::create_transaction_with_hash
typedef struct {
    uint8_t multisig_address[ADDRESS_LEN];
    uint8_t *payload_hash;  // pointer into raw_tx
    uint32_t payload_hash_len;
} args_multisig_create_hash_t;

// Args for approve_transaction, reject_transaction, vote_transaction
typedef struct {
    uint8_t multisig_address[ADDRESS_LEN];
    uint64_t sequence_number;
} args_multisig_vote_t;

// Args for create_with_owners
#define MAX_MULTISIG_OWNERS 6
typedef struct {
    size_t num_owners;
    size_t num_owners_displayed;  // min(num_owners, MAX_MULTISIG_OWNERS)
    uint8_t owners[MAX_MULTISIG_OWNERS][ADDRESS_LEN];
    uint64_t num_signatures_required;
} args_multisig_create_with_owners_t;

typedef struct {
    module_id_t module_id;
    fixed_bytes_t function_name;
    entry_function_known_type_t known_type;
    struct {
        size_t ty_size;
        size_t args_size;
        union {
            args_raw_t raw;
            args_aptos_account_transfer_t transfer;
            args_coin_transfer_t coin_transfer;
            args_fungible_asset_transfer_t fa_transfer;
            args_delegation_pool_transfer_t delegation;
            args_generic_t generic;
            args_multisig_create_t multisig_create;
            args_multisig_create_hash_t multisig_create_hash;
            args_multisig_vote_t multisig_vote;
            args_multisig_create_with_owners_t multisig_owners;
        };
    } args;
} entry_function_payload_t;

typedef struct {
    fixed_bytes_t code;
    size_t ty_size;
    type_tag_t *ty_args;
    size_t args_size;
    fixed_bytes_t *args;
} script_payload_t;

typedef enum {
    TX_RAW = 0,
    TX_RAW_WITH_DATA = 1,
    TX_MESSAGE = 2,
    TX_RAW_MESSAGE = 3,
    TX_UNDEFINED = 1000
} tx_variant_t;

typedef enum {
    PAYLOAD_SCRIPT = 0,
    PAYLOAD_ENTRY_FUNCTION = 2,
    PAYLOAD_MULTISIG = 3,
    PAYLOAD_UNDEFINED = 1000
} payload_variant_t;

typedef struct {
    tx_variant_t tx_variant;
    uint8_t sender[ADDRESS_LEN];
    uint64_t sequence;
    payload_variant_t payload_variant;
    multisig_metadata_t multisig_meta;  // valid when payload_variant == PAYLOAD_MULTISIG
    union {
        script_payload_t script;
        script_payload_parsed_t script_parsed;
        entry_function_payload_t entry_function;
    } payload;
    uint64_t max_gas_amount;
    uint64_t gas_unit_price;
    uint64_t expiration_timestamp_secs;
    uint8_t chain_id;
} aptos_transaction_t;
