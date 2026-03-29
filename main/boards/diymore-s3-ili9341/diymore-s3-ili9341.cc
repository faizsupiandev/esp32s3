/**
 * DIYMORE ESP32-S3 2.8" ILI9341 — Board Profile
 * ===============================================
 * Based on AIPI-Lite with the following changes:
 *   - Display   : ST7789 128×128 → ILI9341 240×320
 *   - Audio     : ES8311 external codec → NoAudioCodecSimplex (direct I2S)
 *   - Touch     : none → FT6336 capacitive (via FT5x06 driver)
 *   - LED       : GPIO46 → GPIO21 (driven LOW at boot to extinguish)
 *   - Removed   : PowerManager, PowerSaveTimer, power button, RTC power control
 *                 (no battery management on this board)
 */

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>   // ILI9341 driver from espressif/esp_lcd_ili9341
#include <esp_lcd_touch_ft5x06.h>    // FT5x06/FT6336 driver from espressif/esp_lcd_touch_ft5x06
#include <esp_lvgl_port.h>
#include <esp_log.h>

#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"   // Direct I2S — no external codec
#include "config.h"
#include "display/lcd_display.h"
#include "led/single_led.h"
#include "system_reset.h"
#include "wifi_board.h"

#define TAG "DiyMoreS3"

class DiyMoreS3Board : public WifiBoard {
   private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button                  boot_button_;
    LcdDisplay*             display_  = nullptr;
    esp_lcd_panel_handle_t  panel_    = nullptr;

    // ── GPIO21 LED: drive LOW immediately to extinguish at boot ──────────────
    void InitializeLed() {
        gpio_config_t cfg = {};
        cfg.pin_bit_mask   = (1ULL << BUILTIN_LED_GPIO);
        cfg.mode           = GPIO_MODE_OUTPUT;
        cfg.pull_up_en     = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type      = GPIO_INTR_DISABLE;
        gpio_config(&cfg);
        gpio_set_level(BUILTIN_LED_GPIO, 0);   // LED OFF
        ESP_LOGI(TAG, "LED GPIO%d → LOW (off)", (int)BUILTIN_LED_GPIO);
    }

    // ── Speaker amplifier enable pin (SD_MODE / SHDN) ────────────────────────
    void InitializePowerAmplifier() {
        if (AUDIO_PA_CTRL_GPIO == GPIO_NUM_NC) return;
        gpio_config_t cfg = {};
        cfg.pin_bit_mask   = (1ULL << AUDIO_PA_CTRL_GPIO);
        cfg.mode           = GPIO_MODE_OUTPUT;
        cfg.pull_up_en     = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type      = GPIO_INTR_DISABLE;
        gpio_config(&cfg);
        gpio_set_level(AUDIO_PA_CTRL_GPIO, 1);  // Amplifier ON
        ESP_LOGI(TAG, "PA enable GPIO%d → HIGH", (int)AUDIO_PA_CTRL_GPIO);
    }

    // ── I2C bus for FT6336 touch ──────────────────────────────────────────────
    void InitializeI2c() {
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.i2c_port             = I2C_NUM_0;
        bus_cfg.sda_io_num           = TOUCH_I2C_SDA_PIN;
        bus_cfg.scl_io_num           = TOUCH_I2C_SCL_PIN;
        bus_cfg.clk_source           = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt    = 7;
        bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
        ESP_LOGI(TAG, "I2C bus init SDA=%d SCL=%d",
                 (int)TOUCH_I2C_SDA_PIN, (int)TOUCH_I2C_SCL_PIN);
    }

    // ── SPI bus for ILI9341 ───────────────────────────────────────────────────
    void InitializeSpi() {
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num      = DISPLAY_SPI_MOSI_PIN;
        bus_cfg.miso_io_num      = GPIO_NUM_NC;
        bus_cfg.sclk_io_num      = DISPLAY_SPI_SCLK_PIN;
        bus_cfg.quadwp_io_num    = GPIO_NUM_NC;
        bus_cfg.quadhd_io_num    = GPIO_NUM_NC;
        bus_cfg.max_transfer_sz  = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
        ESP_LOGI(TAG, "SPI bus init MOSI=%d CLK=%d",
                 (int)DISPLAY_SPI_MOSI_PIN, (int)DISPLAY_SPI_SCLK_PIN);
    }

