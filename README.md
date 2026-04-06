# Ledger Aptos Application

Aptos wallet application supporting all the Ledger devices

## Prerequisite

Be sure to have your environment correctly set up (see [Getting Started](https://developers.ledger.com/docs/nano-app/introduction/)) and [ledgerblue](https://pypi.org/project/ledgerblue/) and installed.

If you want to benefit from [vscode](https://code.visualstudio.com/) integration, it's recommended to move the toolchain in `/opt` and set `BOLOS_ENV` environment variable as follows

```shell
$ cd <your-preferred-dir>
$ mkdir bolos-devenv
$ cd bolos-devenv
$ wget https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q1-update/+download/gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2
$ tar xf gcc-arm-none-eabi-5_3-2016q1-20160330-linux.tar.bz2
$ sudo ln -s <your-preferred-dir>/bolos-devenv /opt/bolos-devenv
```

```shell
$ export BOLOS_ENV=/opt/bolos-devenv
```

and do the same with `BOLOS_SDK` environment variable

```shell
$ cd <your-preferred-dir>
$ git clone https://github.com/LedgerHQ/nanosplus-secure-sdk.git nanosplus-secure-sdk
$ sudo ln -s <your-preferred-dir>/nanosplus-secure-sdk /opt/nanosplus-secure-sdk
```

```shell
$ export BOLOS_SDK=/opt/nanosplus-secure-sdk
```

## Compilation

### Setup

[Ledger App Builder](https://github.com/LedgerHQ/ledger-app-builder) is a container image which holds the all dependencies to compile an application for Nano hardware wallets.

You need to clone it with:

```shell
$ git clone https://github.com/LedgerHQ/ledger-app-builder
```

To use the container image, you need to install [Docker](https://docs.docker.com/get-docker/) and do the following steps:

```shell
$ sudo docker build -t ledger-app-builder:latest .
```

### Build the Application

**For the Nano X and Nano S Plus**

For Nano X and S Plus, specify the `BOLOS_SDK` environment variable before building your app, in the source folder of the app.

For Nano S Plus (or replace ‘realpath’ with your app’s local path):

```shell
$ sudo docker run --rm -ti -v "$(realpath .):/app" ledger-app-builder:latest

root@656be163fe84:/app# BOLOS_SDK=$NANOSP_SDK make
```

For Nano X (or replace ‘realpath’ with your app’s local path):

```shell
$ sudo docker run --rm -ti -v "$(realpath .):/app" ledger-app-builder:latest

root@656be163fe84:/app# BOLOS_SDK=$NANOX_SDK make
```

_**NOTE: If you change the `BOLOS_SDK` variable between two builds, you can first use `make clean` to avoid errors.**_

### Exit the image

The build generates several files in your application folder and especially the `app.elf` that can be loaded to a Nano S Plus or into the Nano X or S Emulator (Speculos).

You can exit the image, with the `exit` command.

_**NOTE: For more information see the Ledger Dev Docs page:**_
https://developers.ledger.com/docs/nano-app/build/

## Load the application (Linux)

_**WARNING: The Nano X does not support side loading, therefore you must use the device emulator `Speculos` for loading to work.**_

### Define the udev rules

If you wish to load applications on your device, you will need to add the appropriate `udev` rules:

```shell
$ wget -q -O - https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh | sudo bash
```

### Load the application from inside the container image

If you want to load and delete the app directly from the container image. You need to compile the application, in the source file of your application, adding the `--privileged` option:

```shell
$ docker run --rm -ti -v "/dev/bus/usb:/dev/bus/usb" -v "$(realpath .):/app" --privileged ledger-app-builder:latest
```

While the container image is running:

1. Plug and unlock the Nano S Plus.
2. Use `BOLOS_SDK=$NANOSP_SDK make load`to load the app to the Nano S Plus and `make delete` to delete it.
3. You can exit the image, with the command `exit`.

_**NOTE: For more information see the Ledger Dev Docs page:**_
https://developers.ledger.com/docs/nano-app/load/

## Load the application (MacOS)

To install the app, we need [virtualenv](https://docs.python.org/3/library/venv.html), this module provides support for creating lightweight “virtual environments” with their own site directories, optionally isolated from system site directories.

### Install virtualenv

```shell
$ sudo apt install python3-pip
$ pip3 install virtualenv
```

We will install the app on the hardware wallet Ledger Nano S Plus. Take Ledger Nano S Plus with its cable and plug it into the MacBook.

### Run virtulenv

Move to the `bin` folder where the compiled sources are located.

```shell
$ cd $(realpath .)/bin
```

And run

```shell
$ virtualenv -p python3 ledger
$ source ledger/bin/activate
$ pip3 install ledgerblue
```

### Deploy app to Ledger

Now it's time to deploy the binary file app.hex into the Ledger device.

```shell
python -m ledgerblue.loadApp --targetId 0x31100004 --apdu --tlv --fileName app.hex --appName Aptos --appFlags 0x00 --icon ""
```

While the process is running, see the screen of Ledger Nano, you need to do some task.

### Uninstall App

To uninstall the app, you need to connect the device to the laptop, input the pin and then execute this command.

```shell
python -m ledgerblue.deleteApp --targetId 0x31100004 --appName "Hello"
```

## Speculos emulator

Speculos is the emulator of Ledger hardware wallets (Nano S, Nano X, Nano S Plus and Blue) on desktop computers. It is particularly useful when

- you don’t have the physical hardware device, or
- you want to facilitate the pressing of Nano buttons.

Usage example:

```shell
$ ./speculos.py app-aptos/bin/app.elf --model nanosp --display headless --button-port 42000 --seed "shoot island position ..."

# ... and open a browser on http://127.0.0.1:5000
```

_**NOTE: For more information see the Speculos GitHub repository:**_
https://github.com/LedgerHQ/speculos/blob/master/docs/index.md

## Clear Signing Support

The Ledger Aptos application implements clear signing for transactions whenever possible, displaying human-readable information about the operation being performed. When a transaction cannot be fully parsed, the application falls back to blind signing, which requires the user to acknowledge that they are signing opaque data.

### Recognized Functions (Known-Type Clear Signing)

The following on-chain functions are specifically recognized and displayed with human-readable labels:

| Function | Display Label |
|---|---|
| `0x1::aptos_account::transfer` | APT transfer |
| `0x1::coin::transfer` | Coin transfer |
| `0x1::aptos_account::transfer_coins` | Coin transfer |
| `0x1::primary_fungible_store::transfer` | Fungible asset transfer |
| `0x1::delegation_pool::add_stake` | Delegation: add stake |
| `0x1::delegation_pool::unlock` | Delegation: unlock |
| `0x1::delegation_pool::reactivate_stake` | Delegation: reactivate stake |
| `0x1::delegation_pool::withdraw` | Delegation: withdraw |
| `0x1::multisig_account::create_transaction` | Multisig proposal (decodes inner payload) |
| `0x1::multisig_account::create_transaction_with_hash` | Multisig proposal with hash |
| `0x1::multisig_account::approve_transaction` | Multisig approval |
| `0x1::multisig_account::reject_transaction` | Multisig rejection |
| `0x1::multisig_account::vote_transaction` | Multisig vote |
| `0x1::multisig_account::create_with_owners` | Create multisig account |

### Generic Clear Signing

Any unknown entry function that does not match the table above is still displayed with clear signing using a generic layout. The device shows:

- The fully qualified function name (e.g., `0xABC::module::function`)
- Up to 6 function arguments displayed as hex values with type hints inferred from the BCS byte length (1 byte = u8, 2 = u16, 4 = u32, 8 = u64, 16 = u128, 32 = address, other = raw bytes)

### Script Payload Support

Script payloads use self-typed arguments (`TransactionArgument` enum), so each argument carries its own type tag. The device displays the script's argument count and each argument with its definitive type label (U8, U64, U128, Address, Bool, U8Vector, U16, U32, U256).

### Multisig Payload Support

For multisig transaction payloads (`0x1::multisig_account`), the device shows the multisig account address. If the payload contains an inner entry function, it is decoded and displayed. For execute-only multisig transactions (no inner payload), the device shows the multisig address with a "Verify payload in wallet" warning.

### Transaction Size Limits

Each device model has a maximum packet size that limits the transaction buffer:

| Device | Max Packet Payload (bytes) |
|---|---|
| Nano S Plus | 106 |
| Nano X | 90 |
| Stax / Flex / Apex P | 70 |

The maximum transaction length is `MAX_TRANSACTION_PACKETS * 255` bytes (currently 510 bytes with 2 packets).

## Documentation

High level documentation such as [APDU](doc/APDU.md), [commands](doc/COMMANDS.md) and [transaction serialization](doc/TRANSACTION.md) are included in developer documentation which can be generated with [doxygen](https://www.doxygen.nl)

```shell
$ doxygen .doxygen/Doxyfile
```

the process outputs HTML and LaTeX documentations in `doc/html` and `doc/latex` folders.

## Tests & Continuous Integration

The flow processed in [GitHub Actions](https://github.com/features/actions) is the following:

- Code formatting with [clang-format](http://clang.llvm.org/docs/ClangFormat.html)
- Compilation of the application for Ledger Nano S Plus in [ledger-app-builder](https://github.com/LedgerHQ/ledger-app-builder)
- Unit tests of C functions with [cmocka](https://cmocka.org/) (see [unit-tests/](unit-tests/))
  - Transaction parser tests (`test_tx_parser.c`) cover 90%+ line coverage of `deserialize.c`, including all known function types (APT transfer, coin transfer, fungible asset transfer, delegation pool, multisig operations), generic entry functions, script payloads, multisig payloads, and error paths (truncated TX, oversized TX, invalid payload variant, TX variant detection)
- End-to-end tests with [Speculos](https://github.com/LedgerHQ/speculos) emulator (see [tests/](tests/))
- Code coverage with [gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)/[lcov](http://ltp.sourceforge.net/coverage/lcov.php) and upload to [codecov.io](https://about.codecov.io)
- Documentation generation with [doxygen](https://www.doxygen.nl)

It outputs 4 artifacts:

- `aptos-app-debug` within output files of the compilation process in debug mode
- `speculos-log` within APDU command/response when executing end-to-end tests
- `code-coverage` within HTML details of code coverage
- `documentation` within HTML auto-generated documentation
