#include "target/target.h"

#ifdef USE_SCREEN
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <hal/log.h>

#include <u8g2.h>

#include "platform/system.h"

#include "logo/logo.h"
#include "ui/screen_i2c.h"

#include "util/time.h"
#include "util/version.h"
#include "util/calc.h"

#include "menu.h"
#include "screen.h"
#include "tracker/tracker.h"
#include "tracker/home.h"
#include "tracker/servo.h"
#include "wifi/wifi.h"
#include "config/settings.h"
#include "sensors/imu_task.h"

#define SCREEN_DRAW_BUF_SIZE 128
#define ANIMATION_FRAME_DURATION_MS 500
#define ANIMATION_TOTAL_DURATION_MS (ANIMATION_REPEAT * ANIMATION_COUNT * ANIMATION_FRAME_DURATION_MS)

static const char *TAG = "Screen";

typedef enum
{
    SCREEN_DIRECTION_HORIZONTAL,
    SCREEN_DIRECTION_VERTICAL,
} screen_direction_e;

#define SCREEN_DIRECTION(s) ((screen_direction_e)s->internal.direction)
#define SCREEN_W(s) (s->internal.w)
#define SCREEN_H(s) (s->internal.h)
#define SCREEN_BUF(s) (s->internal.buf)

typedef enum
{
    SCREEN_MULTILINEOPT_NONE = 0,
    SCREEN_MULTILINEOPT_BORDER = 1,
    SCREEN_MULTILINEOPT_BOX = 2,
} screen_multiline_opt_e;

static u8g2_t u8g2;

bool screen_init(screen_t *screen, screen_i2c_config_t *cfg, tracker_t *tracker)
{
    if (!screen->internal.is_init)
    {
        memset(screen, 0, sizeof(*screen));
        screen->internal.available = screen_i2c_init(cfg, &u8g2);
        screen->internal.cfg = *cfg;
        screen->internal.tracker = tracker;
        screen->internal.main_secondary_mode = SCREEN_MAIN_SECONDARY_MODE_DEFAULT;
        screen->internal.is_init = true;
    }
    
    screen->internal.next_update = time_millis_now() + 40;

    return screen->internal.available;
}

bool screen_is_available(const screen_t *screen)
{
    return screen->internal.available;
}

void screen_shutdown(screen_t *screen)
{
    screen_i2c_shutdown(&screen->internal.cfg, &u8g2);
}

void screen_power_on(screen_t *screen)
{
    screen_i2c_power_on(&screen->internal.cfg, &u8g2);
}

void screen_enter_secondary_mode(screen_t *screen, screen_secondary_mode_e mode)
{
    screen->internal.secondary_mode = mode;
    LOG_I(TAG, "SCREEN_SECONDARY_MODE -> %d", mode);
}

void screen_enter_main_secondary_mode(screen_t *screen, screen_main_secondary_mode_e mode)
{
    screen->internal.main_secondary_mode = mode;
    LOG_I(TAG, "MAIN_SCREEN_SECONDARY_MODE -> %d", mode);
}

