#include "ant_bms_ble_module.h"
#include "ant_bms_ble_client.h"
#include "ui_battery_bridge.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <lvgl.h>
#include <string.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static ant_bms_ble::AntBmsBleClient s_bms;
static bool s_inited = false;
static char s_target_mac[24] = {0};
static char s_selected_mac[24] = {0};
static bool s_connect_requested = false;
static bool s_scanning = false;
static TaskHandle_t s_scan_task = nullptr;
static bool s_battery_was_active = false;

// UI throttling / change detection
static uint32_t s_last_pack_ms = 0;
static uint32_t s_last_temps_ms = 0;
static uint32_t s_last_cells_ms = 0;
static float s_last_pack_v = -9999.0f;
static float s_last_pack_a = -9999.0f;
static bool s_last_chg_on = false;
static bool s_last_dsg_on = false;
static float s_last_t1 = -9999.0f;
static float s_last_t2 = -9999.0f;
static float s_last_tmos = -9999.0f;
static float s_last_delta = -9999.0f;
static float s_last_low_v = -9999.0f;
static float s_last_high_v = -9999.0f;
static int s_last_low_i = -1;
static int s_last_high_i = -1;
static int s_last_cell_count = -1;
static float s_last_cell_v[32];
static float s_last_soc = -9999.0f;

static bool battery_screen_active()
{
    return ui_battery_is_active();
}

static bool looks_like_ant(NimBLEAdvertisedDevice &d)
{
    if (d.isAdvertisingService(ant_bms_ble::kServiceUuid)) return true;
    std::string name = d.getName();
    return name.find("ANT") != std::string::npos || name.find("ant") != std::string::npos;
}

class AntScanCB : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *d) override {
        if (!d) return;
        if (!looks_like_ant(*d)) return;
        // Defer UI updates to LVGL thread.
        struct ScanItem {
            char name[32];
            char mac[24];
            int rssi;
        };
        ScanItem *item = (ScanItem *)lv_malloc(sizeof(ScanItem));
        if (!item) return;
        strncpy(item->name, d->getName().c_str(), sizeof(item->name) - 1);
        item->name[sizeof(item->name) - 1] = '\0';
        strncpy(item->mac, d->getAddress().toString().c_str(), sizeof(item->mac) - 1);
        item->mac[sizeof(item->mac) - 1] = '\0';
        item->rssi = d->getRSSI();
        lv_async_call([](void *p) {
            ScanItem *it = (ScanItem *)p;
            if (battery_screen_active()) {
                ui_battery_scanlist_add(it->name, it->mac, it->rssi);
            }
            lv_free(it);
        }, item);
    }
};

static AntScanCB s_scan_cb;

static bool is_battery_scrolling()
{
    if (!battery_screen_active()) return false;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return false;
    return lv_indev_get_scroll_obj(indev) != NULL;
}

static void scan_task(void *arg)
{
    (void)arg;
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_scan_cb, false);
    scan->setInterval(45);
    scan->setWindow(15);
    scan->setActiveScan(true);

    lv_async_call([](void *) {
        if (!battery_screen_active()) return;
        ui_battery_scanlist_clear();
        ui_battery_set_connection_state(UI_BATT_SCANNING, NULL, NULL);
        ui_battery_set_scan_progress("Scanning...");
    }, nullptr);

    // Non-blocking start (runs in BLE stack task)
    scan->start(0, false);

    // Sleep here on the scan task, not the LVGL task/core
    vTaskDelay(pdMS_TO_TICKS(5000));
    scan->stop();

    s_scanning = false;
    lv_async_call([](void *) {
        if (!battery_screen_active()) return;
        ui_battery_set_scan_progress("Idle");
        if (!s_bms.is_connected()) {
            ui_battery_set_connection_state(UI_BATT_DISCONNECTED, NULL, NULL);
        }
    }, nullptr);

    s_scan_task = nullptr;
    vTaskDelete(NULL);
}

void ant_bms_ble_module_init()
{
    if (s_inited) return;
    s_inited = true;
}

