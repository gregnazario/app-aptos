# APTOS Transaction Serialization

## Overview

All transactions executed on the Aptos blockchain must be signed. Unsigned transactions are known as [RawTransactions](https://aptos.dev/guides/creating-a-signed-transaction/#raw-transaction). They contain all the information about how to execute an operation on an account within Aptos.

In Aptos blockchain, all the data is encoded as [BCS (Binary Canonical Serialization)](https://aptos.dev/guides/creating-a-signed-transaction/#bcs).

Aptos supports many different approaches to signing a transaction, but Ledger uses only the [Ed25519](https://en.wikipedia.org/wiki/EdDSA#Ed25519) signature scheme.

## Structure

### Raw transaction

| Field                       | Size (bytes) | Description                                                                                                                                      |
| --------------------------- | :----------: | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `sender`                    |      32      | Account address of the sender                                                                                                                    |
| `sequence_number`           |      8       | Sequence number of this transaction. This must match the sequence number stored in the sender's account at the time the transaction executes     |
| `payload`                   |     var      | Instructions for the Aptos blockchain, including publishing a module, execute a script function or execute a script payload                      |
| `max_gas_amount`            |      8       | Maximum total gas to spend for this transaction. The account must have more than this gas or the transaction will be discarded during validation |
| `gas_unit_price`            |      8       | Price to be paid per gas unit                                                                                                                    |
| `expiration_timestamp_secs` |      8       | The blockchain timestamp at which the blockchain would discard this transaction                                                                  |
| `chain_id`                  |      1       | The chain ID of the blockchain that this transaction is intended to be run on                                                                    |

### BCS (Binary Canonical Serialization)

Binary Canonical Serialization (BCS) is a serialization format applied to the raw (unsigned) transaction. BCS is not a self-descriptive format. Therefore, to deserialize a message, you need to know its type and scheme beforehand.

### Signing message

The bytes of a BCS-serialized raw transaction are referred as a **signing message**.
In addition, in Aptos, any content that is signed or hashed is salted with a unique prefix to distinguish it from other types of messages. This is done to ensure that the content can only be used in the intended scenarios. The signing message of a `RawTransaction` is prefixed with `prefix_bytes`, which is `sha3_256("APTOS::RawTransaction")`. Therefore: `signing_message = prefix_bytes | bcs_bytes_of_raw_transaction`.

Aptos also supports signing with the [MultiEd25519](https://aptos.dev/concepts/accounts#multi-signer-authentication) signature scheme, which corresponds to the salt `sha3_256("APTOS::RawTransactionWithData")`, but Ledger, at this stage, does not support deserialization of such a transaction.

Any other data is treated as a request to sign an arbitrary message, first verifying that it contains only ASCII characters.

## Payload Types

### Entry Function Payload (variant 2)

The entry function payload is the most common transaction type. After the ULEB128 payload variant, it contains:

| Field | Size | Description |
|---|:---:|---|
| `module_address` | 32 | Address of the module containing the function |
| `module_name` | ULEB128 len + bytes | Name of the module |
| `function_name` | ULEB128 len + bytes | Name of the function |
| `type_args` | ULEB128 count + tags | Type arguments (each is a recursive TypeTag enum) |
| `args` | ULEB128 count + args | Function arguments (each is ULEB128 len + raw BCS bytes) |

#### Generic Entry Function Argument Display

For unknown (generic) entry functions, the Ledger app infers argument types from the BCS byte length of each argument:

| Byte Length | Inferred Type | Display |
|:---:|---|---|
| 1 | `u8` | Decimal value |
| 2 | `u16` | Decimal value |
| 4 | `u32` | Decimal value |
| 8 | `u64` | Decimal value |
| 16 | `u128` | Hex value |
| 32 | `address` | Hex address |
| other | `bytes` | Hex dump |

Up to 6 arguments are displayed on-device. If a function has more than 6 arguments, only the first 6 are shown.

### Script Payload (variant 0)

Script payloads contain inline Move bytecode with self-typed arguments. After the ULEB128 payload variant:

| Field | Size | Description |
|---|:---:|---|
| `code` | ULEB128 len + bytes | Compiled Move script bytecode |
| `type_args` | ULEB128 count + tags | Type arguments (TypeTag enum, skipped for display) |
| `args` | ULEB128 count + args | Script arguments (TransactionArgument enum) |

Each script argument is self-typed using the `TransactionArgument` enum:

| Variant | Type | Data Size |
|:---:|---|:---:|
| 0 | U8 | 1 |
| 1 | U64 | 8 |
| 2 | U128 | 16 |
| 3 | Address | 32 |
| 4 | U8Vector | ULEB128 len + bytes |
| 5 | Bool | 1 |
| 6 | U16 | 2 |
| 7 | U32 | 4 |
| 8 | U256 | 32 |

Up to 6 arguments are displayed. Because script arguments carry their own type tag, the display labels are definitive (not inferred from length).

### Multisig Payload (variant 3)

Multisig payloads wrap an optional inner entry function for execution by a multisig account. After the ULEB128 payload variant:

| Field | Size | Description |
|---|:---:|---|
| `multisig_address` | 32 | Address of the multisig account |
| `has_payload` | 1 | Boolean: whether an inner entry function is present |
| `inner_variant` | ULEB128 | (if has_payload) MultisigTransactionPayload variant (0 = EntryFunction) |
| `inner_payload` | var | (if has_payload) Entry function fields (same layout as entry function payload above) |

When the inner payload is present, it is decoded and displayed as if it were a standalone entry function. When absent (execute-only transactions), only the multisig address is shown with a warning to verify the payload in the companion wallet.
