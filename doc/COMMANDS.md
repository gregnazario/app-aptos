# Aptos commands

## Overview

| Command name     | INS  | Description                                           |
| ---------------- | ---- | ----------------------------------------------------- |
| `GET_VERSION`    | 0x03 | Get application version as `MAJOR`, `MINOR`, `PATCH`  |
| `GET_APP_NAME`   | 0x04 | Get ASCII encoded application name                    |
| `GET_PUBLIC_KEY` | 0x05 | Get public key given BIP32 path                       |
| `SIGN_TX`        | 0x06 | Sign transaction given BIP32 path and raw transaction |

## GET_VERSION

### Command

| CLA  | INS  | P1   | P2   | Lc   | CData |
| ---- | ---- | ---- | ---- | ---- | ----- |
| 0x5B | 0x03 | 0x00 | 0x00 | 0x00 | -     |

### Response

| Response length (bytes) | SW     | RData                                         |
| ----------------------- | ------ | --------------------------------------------- |
| 3                       | 0x9000 | `MAJOR (1)` \|\| `MINOR (1)` \|\| `PATCH (1)` |

## GET_APP_NAME

### Command

| CLA  | INS  | P1   | P2   | Lc   | CData |
| ---- | ---- | ---- | ---- | ---- | ----- |
| 0x5B | 0x04 | 0x00 | 0x00 | 0x00 | -     |

### Response

| Response length (bytes) | SW     | RData           |
| ----------------------- | ------ | --------------- |
| var                     | 0x9000 | `APPNAME (var)` |

## GET_PUBLIC_KEY

### Command

| CLA  | INS  | P1                                    | P2   | Lc     | CData                                                                                        |
| ---- | ---- | ------------------------------------- | ---- | ------ | -------------------------------------------------------------------------------------------- |
| 0x5B | 0x05 | 0x00 (no display) <br> 0x01 (display) | 0x00 | 1 + 4n | `len(bip32_path) (1)` \|\|<br> `bip32_path{1} (4)` \|\|<br>`...` \|\|<br>`bip32_path{n} (4)` |

### Response

| Response length (bytes) | SW     | RData                                                                                                        |
| ----------------------- | ------ | ------------------------------------------------------------------------------------------------------------ |
| var                     | 0x9000 | `len(public_key) (1)` \|\|<br> `public_key (var)` \|\|<br> `len(chain_code) (1)` \|\|<br> `chain_code (var)` |

## SIGN_TX

### Command

_Note:_ The maximum number of chunks depends on the maximum available RAM on each device and is equal to N.

| Device           | N (hex) | N (decimal) | Max TX size |
| ---------------- | ------- | ----------- | ----------- |
| Ledger Nano S+   | 0x6A    | 106         | 27,030 bytes |
| Ledger Nano X    | 0x5F    | 95          | 24,225 bytes |
| Stax/Flex/Apex P | 0x52    | 82          | 20,910 bytes |

| CLA  | INS  | P1   | P2          | Lc     | CData                                                                                        |
| ---- | ---- | ---- | ----------- | ------ | -------------------------------------------------------------------------------------------- |
| 0x5B | 0x06 | 0x00 | 0x80 (more) | 1 + 4n | `len(bip32_path) (1)` \|\|<br> `bip32_path{1} (4)` \|\|<br>`...` \|\|<br>`bip32_path{n} (4)` |

| CLA  | INS  | P1                   | P2                           | Lc  | CData                       |
| ---- | ---- | -------------------- | ---------------------------- | --- | --------------------------- |
| 0x5B | 0x06 | 0x01-N (chunk index) | 0x80 (more) <br> 0x00 (last) | var | `serialized_tx_chunk (var)` |

### Response

| Response length (bytes) | SW     | RData                                            |
| ----------------------- | ------ | ------------------------------------------------ |
| var                     | 0x9000 | `len(signature) (1)` \|\| <br> `signature (var)` |

## Status Words

