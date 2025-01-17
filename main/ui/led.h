#pragma once

#include <stdbool.h>

#include "../main/target/target.h"

#include "util/time.h"

#if defined(LED_USE_WS2812)
#include <hal/ws2812.h>
#endif

#if defined(LED_1_GPIO)
#define USE_LED
#endif

#if defined(LED_1_GPIO) && defined(LED_1_USE_PWM)
#define LED_USE_PWM
#endif

#if defined(LED_1_GPIO) && defined(LED_1_USE_WS2812)
#define LED_USE_WS2812
#endif

#if defined(LED_USE_PWM) || defined(LED_USE_WS2812)
#define LED_USE_FADING
#endif

#define LED_REPEAT_NONE 0
#define LED_REPEAT_FOREVER 0xFF

#define LED_LEVEL_MIN 0
#define LED_LEVEL_CENTER 128 // GPIO-only leds will be turned on for values >= LED_LEVEL_CENTER
#define LED_LEVEL_MAX 255

#define LED_PWM_RESOLUTION_BITS 10
#define LED_PWM_MAX ((1 << LED_PWM_RESOLUTION_BITS) - 1)

#define LED_PATTERN(var, s, r) \
    static const led_pattern_t var = ((led_pattern_t){.stages = s, .count = ARRAY_COUNT(s), .repeat = r})

#if defined(LED_USE_WS2812)
#define LED_PATTERN_GRADUAL(var, s, r, g, v) \
    static const led_pattern_t var = ((led_pattern_t){.stages = s, .count = ARRAY_COUNT(s), .repeat = r, .gradual_change = g, .gradual_value = v})
#endif

typedef struct led_stage_s
{
    uint16_t duration; 
#if defined(LED_USE_FADING)
    uint16_t fade_duration;
#endif
#if defined(LED_USE_WS2812)
    hal_ws2812_color_t color;
#endif
    uint8_t level;
} led_stage_t;

typedef struct led_pattern_s
{
    const led_stage_t *stages;
    uint8_t count;
    uint8_t repeat;
#if defined(LED_USE_WS2812)
    bool gradual_change;
    uint8_t gradual_value;
#endif
} led_pattern_t;

#if defined(LED_USE_WS2812)
#define LED_STAGE(l, c, d, f) ((const led_stage_t){.duration = d, .color = c, .fade_duration = f, .level = l})
#elif defined(LED_USE_FADING)
#define LED_STAGE(l, c, d, f) ((const led_stage_t){.duration = d, .fade_duration = f, .level = l})
#else
#define LED_STAGE(l, c, d, f) ((const led_stage_t){.duration = d, .level = l})
#endif

#define LED_STAGE_OFF(d, f) LED_STAGE(0, HAL_WS2812_OFF, d, f)

typedef enum
{
    LED_ID_1,
} led_id_e;

typedef enum
{
    LED_MODE_NONE,

    LED_MODE_TRACKING,

    LED_MODE_WAIT_SERVER,

    LED_MODE_WAIT_CONNECT,

    LED_MODE_SMART_CONFIG,

    LED_MODE_BOOT,

    LED_MODE_REVERSING,

    LED_MODE_SETED,

    LED_MODE_CAL_STEP,

    LED_MODE_CAL_DONE,

    LED_MODE_COUNT,

} led_mode_e;

// #if defined(LED_1_USE_WS2812)
// void led_init(led_gradual_target_t *led_gradual_target);
// #else
void led_init(void);
// #endif

void led_update(void);

void led_pause(void);
void led_resume(void);

bool led_is_fading(void);

void led_start_pattern(led_id_e led_id, const led_pattern_t *pattern);

void led_mode_set(led_mode_e mode, bool set);
void led_mode_add(led_mode_e mode);
void led_mode_remove(led_mode_e mode);
bool led_mode_is_enable(led_mode_e mode);
