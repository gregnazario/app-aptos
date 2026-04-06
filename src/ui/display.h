#pragma once

#include "../bcs/types.h"

#define UI_PREPARED -10

extern char g_bip32_path[60];
extern char g_tx_type[60];
extern char g_address[67];
extern char g_gas_fee[30];
extern char g_struct[120];
extern char g_function[120];
extern char g_amount[30];
extern int g_is_token_listed;

#define MAX_GENERIC_ARG_DISPLAY_LEN 70  // "0x" + 64 hex chars + null
extern char g_arg_labels[MAX_GENERIC_ARGS][20];
extern char g_arg_values[MAX_GENERIC_ARGS][MAX_GENERIC_ARG_DISPLAY_LEN];
extern int g_num_display_args;
extern char g_extra_info[30];
extern char g_multisig_addr[67];

#include "../types.h"

/**
 * Display address on the device and ask confirmation to export.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_address(void);
int ui_prepare_address(void);

/**
 * Display transaction information on the device and ask confirmation to sign.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_transaction(void);
int ui_prepare_transaction(void);

int ui_display_message(void);
int ui_display_raw_message(void);

int ui_display_entry_function(void);
int ui_prepare_entry_function(void);

int ui_display_tx_aptos_account_transfer(void);
int ui_prepare_tx_aptos_account_transfer(void);

int ui_display_tx_coin_transfer(void);
int ui_prepare_tx_coin_transfer(void);

int ui_display_tx_fungible_asset_transfer(void);
int ui_prepare_tx_fungible_asset_transfer(void);

int ui_display_delegation_pool_transfer(entry_function_known_type_t function_type);
int ui_prepare_delegation_pool_transfer(void);

int ui_display_generic_entry_function(void);
int ui_prepare_generic_entry_function(void);

int ui_display_script_payload(void);
int ui_prepare_script_payload(void);

int ui_display_multisig_payload(void);
int ui_prepare_multisig_payload(void);

int ui_display_multisig_create_transaction(void);
int ui_prepare_multisig_create_transaction(void);

int ui_display_multisig_create_hash(void);
int ui_prepare_multisig_create_hash(void);

#if defined(TARGET_STAX) || defined(TARGET_FLEX)
#define ICON_APP_HOME C_aptos_logo_64px
#elif defined(TARGET_APEX_P)
#define ICON_APP_HOME C_aptos_logo_48px
#endif