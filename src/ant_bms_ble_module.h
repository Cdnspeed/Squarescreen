#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Init ANT BMS BLE client module (requires NimBLEDevice already initialized).
void ant_bms_ble_module_init();

// Tick ANT BMS BLE client module (call from loop).
void ant_bms_ble_module_tick(uint32_t now_ms);

// Set target device by MAC (string "AA:BB:CC:DD:EE:FF").
void ant_bms_ble_module_set_target(const char *mac);
void ant_bms_ble_module_set_selected(const char *mac);

// Connect to target if set; returns false if no target.
bool ant_bms_ble_module_connect_target();

// Disconnect active client.
void ant_bms_ble_module_disconnect();

// True when BLE client is connected to BMS.
bool ant_bms_ble_module_is_connected();
void ant_bms_ble_module_flush_ui();

// Scan helpers
void ant_bms_ble_module_scan_start();
void ant_bms_ble_module_scan_stop();

#ifdef __cplusplus
} /*extern "C"*/
#endif