| SW     | SW name                      | Description                                 |
| ------ | ---------------------------- | ------------------------------------------- |
| 0x9000 | `OK`                         | Success                                     |
| 0x6985 | `SW_DENY`                    | Rejected by user                            |
| 0x6A86 | `SW_WRONG_P1P2`              | Either `P1` or `P2` is incorrect            |
| 0x6A87 | `SW_WRONG_DATA_LENGTH`       | `Lc` or minimum APDU length is incorrect    |
| 0x6D00 | `SW_INS_NOT_SUPPORTED`       | No command exists with `INS`                |
| 0x6E00 | `SW_CLA_NOT_SUPPORTED`       | Bad `CLA` used for this application         |
| 0xB000 | `SW_WRONG_RESPONSE_LENGTH`   | Wrong response length (buffer size problem) |
| 0xB001 | `SW_DISPLAY_BIP32_PATH_FAIL` | BIP32 path conversion to string failed      |
| 0xB002 | `SW_DISPLAY_ADDRESS_FAIL`    | Address conversion to string failed         |
| 0xB003 | `SW_DISPLAY_AMOUNT_FAIL`     | Amount conversion to string failed          |
| 0xB004 | `SW_WRONG_TX_LENGTH`         | Wrong raw transaction length                |
| 0xB005 | `SW_TX_PARSING_FAIL`         | Failed to parse raw transaction             |
| 0xB006 | `SW_GET_PUB_KEY_FAIL`        | Failed to derive or export public key       |
| 0xB007 | `SW_BAD_STATE`               | Security issue with bad state               |
| 0xB008 | `SW_SIGNATURE_FAIL`          | Signature of raw transaction failed         |
| 0xB009 | `SW_DISPLAY_GAS_FEE_FAIL`    | Failed to display gas fee                   |
| 0xB00A | `SW_SWAP_CHECKING_FAIL`      | Failed to validate a swap transaction       |

### Error Code Reference

This section provides user-friendly explanations for each error code, including what went wrong and how to resolve it.

#### 0x9000 — Success

The command completed successfully. For `SIGN_TX`, the response contains the Ed25519 signature.

#### 0x6985 — User Denied (`SW_DENY`)

**What happened:** You rejected the transaction or address confirmation on the device screen.

**What to do:** This is expected behavior when you press "Reject" on the Ledger. If you intended to approve, retry the operation and press "Approve" instead.

#### 0x6A86 — Wrong Parameters (`SW_WRONG_P1P2`)

**What happened:** The APDU command used invalid P1 or P2 values. During `SIGN_TX`, this means a chunk was sent out of order (e.g., chunk 3 sent after chunk 1, skipping chunk 2).

**What to do:** This indicates a bug in the wallet software (Ledger Live, Petra, etc.). Try updating your wallet app. If the error persists, report it to the wallet developer.

#### 0x6A87 — Wrong Data Length (`SW_WRONG_DATA_LENGTH`)

**What happened:** The APDU command data was too short or had an invalid length. For `SIGN_TX` chunk 0, this means the BIP32 derivation path was malformed.

**What to do:** Check that the wallet is using the correct Aptos derivation path (`44'/637'/...`). This usually indicates a wallet software bug.

#### 0x6D00 — Unsupported Instruction (`SW_INS_NOT_SUPPORTED`)

**What happened:** The wallet sent a command the Aptos app doesn't recognize.

**What to do:** Make sure you have the Aptos app open on the Ledger (not another blockchain app). Update the Aptos app to the latest version via Ledger Live.

#### 0x6E00 — Wrong Application (`SW_CLA_NOT_SUPPORTED`)

**What happened:** The command was sent to the wrong application. The Aptos app expects CLA=0x5B.

**What to do:** Ensure the Aptos app is open on your Ledger device before initiating the transaction.

#### 0xB000 — Response Buffer Error (`SW_WRONG_RESPONSE_LENGTH`)

**What happened:** Internal error — the response data doesn't fit in the output buffer.

**What to do:** This is a bug in the Ledger app. Please report it with the steps to reproduce.

#### 0xB001 — BIP32 Path Display Failed (`SW_DISPLAY_BIP32_PATH_FAIL`)

**What happened:** The derivation path could not be formatted for display on screen.

**What to do:** The wallet may be using an unusually long or non-standard BIP32 path. Try the default path `m/44'/637'/0'/0'/0'`.

#### 0xB002 — Address Display Failed (`SW_DISPLAY_ADDRESS_FAIL`)

**What happened:** An Aptos address in the transaction could not be formatted as a hex string for display.

**What to do:** This is a bug in the Ledger app. Please report it with the transaction details.

