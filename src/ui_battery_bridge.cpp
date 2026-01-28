#include "ui_battery_bridge.h"

#include "ui.h"
#include "ui_Settings.h"
#include "ant_bms_ble_module.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_conn_title = NULL;
static lv_obj_t *s_conn_status = NULL;
static lv_obj_t *s_conn_device = NULL;
static lv_obj_t *s_conn_mac = NULL;
static lv_obj_t *s_conn_mos = NULL;
static lv_obj_t *s_conn_mos_c = NULL;
static lv_obj_t *s_conn_mos_d = NULL;
static lv_obj_t *s_conn_led = NULL;
static lv_obj_t *s_btn_disconnect = NULL;
static lv_obj_t *s_btn_scan = NULL;
static lv_obj_t *s_scan_progress = NULL;
static lv_obj_t *s_scan_list = NULL;
static lv_obj_t *s_btn_connect = NULL;

static lv_obj_t *s_cell_title = NULL;
static lv_obj_t *s_cell_delta = NULL;
static lv_obj_t *s_cell_high = NULL;
static lv_obj_t *s_cell_low = NULL;
static lv_obj_t *s_cell_list = NULL;

static lv_obj_t *s_cell_rows[32];
static lv_obj_t *s_cell_value_labels[32];
static lv_obj_t *s_cell_bars[32];
static int s_cell_count = 0;

static lv_obj_t *s_scan_rows[24];
static char s_scan_macs[24][24];
static char s_scan_names[24][32];
static int s_scan_count = 0;
static char s_selected_mac[24] = {0};
static char s_selected_name[32] = {0};

static void battery_row_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *row = (lv_obj_t *)lv_event_get_target(e);
    const char *mac = (const char *)lv_event_get_user_data(e);
    if (!mac) return;
    strncpy(s_selected_mac, mac, sizeof(s_selected_mac) - 1);
    s_selected_mac[sizeof(s_selected_mac) - 1] = '\0';
    for (int i = 0; i < s_scan_count; i++) {
        if (strncmp(mac, s_scan_macs[i], sizeof(s_scan_macs[i])) == 0) {
            strncpy(s_selected_name, s_scan_names[i], sizeof(s_selected_name) - 1);
            s_selected_name[sizeof(s_selected_name) - 1] = '\0';
            break;
        }
    }
    ui_battery_scanlist_set_selected(s_selected_mac);
    onBatteryDeviceSelected(s_selected_mac);
    (void)row;
}

static void battery_btn_disconnect_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) onBatteryDisconnectPressed();
    (void)e;
}

static void battery_btn_scan_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) onBatteryScanPressed();
    (void)e;
}

static void battery_btn_connect_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) onBatteryConnectPressed(s_selected_mac);
    (void)e;
}

bool ui_battery_is_active(void)
{
    if (!ui_Settings || !ui_TabView1) return false;
    if (!lv_obj_is_valid(ui_Settings) || !lv_obj_is_valid(ui_TabView1)) return false;
    if (lv_screen_active() != ui_Settings) return false;
    return (lv_tabview_get_tab_act(ui_TabView1) == 0);
}

