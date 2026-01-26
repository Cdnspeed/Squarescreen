#include "lv_port.h"
#include "esp_lcd_touch_axs15231b.h"

static Arduino_GFX *s_gfx = nullptr;
static lv_display_t *s_disp = nullptr;
static lv_indev_t *s_touch = nullptr;
static lv_color_t *s_buf1 = nullptr;
static lv_color_t *s_buf2 = nullptr;
static uint32_t s_last_ms = 0;
static int32_t s_y_offset = 200;
static int32_t s_phys_ver_res = 0;
static int32_t s_logical_ver_res = 0;

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    (void)display;
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    int32_t x = area->x1;
    int32_t y = area->y1 + s_y_offset;
    if (y >= 0 && (y + h) <= s_phys_ver_res) {
        s_gfx->draw16bitRGBBitmap(x, y, (uint16_t *)px_map, w, h);
    }
    lv_display_flush_ready(display);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    touch_data_t touch_data = {0};
    bsp_touch_read();
    if (bsp_touch_get_coordinates(&touch_data) && touch_data.touch_num > 0) {
        int32_t x = touch_data.coords[0].x;
        int32_t y = touch_data.coords[0].y - s_y_offset;
        if (x >= 0 && x < (int32_t)lv_display_get_horizontal_resolution(s_disp) &&
            y >= 0 && y < s_logical_ver_res) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = x;
            data->point.y = y;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_init(const lv_port_disp_cfg_t *disp_cfg, const lv_port_touch_cfg_t *touch_cfg)
{
    s_gfx = disp_cfg->gfx;
    s_gfx->begin();
    s_gfx->fillScreen(0x0000);

    lv_init();

    s_phys_ver_res = disp_cfg->ver_res;
    s_logical_ver_res = disp_cfg->ver_res - s_y_offset;
    if (s_logical_ver_res < 1) {
        s_logical_ver_res = disp_cfg->ver_res;
        s_y_offset = 0;
    }
    s_disp = lv_display_create(disp_cfg->hor_res, s_logical_ver_res);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_physical_resolution(s_disp, disp_cfg->hor_res, disp_cfg->ver_res);

    const uint32_t buf_pixels = disp_cfg->hor_res * 40;
#if defined(ESP32)
    s_buf1 = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buf_pixels);
    s_buf2 = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buf_pixels);
    if (!s_buf1) s_buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_pixels);
    if (!s_buf2) s_buf2 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_pixels);
#else
    s_buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_pixels);
    s_buf2 = (lv_color_t *)malloc(sizeof(lv_color_t) * buf_pixels);
#endif
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(lv_color_t) * buf_pixels, LV_DISPLAY_RENDER_MODE_PARTIAL);

    if (touch_cfg && touch_cfg->i2c) {
        bsp_touch_init(touch_cfg->i2c, touch_cfg->touch_rst, touch_cfg->rotation, touch_cfg->width, touch_cfg->height);
        s_touch = lv_indev_create();
        lv_indev_set_type(s_touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_touch, lvgl_touch_cb);
    }

    s_last_ms = millis();
}

lv_display_t *lv_port_get_display(void)
{
    return s_disp;
}

lv_indev_t *lv_port_get_touch(void)
{
    return s_touch;
}

void lv_port_task_handler(void)
{
    uint32_t now = millis();
    lv_tick_inc(now - s_last_ms);
    s_last_ms = now;
    lv_timer_handler();
}