#### 0xB003 — Amount Display Failed (`SW_DISPLAY_AMOUNT_FAIL`)

**What happened:** A token amount in the transaction could not be converted to a human-readable decimal string.

**What to do:** This is a bug in the Ledger app. Please report it with the transaction details.

#### 0xB004 — Transaction Too Large (`SW_WRONG_TX_LENGTH`)

**What happened:** The serialized transaction exceeds the maximum buffer size for your device.

| Device         | Max transaction size |
| -------------- | -------------------- |
| Nano S Plus    | 27,030 bytes (106 chunks) |
| Nano X         | 24,225 bytes (95 chunks) |
| Stax/Flex/Apex P | 20,910 bytes (82 chunks) |

**What to do:** The transaction payload is too large to fit in device memory. This typically happens with:
- Move package publish transactions (compiled bytecode can be very large)
- Batch operations with hundreds of entries
- Transactions with large vector arguments

There is no workaround on the device side — the transaction must be smaller, or signed through a different mechanism.

#### 0xB005 — Transaction Parsing Failed (`SW_TX_PARSING_FAIL`)

**What happened:** The raw transaction bytes could not be deserialized. The BCS (Binary Canonical Serialization) data is malformed or uses an unrecognized format.

**Common causes:**
- The transaction was serialized with an incompatible version of the Aptos SDK
- The transaction uses a payload type the app doesn't support
- The transaction data was corrupted during transmission
- A multisig inner payload uses an unsupported variant (only EntryFunction is supported)

**Internal parser error codes** (for developers — these are not returned as status words but logged internally):

| Code   | Name                                  | Meaning                                              |
| ------ | ------------------------------------- | ---------------------------------------------------- |
| 1      | `PARSING_OK`                          | Success                                              |
| -1     | `HASHED_PREFIX_READ_ERROR`            | Could not read 32-byte transaction prefix            |
| -2     | `SENDER_READ_ERROR`                   | Could not read sender address                        |
| -3     | `SEQUENCE_READ_ERROR`                 | Could not read sequence number                       |
| -4     | `MAX_GAS_READ_ERROR`                  | Could not read max gas amount                        |
| -5     | `GAS_UNIT_PRICE_READ_ERROR`           | Could not read gas unit price                        |
| -6     | `EXPIRATION_READ_ERROR`               | Could not read expiration timestamp                  |
| -7     | `CHAIN_ID_READ_ERROR`                 | Could not read chain ID                              |
| -8     | `PAYLOAD_VARIANT_READ_ERROR`          | Could not read payload type discriminant             |
| -9     | `PAYLOAD_UNDEFINED_ERROR`             | Unknown or unsupported payload type                  |
| -10    | `MODULE_ID_ADDR_READ_ERROR`           | Could not read module address                        |
| -11    | `MODULE_ID_NAME_LEN_READ_ERROR`       | Could not read module name length                    |
| -12    | `MODULE_ID_NAME_BYTES_READ_ERROR`     | Could not read module name                           |
| -13    | `FUNCTION_NAME_LEN_READ_ERROR`        | Could not read function name length                  |
| -14    | `FUNCTION_NAME_BYTES_READ_ERROR`      | Could not read function name                         |
| -15    | `TYPE_ARGS_SIZE_READ_ERROR`           | Could not read type arguments count                  |
| -16    | `TYPE_ARGS_SIZE_UNEXPECTED_ERROR`     | Wrong number of type arguments for known function    |
| -17    | `ARGS_SIZE_READ_ERROR`                | Could not read function arguments count              |
| -18    | `ARGS_SIZE_UNEXPECTED_ERROR`          | Wrong number of arguments for known function         |
| -19    | `RECEIVER_ADDR_LEN_READ_ERROR`        | Could not read receiver address length prefix        |
| -20    | `WRONG_ADDRESS_LEN_ERROR`             | Address length is not 32 bytes                       |
| -21    | `RECEIVER_ADDR_READ_ERROR`            | Could not read receiver address                      |
| -22    | `AMOUNT_LEN_READ_ERROR`               | Could not read amount length prefix                  |
| -23    | `WRONG_AMOUNT_LEN_ERROR`              | Amount length is not 8 bytes (expected u64)          |
| -24    | `AMOUNT_READ_ERROR`                   | Could not read amount value                          |
| -25    | `TYPE_TAG_READ_ERROR`                 | Could not read type tag variant                      |
| -26    | `TYPE_TAG_UNEXPECTED_ERROR`           | Unexpected type tag (expected struct for coin type)  |
| -27    | `STRUCT_ADDRESS_READ_ERROR`           | Could not read struct type address                   |
| -28    | `STRUCT_MODULE_LEN_READ_ERROR`        | Could not read struct module name length             |
| -29    | `STRUCT_MODULE_BYTES_READ_ERROR`      | Could not read struct module name                    |
| -30    | `STRUCT_NAME_LEN_READ_ERROR`          | Could not read struct name length                    |
| -31    | `STRUCT_NAME_BYTES_READ_ERROR`        | Could not read struct name                           |
| -32    | `STRUCT_TYPE_ARGS_SIZE_READ_ERROR`    | Could not read struct type arguments count           |
| -33    | `STRUCT_TYPE_ARGS_SIZE_UNEXPECTED_ERROR` | Struct has unexpected nested type arguments        |
| -34    | `TX_VARIANT_READ_ERROR`               | Could not read transaction variant prefix            |
| -35    | `TX_VARIANT_UNDEFINED_ERROR`          | Transaction variant mismatch                         |
| -40    | `GENERIC_ARG_LEN_READ_ERROR`          | Could not read generic argument length               |
| -41    | `GENERIC_ARG_BYTES_READ_ERROR`        | Could not read generic argument data                 |
| -42    | `TYPE_TAG_SKIP_ERROR`                 | Failed to skip over type tag (nesting too deep or malformed) |
| -43    | `SCRIPT_CODE_LEN_READ_ERROR`          | Could not read script bytecode length                |
| -44    | `SCRIPT_CODE_READ_ERROR`              | Could not read script bytecode                       |
| -45    | `SCRIPT_ARG_VARIANT_READ_ERROR`       | Could not read script argument type tag              |
| -46    | `SCRIPT_ARG_READ_ERROR`               | Could not read script argument value                 |
| -47    | `MULTISIG_ADDRESS_READ_ERROR`         | Could not read multisig account address              |
| -48    | `MULTISIG_OPTION_READ_ERROR`          | Could not read multisig optional payload flag        |
| -2000  | `WRONG_LENGTH_ERROR`                  | Transaction data exceeds maximum allowed length      |

