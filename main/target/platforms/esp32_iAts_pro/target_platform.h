#pragma once

#include <hal/gpio.h>
#include <driver/adc.h>
#include <driver/i2c.h>
#include "default.h"

#define USE_MCPWM
//#define USE_PWMC
//#define USE_TRACKING_LED

#define I2C_BUS I2C_NUM_0
//#define I2C_GPIO_SDA 16 
//#define I2C_GPIO_SCL 17
#define I2C_GPIO_SDA 32
#define I2C_GPIO_SCL 33

#define USE_SCREEN
#define SCREEN_I2C_BUS I2C_NUM_1
#define SCREEN_I2C_ADDR 0x3c
#define SCREEN_I2C_MASTER_FREQ_HZ 1250000
#define SCREEN_GPIO_SDA 16
#define SCREEN_GPIO_SCL 17
#define SCREEN_GPIO_RST 0xFF

#define USE_BEEPER
#define BEEPER_GPIO 13

#define USE_BUTTON_5WAY 
#define BUTTON_ENTER_GPIO 0
#define BUTTON_RIGHT_GPIO 34
#define BUTTON_LEFT_GPIO 14
#define BUTTON_UP_GPIO 35
#define BUTTON_DOWN_GPIO 12

#define USE_LED
#define LED_USE_WS2812
#define LED_USE_FADING
#define LED_1_GPIO 21
#define LED_1_USE_WS2812

#define USE_WIFI

#define SERVO_PAN_GPIO 27
#define SERVO_TILT_GPIO 26

#define USE_GPIO_REMAP

#define TX_DEFAULT_GPIO 4
#define RX_DEFAULT_GPIO 5

#define TX_UNUSED_GPIO 4
#define RX_UNUSED_GPIO 5

#define UART1_TX_DEFAULT_GPIO 4
#define UART1_RX_DEFAULT_GPIO 5

#define UART2_TX_DEFAULT_GPIO 18
#define UART2_RX_DEFAULT_GPIO 19

#define HAL_GPIO_USER_MASK (HAL_GPIO_M(UART1_TX_DEFAULT_GPIO) | HAL_GPIO_M(UART1_RX_DEFAULT_GPIO) | HAL_GPIO_M(UART2_TX_DEFAULT_GPIO) | HAL_GPIO_M(UART2_RX_DEFAULT_GPIO))

#define USE_MONITORING

#if defined(USE_MONITORING)
    #define USE_BATTERY_MONITORING
    /* The ratio of voltage resistors */
    #define BATTERY_PARTIAL_PRESSURE_VALUE 90.00f
    #define BATTERY_ADC_CHAENNL ADC1_CHANNEL_0
    #define BATTERY_ADC_ATTEN ADC_ATTEN_DB_2_5
    #define BATTERY_ADC_UNIT ADC_UNIT_1
    #define BATTERY_ADC_WIDTH ADC_WIDTH_BIT_12

    #define USE_POWER_MONITORING
    #define POWER_MONITORING_GPIO 2
    #define POWER_REMOTE_GPIO 15
#endif

#if defined(USE_RX5808)
    /* The ratio of voltage resistors */
    #define RX5808_RSSI_ADC_CHAENNL ADC1_CHANNEL_3
    #define RX5808_RSSI_ADC_ATTEN ADC_ATTEN_DB_2_5
    #define RX5808_RSSI_ADC_UNIT ADC_UNIT_1
    #define RX5808_RSSI_ADC_WIDTH ADC_WIDTH_BIT_12
    #define RX5808_SPI_BUS HSPI_HOST
    #define RX5808_SPI_CLK_GPIO 6
    #define RX5808_SPI_CS_GPIO 7
    #define RX5808_SPI_MOSI_GPIO 8

    #define RX5808_SPI_ADDRESS_SYNTH_A 0x01
    #define RX5808_SPI_ADDRESS_POWER 0x0A
#endif

#if defined(USE_IMU)
    #define MPU9250
#endif

#define BOARD_NAME "iAts_pro"