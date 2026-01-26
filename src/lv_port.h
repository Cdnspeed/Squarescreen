#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

typedef struct {
    Arduino_GFX *gfx;
    uint16_t hor_res;
    uint16_t ver_res;
} lv_port_disp_cfg_t;

typedef struct {
    TwoWire *i2c;
    int touch_rst;
    uint16_t rotation;
    uint16_t width;
    uint16_t height;
} lv_port_touch_cfg_t;

void lv_port_init(const lv_port_disp_cfg_t *disp_cfg, const lv_port_touch_cfg_t *touch_cfg);
lv_display_t *lv_port_get_display(void);
lv_indev_t *lv_port_get_touch(void);
void lv_port_task_handler(void);