static void screen_splash_task(void *arg)
{
    screen_t *screen = arg;
    screen->internal.splashing = true;
    uint16_t w = u8g2_GetDisplayWidth(&u8g2);
    uint16_t h = u8g2_GetDisplayHeight(&u8g2);

    uint16_t anim_x = (w - AF_LOGO_WIDTH) / 2;
    uint16_t anim_y = (h - AF_LOGO_HEIGHT) / 2 + 2;

#define SPLASH_TOP FIRMWARE_NAME
#define SPLASH_AUTHOR "A.T"
#define SPLASH_AUTHOR_LABEL " Design by A.T"
#define SPLASH_VERSION_LABEL "Version:"
#define SPLASH_BOTTOM_HORIZONTAL SOFTWARE_VERSION

    for (int ii = 0; ii < AF_LOGO_ANIMATION_REPEAT + 1; ii++)
    {
        if (ii < AF_LOGO_ANIMATION_REPEAT)
        {
            // Draw logo by animation
            for (int jj = 0; jj < AF_LOGO_ANIMATION_COUNT; jj++)
            {
                u8g2_ClearBuffer(&u8g2);

                u8g2_DrawXBM(&u8g2, anim_x, anim_y, AF_LOGO_WIDTH, AF_LOGO_HEIGHT, (uint8_t *)af_logo_images[jj]);

                u8g2_SendBuffer(&u8g2);
                time_millis_delay(ANIMATION_FRAME_DURATION_MS);
            }
        }
        else
        {
            // Draw all startup info
            u8g2_ClearBuffer(&u8g2);

            u8g2_SetFontPosTop(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
            uint16_t tw = u8g2_GetStrWidth(&u8g2, SPLASH_TOP);
            u8g2_DrawStr(&u8g2, 5, 3, SPLASH_TOP);

            u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

            tw = u8g2_GetStrWidth(&u8g2, SOFTWARE_VERSION);
            u8g2_DrawStr(&u8g2, w - (w / 4) - tw + 17, 6, SOFTWARE_VERSION);

            u8g2_SetFontPosBottom(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);

            uint16_t bw = u8g2_GetStrWidth(&u8g2, SPLASH_AUTHOR_LABEL);
            u8g2_DrawStr(&u8g2, (w - bw) / 2, h, SPLASH_AUTHOR_LABEL);

            u8g2_DrawXBM(&u8g2, anim_x, anim_y, AF_LOGO_WIDTH, AF_LOGO_HEIGHT, AF_LOGO);

            u8g2_SendBuffer(&u8g2);
            time_millis_delay(ANIMATION_FRAME_DURATION_MS);
        }
    }

#ifdef USE_WIFI
    if (screen->internal.wifi->enable)
    {
        screen->internal.tracker->internal.status_changed(screen->internal.tracker, TRACKER_STATUS_WIFI_CONNECTING);
        wifi_start(screen->internal.wifi);
    }
    else
    {
        screen->internal.wifi->status_change(screen->internal.wifi, WIFI_STATUS_NONE);
    }
#endif

    time_millis_delay(ANIMATION_FRAME_DURATION_MS);

    screen->internal.splashing = false;
    LOG_I(TAG, "Screen splashing done.");

    vTaskDelete(NULL);
}

void screen_splash(screen_t *screen)
{
    xTaskCreatePinnedToCore(screen_splash_task, "SCREEN-SPLASH", 4096, screen, 2, NULL, xPortGetCoreID());
}

void screen_set_orientation(screen_t *screen, screen_orientation_e orientation)
{
    const u8g2_cb_t *rotation = U8G2_R0;
    switch (orientation)
    {
    case SCREEN_ORIENTATION_HORIZONTAL_LEFT:
        // Default rotation, buttons at the left
        break;
    case SCREEN_ORIENTATION_HORIZONTAL_RIGHT:
        // Buttons at the right
        rotation = U8G2_R2;
        break;
    case SCREEN_ORIENTATION_VERTICAL:
        // Buttons on botttom
        rotation = U8G2_R1;
        break;
    case SCREEN_ORIENTATION_VERTICAL_UPSIDE_DOWN:
        rotation = U8G2_R3;
        break;
    }
    u8g2_SetDisplayRotation(&u8g2, rotation);
}

void screen_set_brightness(screen_t *screen, screen_brightness_e brightness)
{
    int contrast = -1;
    switch (brightness)
    {
    case SCREEN_BRIGHTNESS_LOW:
        /* XXX: Don't use 0 contrast here, since some screens will turn
         * off completely with zero. Using 1 has no noticeable difference
         * and let's us easily make the same code work in all supported
         * boards. See #1 for discussion.
         */
        contrast = 1;
        break;
    case SCREEN_BRIGHTNESS_MEDIUM:
        contrast = 128;
        break;
    case SCREEN_BRIGHTNESS_HIGH:
        contrast = 255;
        break;
    }
    if (contrast >= 0)
    {
        u8g2_SetContrast(&u8g2, contrast);
    }
}

bool screen_is_animating(const screen_t *screen)
{
    return false;
}


static bool screen_manual_button_handle(tracker_t *t, const button_t *btn)
{
    switch (btn->id)
    {
    case BUTTON_ID_ENTER:
        return false;
    case BUTTON_ID_LEFT:
        tracker_pan_move(t, -1);
        break;
    case BUTTON_ID_RIGHT:
        tracker_pan_move(t, 1);
        break;
    case BUTTON_ID_UP:
        tracker_tilt_move(t, 1);
        break;
    case BUTTON_ID_DOWN:
        tracker_tilt_move(t, -1);
        break;
    }

    return true;
}

static bool screen_button_enter_press(screen_t *screen, const button_event_t *ev)
{
    tracker_t *t = screen->internal.tracker;

    switch (ev->type)
    {
    case BUTTON_EVENT_TYPE_SHORT_PRESS:
#ifdef USE_WIFI
        if (screen->internal.main_mode == SCREEN_MODE_WIFI_CONFIG)
        {
            screen->internal.wifi->status_change(screen->internal.wifi, WIFI_STATUS_CONNECTING);
            wifi_smartconfig_stop(screen->internal.wifi);
            wifi_start(screen->internal.wifi);
            return true;
        }
#endif
        break;
    case BUTTON_EVENT_TYPE_DOUBLE_PRESS:
        if (screen->internal.main_mode == SCREEN_MODE_WIFI_CONFIG)
        {
            const setting_t *setting = settings_get_key(SETTING_KEY_WIFI_ENABLE);
            setting_set_bool(setting, false);
            screen->internal.wifi->status_change(screen->internal.wifi, WIFI_STATUS_NONE);
            wifi_smartconfig_stop(screen->internal.wifi);
            return true;
        }

        if (t->internal.status >= TRACKER_STATUS_WIFI_CONNECTING && t->internal.status != TRACKER_STATUS_MANUAL) 
        {
            t->internal.status_changed(t, TRACKER_STATUS_MANUAL);
        } 
        else if (t->internal.status == TRACKER_STATUS_MANUAL) 
        { 
            t->internal.status_changed(t, TRACKER_STATUS_TRACKING);
        }
        break;
    case BUTTON_EVENT_TYPE_LONG_PRESS:
#ifdef USE_WIFI
        if (screen->internal.main_mode == SCREEN_MODE_WAIT_CONNECT)
        {
            screen->internal.wifi->status_change(screen->internal.wifi, WIFI_STATUS_SMARTCONFIG);
            return true;
        }
#endif
        if (t->internal.status == TRACKER_STATUS_MANUAL && screen->internal.main_mode == SCREEN_MODE_MAIN)
        {
            const setting_t *setting_course = settings_get_key(SETTING_KEY_SERVO_COURSE);
            servo_config_t *config = &screen->internal.tracker->servo->internal.pan.config;
            servo_status_t *servo_pan = &screen->internal.tracker->servo->internal.pan;
            uint16_t deg = (servo_pan->currtent_pulsewidth - config->min_pulsewidth) / ((config->max_pulsewidth - config->min_pulsewidth) / config->max_degree);
            if (servo_pan->is_reverse) deg = 359 - deg;
            t->home->heading = deg;
            t->servo->internal.course = deg;
            setting_set_u16(setting_course, deg);
            
            LOG_I(TAG, "Course degree set to %d", deg);

            return true;
        }
        break;
    case BUTTON_EVENT_TYPE_REALLY_LONG_PRESS:
        if (t->internal.status == TRACKER_STATUS_MANUAL && screen->internal.main_mode == SCREEN_MODE_MAIN)
        {
            const setting_t *setting_course = settings_get_key(SETTING_KEY_SERVO_COURSE);
            uint16_t deg = 0;
            t->servo->internal.course = deg;
            t->home->heading = deg;
            setting_set_u16(setting_course, deg);

            LOG_I(TAG, "Course degree set to %d", deg);

            return true;
        }
        break;
    }

    return false;
}

static bool screen_button_left_press(screen_t *screen, const button_event_t *ev)
{
    tracker_t *t = screen->internal.tracker;

    if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS || ev->type == BUTTON_EVENT_TYPE_DOUBLE_PRESS)
    {
        if (t->internal.status == TRACKER_STATUS_MANUAL)
        {
            return screen_manual_button_handle(t, ev->button);
        }
    }

    if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS)
    {
        screen->internal.main_secondary_mode = screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_DEFAULT ? SCREEN_MAIN_SECONDARY_MODE_NUM - 1 : screen->internal.main_secondary_mode - 1;
        
        if (!screen->internal.tracker->imu->enable)
        {
            if (screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_IMU || 
                screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_HORIZONTAL)
            {
                screen->internal.main_secondary_mode = SCREEN_MAIN_SECONDARY_MODE_TRACKER;
            }
        }

        return true;
    }

    return false;
}

