#include "target/target.h"

#if defined(ESP32) && defined(USE_MCPWM)
#include <stdint.h>

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

#include "util/ease.h"

#define MAX_PAN_DEGREE 180
#define MAX_TILT_DEGREE 90

#define SERVO_MCPWM_FREQUENCY 50

#define SERVO_MCPWM_UNIT MCPWM_UNIT_0
#define SERVO_MCPWM_TIMER MCPWM_TIMER_0
#define SERVO_MCPWM_DUTY_MODE MCPWM_DUTY_MODE_0
#define SERVO_MCPWM_COUNTER_MODE MCPWM_UP_COUNTER

#define SERVO_PAN_MCPWM_IO_SIGNALS MCPWM0A
#define SERVO_TILT_MCPWM_IO_SIGNALS MCPWM0B

#define SERVO_PAN_MCPWM_OPR MCPWM_OPR_A
#define SERVO_TILT_MCPWM_OPR MCPWM_OPR_B

typedef struct servo_mcpwm_config_s
{
    //gpio
    uint8_t gpio;

    uint16_t min_pulsewidth;
    uint16_t max_pulsewidth;
    uint16_t max_degree;
    uint16_t min_degree;
} servo_mcpwm_config_t;

typedef struct servo_mcpwm_status_s
{
    uint16_t currtent_degree;
    uint32_t currtent_pulsewidth;
    uint32_t last_pulsewidth;

    uint16_t step_positon;
    bool is_easing;
    bool is_reverse;

    servo_mcpwm_config_t config;
    uint16_t (*pTr_pulsewidth_cal)(servo_mcpwm_config_t *config, uint16_t degree_of_rotation, bool is_reverse);
} servo_mcpwm_status_t;

typedef struct servo_mcpwm_s
{
    struct 
    { 
        servo_mcpwm_status_t pan;
        servo_mcpwm_status_t tilt;

        ease_config_t ease_config;
    } internal;
} servo_mcpwm_t;

void servo_pwm_initialize(servo_mcpwm_t *servo);
void servo_pwm_configuration(servo_mcpwm_status_t *servo, hal_gpio_t gpio);
void servo_pwm_out(servo_mcpwm_status_t *status, uint16_t pulsewidth);
#endif