void ant_bms_ble_module_set_target(const char *mac)
{
    if (!mac) {
        s_target_mac[0] = '\0';
        return;
    }
    strncpy(s_target_mac, mac, sizeof(s_target_mac) - 1);
    s_target_mac[sizeof(s_target_mac) - 1] = '\0';
}

void ant_bms_ble_module_set_selected(const char *mac)
{
    if (!mac) {
        s_selected_mac[0] = '\0';
        return;
    }
    strncpy(s_selected_mac, mac, sizeof(s_selected_mac) - 1);
    s_selected_mac[sizeof(s_selected_mac) - 1] = '\0';
}

bool ant_bms_ble_module_connect_target()
{
    if (s_target_mac[0] == '\0') return false;
    s_connect_requested = true;
    return true;
}

void ant_bms_ble_module_disconnect()
{
    // recreate client by forcing begin with invalid addr
    s_connect_requested = false;
    ant_bms_ble_module_set_target(NULL);
}

bool ant_bms_ble_module_is_connected()
{
    return s_bms.is_connected();
}

void ant_bms_ble_module_flush_ui()
{
    if (!battery_screen_active()) return;
    ui_battery_set_connection_state(UI_BATT_DISCONNECTED, NULL, NULL);
    ui_battery_scanlist_clear();
    ui_battery_set_scan_progress("Idle");
    ui_battery_set_pack_values(0.0f, 0.0f, false, false);
    ui_battery_set_soc(-1.0f);
    float zeros[6] = {0};
    ui_battery_set_temps_all(zeros, 0, 0.0f, false);
    ui_battery_set_cell_summary(0.0f, 0, 0.0f, 0, 0.0f);
    ui_battery_cells_set_count(0);

    // Reset UI caches so next entry repopulates cells immediately.
    s_last_cells_ms = 0;
    s_last_cell_count = -1;
    for (int i = 0; i < 32; i++) {
        s_last_cell_v[i] = -9999.0f;
    }
}

void ant_bms_ble_module_scan_start()
{
    if (s_scanning) return;
    s_scanning = true;
    if (s_scan_task == nullptr) {
        xTaskCreatePinnedToCore(scan_task, "ant_bms_scan", 4096, nullptr, 1, &s_scan_task, 0);
    }
}

void ant_bms_ble_module_scan_stop()
{
    if (!s_scanning) return;
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->stop();
    s_scanning = false;
    lv_async_call([](void *) {
        if (!battery_screen_active()) return;
        ui_battery_set_scan_progress("Idle");
        if (!s_bms.is_connected()) {
            ui_battery_set_connection_state(UI_BATT_DISCONNECTED, NULL, NULL);
        }
    }, nullptr);
}

