# Testing Guide

This guide covers how to build, test, and verify the Aptos Ledger app — including the clear signing features — on both the Speculos emulator and physical Ledger devices.

## Prerequisites

- [Docker](https://docs.docker.com/get-docker/) installed
- Python 3.8+ with pip
- A Ledger device (optional — Speculos emulator works without hardware)

Pull the Ledger build container:

```bash
docker pull ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest
```

## Building the App

Build for a specific device target inside Docker:

```bash
# Nano S Plus
docker run --rm -ti -v "$(pwd):/app" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$NANOSP_SDK make clean && BOLOS_SDK=\$NANOSP_SDK make"

# Nano X
docker run --rm -ti -v "$(pwd):/app" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$NANOX_SDK make clean && BOLOS_SDK=\$NANOX_SDK make"

# Stax
docker run --rm -ti -v "$(pwd):/app" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$STAX_SDK make clean && BOLOS_SDK=\$STAX_SDK make"

# Flex
docker run --rm -ti -v "$(pwd):/app" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$FLEX_SDK make clean && BOLOS_SDK=\$FLEX_SDK make"
```

Output: `bin/app.elf` (for emulation) and `bin/app.hex` (for device install).

## Testing on Speculos Emulator

Speculos emulates Ledger hardware on your computer — no physical device required.

### Install Speculos

```bash
pip install speculos
```

### Run the app in Speculos

```bash
# Nano S Plus emulator with GUI
speculos bin/app.elf --model nanosp --display qt

# Nano X emulator (headless, for automated tests)
speculos bin/app.elf --model nanox --display headless

# Stax emulator
speculos bin/app.elf --model stax --display qt
```

The emulator opens on `http://127.0.0.1:5000` (web UI) and listens for APDU commands on the default port.

### Run functional tests against Speculos

```bash
pip install -r tests/requirements.txt

# Run all tests
pytest tests/ -v --tb=short --device nanosp

# Run only signing tests (to verify clear signing)
pytest tests/test_sign_cmd.py -v --device nanosp

# Run with visible display (see what the emulated screen shows)
pytest tests/ -v --device nanosp --display qt
```

## Sideloading onto a Physical Ledger

**Supported devices:** Nano S Plus, Stax, Flex, Apex P (sideloading supported).

**Not supported:** Nano X (does not support sideloading — use Speculos).

### Load the app

Connect your Ledger via USB, unlock it, then:

```bash
docker run --rm -ti -v "$(pwd):/app" \
  --privileged -v "/dev/bus/usb:/dev/bus/usb" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$NANOSP_SDK make clean && BOLOS_SDK=\$NANOSP_SDK make load"
```

Confirm the installation on the device screen. The app appears as "Aptos" in the app list.

### Uninstall the app

```bash
docker run --rm -ti -v "$(pwd):/app" \
  --privileged -v "/dev/bus/usb:/dev/bus/usb" \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "BOLOS_SDK=\$NANOSP_SDK make delete"
```

## Unit Tests

Unit tests run on the host (inside Docker) without a device or emulator.

### Run tests

```bash
docker run --rm -v "$(pwd):/app" \
  -e BOLOS_SDK=/opt/nanosplus-secure-sdk \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "cp -r /app /tmp/build && cd /tmp/build/unit-tests && \
    rm -rf build && cmake -Bbuild -H. && make -C build && \
    CTEST_OUTPUT_ON_FAILURE=1 make -C build test"
```

### Run with coverage

```bash
docker run --rm -v "$(pwd):/app" \
  -e BOLOS_SDK=/opt/nanosplus-secure-sdk \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "cp -r /app /tmp/build && cd /tmp/build/unit-tests && \
    rm -rf build && cmake -Bbuild -H. && make -C build && \
    make -C build test && ./gen_coverage.sh"
```

### Run with sanitizers (ASan + UBSan)

```bash
docker run --rm -v "$(pwd):/app" \
  -e BOLOS_SDK=/opt/nanosplus-secure-sdk \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "cp -r /app /tmp/build && cd /tmp/build/unit-tests && \
    cmake -Bbuild-san -H. \
      -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1' \
      -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' \
      -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined' && \
    make -C build-san && \
    CTEST_OUTPUT_ON_FAILURE=1 make -C build-san test"
```

### Run with Valgrind

```bash
docker run --rm -v "$(pwd):/app" \
  -e BOLOS_SDK=/opt/nanosplus-secure-sdk \
  ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
  bash -c "apt-get update -qq && apt-get install -y -qq valgrind && \
    cp -r /app /tmp/build && cd /tmp/build/unit-tests && \
    rm -rf build && cmake -Bbuild -H. && make -C build && \
    valgrind --leak-check=full --error-exitcode=1 build/test_tx_parser && \
    valgrind --leak-check=full --error-exitcode=1 build/test_bcs && \
    valgrind --leak-check=full --error-exitcode=1 build/test_tx_utils"
```

## Code Quality (Makefile.quality)

These targets can run without Docker (except test/coverage/sanitize which need BOLOS_SDK headers):

```bash
# Check formatting (CI-compatible, fails on violations)
make -f Makefile.quality format-check

# Auto-format all source files
make -f Makefile.quality format

# Static analysis with cppcheck
make -f Makefile.quality lint

# Run all quality checks (format + lint + sanitize)
make -f Makefile.quality all
```

## Verifying Clear Signing

After building and running on Speculos or a physical device, you can verify that the new clear signing features work correctly.

### What to look for

| Transaction type | Before (blind) | After (clear) |
| --- | --- | --- |
| Unknown entry function | "Blind Signing" warning + function name only | Function name + up to 6 hex-encoded args with type hints |
| Script payload | "Blind Signing" warning + generic TX type | "Script execution" + self-typed args |
| `create_transaction` (multisig) | Generic hex args | Multisig address + decoded inner function + inner args |
| `approve_transaction` | Generic hex args | "Approve multisig TX" + multisig address + TX sequence |
| `reject_transaction` | Generic hex args | "Reject multisig TX" + multisig address + TX sequence |
| `create_with_owners` | Generic hex args | "Create multisig" + threshold + owner addresses |
| Multisig execute | "Blind Signing" + payload type only | "Multisig execute" + address + "Verify payload in wallet" |

### Testing with custom transactions

You can construct and sign arbitrary Aptos transactions using the Python SDK:

```python
from aptos_sdk.transactions import RawTransaction, EntryFunction, TransactionArgument
from aptos_sdk.account_address import AccountAddress
from aptos_sdk.bcs import Serializer
import hashlib

# Example: approve a multisig transaction
payload = EntryFunction.natural(
    "0x1::multisig_account",
    "approve_transaction",
    [],
    [
        TransactionArgument(
            AccountAddress.from_str("0xc0deb00c405f84c85dc13442e305df75d1288100cdd82675695f6148c7ece51c"),
            Serializer.struct
        ),
        TransactionArgument(1, Serializer.u64),
    ]
)

txn = RawTransaction(
    sender=AccountAddress.from_str("0x1234..."),  # your address
    sequence_number=0,
    payload=payload,
    max_gas_amount=10000,
    gas_unit_price=100,
    expiration_timestamps_secs=1700000000,
    chain_id=1,
)

# Serialize with the signing prefix
serializer = Serializer()
txn.serialize(serializer)
prefix = hashlib.sha3_256(b"APTOS::RawTransaction").digest()
signing_message = prefix + serializer.output()

# Send to Ledger via the test client
from tests.application_client.aptos_command_sender import AptosCommandSender
client = AptosCommandSender(backend)
with client.sign_tx(path="m/44'/637'/0'/0'/0'", transaction=signing_message):
    # Navigate the device screens to approve
    pass
```

### Expected display on device

For the `approve_transaction` example above, the Ledger should show:

**Nano S Plus / Nano X (BAGL):**
```
[Review Transaction]
[Transaction Type: Approve multisig TX]
[Multisig: 0xC0DEB00C405F84C85DC13442E305DF75...]
[Transaction: #1]
[Gas Fee: APT 0.00100000]
[Approve]  [Reject]
```

**Stax / Flex / Apex P (NBGL):**
```
Transaction type    Approve multisig TX
Multisig            0xC0DEB00C405F84C85DC13442E305DF75D1288100...
Transaction         #1
Gas fee             APT 0.00100000

                [Sign transaction?]
```

## Transaction Size Limits

If a transaction exceeds the device buffer, the app returns error `0xB004` (`SW_WRONG_TX_LENGTH`).

| Device | Max chunks | Max transaction size |
| --- | --- | --- |
| Nano S Plus | 106 | 27,030 bytes |
| Nano X | 95 | 24,225 bytes |
| Stax / Flex / Apex P | 82 | 20,910 bytes |

Transactions that commonly exceed these limits:
- Move package publish (`0x1::code::publish_package_txn`) with large bytecode
- Batch operations with hundreds of entries

See [COMMANDS.md](COMMANDS.md#error-code-reference) for a full error code reference.