static void setup_conn_container(void)
{
    if (!ui_Battery_Connect_info) return;
    lv_obj_set_flex_flow(ui_Battery_Connect_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Battery_Connect_info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(ui_Battery_Connect_info, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_Battery_Connect_info, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Battery_Connect_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_Battery_Connect_info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_Battery_Connect_info, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(ui_Battery_Connect_info, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(ui_Battery_Connect_info, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(ui_Battery_Connect_info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(ui_Battery_Connect_info, LV_DIR_VER);

    s_conn_title = lv_label_create(ui_Battery_Connect_info);
    lv_label_set_text(s_conn_title, "Battery");
    lv_obj_set_style_text_font(s_conn_title, &ui_font_euro20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *status_row = lv_obj_create(ui_Battery_Connect_info);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_width(status_row, lv_pct(100));
    lv_obj_set_height(status_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_led = lv_obj_create(status_row);
    lv_obj_set_size(s_conn_led, 16, 16);
    lv_obj_set_style_radius(s_conn_led, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_conn_led, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_conn_led, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_status = lv_label_create(status_row);
    lv_label_set_text(s_conn_status, "Disconnected");
    lv_obj_set_style_text_font(s_conn_status, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_device = lv_label_create(ui_Battery_Connect_info);
    lv_label_set_text(s_conn_device, "No device");
    lv_obj_set_style_text_font(s_conn_device, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_mac = lv_label_create(ui_Battery_Connect_info);
    lv_label_set_text(s_conn_mac, "--:--:--:--:--:--");
    lv_obj_set_style_text_font(s_conn_mac, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mos_row = lv_obj_create(ui_Battery_Connect_info);
    lv_obj_remove_style_all(mos_row);
    lv_obj_set_width(mos_row, lv_pct(100));
    lv_obj_set_height(mos_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mos_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mos_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mos_row, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_mos = lv_label_create(mos_row);
    lv_label_set_text(s_conn_mos, "MOS:");
    lv_obj_set_style_text_font(s_conn_mos, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_mos_c = lv_label_create(mos_row);
    lv_label_set_text(s_conn_mos_c, "C OFF");
    lv_obj_set_style_text_font(s_conn_mos_c, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_conn_mos_d = lv_label_create(mos_row);
    lv_label_set_text(s_conn_mos_d, "D OFF");
    lv_obj_set_style_text_font(s_conn_mos_d, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *btn_row = lv_obj_create(ui_Battery_Connect_info);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_btn_scan = lv_btn_create(btn_row);
    lv_obj_set_width(s_btn_scan, lv_pct(45));
    lv_obj_set_height(s_btn_scan, 28);
    lv_obj_add_event_cb(s_btn_scan, battery_btn_scan_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *scan_label = lv_label_create(s_btn_scan);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_center(scan_label);

    s_btn_disconnect = lv_btn_create(btn_row);
    lv_obj_set_width(s_btn_disconnect, lv_pct(45));
    lv_obj_set_height(s_btn_disconnect, 28);
    lv_obj_add_state(s_btn_disconnect, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_btn_disconnect, battery_btn_disconnect_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *disc_label = lv_label_create(s_btn_disconnect);
    lv_label_set_text(disc_label, "Disconnect");
    lv_obj_center(disc_label);

    s_scan_progress = lv_label_create(ui_Battery_Connect_info);
    lv_label_set_text(s_scan_progress, "Idle");
    lv_obj_set_style_text_font(s_scan_progress, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_scan_list = lv_obj_create(ui_Battery_Connect_info);
    lv_obj_set_width(s_scan_list, lv_pct(100));
    lv_obj_set_height(s_scan_list, LV_SIZE_CONTENT);
    lv_obj_add_flag(s_scan_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scan_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(s_scan_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(s_scan_list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(s_scan_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(s_scan_list, LV_DIR_VER);
    lv_obj_set_flex_flow(s_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_scan_list, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_scan_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_scan_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_btn_connect = lv_btn_create(ui_Battery_Connect_info);
    lv_obj_set_width(s_btn_connect, lv_pct(70));
    lv_obj_set_height(s_btn_connect, 28);
    lv_obj_add_state(s_btn_connect, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_btn_connect, battery_btn_connect_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *conn_label = lv_label_create(s_btn_connect);
    lv_label_set_text(conn_label, "Connect");
    lv_obj_center(conn_label);
}

static void setup_cell_container(void)
{
    if (!ui_Cell_info) return;
    lv_obj_set_flex_flow(ui_Cell_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Cell_info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(ui_Cell_info, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_Cell_info, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Cell_info, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_Cell_info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_Cell_info, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(ui_Cell_info, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(ui_Cell_info, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(ui_Cell_info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(ui_Cell_info, LV_DIR_VER);

    s_cell_title = lv_label_create(ui_Cell_info);
    lv_label_set_text(s_cell_title, "Cells");
    lv_obj_set_style_text_font(s_cell_title, &ui_font_euro20, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_cell_delta = lv_label_create(ui_Cell_info);
    lv_label_set_text(s_cell_delta, "Δ Cell: ---- V");
    lv_obj_set_style_text_font(s_cell_delta, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_cell_high = lv_label_create(ui_Cell_info);
    lv_label_set_text(s_cell_high, "High: --");
    lv_obj_set_style_text_font(s_cell_high, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_cell_low = lv_label_create(ui_Cell_info);
    lv_label_set_text(s_cell_low, "Low: --");
    lv_obj_set_style_text_font(s_cell_low, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_cell_list = lv_obj_create(ui_Cell_info);
    lv_obj_set_width(s_cell_list, lv_pct(100));
    lv_obj_set_height(s_cell_list, LV_SIZE_CONTENT);
    lv_obj_add_flag(s_cell_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cell_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(s_cell_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(s_cell_list, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(s_cell_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(s_cell_list, LV_DIR_VER);
    lv_obj_set_flex_flow(s_cell_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_cell_list, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_cell_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_cell_list, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_battery_bridge_init(void)
{
    if (!ui_Battery_Connect_info || !ui_Cell_info) return;
    setup_conn_container();
    setup_cell_container();
}

void ui_battery_bridge_deinit(void)
{
    s_conn_title = NULL;
    s_conn_status = NULL;
    s_conn_device = NULL;
    s_conn_mac = NULL;
    s_conn_mos = NULL;
    s_conn_mos_c = NULL;
    s_conn_mos_d = NULL;
    s_conn_led = NULL;
    s_btn_disconnect = NULL;
    s_btn_scan = NULL;
    s_scan_progress = NULL;
    s_scan_list = NULL;
    s_btn_connect = NULL;
    s_cell_title = NULL;
    s_cell_delta = NULL;
    s_cell_high = NULL;
    s_cell_low = NULL;
    s_cell_list = NULL;
    s_cell_count = 0;
    s_scan_count = 0;
    s_selected_mac[0] = '\0';
    s_selected_name[0] = '\0';
    for (int i = 0; i < 32; i++) {
        s_cell_rows[i] = NULL;
        s_cell_bars[i] = NULL;
        s_cell_value_labels[i] = NULL;
    }
    for (int i = 0; i < 24; i++) {
        s_scan_rows[i] = NULL;
        s_scan_macs[i][0] = '\0';
    }
}

void onBatteryDisconnectPressed(void) { ant_bms_ble_module_disconnect(); }
void onBatteryScanPressed(void) { ant_bms_ble_module_scan_start(); }
void onBatteryConnectPressed(const char *mac)
{
    if (!mac || !mac[0]) return;
    ant_bms_ble_module_set_target(mac);
    ant_bms_ble_module_connect_target();
}
void onBatteryDeviceSelected(const char *mac) { ant_bms_ble_module_set_selected(mac); }

void ui_battery_set_connection_state(ui_battery_conn_state_t state, const char *name, const char *mac)
{
    if (!s_conn_status || !s_conn_led || !s_conn_device || !s_conn_mac) return;

    const char *status = "Disconnected";
    lv_color_t led = lv_color_hex(0x888888);
    bool can_disconnect = false;
    if (state == UI_BATT_SCANNING) { status = "Scanning"; led = lv_color_hex(0x3A7AFE); }
    else if (state == UI_BATT_CONNECTING) { status = "Connecting"; led = lv_color_hex(0xFFB300); }
    else if (state == UI_BATT_CONNECTED) { status = "Connected"; led = lv_color_hex(0x3AD16A); can_disconnect = true; }

    lv_label_set_text(s_conn_status, status);
    lv_obj_set_style_bg_color(s_conn_led, led, LV_PART_MAIN | LV_STATE_DEFAULT);

    char buf[64];
    if (name && name[0]) snprintf(buf, sizeof(buf), "%s", name);
    else if (s_selected_name[0]) snprintf(buf, sizeof(buf), "%s", s_selected_name);
    else snprintf(buf, sizeof(buf), "No device");
    lv_label_set_text(s_conn_device, buf);

    if (mac && mac[0]) snprintf(buf, sizeof(buf), "%s", mac);
    else snprintf(buf, sizeof(buf), "--:--:--:--:--:--");
    lv_label_set_text(s_conn_mac, buf);

    if (s_btn_disconnect) {
        if (can_disconnect) lv_obj_clear_state(s_btn_disconnect, LV_STATE_DISABLED);
        else lv_obj_add_state(s_btn_disconnect, LV_STATE_DISABLED);
    }
}

void ui_battery_set_pack_values(float pack_v, float pack_a, bool chg_on, bool dsg_on)
{
    char buf[24];
    if (ui_Voltage) {
        snprintf(buf, sizeof(buf), "%.1fv", pack_v);
        lv_label_set_text(ui_Voltage, buf);
    }
    if (ui_Amps) {
        snprintf(buf, sizeof(buf), "%.1fa", pack_a);
        lv_label_set_text(ui_Amps, buf);
    }
    if (s_conn_mos_c && s_conn_mos_d) {
        lv_label_set_text(s_conn_mos_c, chg_on ? "C ON" : "C OFF");
        lv_label_set_text(s_conn_mos_d, dsg_on ? "D ON" : "D OFF");
        lv_obj_set_style_text_color(s_conn_mos_c,
                                    chg_on ? lv_color_hex(0x3AD16A) : lv_color_hex(0xD13A3A),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(s_conn_mos_d,
                                    dsg_on ? lv_color_hex(0x3AD16A) : lv_color_hex(0xD13A3A),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_battery_set_soc(float soc_pct)
{
    if (!ui_battery_percent) return;
    if (soc_pct < 0.0f || soc_pct > 1000.0f) {
        lv_label_set_text(ui_battery_percent, "--");
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%%", soc_pct);
    lv_label_set_text(ui_battery_percent, buf);
}

void ui_battery_set_temps(float t1, float t2, float t_mos_optional, bool has_mos_temp)
{
    (void)t1;
    (void)t2;
    (void)t_mos_optional;
    (void)has_mos_temp;
}

void ui_battery_set_temps_all(const float *temps, int count, float t_mos_optional, bool has_mos_temp)
{
    (void)temps;
    (void)count;
    (void)t_mos_optional;
    (void)has_mos_temp;
}

void ui_battery_set_cell_summary(float delta_v, int low_i, float low_v, int high_i, float high_v)
{
    if (!s_cell_delta || !s_cell_high || !s_cell_low) return;
    char buf[40];
    snprintf(buf, sizeof(buf), "Δ Cell: %.3f V", delta_v);
    lv_label_set_text(s_cell_delta, buf);
    snprintf(buf, sizeof(buf), "High: C%02d %.3fV", high_i + 1, high_v);
    lv_label_set_text(s_cell_high, buf);
    snprintf(buf, sizeof(buf), "Low:  C%02d %.3fV", low_i + 1, low_v);
    lv_label_set_text(s_cell_low, buf);
}

void ui_battery_cells_set_count(int n)
{
    if (!s_cell_list) return;
    if (n < 0) n = 0;
    if (n > 32) n = 32;
    s_cell_count = n;
    lv_obj_clean(s_cell_list);
    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_create(s_cell_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 24);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *bar = lv_bar_create(row);
        lv_obj_set_width(bar, lv_pct(100));
        lv_obj_set_height(bar, 20);
        lv_obj_set_align(bar, LV_ALIGN_CENTER);
        lv_bar_set_range(bar, 3000, 4200);
        lv_bar_set_value(bar, 3600, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(bar, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_label_create(bar);
        lv_label_set_text(label, "C00 --.-");
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        s_cell_rows[i] = row;
        s_cell_bars[i] = bar;
        s_cell_value_labels[i] = label;
    }
    lv_obj_set_height(s_cell_list, LV_SIZE_CONTENT);
}

void ui_battery_cells_set_value(int i, float v)
{
    if (i < 0 || i >= s_cell_count) return;
    if (!s_cell_value_labels[i]) return;
    int mv = (int)(v * 1000.0f);
    if (mv < 3000) mv = 3000;
    if (mv > 4200) mv = 4200;
    if (s_cell_bars[i]) {
        lv_bar_set_value(s_cell_bars[i], mv, LV_ANIM_OFF);
        lv_color_t c = lv_color_hex(0xD13A3A);
        if (v >= 3.6f) c = lv_color_hex(0x3AD16A);
        else if (v >= 3.2f) c = lv_color_hex(0xD1B93A);
        lv_obj_set_style_bg_color(s_cell_bars[i], c, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "C%02d %.3f", i + 1, v);
    lv_label_set_text(s_cell_value_labels[i], buf);
}

void ui_battery_scanlist_clear(void)
{
    if (!s_scan_list) return;
    lv_obj_clean(s_scan_list);
    s_scan_count = 0;
    s_selected_mac[0] = '\0';
    s_selected_name[0] = '\0';
    lv_obj_set_height(s_scan_list, LV_SIZE_CONTENT);
    if (ui_Battery_Connect_info) {
        lv_obj_set_height(ui_Battery_Connect_info, LV_SIZE_CONTENT);
        lv_obj_update_layout(ui_Battery_Connect_info);
        lv_obj_update_layout(lv_obj_get_parent(ui_Battery_Connect_info));
    }
}

void ui_battery_scanlist_add(const char *name, const char *mac, int rssi)
{
    if (!s_scan_list || s_scan_count >= 24) return;
    if (!mac) mac = "";
    if (!name) name = "ANT BMS";
    lv_obj_t *row = lv_obj_create(s_scan_list);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label1 = lv_label_create(row);
    lv_label_set_text(label1, name);
    lv_obj_set_style_text_font(label1, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *label2 = lv_label_create(row);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  RSSI %d", mac, rssi);
    lv_label_set_text(label2, buf);
    lv_obj_set_style_text_font(label2, &ui_font_Euro15, LV_PART_MAIN | LV_STATE_DEFAULT);

    strncpy(s_scan_macs[s_scan_count], mac, sizeof(s_scan_macs[s_scan_count]) - 1);
    s_scan_macs[s_scan_count][sizeof(s_scan_macs[s_scan_count]) - 1] = '\0';
    strncpy(s_scan_names[s_scan_count], name, sizeof(s_scan_names[s_scan_count]) - 1);
    s_scan_names[s_scan_count][sizeof(s_scan_names[s_scan_count]) - 1] = '\0';
    s_scan_rows[s_scan_count] = row;
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, battery_row_event_cb, LV_EVENT_ALL, s_scan_macs[s_scan_count]);
    s_scan_count++;
    lv_obj_set_height(s_scan_list, LV_SIZE_CONTENT);
    if (ui_Battery_Connect_info) {
        lv_obj_set_height(ui_Battery_Connect_info, LV_SIZE_CONTENT);
        lv_obj_update_layout(ui_Battery_Connect_info);
        lv_obj_update_layout(lv_obj_get_parent(ui_Battery_Connect_info));
    }
}

void ui_battery_scanlist_set_selected(const char *mac)
{
    if (!mac) return;
    for (int i = 0; i < s_scan_count; i++) {
        if (!s_scan_rows[i]) continue;
        bool sel = (strncmp(mac, s_scan_macs[i], sizeof(s_scan_macs[i])) == 0);
        if (sel) {
            lv_obj_set_style_bg_color(s_scan_rows[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(s_scan_rows[i], 40, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_opa(s_scan_rows[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    if (s_btn_connect) {
        if (strlen(mac) > 0) lv_obj_clear_state(s_btn_connect, LV_STATE_DISABLED);
        else lv_obj_add_state(s_btn_connect, LV_STATE_DISABLED);
    }
}

void ui_battery_set_scan_progress(const char *text)
{
    if (!s_scan_progress) return;
    lv_label_set_text(s_scan_progress, text ? text : "Idle");
}
