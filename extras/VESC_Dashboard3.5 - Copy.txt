/*
 * VESC Dashboard UI Test
 * For DIYmalls JC3248W535C (320x480 vertical)
 * 
 * Just UI - no BLE yet, uses dummy data
 */

#include <Arduino.h>
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"

// Set to 0 for vertical (portrait) orientation
#define LVGL_PORT_ROTATION_DEGREE  (0)

// UI Elements
static lv_obj_t *screen;
static lv_obj_t *lbl_speed;
static lv_obj_t *lbl_speed_unit;
static lv_obj_t *lbl_voltage;
static lv_obj_t *lbl_battery;
static lv_obj_t *bar_battery;
static lv_obj_t *lbl_motor_temp;
static lv_obj_t *lbl_ctrl_temp;
static lv_obj_t *lbl_amps;
static lv_obj_t *lbl_watts;

// Simulated VESC data
float speed_mph = 0.0;
float voltage = 50.4;
int battery_pct = 85;
float motor_temp = 32.0;
float ctrl_temp = 28.0;
float amps = 0.0;
float watts = 0.0;

// Colors
#define COLOR_BG        lv_color_hex(0x1a1a2e)
#define COLOR_CARD      lv_color_hex(0x16213e)
#define COLOR_PRIMARY   lv_color_hex(0x0f3460)
#define COLOR_ACCENT    lv_color_hex(0xe94560)
#define COLOR_GREEN     lv_color_hex(0x00ff88)
#define COLOR_YELLOW    lv_color_hex(0xffc107)
#define COLOR_RED       lv_color_hex(0xff4444)
#define COLOR_WHITE     lv_color_hex(0xffffff)
#define COLOR_GRAY      lv_color_hex(0x888888)

