#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

/**
 * DIYMORE ESP32-S3 2.8" ILI9341 — Board Config
 * =============================================
 * Board: DIYMORE ESP32-S3 Dev Board + ILI9341 2.8" Capacitive Touch
 *        (i.145270449.47000755039 — "Supports Xiaozhi")
 *
 * ⚠  Pin verification notes:
 *    DISPLAY_SPI_RESET_PIN = GPIO_NUM_NC → uses SPI software reset.
 *    If LCD does not init, check if RST shares GPIO with another pin.
 *
 *    DISPLAY_BACKLIGHT_PIN = GPIO_NUM_48 → change to GPIO_NUM_NC
 *    if backlight is hardwired to 3.3V on your board.
 *
 *    BUILTIN_LED_GPIO = GPIO_NUM_21 → driven LOW in constructor
 *    to extinguish the white LED at boot.
 */

#include <driver/gpio.h>

// ─── Audio — no external codec, direct I2S ───────────────────────────────────
// 16 kHz avoids AFE ringbuffer-full seen at 24 kHz AIPI-Lite rates.
#define AUDIO_INPUT_SAMPLE_RATE   16000
#define AUDIO_OUTPUT_SAMPLE_RATE  16000

// Microphone — standard I2S (3-wire: WS + BCLK + DATA)
// If your mic only has 2 wires (CLK + DATA) it is PDM — see .cc for how to swap.
#define AUDIO_I2S_MIC_GPIO_WS     GPIO_NUM_4    // LRCK / Word Select
#define AUDIO_I2S_MIC_GPIO_SCK    GPIO_NUM_5    // BCLK / Bit Clock
#define AUDIO_I2S_MIC_GPIO_DIN    GPIO_NUM_6    // DATA from microphone

// Speaker — I2S to external Class-D amplifier (MAX98357A / NS4168 / similar)
#define AUDIO_I2S_SPK_GPIO_BCLK   GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_WS     GPIO_NUM_16
#define AUDIO_I2S_SPK_GPIO_DOUT   GPIO_NUM_7
// Amplifier SD_MODE / shutdown pin  (HIGH = on, LOW = off/low-power)
#define AUDIO_PA_CTRL_GPIO        GPIO_NUM_17

// ─── Display — ILI9341 SPI 240×320 ───────────────────────────────────────────
#define DISPLAY_WIDTH             240
#define DISPLAY_HEIGHT            320
#define DISPLAY_OFFSET_X          0
#define DISPLAY_OFFSET_Y          0
#define DISPLAY_MIRROR_X          false
#define DISPLAY_MIRROR_Y          false
#define DISPLAY_SWAP_XY           false
#define DISPLAY_INVERT_COLOR      false           // ILI9341 does not need inversion
#define DISPLAY_RGB_ORDER         LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_SPI_MODE          0

#define DISPLAY_SPI_SCLK_PIN      GPIO_NUM_12
#define DISPLAY_SPI_MOSI_PIN      GPIO_NUM_11
#define DISPLAY_SPI_CS_PIN        GPIO_NUM_10
#define DISPLAY_SPI_DC_PIN        GPIO_NUM_46
// RST = NC → board pulls RST to 3.3V; firmware does software reset via SPI.
#define DISPLAY_SPI_RESET_PIN     GPIO_NUM_NC
#define DISPLAY_SPI_SCLK_HZ       (40 * 1000 * 1000)

// Backlight PWM (active-high).
#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_45
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// ─── Touch — FT6336 I2C (uses FT5x06 driver, address 0x38) ──────────────────
#define TOUCH_I2C_SDA_PIN         GPIO_NUM_9
#define TOUCH_I2C_SCL_PIN         GPIO_NUM_8
#define TOUCH_INT_PIN             GPIO_NUM_NC  // NC = polling mode
#define TOUCH_RST_PIN             GPIO_NUM_NC

// ─── Board indicators & buttons ──────────────────────────────────────────────
// White LED: active-HIGH.  Driven LOW at boot to keep it off.
#define BUILTIN_LED_GPIO          GPIO_NUM_21
// Boot / BOOT button (active-LOW, internal pull-up)
#define BOOT_BUTTON_GPIO          GPIO_NUM_0

#endif  // _BOARD_CONFIG_H_