static bool screen_button_right_press(screen_t *screen, const button_event_t *ev)
{
    tracker_t *t = screen->internal.tracker;

    if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS || ev->type == BUTTON_EVENT_TYPE_DOUBLE_PRESS)
    {
        if (t->internal.status == TRACKER_STATUS_MANUAL)
        {
            return screen_manual_button_handle(t, ev->button);
        }

        if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS)
        {
            screen->internal.main_secondary_mode = screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_NUM - 1 ? SCREEN_MAIN_SECONDARY_MODE_DEFAULT : screen->internal.main_secondary_mode + 1;

            if (!screen->internal.tracker->imu->enable)
            {
                if (screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_IMU || 
                    screen->internal.main_secondary_mode == SCREEN_MAIN_SECONDARY_MODE_HORIZONTAL)
                {
                    screen->internal.main_secondary_mode = SCREEN_MAIN_SECONDARY_MODE_DEFAULT;
                }
            }

            return true;
        }
    }

    return false;
}

static bool screen_button_up_press(screen_t *screen, const button_event_t *ev)
{
    tracker_t *t = screen->internal.tracker;

    if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS || ev->type == BUTTON_EVENT_TYPE_DOUBLE_PRESS)
    {
        if (t->internal.status == TRACKER_STATUS_MANUAL)
        {
            return screen_manual_button_handle(t, ev->button);
        }
    }
    

    return false;
}

static bool screen_button_down_press(screen_t *screen, const button_event_t *ev)
{
    tracker_t *t = screen->internal.tracker;

    if (ev->type == BUTTON_EVENT_TYPE_SHORT_PRESS || ev->type == BUTTON_EVENT_TYPE_DOUBLE_PRESS)
    {
        if (t->internal.status == TRACKER_STATUS_MANUAL)
        {
            return screen_manual_button_handle(t, ev->button);
        }
    }

    return false;
}

static uint16_t screen_animation_offset(uint16_t width, uint16_t max_width, uint16_t *actual_width)
{
    if (width > max_width)
    {
        // Value is too wide, gotta animate
        uint16_t extra = width - max_width;
        // Move 1 pixel every 200ms, stopping for 3 cycles at each end
        time_ticks_t step_duration = MILLIS_TO_TICKS(200);
        uint16_t stop = 3;
        uint16_t offset = (time_ticks_now() / step_duration) % (extra + stop * 2);
        if (offset < stop * 2)
        {
            offset = 0;
        }
        else
        {
            offset -= stop;
            if (offset > extra - 1)
            {
                offset = extra - 1;
            }
        }
        if (actual_width)
        {
            *actual_width = max_width;
        }
        return offset;
    }
    if (actual_width)
    {
        *actual_width = width;
    }
    return 0;
}

static int screen_autosplit_lines(char *buf, uint16_t max_width)
{
    uint16_t line_width;
    size_t len = strlen(buf);
    const char *p = buf;
    int lines = 1;
    int sep = -1;
    for (int ii = 0; ii <= len; ii++)
    {
        if (buf[ii] == ' ' || buf[ii] == '\0')
        {
            buf[ii] = '\0';
            line_width = u8g2_GetStrWidth(&u8g2, p);
            if (line_width <= max_width)
            {
                // Still fits in the line. Store the separator and continue.
                sep = ii;
                buf[ii] = ' ';
            }
            else
            {
                // We need a new line. Check if we had a previous separator,
                // otherwise break here
                if (sep >= 0)
                {
                    buf[ii] = ' ';
                    buf[sep] = '\n';
                    p = &buf[sep + 1];
                    sep = ii;
                }
                else
                {
                    if (ii == len)
                    {
                        // String ends exactly here, the last line scrolls
                        break;
                    }
                    buf[ii] = '\n';
                    p = &buf[ii + 1];
                    sep = -1;
                }
                lines++;
            }
        }
    }
    buf[len] = '\0';
    return lines;
}

static int screen_draw_multiline(char *buf, uint16_t y, screen_multiline_opt_e opt)
{
    const uint16_t display_width = u8g2_GetDisplayWidth(&u8g2);
    uint16_t max_width = display_width;
    int border = 0;
    int x = 0;
    if (opt == SCREEN_MULTILINEOPT_BORDER)
    {
        max_width -= 2;
        border = 2;
    }
    int lines = screen_autosplit_lines(buf, max_width);
    int line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    int text_height = lines * line_height;
    int yy = y;

    u8g2_SetFontPosTop(&u8g2);

    switch (opt)
    {
    case SCREEN_MULTILINEOPT_NONE:
        u8g2_SetDrawColor(&u8g2, 1);
        break;
    case SCREEN_MULTILINEOPT_BORDER:
        u8g2_SetDrawColor(&u8g2, 1);
        // Draw the frame later in case the text needs animation
        yy += 1;
        x += 1;
        break;
    case SCREEN_MULTILINEOPT_BOX:
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawBox(&u8g2, 0, y, display_width, text_height);
        u8g2_SetDrawColor(&u8g2, 0);
        break;
    }
    for (int ii = 0; ii < lines; ii++)
    {
        const char *s = u8x8_GetStringLineStart(ii, buf);
        uint16_t width = u8g2_GetStrWidth(&u8g2, s);
        if (width <= max_width)
        {
            u8g2_DrawStr(&u8g2, x + (max_width - width) / 2, yy, s);
        }
        else
        {
            // Animate
            int offset = screen_animation_offset(width, max_width - 2, NULL);
            u8g2_DrawStr(&u8g2, x - offset, yy, s);
        }
        yy += line_height;
    }

    if (opt == SCREEN_MULTILINEOPT_BORDER)
    {
        u8g2_DrawFrame(&u8g2, 0, y, display_width, text_height);
    }

    return text_height + border;
}