    // ── ILI9341 LCD + LVGL ────────────────────────────────────────────────────
    void InitializeLcdDisplay() {
        // 1. Panel IO (SPI)
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_io_spi_config_t io_cfg = {};
        io_cfg.cs_gpio_num        = DISPLAY_SPI_CS_PIN;
        io_cfg.dc_gpio_num        = DISPLAY_SPI_DC_PIN;
        io_cfg.spi_mode           = DISPLAY_SPI_MODE;
        io_cfg.pclk_hz            = DISPLAY_SPI_SCLK_HZ;
        io_cfg.trans_queue_depth  = 10;
        io_cfg.lcd_cmd_bits       = 8;
        io_cfg.lcd_param_bits     = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_cfg, &panel_io));

        // 2. ILI9341 panel driver
        esp_lcd_panel_dev_config_t panel_cfg = {};
        panel_cfg.reset_gpio_num  = DISPLAY_SPI_RESET_PIN;  // NC = software reset
        panel_cfg.rgb_ele_order   = DISPLAY_RGB_ORDER;
        panel_cfg.bits_per_pixel  = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_cfg, &panel_));

        esp_lcd_panel_reset(panel_);
        esp_lcd_panel_init(panel_);
        esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel_,    DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel_,     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel_, true);

        // 3. SpiLcdDisplay wraps panel for LVGL
        display_ = new SpiLcdDisplay(
            panel_io, panel_,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            DISPLAY_SWAP_XY);

        ESP_LOGI(TAG, "ILI9341 ready %dx%d CS=%d DC=%d",
                 DISPLAY_WIDTH, DISPLAY_HEIGHT,
                 (int)DISPLAY_SPI_CS_PIN, (int)DISPLAY_SPI_DC_PIN);
    }

    // ── FT6336 capacitive touch (uses FT5x06 compatible driver) ──────────────
    void InitializeTouch() {
        // Give touch chip time to power up after display init
        vTaskDelay(pdMS_TO_TICKS(100));

        esp_lcd_touch_handle_t     touch_handle = nullptr;
        esp_lcd_panel_io_handle_t  tp_io        = nullptr;

        // FT6336 is I2C-compatible with the FT5x06 driver (same address 0x38)
        // Use 100kHz for better reliability with capacitive touch chips
        esp_lcd_panel_io_i2c_config_t tp_io_cfg =
            ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_cfg.scl_speed_hz = 100000;

        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_cfg, &tp_io);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Touch I2C init failed (0x%x) — running without touch", ret);
            return;
        }

        esp_lcd_touch_config_t tp_cfg = {};
        tp_cfg.x_max          = DISPLAY_WIDTH;
        tp_cfg.y_max          = DISPLAY_HEIGHT;
        tp_cfg.rst_gpio_num   = TOUCH_RST_PIN;  // NC
        tp_cfg.int_gpio_num   = TOUCH_INT_PIN;
        tp_cfg.flags.swap_xy  = DISPLAY_SWAP_XY;
        tp_cfg.flags.mirror_x = DISPLAY_MIRROR_X;
        tp_cfg.flags.mirror_y = DISPLAY_MIRROR_Y;

        ret = esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &touch_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "FT6336 not found at 0x38 (0x%x) — check TOUCH_I2C_SDA/SCL pins in config.h", ret);
            esp_lcd_panel_io_del(tp_io);
            return;
        }

        // Register with LVGL port
        const lvgl_port_touch_cfg_t lvgl_touch_cfg = {
            .disp   = lv_display_get_default(),
            .handle = touch_handle,
        };
        lvgl_port_add_touch(&lvgl_touch_cfg);

        ESP_LOGI(TAG, "FT6336 touch ready SDA=%d SCL=%d INT=%d",
                 (int)TOUCH_I2C_SDA_PIN, (int)TOUCH_I2C_SCL_PIN,
                 (int)TOUCH_INT_PIN);
    }

    // ── Boot button ───────────────────────────────────────────────────────────
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnLongPress([this]() {
            EnterWifiConfigMode();
        });
    }

   public:
    DiyMoreS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        // Order matters — LED first to silence it before display lights up.
        InitializeLed();
        InitializePowerAmplifier();
        InitializeI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeButtons();

        // Restore saved backlight brightness (if backlight pin is configured).
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    // ── Board interface ───────────────────────────────────────────────────────

    virtual Led* GetLed() override {
        // LED is already driven LOW in InitializeLed().
        // SingleLed will control it normally (HIGH = on, LOW = off).
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    /**
     * Audio codec — speaker only, no digital microphone.
     *
     * Hardware has an analog mic (LMA2718 via ADC), not a digital I2S mic.
     * NoAudioCodecSpkOnly initialises only I2S port 0 for the speaker (TX).
     * Read() returns 0 samples → AFE is never fed → no ringbuffer overflow.
     *
     * To add mic support later:
     *   - If you wire a digital I2S mic: switch back to NoAudioCodecSimplex
     *     and set AUDIO_I2S_MIC_GPIO_* pins in config.h.
     *   - If you wire a PDM mic: use NoAudioCodecSimplexPdm.
     */
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSpkOnly audio_codec(
            AUDIO_OUTPUT_SAMPLE_RATE,   // 16000
            AUDIO_I2S_SPK_GPIO_BCLK,   // GPIO15
            AUDIO_I2S_SPK_GPIO_WS,     // GPIO16
            AUDIO_I2S_SPK_GPIO_DOUT);  // GPIO7
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(
                DISPLAY_BACKLIGHT_PIN,
                DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(DiyMoreS3Board);
