#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bridge setup/teardown for TabPage1 battery UI.
void ui_battery_bridge_init(void);
void ui_battery_bridge_deinit(void);

// True when the Settings screen is active and the Battery tab is selected.
bool ui_battery_is_active(void);

typedef enum {
    UI_BATT_DISCONNECTED = 0,
    UI_BATT_SCANNING,
    UI_BATT_CONNECTING,
    UI_BATT_CONNECTED,
} ui_battery_conn_state_t;

// Event stubs (wired to BLE module).
void onBatteryDisconnectPressed(void);
void onBatteryScanPressed(void);
void onBatteryConnectPressed(const char *mac);
void onBatteryDeviceSelected(const char *mac);

// Public UI bridge API (same as Waveshareport battery screen).
void ui_battery_set_connection_state(ui_battery_conn_state_t state, const char *name, const char *mac);
void ui_battery_set_pack_values(float pack_v, float pack_a, bool chg_on, bool dsg_on);
void ui_battery_set_soc(float soc_pct);
void ui_battery_set_temps(float t1, float t2, float t_mos_optional, bool has_mos_temp);
void ui_battery_set_temps_all(const float *temps, int count, float t_mos_optional, bool has_mos_temp);
void ui_battery_set_cell_summary(float delta_v, int low_i, float low_v, int high_i, float high_v);
void ui_battery_cells_set_count(int n);
void ui_battery_cells_set_value(int i, float v);

// Scan list control.
void ui_battery_scanlist_clear(void);
void ui_battery_scanlist_add(const char *name, const char *mac, int rssi);
void ui_battery_scanlist_set_selected(const char *mac);
void ui_battery_set_scan_progress(const char *text);

#ifdef __cplusplus
} /*extern "C"*/
#endif