void ant_bms_ble_module_tick(uint32_t now_ms)
{
    if (!s_inited) return;

    bool battery_active = battery_screen_active();
    if (battery_active && !s_battery_was_active) {
        // Screen just became active: force refresh on next update.
        s_last_pack_ms = 0;
        s_last_temps_ms = 0;
        s_last_cells_ms = 0;
        s_last_pack_v = -9999.0f;
        s_last_pack_a = -9999.0f;
        s_last_t1 = -9999.0f;
        s_last_t2 = -9999.0f;
        s_last_tmos = -9999.0f;
        s_last_delta = -9999.0f;
        s_last_low_v = -9999.0f;
        s_last_high_v = -9999.0f;
        s_last_low_i = -1;
        s_last_high_i = -1;
        s_last_cell_count = -1;
        s_last_soc = -9999.0f;
        for (int i = 0; i < 32; i++) s_last_cell_v[i] = -9999.0f;

        lv_async_call([](void *) {
            if (!battery_screen_active()) return;
            ui_battery_set_connection_state(s_bms.is_connected() ? UI_BATT_CONNECTED : UI_BATT_DISCONNECTED,
                                            NULL, s_target_mac);
        }, nullptr);
    }
    if (!battery_active && s_battery_was_active) {
        ant_bms_ble_module_scan_stop();
    }
    s_battery_was_active = battery_active;

    if (s_connect_requested && s_target_mac[0] != '\0' && !s_bms.is_connected()) {
        NimBLEAddress addr(s_target_mac);
        (void)s_bms.begin(addr);
        s_connect_requested = false;
        if (battery_active) {
            ui_battery_set_connection_state(UI_BATT_CONNECTING, NULL, s_target_mac);
        }
    }

    s_bms.tick(now_ms);

    if (s_bms.is_connected()) {
        if (battery_active && !is_battery_scrolling()) {
            ui_battery_set_connection_state(UI_BATT_CONNECTED, NULL, s_target_mac);
        }
        if (battery_active && !is_battery_scrolling() && s_bms.has_status()) {
            const auto &st = s_bms.status();
            // Pack values: ~4 Hz or on change
            if ((now_ms - s_last_pack_ms) >= 250 ||
                st.total_voltage_v != s_last_pack_v ||
                st.current_a != s_last_pack_a ||
                (st.charge_mosfet_status == 0x01) != s_last_chg_on ||
                (st.discharge_mosfet_status == 0x01) != s_last_dsg_on) {
                s_last_pack_ms = now_ms;
                s_last_pack_v = st.total_voltage_v;
                s_last_pack_a = st.current_a;
                s_last_chg_on = (st.charge_mosfet_status == 0x01);
                s_last_dsg_on = (st.discharge_mosfet_status == 0x01);
                ui_battery_set_pack_values(st.total_voltage_v, st.current_a, s_last_chg_on, s_last_dsg_on);
            }
            if (!isnan(st.soc_pct) && st.soc_pct != s_last_soc) {
                s_last_soc = st.soc_pct;
                ui_battery_set_soc(st.soc_pct);
            }

            // Temps: ~2 Hz or on change
            if ((now_ms - s_last_temps_ms) >= 500 ||
                st.temp_c[0] != s_last_t1 ||
                st.temp_c[1] != s_last_t2 ||
                st.temp_c[6] != s_last_tmos) {
                s_last_temps_ms = now_ms;
                s_last_t1 = st.temp_c[0];
                s_last_t2 = st.temp_c[1];
                s_last_tmos = st.temp_c[6];
                ui_battery_set_temps_all(st.temp_c, st.temp_sensor_count, st.temp_c[6], true);
            }

            // Cell summary: ~1 Hz or on change
            int low_i = (int)(st.min_cell_idx > 0 ? (st.min_cell_idx - 1) : 0);
            int high_i = (int)(st.max_cell_idx > 0 ? (st.max_cell_idx - 1) : 0);
            if ((now_ms - s_last_cells_ms) >= 1000 ||
                st.delta_cell_v != s_last_delta ||
                st.min_cell_v != s_last_low_v ||
                st.max_cell_v != s_last_high_v ||
                low_i != s_last_low_i ||
                high_i != s_last_high_i) {
                s_last_cells_ms = now_ms;
                s_last_delta = st.delta_cell_v;
                s_last_low_v = st.min_cell_v;
                s_last_high_v = st.max_cell_v;
                s_last_low_i = low_i;
                s_last_high_i = high_i;
                ui_battery_set_cell_summary(st.delta_cell_v, low_i, st.min_cell_v, high_i, st.max_cell_v);
            }

            // Cell list: ~1 Hz or on change count/values
            if ((now_ms - s_last_cells_ms) >= 1000 || st.cell_count != s_last_cell_count) {
                s_last_cell_count = st.cell_count;
                ui_battery_cells_set_count(st.cell_count);
                for (int i = 0; i < st.cell_count && i < 32; i++) {
                    if (st.cell_v[i] != s_last_cell_v[i]) {
                        s_last_cell_v[i] = st.cell_v[i];
                        ui_battery_cells_set_value(i, st.cell_v[i]);
                    }
                }
            }
        }
    } else if (!s_scanning && battery_active && !is_battery_scrolling()) {
        ui_battery_set_connection_state(UI_BATT_DISCONNECTED, NULL, NULL);
    }
}