static void screen_draw_label_value(screen_t *screen, const char *label, const char *val, uint16_t w, uint16_t y, uint16_t sep)
{
    uint16_t label_width = (label ? u8g2_GetStrWidth(&u8g2, label) : 0) + sep;
    uint16_t max_value_width = w - label_width - 1;
    uint16_t val_width = u8g2_GetStrWidth(&u8g2, val);
    uint16_t val_offset = screen_animation_offset(val_width, max_value_width, &val_width);
    u8g2_DrawStr(&u8g2, label_width - val_offset, y, val);
    u8g2_SetDrawColor(&u8g2, 0);
    uint16_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2);
    u8g2_DrawBox(&u8g2, 0, y - line_height, label_width, line_height);
    u8g2_SetDrawColor(&u8g2, 1);
    if (label)
    {
        u8g2_DrawStr(&u8g2, 0, y, label);
    }
}

static void screen_draw_servo(screen_t *s)
{
    u8g2_SetDrawColor(&u8g2, 1);

    const uint16_t per_h = s->internal.h / 16;
    const uint8_t frame_height = 7;

    u8g2_SetFontPosTop(&u8g2);

    char *buf = SCREEN_BUF(s);

    const uint16_t bar_width = 88;
    const uint8_t bar_start_x = 18;
    const uint8_t per_start_x = 108;
    uint8_t tilt_percentage = servo_get_per_pulsewidth(&s->internal.tracker->servo->internal.tilt);
    uint16_t tilt_box_width = (bar_width * tilt_percentage) / 100;
    // icon
    u8g2_DrawXBM(&u8g2, 0, per_h * 3, TILT_ICON_WIDTH, TILT_ICON_HEIGHT, s->internal.tracker->servo->is_reversing ? TILT_ICON : TILT_R_ICON);
    // pluse width and degree
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P:%4dus    D:%3d",
             servo_get_pulsewidth(&s->internal.tracker->servo->internal.tilt), servo_get_degree(&s->internal.tracker->servo->internal.tilt));
    u8g2_DrawStr(&u8g2, bar_start_x, per_h * 3, buf);
    u8g2_DrawHLine(&u8g2, per_start_x - 3, per_h * 3, 2);
    u8g2_DrawHLine(&u8g2, per_start_x - 3, per_h * 3 + 1, 2);
    // percent pulse width
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%3d%%", tilt_percentage);
    u8g2_DrawStr(&u8g2, per_start_x, per_h * 5, buf);
    // frame
    u8g2_DrawFrame(&u8g2, bar_start_x, per_h * 5, bar_width, frame_height);
    // box
    u8g2_DrawBox(&u8g2, bar_start_x, per_h * 5, tilt_box_width, frame_height);

    uint8_t pan_percentage = servo_get_per_pulsewidth(&s->internal.tracker->servo->internal.pan);
    uint16_t pan_box_width = (bar_width * pan_percentage) / 100;
    // label
    u8g2_DrawXBM(&u8g2, 0, per_h * 7, PAN_ICON_WIDTH, PAN_ICON_HEIGHT, PAN_ICON);
    // pluse width and degree
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P:%4dus    D:%3d",
             servo_get_pulsewidth(&s->internal.tracker->servo->internal.pan), servo_get_course_to_degree(s->internal.tracker->servo));
    u8g2_DrawStr(&u8g2, bar_start_x, per_h * 7, buf);
    u8g2_DrawHLine(&u8g2, per_start_x - 3, per_h * 7, 2);
    u8g2_DrawHLine(&u8g2, per_start_x - 3, per_h * 7 + 1, 2);
    // percent pulse width
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%3d%%", pan_percentage);
    u8g2_DrawStr(&u8g2, per_start_x, per_h * 9, buf);
    // frame
    u8g2_DrawFrame(&u8g2, bar_start_x, per_h * 9, bar_width, frame_height);
    // box
    u8g2_DrawBox(&u8g2, bar_start_x, per_h * 9, pan_box_width, frame_height);

    const char *manual = "MANUAL";
    const char *tracking = "TRACKING";
    
    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    uint16_t tw = u8g2_GetStrWidth(&u8g2, s->internal.tracker->internal.status == TRACKER_STATUS_MANUAL ? manual : tracking);
    u8g2_DrawStr(&u8g2, s->internal.w - tw, per_h * 14, s->internal.tracker->internal.status == TRACKER_STATUS_MANUAL ? manual : tracking);

    if (s->internal.tracker->internal.show_coordinate)
    {
        u8g2_SetFontPosTop(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "lat:%03.7f", get_plane_lat());
        u8g2_DrawStr(&u8g2, 0, per_h * 12, buf);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "lon:%03.7f", get_plane_lon());
        u8g2_DrawStr(&u8g2, 0, (per_h * 14), buf);
    }
}


