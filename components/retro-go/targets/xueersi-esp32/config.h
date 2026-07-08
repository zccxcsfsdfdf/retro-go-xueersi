// Target definition
#define RG_TARGET_NAME             "XUEERSI-XIAOMIAO"
#define RG_PROJECT_NAME            "Xueersi Retro-Go"

// Storage: MicroSD over SPI2, shared with TFT (lower speed for bus stability)
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT

// Audio: Passive Buzzer on GPIO14 (PWM/LEDC driven)
#define RG_AUDIO_USE_BUZZER_PIN     14
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        0   // 0 = Disable, 1 = Enable

// Video: ST7735 160x128 landscape, SPI2 @ 20MHz (safe speed for shared bus)
#define RG_SCREEN_DRIVER            2   // 2 = ST7735
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_20M
#define RG_SCREEN_BACKLIGHT         0
#define RG_SCREEN_WIDTH             160
#define RG_SCREEN_HEIGHT            128
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_MADCTL            0x60 // MX | MV, landscape
#define RG_SCREEN_SWAP_BYTES        1

// Input: 6 GPIO buttons, active-low
// START=UP+DOWN, SELECT=LEFT+RIGHT, MENU=UP+DOWN+B
#define RG_RECOVERY_BTN             RG_KEY_MENU
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_2,  .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_13, .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_27, .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_35, .pullup = 0, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_34, .pullup = 0, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_12, .pullup = 1, .level = 0},\
}
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_START,  .src = RG_KEY_UP | RG_KEY_DOWN},\
    {RG_KEY_SELECT, .src = RG_KEY_LEFT | RG_KEY_RIGHT},\
    {RG_KEY_MENU,   .src = RG_KEY_UP | RG_KEY_DOWN | RG_KEY_B},\
}

// Sensors are present but unused by retro-go
#define RG_BATTERY_DRIVER           0
#define RG_BATTERY_CALC_PERCENT(raw) (100)
#define RG_BATTERY_CALC_VOLTAGE(raw) (0)

// SPI Display
#define RG_GPIO_LCD_MISO            GPIO_NUM_19
#define RG_GPIO_LCD_MOSI            GPIO_NUM_23
#define RG_GPIO_LCD_CLK             GPIO_NUM_18
#define RG_GPIO_LCD_CS              GPIO_NUM_5
#define RG_GPIO_LCD_DC              GPIO_NUM_4

// SPI SD Card
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_19
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_23
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_18
#define RG_GPIO_SDSPI_CS            GPIO_NUM_22