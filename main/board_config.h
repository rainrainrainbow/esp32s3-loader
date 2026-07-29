#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/uart.h>

/* ── Audio Configuration (ES8311) ─────────────────────────────────────── */
#define AUDIO_INPUT_SAMPLE_RATE     16000
#define AUDIO_OUTPUT_SAMPLE_RATE    16000

#define AUDIO_I2S_GPIO_MCLK         GPIO_NUM_45
#define AUDIO_I2S_GPIO_WS           GPIO_NUM_41
#define AUDIO_I2S_GPIO_BCLK         GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN          GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT         GPIO_NUM_42
#define AUDIO_CODEC_PA_PIN          GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN     GPIO_NUM_4
#define AUDIO_CODEC_I2C_SCL_PIN     GPIO_NUM_5

/* ── Display (ST7789 320x240 SPI) ─────────────────────────────────────── */
#define DISPLAY_WIDTH               320
#define DISPLAY_HEIGHT              240
#define DISPLAY_MIRROR_X            true
#define DISPLAY_MIRROR_Y            false
#define DISPLAY_SWAP_XY             true
#define DISPLAY_OFFSET_X            0
#define DISPLAY_OFFSET_Y            0

#define DISPLAY_DC_GPIO             GPIO_NUM_1
#define DISPLAY_CS_GPIO             GPIO_NUM_2
#define DISPLAY_CLK_GPIO            GPIO_NUM_21
#define DISPLAY_MOSI_GPIO           GPIO_NUM_47
#define DISPLAY_RST_GPIO            GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_PIN       GPIO_NUM_14
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE            0
#define DISPLAY_SPI_HOST            SPI2_HOST

/* ── Touch (GT911 I2C) ────────────────────────────────────────────────── */
#define TOUCH_SDA_GPIO              GPIO_NUM_38
#define TOUCH_SCL_GPIO              GPIO_NUM_48
#define TOUCH_INT_GPIO              GPIO_NUM_NC
#define TOUCH_RST_GPIO              GPIO_NUM_NC
#define TOUCH_I2C_HOST              I2C_NUM_0
#define TOUCH_I2C_CLK_HZ            400000

/* ── Buttons ──────────────────────────────────────────────────────────── */
#define LEFT_BUTTON_GPIO            GPIO_NUM_0
#define RIGHT_BUTTON_GPIO           GPIO_NUM_43

#endif /* _BOARD_CONFIG_H_ */