static void screen_draw_wait_server(screen_t *s)
{
    const char *txt = "Connecting to server";
    const char *conn = "<-->";

    u8g2_DrawXBM(&u8g2, 24, 24, TRACKER_WIDTH, TRACKER_HEIGHT, TRACKER_IMG);
    u8g2_DrawXBM(&u8g2, s->internal.w - PHONE_WIDTH - 24, 24, PHONE_WIDTH, PHONE_HEIGHT, PHONE_IMG);

    if (TIME_CYCLE_EVERY_MS(200, 2) == 0)
    {
        u8g2_SetFontPosCenter(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
        uint16_t tw = u8g2_GetStrWidth(&u8g2, conn);
        u8g2_DrawStr(&u8g2, (s->internal.w / 2) - tw / 2, 38, conn);
    }

    if (TIME_CYCLE_EVERY_MS(800, 2) == 0)
    {
        u8g2_SetFontPosCenter(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
        uint16_t tw = u8g2_GetStrWidth(&u8g2, txt);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) + 10, txt);
    }
}

static void screen_draw_plane(screen_t *s)
{
    const uint16_t per_h = s->internal.h / 16;
    char *buf = SCREEN_BUF(s);

    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P.lat:%03.7f", get_plane_lat());
    u8g2_DrawStr(&u8g2, 0, per_h * 4, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P.lon:%03.7f", get_plane_lon());
    u8g2_DrawStr(&u8g2, 0, per_h * 6, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P.alt:%04.2f", get_plane_alt());
    u8g2_DrawStr(&u8g2, 0, per_h * 8, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P.spd:%04.2f", get_plane_speed());
    u8g2_DrawStr(&u8g2, 0, per_h * 10, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "P.dic:%d", get_plane_direction());
    u8g2_DrawStr(&u8g2, 0, per_h * 12, buf);
}

static void screen_draw_tracker(screen_t *s)
{
    const uint16_t per_h = s->internal.h / 16;
    char *buf = SCREEN_BUF(s);

    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.lat:%03.7f", get_tracker_lat());
    u8g2_DrawStr(&u8g2, 0, per_h * 4, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.lon:%03.7f", get_tracker_lon());
    u8g2_DrawStr(&u8g2, 0, per_h * 6, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.alt:%04.2f", get_tracker_alt());
    u8g2_DrawStr(&u8g2, 0, per_h * 8, buf);
}

static void screen_draw_imu(screen_t *s)
{
    const uint16_t per_h = s->internal.h / 16;
    char *buf = SCREEN_BUF(s);

    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.Roll :%-5.2f", get_tracker_roll());
    u8g2_DrawStr(&u8g2, 0, per_h * 4, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.Pitch:%-5.2f", get_tracker_pitch());
    u8g2_DrawStr(&u8g2, 0, per_h * 6, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.Yaw  :%-5.2f", get_tracker_yaw());
    u8g2_DrawStr(&u8g2, 0, per_h * 8, buf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "T.Feq  :%dhz", get_tracker_imu_hz());
    u8g2_DrawStr(&u8g2, 0, per_h * 10, buf);
}

static void screen_draw_horizontal(screen_t *s)
{
    const int line_w = s->internal.h - 4;
    const int line_w_h = line_w / 2;

    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

    const int c_x = s->internal.w / 2;
    const int c_y = (s->internal.h / 2) + 4;

    u8g2_DrawLine(&u8g2, c_x - 2, c_y, c_x + 2, c_y);
    u8g2_DrawLine(&u8g2, c_x, c_y - 2, c_x, c_y + 2);

    double d_x = line_w_h * sin((90 - get_tracker_roll()) * DEG_TO_RAD);
    double d_y = line_w_h * sin(get_tracker_roll() * DEG_TO_RAD);

    int x0 = c_x + ((d_x < 1.00 && d_x > - 1.00) ? 0 : d_x);
    int y0 = c_y + ((d_y < 1.00 && d_y > - 1.00) ? 0 : d_y);
    int x1 = c_x - ((d_x < 1.00 && d_x > - 1.00) ? 0 : d_x);
    int y1 = c_y - ((d_y < 1.00 && d_y > - 1.00) ? 0 : d_y);

    int pitch_l = ((line_w / 90.00f) * get_tracker_pitch()) / 2;

    if (abs(pitch_l) > line_w_h) 
    {
        pitch_l = pitch_l < 0 ? line_w - abs(pitch_l) : 0 - line_w - abs(pitch_l);
    }

    u8g2_DrawLine(&u8g2, x0, y0 + pitch_l, x1, y1 + pitch_l);
}

static void screen_draw_main(screen_t *s)
{
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFontPosTop(&u8g2);

    char *buf = SCREEN_BUF(s);

    uint8_t icon_index = 0;
    uint16_t tw = 0;

    if (s->internal.tracker->internal.flag & TRACKER_FLAG_WIFI_CONNECTED)
    {
        u8g2_DrawXBM(&u8g2, icon_index, 0, SMALL_WIFI_WIDTH, SMALL_WIFI_HEIGHT, SMALL_WIFI_ICON);
        icon_index += SMALL_WIFI_WIDTH + 4;
    }

    if (s->internal.tracker->internal.flag & TRACKER_FLAG_HOMESETED)
    {
        u8g2_DrawXBM(&u8g2, icon_index, 0, HOME_WIDTH, HOME_HEIGHT, HOME_ICON);
        icon_index += HOME_WIDTH + 4;
    }

    if (s->internal.tracker->internal.flag & TRACKER_FLAG_PLANESETED)
    {
        u8g2_DrawXBM(&u8g2, icon_index, 0, AIRPLANE_WIDTH, AIRPLANE_HEIGHT, AIRPLANE_ICON);
        icon_index += AIRPLANE_WIDTH + 4;
    }

#ifdef USE_POWER_MONITORING
    if (s->internal.power->enable && settings_get_key_bool(SETTING_KEY_TRACKER_MONITOR_POWER_TURN))
    {
        if (power_get_power_good(s->internal.power) || (TIME_CYCLE_EVERY_MS(500, 2) == 0))
        {
            u8g2_DrawXBM(&u8g2, icon_index, 0, POWER_WIDTH, POWER_HEIGHT, POWER_ICON);
        }

        icon_index += POWER_WIDTH + 4;
    }
#endif

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "C->%d", s->internal.tracker->home->heading);
    u8g2_DrawStr(&u8g2, icon_index, 0, buf);
    icon_index += (u8g2_GetStrWidth(&u8g2, buf) + 1);
    u8g2_DrawHLine(&u8g2, icon_index, 0, 2);
    u8g2_DrawHLine(&u8g2, icon_index, 1, 2);

#ifdef USE_BATTERY_MONITORING

    if (s->internal.battery->enable)
    {
        float voltage = battery_get_voltage(s->internal.battery);
        int per_voltage = 0;

        if (voltage >= s->internal.battery->center_voltage)
        {
            per_voltage = (((voltage - s->internal.battery->center_voltage) / (s->internal.battery->max_voltage - s->internal.battery->center_voltage) * 0.5) + 0.5) * BATTERY_BOX_WIDTH;
        }
        else if (voltage >= s->internal.battery->min_voltage)
        {
            per_voltage = ((voltage - s->internal.battery->min_voltage) / (s->internal.battery->center_voltage - s->internal.battery->min_voltage)) * (BATTERY_BOX_WIDTH / 2);
        }

        if (voltage > s->internal.battery->center_voltage - ((s->internal.battery->center_voltage - s->internal.battery->min_voltage) / 2) || (TIME_CYCLE_EVERY_MS(500, 2) == 0))
        {
            // battery icon
            u8g2_DrawXBM(&u8g2, s->internal.w - BATTERY_WIDTH, 0, BATTERY_WIDTH, BATTERY_HEIGHT, BATTERY_IMG);
            u8g2_DrawBox(&u8g2, s->internal.w - BATTERY_WIDTH + 1, 1, per_voltage, BATTERY_BOX_HEIGHT);
        }
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%2.2fv", voltage);
        tw = u8g2_GetStrWidth(&u8g2, buf) + 1;
        u8g2_DrawStr(&u8g2, s->internal.w - BATTERY_WIDTH - tw, 0, buf);

        s->internal.battery->voltage = voltage;

        if (s->internal.power->enable)
        {
            if (voltage < s->internal.battery->min_voltage && s->internal.power->turn_status)
            {
                power_turn_off(s->internal.power);
            }

            if (!s->internal.power->turn_status && voltage > s->internal.battery->min_voltage && settings_get_key_bool(SETTING_KEY_TRACKER_MONITOR_POWER_TURN))
            {
                power_turn_on(s->internal.power);
            }
        }
    }
#endif

    if ((s->internal.tracker->internal.flag & TRACKER_FLAG_SERVER_CONNECTED) || s->internal.tracker->internal.status == TRACKER_STATUS_MANUAL 
        || (s->internal.tracker->internal.status == TRACKER_STATUS_TRACKING && s->internal.wifi->status == WIFI_STATUS_NONE))
    {
        switch (s->internal.main_secondary_mode)
        {
        case SCREEN_MAIN_SECONDARY_MODE_DEFAULT:
            screen_draw_servo(s);
            break;
        case SCREEN_MAIN_SECONDARY_MODE_PLANE:
            screen_draw_plane(s);
            break;
        case SCREEN_MAIN_SECONDARY_MODE_TRACKER:
            screen_draw_tracker(s);
            break;
        case SCREEN_MAIN_SECONDARY_MODE_IMU:
            screen_draw_imu(s);
            break;
        case SCREEN_MAIN_SECONDARY_MODE_HORIZONTAL:
            screen_draw_horizontal(s);
            break;
        }
    }
    else
    {
        screen_draw_wait_server(s);
    }
}

#ifdef USE_WIFI
static void screen_draw_wifi_config(screen_t *s)
{
    const char *sc_marker = "* * * * * * *";
    bool draw_sc_marker = TIME_CYCLE_EVERY_MS(200, 2) == 0;

    u8g2_SetDrawColor(&u8g2, 1);
    char *buf = SCREEN_BUF(s);
    uint16_t tw = 0;

     u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
    u8g2_SetFontPosCenter(&u8g2);

    if (draw_sc_marker)
    {
        tw = u8g2_GetStrWidth(&u8g2, sc_marker);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, (s->internal.h / 2) - 15, sc_marker);
    }

    //draw smart config
    if (TIME_CYCLE_EVERY_MS(800, 2) == 0)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "SmartConfig");
        tw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h / 2, buf);
    }

    if (draw_sc_marker)
    {
        tw = u8g2_GetStrWidth(&u8g2, sc_marker);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, (s->internal.h / 2) + 20, sc_marker);
    }
}
#endif

uint8_t wifi_index = 0;
bool last_flash = true;

#ifdef USE_WIFI
static void screen_draw_wait_connect(screen_t *s)
{
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_micro_tr);
    u8g2_SetFontPosTop(&u8g2);

    char *buf = SCREEN_BUF(s);

    bool flash = TIME_CYCLE_EVERY_MS(400, 2) == 0;

    if (last_flash != flash)
    {
        wifi_index++;
        last_flash = flash;
    }

    if (wifi_index > 3)
        wifi_index = 0;

    u8g2_DrawXBM(&u8g2, 0, 0, WIFI_WIDTH, WIFI_HEIGHT, (uint8_t *)wifi_images[wifi_index]);

    u8g2_SetFontPosBottom(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "SSID:%s", (char *)&s->internal.wifi->config->sta.ssid);
    u8g2_DrawStr(&u8g2, WIFI_WIDTH + 6, WIFI_HEIGHT / 2 + 5, buf);

    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "PWD:%s", (char *)&s->internal.wifi->config->sta.password);
    u8g2_DrawStr(&u8g2, WIFI_WIDTH + 6, WIFI_HEIGHT, buf);

    //draw connecting
    if (TIME_CYCLE_EVERY_MS(600, 2) == 0)
    {
        u8g2_SetFontPosCenter(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "== CONNECTING ==");
        uint16_t tw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) - 3, buf);
    }

    u8g2_SetFontPosBottom(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "long press to config");
    uint16_t tw = u8g2_GetStrWidth(&u8g2, buf);
    u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h, buf);
}
#endif

