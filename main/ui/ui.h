#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/gpio.h>

#include "../main/target/target.h"

#include "ui/led.h"
#include "ui/beeper.h"
#include "ui/button.h"
#include "ui/screen.h"
#include "ui/screen_i2c.h"

typedef enum
{
    UI_SCREEN_AUTOOFF_DISABLED,
    UI_SCREEN_AUTOOFF_30_SEC,
    UI_SCREEN_AUTOOFF_1_MIN,
    UI_SCREEN_AUTOOFF_5_MIN,
    UI_SCREEN_AUTOOFF_10_MIN,

    UI_SCREEN_AUTOOFF_DEFAULT = UI_SCREEN_AUTOOFF_DISABLED,
} ui_screen_autooff_e;

#if defined(USE_BUTTON_5WAY)
#define UI_BUTTON_COUNT 5
#else
#define UI_BUTTON_COUNT 1
#endif

typedef struct ui_config_s
{
    button_config_t buttons[UI_BUTTON_COUNT];
#if defined(USE_BEEPER)
    hal_gpio_t beeper;
#endif
#ifdef USE_SCREEN
    screen_i2c_config_t screen;
#endif
} ui_config_t;

typedef struct ui_s
{
    struct
    {
        tracker_t *tracker;
#if defined(USE_WIFI)
        wifi_t *wifi;
#endif
        ui_config_t cfg;
#if defined(USE_BEEPER)
        beeper_t beeper;
#endif
// #if defined(LED_1_USE_WS2812)
//         led_gradual_target_t led_gradual_target;
// #endif
        button_t buttons[UI_BUTTON_COUNT];
#ifdef USE_SCREEN
        time_ticks_t screen_autooff_interval;
        time_ticks_t screen_off_at;
        bool screen_is_off;
        screen_t screen;
#endif

    } internal;
} ui_t;

#if defined(USE_WIFI)
void ui_init(ui_t *ui, ui_config_t *cfg, tracker_t *tracker, wifi_t *wifi);
#else
void ui_init(ui_t *ui, ui_config_t *cfg, tracker_t *tracker);
#endif
bool ui_screen_init(ui_t *ui, ui_config_t *cfg, tracker_t *tracker);
bool ui_screen_is_available(const ui_t *ui);
void ui_screen_splash(ui_t *ui);
bool ui_is_animating(const ui_t *ui);
void ui_update(ui_t *ui);
void ui_yield(ui_t *ui);
void ui_set_screen_set_autooff(ui_t *ui, ui_screen_autooff_e autooff);
void ui_shutdown(ui_t *ui);