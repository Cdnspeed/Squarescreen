#include <Arduino.h>
#include <lvgl.h>
#include "draw/sw/lv_draw_sw_utils.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "esp_lcd_touch_axs15231b.h"
#include "esp_heap_caps.h"
#include "ui.h"
#include <NimBLEDevice.h>
#include "ant_bms_ble_module.h"

#define DIRECT_RENDER_MODE
#define ROTATE_LVGL_CW 1

// Display pins
#define GFX_BL     1
#define LCD_QSPI_CS  45
#define LCD_QSPI_CLK 47
#define LCD_QSPI_D0  21
#define LCD_QSPI_D1  48
#define LCD_QSPI_D2  40
#define LCD_QSPI_D3  39

// Touch pins
#define I2C_SDA 4
#define I2C_SCL 8

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);
Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1 /* RST */, 0 /* rotation */, false, 320, 480);

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;
lv_color_t *rot_buf;

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}
#endif

uint32_t millis_cb(void)
{
  return millis();
}

/* LVGL calls it when a rendered image needs to copied to the display */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  if (ROTATE_LVGL_CW) {
    if (!rot_buf) {
      rot_buf = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!rot_buf) {
        rot_buf = (lv_color_t *)malloc(screenWidth * screenHeight * 2);
      }
    }
    if (rot_buf && area->x1 == 0 && area->y1 == 0 && w == screenWidth && h == screenHeight) {
      lv_draw_sw_rotate(px_map, rot_buf, screenWidth, screenHeight,
                        screenWidth * 2, screenHeight * 2,
                        LV_DISPLAY_ROTATION_90, LV_COLOR_FORMAT_RGB565);
      gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)rot_buf, screenHeight, screenWidth);
    }
  } else {
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  }

  /* Call it to tell LVGL you are ready */
  lv_disp_flush_ready(disp);
}

/* Read the touchpad */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  touch_data_t touch_data;
  bsp_touch_read();

  if (bsp_touch_get_coordinates(&touch_data)) {
    int32_t x = touch_data.coords[0].x;
    int32_t y = touch_data.coords[0].y;
    if (ROTATE_LVGL_CW) {
      int32_t lx = (int32_t)screenWidth - 1 - y;
      int32_t ly = x;
      if (lx >= 0 && lx < (int32_t)screenWidth && ly >= 0 && ly < (int32_t)screenHeight) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = lx;
        data->point.y = ly;
      } else {
        data->state = LV_INDEV_STATE_REL;
      }
    } else {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = x;
      data->point.y = y;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void setup()
{
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.begin(115200);
  Serial.println("Arduino_GFX LVGL_Arduino_v9 example");

  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->setRotation(0);
  gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  lv_init();

  /* Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(millis_cb);

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  screenWidth = gfx->height();
  screenHeight = gfx->width();
  bsp_touch_init(&Wire, -1, 0, gfx->width(), gfx->height());

#ifdef DIRECT_RENDER_MODE
  bufSize = screenWidth * screenHeight;
  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  bufSize = screenWidth * 40;
  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
  if (!disp_draw_buf1 || !disp_draw_buf2)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
#ifdef DIRECT_RENDER_MODE
    lv_display_set_buffers(disp, disp_draw_buf1, disp_draw_buf2, bufSize * 2, LV_DISPLAY_RENDER_MODE_FULL);
#else
    lv_display_set_buffers(disp, disp_draw_buf1, disp_draw_buf2, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    ui_init();
  }

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  ant_bms_ble_module_init();

  Serial.println("Setup done");
}

void loop()
{
  lv_task_handler();
  ant_bms_ble_module_tick(millis());
  delay(5);
}