static void screen_draw_menu(screen_t *s, menu_t *menu, uint16_t y)
{
#define MENU_LINE_HEIGHT 12
    int entries = menu_get_num_entries(menu);
    u8g2_SetFontPosTop(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf);
    const char *prompt = menu_get_prompt(menu);
    if (prompt)
    {
        uint16_t pw = u8g2_GetStrWidth(&u8g2, prompt);
        uint16_t mpw = SCREEN_W(s) - 4;
        uint16_t px;
        if (pw <= mpw)
        {
            px = (SCREEN_W(s) - pw) / 2;
        }
        else
        {
            uint16_t po = screen_animation_offset(pw, mpw, &pw);
            px = 2 - po;
        }
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawStr(&u8g2, px, y + 2, prompt);
        u8g2_DrawFrame(&u8g2, 0, y, SCREEN_W(s), MENU_LINE_HEIGHT + 1);
        y += MENU_LINE_HEIGHT + 2;
    }
    // Check if we need to skip some entries to allow the selected entry
    // to be displayed.
    int start = 0;
    int selected = menu_get_entry_selected(menu);
    while (y + ((selected - start + 1) * MENU_LINE_HEIGHT) > SCREEN_H(s))
    {
        start++;
    }
    for (int ii = start; ii < entries; ii++)
    {
        const char *title = menu_entry_get_title(menu, ii, SCREEN_BUF(s), SCREEN_DRAW_BUF_SIZE);
        u8g2_SetDrawColor(&u8g2, 1);
        if (menu_is_entry_selected(menu, ii))
        {
            u8g2_DrawBox(&u8g2, 0, y, SCREEN_W(s), 12);
            u8g2_SetDrawColor(&u8g2, 0);
        }
        uint16_t tw = u8g2_GetStrWidth(&u8g2, title);
        uint16_t offset = screen_animation_offset(tw, SCREEN_W(s) - 2, &tw);
        u8g2_DrawStr(&u8g2, 0 - offset, y, title);
        y += MENU_LINE_HEIGHT;
    }
}