**What to do:** Try updating both the wallet software and the Ledger Aptos app. If the error persists, the transaction format may be incompatible with this app version.

#### 0xB006 — Public Key Derivation Failed (`SW_GET_PUB_KEY_FAIL`)

**What happened:** The Ed25519 public key could not be derived from the provided BIP32 path, or the path doesn't conform to the Aptos derivation scheme (`44'/637'/...`).

**What to do:** Ensure the wallet uses a valid Aptos BIP32 path. The standard path is `m/44'/637'/0'/0'/0'`.

#### 0xB007 — Bad State (`SW_BAD_STATE`)

**What happened:** The app received a command in an unexpected state. For example, trying to display a transaction that hasn't been parsed yet, or sending a signing chunk without first sending the BIP32 path.

**What to do:** Restart the transaction flow from the beginning. If this happens consistently, it may indicate a bug in the wallet software's APDU sequencing.

#### 0xB008 — Signature Failed (`SW_SIGNATURE_FAIL`)

**What happened:** The Ed25519 signature computation failed internally.

**What to do:** Retry the transaction. If the error persists, the device may have a hardware issue. Try with a different Ledger device.

#### 0xB009 — Gas Fee Display Failed (`SW_DISPLAY_GAS_FEE_FAIL`)

**What happened:** The gas fee (gas_unit_price × max_gas_amount) could not be formatted for display. This can happen if the computed fee overflows.

**What to do:** Check that the transaction has reasonable gas parameters. This is typically a bug in the transaction construction.

#### 0xB00A — Swap Validation Failed (`SW_SWAP_CHECKING_FAIL`)

**What happened:** When signing a transaction initiated by Ledger's Exchange app (swap/buy/sell), the transaction details didn't match what was previously displayed and approved by the user.

**What to do:** Retry the swap operation from Ledger Live. This is a security check — the app refuses to sign if the swap parameters have changed between the approval screen and the actual signing request.