// Create a data card
lv_obj_t* create_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void create_ui() {
    // Main screen
    screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    // ===== SPEED (big, top) =====
    lv_obj_t *speed_card = create_card(screen, 10, 10, 300, 140);
    lv_obj_set_style_bg_color(speed_card, COLOR_PRIMARY, 0);
    
    lbl_speed = lv_label_create(speed_card);
    lv_label_set_text(lbl_speed, "0.0");
    lv_obj_set_style_text_font(lbl_speed, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_speed, COLOR_WHITE, 0);
    lv_obj_align(lbl_speed, LV_ALIGN_CENTER, 0, -10);
    
    lbl_speed_unit = lv_label_create(speed_card);
    lv_label_set_text(lbl_speed_unit, "MPH");
    lv_obj_set_style_text_font(lbl_speed_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_speed_unit, COLOR_GRAY, 0);
    lv_obj_align(lbl_speed_unit, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // ===== BATTERY (with bar) =====
    lv_obj_t *batt_card = create_card(screen, 10, 160, 300, 80);
    
    lv_obj_t *batt_label = lv_label_create(batt_card);
    lv_label_set_text(batt_label, "BATTERY");
    lv_obj_set_style_text_font(batt_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(batt_label, COLOR_GRAY, 0);
    lv_obj_align(batt_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_battery = lv_label_create(batt_card);
    lv_label_set_text(lbl_battery, "85%");
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_battery, COLOR_GREEN, 0);
    lv_obj_align(lbl_battery, LV_ALIGN_TOP_RIGHT, 0, -5);
    
    bar_battery = lv_bar_create(batt_card);
    lv_obj_set_size(bar_battery, 280, 20);
    lv_obj_align(bar_battery, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_value(bar_battery, 85, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_battery, COLOR_BG, 0);
    lv_obj_set_style_bg_color(bar_battery, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_battery, 5, 0);
    lv_obj_set_style_radius(bar_battery, 5, LV_PART_INDICATOR);
    
    // ===== VOLTAGE =====
    lv_obj_t *volt_card = create_card(screen, 10, 250, 145, 70);
    
    lv_obj_t *volt_label = lv_label_create(volt_card);
    lv_label_set_text(volt_label, "VOLTAGE");
    lv_obj_set_style_text_font(volt_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(volt_label, COLOR_GRAY, 0);
    lv_obj_align(volt_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_voltage = lv_label_create(volt_card);
    lv_label_set_text(lbl_voltage, "50.4V");
    lv_obj_set_style_text_font(lbl_voltage, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_voltage, COLOR_WHITE, 0);
    lv_obj_align(lbl_voltage, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // ===== POWER (Watts) =====
    lv_obj_t *watt_card = create_card(screen, 165, 250, 145, 70);
    
    lv_obj_t *watt_label = lv_label_create(watt_card);
    lv_label_set_text(watt_label, "POWER");
    lv_obj_set_style_text_font(watt_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(watt_label, COLOR_GRAY, 0);
    lv_obj_align(watt_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_watts = lv_label_create(watt_card);
    lv_label_set_text(lbl_watts, "0W");
    lv_obj_set_style_text_font(lbl_watts, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_watts, COLOR_ACCENT, 0);
    lv_obj_align(lbl_watts, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // ===== AMPS =====
    lv_obj_t *amp_card = create_card(screen, 10, 330, 300, 60);
    
    lv_obj_t *amp_label = lv_label_create(amp_card);
    lv_label_set_text(amp_label, "CURRENT");
    lv_obj_set_style_text_font(amp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(amp_label, COLOR_GRAY, 0);
    lv_obj_align(amp_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_amps = lv_label_create(amp_card);
    lv_label_set_text(lbl_amps, "0.0 A");
    lv_obj_set_style_text_font(lbl_amps, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_amps, COLOR_WHITE, 0);
    lv_obj_align(lbl_amps, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    // ===== TEMPS (Motor & Controller) =====
    lv_obj_t *temp_card = create_card(screen, 10, 400, 145, 70);
    
    lv_obj_t *mtemp_label = lv_label_create(temp_card);
    lv_label_set_text(mtemp_label, "MOTOR");
    lv_obj_set_style_text_font(mtemp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mtemp_label, COLOR_GRAY, 0);
    lv_obj_align(mtemp_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_motor_temp = lv_label_create(temp_card);
    lv_label_set_text(lbl_motor_temp, "32°C");
    lv_obj_set_style_text_font(lbl_motor_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_motor_temp, COLOR_GREEN, 0);
    lv_obj_align(lbl_motor_temp, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    
    lv_obj_t *ctemp_card = create_card(screen, 165, 400, 145, 70);
    
    lv_obj_t *ctemp_label = lv_label_create(ctemp_card);
    lv_label_set_text(ctemp_label, "ESC");
    lv_obj_set_style_text_font(ctemp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ctemp_label, COLOR_GRAY, 0);
    lv_obj_align(ctemp_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lbl_ctrl_temp = lv_label_create(ctemp_card);
    lv_label_set_text(lbl_ctrl_temp, "28°C");
    lv_obj_set_style_text_font(lbl_ctrl_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_ctrl_temp, COLOR_GREEN, 0);
    lv_obj_align(lbl_ctrl_temp, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

// Update UI with current values
void update_ui() {
    static char buf[32];
    
    // Speed
    snprintf(buf, sizeof(buf), "%.1f", speed_mph);
    lv_label_set_text(lbl_speed, buf);
    
    // Voltage
    snprintf(buf, sizeof(buf), "%.1fV", voltage);
    lv_label_set_text(lbl_voltage, buf);
    
    // Battery
    snprintf(buf, sizeof(buf), "%d%%", battery_pct);
    lv_label_set_text(lbl_battery, buf);
    lv_bar_set_value(bar_battery, battery_pct, LV_ANIM_ON);
    
    // Battery color based on level
    lv_color_t batt_color;
    if (battery_pct > 50) batt_color = COLOR_GREEN;
    else if (battery_pct > 20) batt_color = COLOR_YELLOW;
    else batt_color = COLOR_RED;
    lv_obj_set_style_text_color(lbl_battery, batt_color, 0);
    lv_obj_set_style_bg_color(bar_battery, batt_color, LV_PART_INDICATOR);
    
    // Amps
    snprintf(buf, sizeof(buf), "%.1f A", amps);
    lv_label_set_text(lbl_amps, buf);
    
    // Watts
    snprintf(buf, sizeof(buf), "%.0fW", watts);
    lv_label_set_text(lbl_watts, buf);
    
    // Motor temp with color
    snprintf(buf, sizeof(buf), "%.0f°C", motor_temp);
    lv_label_set_text(lbl_motor_temp, buf);
    if (motor_temp > 80) lv_obj_set_style_text_color(lbl_motor_temp, COLOR_RED, 0);
    else if (motor_temp > 60) lv_obj_set_style_text_color(lbl_motor_temp, COLOR_YELLOW, 0);
    else lv_obj_set_style_text_color(lbl_motor_temp, COLOR_GREEN, 0);
    
    // Controller temp with color
    snprintf(buf, sizeof(buf), "%.0f°C", ctrl_temp);
    lv_label_set_text(lbl_ctrl_temp, buf);
    if (ctrl_temp > 70) lv_obj_set_style_text_color(lbl_ctrl_temp, COLOR_RED, 0);
    else if (ctrl_temp > 50) lv_obj_set_style_text_color(lbl_ctrl_temp, COLOR_YELLOW, 0);
    else lv_obj_set_style_text_color(lbl_ctrl_temp, COLOR_GREEN, 0);
}

// Simulate riding data
void simulate_data() {
    static float time_s = 0;
    time_s += 0.1;
    
    // Simulate acceleration/deceleration
    speed_mph = 15.0 + 10.0 * sin(time_s * 0.5);
    if (speed_mph < 0) speed_mph = 0;
    
    // Current based on "throttle"
    amps = speed_mph * 2.0 + random(-5, 5);
    if (amps < 0) amps = 0;
    
    // Power
    watts = voltage * amps;
    
    // Battery slowly drains
    voltage = 50.4 - (time_s * 0.01);
    if (voltage < 42.0) voltage = 42.0;
    
    // Battery percent from voltage (12S: 42V-50.4V)
    battery_pct = map(voltage * 10, 420, 504, 0, 100);
    battery_pct = constrain(battery_pct, 0, 100);
    
    // Temps rise slowly with use
    motor_temp = 32.0 + (amps * 0.5);
    ctrl_temp = 28.0 + (amps * 0.3);
}

void setup() {
    Serial.begin(115200);
    Serial.println("VESC Dashboard UI Test");

    // Initialize display - VERTICAL orientation
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate = LV_DISP_ROT_270,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate = LV_DISP_ROT_180,
#else
        .rotate = LV_DISP_ROT_NONE,
#endif
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    create_ui();
    bsp_display_unlock();

    Serial.println("UI Ready - Simulating data");
}

void loop() {
    // Simulate VESC data
    simulate_data();
    
    // Update display
    bsp_display_lock(0);
    update_ui();
    bsp_display_unlock();
    
    delay(100);
}