static void screen_draw_debug_info(screen_t *s)
{
    char *buf = SCREEN_BUF(s);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFontPosBottom(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

    uint16_t y = 8;

    snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%.02f C", system_temperature());
    screen_draw_label_value(s, "Core Temp:", buf, SCREEN_W(s), y, 3);
    y += 8;

    if (s->internal.tracker->internal.flag && TRACKER_FLAG_HOMESETED)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%03.7f", get_tracker_lat());
        screen_draw_label_value(s, "H.Lat:", buf, SCREEN_W(s), y, 3);
        y += 8;

        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%03.7f", get_tracker_lon());
        screen_draw_label_value(s, "H.Lon:", buf, SCREEN_W(s), y, 3);
        y += 8;

        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%05.2f", get_tracker_alt());
        screen_draw_label_value(s, "H.Alt:", buf, SCREEN_W(s), y, 3);
        y += 8;
    }

    switch (s->internal.tracker->internal.status)
    {
    case TRACKER_STATUS_BOOTING:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "BOOTING");
        break;
    case TRACKER_STATUS_WIFI_SMART_CONFIG:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "WIFI_SMART_CONFIG");
        break;
    case TRACKER_STATUS_WIFI_CONNECTING:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "WIFI_CONNECTING");
        break;
    case TRACKER_STATUS_WIFI_CONNECTED:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "WIFI_CONNECTED");
        break;
    case TRACKER_STATUS_TRACKING:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "TRACKING");
        break;
    case TRACKER_STATUS_MANUAL:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "MANUAL");
        break;
    }

    screen_draw_label_value(s, "T.Status:", buf, SCREEN_W(s), y, 3);
    y += 8;

    switch (s->internal.wifi->status)
    {
    case WIFI_STATUS_NONE:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "NONE");
        break;
    case WIFI_STATUS_SMARTCONFIG:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "SMARTCONFIG");
        break;
    case WIFI_STATUS_DISCONNECTED:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "DISCONNECTED");
        break;
    case WIFI_STATUS_CONNECTING:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "CONNECTING");
        break;
    case WIFI_STATUS_CONNECTED:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "CONNECTED");
        break;
    case WIFI_STATUS_UDP_CONNECTED:
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "UDP_CONNECTED");
        break;
    }

    screen_draw_label_value(s, "W.Status:", buf, SCREEN_W(s), y, 3);
    y += 8;

    snprintf(buf, SCREEN_DRAW_BUF_SIZE, VERSION);
    screen_draw_label_value(s, "Version:", buf, SCREEN_W(s), y, 3);
    y += 8;
}

static void screen_draw_calibration_acc(screen_t *s)
{
    uint16_t tw;
    char *buf = SCREEN_BUF(s);
    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);

    if (s->internal.tracker->imu->calibration_acc_step <= 6)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "Calibration Step %d", s->internal.tracker->imu->calibration_acc_step);
        tw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) * 3, buf);

        if (s->internal.tracker->imu->calibration_time > 0)
        {
            snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%d", s->internal.tracker->imu->calibration_time);
            //u8g2_SetFontPosCenter(&u8g2);
            tw = u8g2_GetStrWidth(&u8g2, buf);
            u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) - 3, buf);
        }
        else
        {
            if (s->internal.tracker->imu->calibration_acc_step == 0)
            {
                snprintf(buf, SCREEN_DRAW_BUF_SIZE, "Press [enter] to start");
            }
            else
            {
                snprintf(buf, SCREEN_DRAW_BUF_SIZE, "Press [enter] to next");
            }

            u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
            //u8g2_SetFontPosCenter(&u8g2);
            tw = u8g2_GetStrWidth(&u8g2, buf);
            u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) - 3, buf);
        }
    }
    else
    {
        if (TIME_CYCLE_EVERY_MS(600, 2) == 0)
        {
            snprintf(buf, SCREEN_DRAW_BUF_SIZE, "== DONE ==");
            tw = u8g2_GetStrWidth(&u8g2, buf);
            u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 2) - 4, buf);
        }
    }
}

static void screen_draw_calibration(screen_t *s)
{
    uint16_t tw;
    char *buf = SCREEN_BUF(s);
    u8g2_SetFontPosCenter(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont15_tf);

    if (s->internal.tracker->imu->calibration_time > 0)
    {
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "Calibration");
        tw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) * 3, buf);
        snprintf(buf, SCREEN_DRAW_BUF_SIZE, "%d", s->internal.tracker->imu->calibration_time);
        tw = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 4) - 3, buf);
    }
    else
    {
        if (TIME_CYCLE_EVERY_MS(600, 2) == 0)
        {
            snprintf(buf, SCREEN_DRAW_BUF_SIZE, "== DONE ==");
            uint16_t tw = u8g2_GetStrWidth(&u8g2, buf);
            u8g2_DrawStr(&u8g2, (s->internal.w - tw) / 2, s->internal.h - (s->internal.h / 2), buf);
        }
    }
}

static void screen_draw(screen_t *screen)
{
    menu_t *menu = menu_get_active();

    // There's nothing overriding the screen, draw the normal interface
    while (menu == &menu_empty)
    {
        menu_pop_active();
        menu = menu_get_active();
    }
    if (menu != NULL && screen->internal.secondary_mode == SCREEN_SECONDARY_MODE_NONE)
    {
        screen_draw_menu(screen, menu, 0);
        
#ifdef USE_BATTERY_MONITORING
        if (screen->internal.battery->enable)
        {
            screen->internal.battery->voltage = battery_get_voltage(screen->internal.battery);
            const setting_t *setting_voltage = settings_get_key(SETTING_KEY_TRACKER_MONITOR_BATTERY_VOLTAGE);
            setting_set_u16(setting_voltage, screen->internal.battery->voltage * 100);
        }
#endif
    }
    else
    {
        switch ((screen_secondary_mode_e)screen->internal.secondary_mode)
        {
        case SCREEN_SECONDARY_MODE_NONE:
            switch ((screen_main_mode_e)screen->internal.main_mode)
            {
            case SCREEN_MODE_MAIN:
                screen_draw_main(screen);
                break;
            case SCREEN_MODE_WIFI_CONFIG:
#ifdef USE_WIFI
                screen_draw_wifi_config(screen);
#endif
                break;
            case SCREEN_MODE_WAIT_CONNECT:
#ifdef USE_WIFI
                screen_draw_wait_connect(screen);
#endif
                break;
            }
            break;
        case SCREEN_SECONDARY_MODE_FREQUENCIES:
            //screen_draw_frequencies(screen);
            break;
        case SCREEN_SECONDARY_MODE_DEBUG_INFO:
            screen_draw_debug_info(screen);
            break;
        case SCREEN_SECONDARY_MODE_CALIBRATION_ACC:
            screen_draw_calibration_acc(screen);
            break;
        case SCREEN_SECONDARY_MODE_CALIBRATION_GYRO:
            screen_draw_calibration(screen);
            break;
        case SCREEN_SECONDARY_MODE_CALIBRATION_MAG:
            screen_draw_calibration(screen);
            break;
        }
    }
}

bool screen_handle_button_event(screen_t *screen, bool before_menu, const button_event_t *ev)
{
    if (screen->internal.secondary_mode != SCREEN_SECONDARY_MODE_NONE)
    {
        if ((screen->internal.secondary_mode == SCREEN_SECONDARY_MODE_CALIBRATION_GYRO || 
                screen->internal.secondary_mode == SCREEN_SECONDARY_MODE_CALIBRATION_MAG)
            && screen->internal.tracker->imu->calibration_time > 0)
        {
            return true;
        }

        if (screen->internal.secondary_mode == SCREEN_SECONDARY_MODE_CALIBRATION_ACC)
        {
            if (screen->internal.tracker->imu->calibration_time <= 0)
            {
                if (screen->internal.tracker->imu->calibration_acc_step <= 6)
                {
                    if (screen->internal.tracker->imu->calibration_acc_step == 6)
                    {
                        imu_task_do_accel_calibration_finish();
                    }
                    else
                    {
                        imu_task_do_accel_calibration_start();
                    }
                
                    screen->internal.tracker->imu->calibration_acc_step++;
                    return true;
                }
            }
            else
            {
                return true;
            }
        }

        screen->internal.secondary_mode = SCREEN_SECONDARY_MODE_NONE;
        return true;
    }

    menu_t *menu = menu_get_active();
    if (menu) return false;

    switch (ev->button->id)
    {
        case BUTTON_ID_ENTER:
            return screen_button_enter_press(screen, ev);
        case BUTTON_ID_LEFT:
            return screen_button_left_press(screen, ev);
        case BUTTON_ID_RIGHT:
            return screen_button_right_press(screen, ev);
        case BUTTON_ID_UP:
            return screen_button_up_press(screen, ev);
        case BUTTON_ID_DOWN:
            return screen_button_down_press(screen, ev);
    }

     return false;
}

void screen_update(screen_t *screen)
{
    if (screen->internal.splashing)
    {
        return;
    }
    
    if (time_millis_now() > screen->internal.next_update)
    {
        char buf[SCREEN_DRAW_BUF_SIZE];

        screen->internal.next_update = time_millis_now() + 10;

        xSemaphoreTake(screen->internal.cfg.i2c_cfg->xSemaphore, portMAX_DELAY );
        {
            screen->internal.w = u8g2_GetDisplayWidth(&u8g2);
            screen->internal.h = u8g2_GetDisplayHeight(&u8g2);
            screen->internal.direction = SCREEN_W(screen) > SCREEN_H(screen) ? SCREEN_DIRECTION_HORIZONTAL : SCREEN_DIRECTION_VERTICAL;
            screen->internal.buf = buf;
            u8g2_ClearBuffer(&u8g2);
            screen_draw(screen);
            u8g2_SendBuffer(&u8g2);
        }
        xSemaphoreGive(screen->internal.cfg.i2c_cfg->xSemaphore);
    }
}

#endif