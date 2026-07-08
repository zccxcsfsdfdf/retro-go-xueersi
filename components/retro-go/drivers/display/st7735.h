#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

static spi_device_handle_t spi_dev;
static QueueHandle_t spi_transactions;
static QueueHandle_t spi_buffers;

#define SPI_TRANSACTION_COUNT (10)
#define SPI_BUFFER_COUNT      (5)
#define SPI_BUFFER_LENGTH     (LCD_BUFFER_LENGTH * 2)

#define ST7735_CMD(cmd, data...)                    \
    {                                                \
        const uint8_t c = cmd, x[] = {data};         \
        spi_queue_transaction(&c, 1, 0);             \
        if (sizeof(x))                               \
            spi_queue_transaction(&x, sizeof(x), 1); \
    }

static inline uint16_t *spi_take_buffer(void)
{
    uint16_t *buffer;
    if (xQueueReceive(spi_buffers, &buffer, pdMS_TO_TICKS(2500)) != pdTRUE)
        RG_PANIC("display");
    return buffer;
}

static inline void spi_give_buffer(uint16_t *buffer)
{
    xQueueSend(spi_buffers, &buffer, portMAX_DELAY);
}

static inline void spi_queue_transaction(const void *data, size_t length, uint32_t type)
{
    if (!data || !length)
        return;

    spi_transaction_t *t;
    xQueueReceive(spi_transactions, &t, portMAX_DELAY);

    *t = (spi_transaction_t){
        .tx_buffer = NULL,
        .length = length * 8, // In bits
        .user = (void *)type,
        .flags = 0,
    };

    if (type & 2)
    {
        t->tx_buffer = data;
    }
    else if (length < 5)
    {
        memcpy(t->tx_data, data, length);
        t->flags = SPI_TRANS_USE_TXDATA;
    }
    else
    {
        t->tx_buffer = memcpy(spi_take_buffer(), data, length);
        t->user = (void *)(type | 2);
    }

    if (spi_device_queue_trans(spi_dev, t, pdMS_TO_TICKS(2500)) != ESP_OK)
    {
        RG_PANIC("display");
    }
}

IRAM_ATTR
static void spi_pre_transfer_cb(spi_transaction_t *t)
{
    // Set the data/command line accordingly
    gpio_set_level(RG_GPIO_LCD_DC, (int)t->user & 1);
}

IRAM_ATTR
static void spi_task(void *arg)
{
    spi_transaction_t *t;

    while (spi_device_get_trans_result(spi_dev, &t, portMAX_DELAY) == ESP_OK)
    {
        if ((int)t->user & 2)
            spi_give_buffer((uint16_t *)t->tx_buffer);
        xQueueSend(spi_transactions, &t, portMAX_DELAY);
    }
}

static void spi_init(void)
{
    spi_transactions = xQueueCreate(SPI_TRANSACTION_COUNT, sizeof(spi_transaction_t *));
    spi_buffers = xQueueCreate(SPI_BUFFER_COUNT, sizeof(uint16_t *));

    while (uxQueueSpacesAvailable(spi_transactions))
    {
        void *trans = malloc(sizeof(spi_transaction_t));
        xQueueSend(spi_transactions, &trans, portMAX_DELAY);
    }

    while (uxQueueSpacesAvailable(spi_buffers))
    {
        void *buffer = rg_alloc(SPI_BUFFER_LENGTH, MEM_DMA);
        xQueueSend(spi_buffers, &buffer, portMAX_DELAY);
    }

    const spi_bus_config_t buscfg = {
        .miso_io_num = RG_GPIO_LCD_MISO,
        .mosi_io_num = RG_GPIO_LCD_MOSI,
        .sclk_io_num = RG_GPIO_LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    const spi_device_interface_config_t devcfg = {
        .clock_speed_hz = RG_SCREEN_SPEED,
        .mode = 0,
        .spics_io_num = RG_GPIO_LCD_CS,
        .queue_size = SPI_TRANSACTION_COUNT,
        .pre_cb = &spi_pre_transfer_cb,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    esp_err_t ret;
    ret = spi_bus_initialize(RG_SCREEN_HOST, &buscfg, SPI_DMA_CH_AUTO);
    RG_ASSERT(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE, "spi_bus_initialize failed.");
    ret = spi_bus_add_device(RG_SCREEN_HOST, &devcfg, &spi_dev);
    RG_ASSERT(ret == ESP_OK, "spi_bus_add_device failed.");

    rg_task_create("rg_spi", &spi_task, NULL, 1.5 * 1024, RG_TASK_PRIORITY_7, 1);
}

static void spi_deinit(void)
{
    if (spi_bus_remove_device(spi_dev) == ESP_OK)
        spi_bus_free(RG_SCREEN_HOST);
    else
        RG_LOGE("Failed to properly terminate SPI driver!");
}

static void lcd_init(void)
{
    RG_LOGI("ST7735 LCD init");

    gpio_set_direction(RG_GPIO_LCD_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_DC, 1);

#if defined(RG_GPIO_LCD_RST)
    gpio_set_direction(RG_GPIO_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(RG_GPIO_LCD_RST, 0);
    rg_usleep(100 * 1000);
    gpio_set_level(RG_GPIO_LCD_RST, 1);
    rg_usleep(10 * 1000);
#endif

    spi_init();

    // ST7735 software reset
    ST7735_CMD(0x01);
    rg_usleep(150 * 1000);

    // Exit sleep
    ST7735_CMD(0x11);
    rg_usleep(10 * 1000);

    // Frame rate control
    ST7735_CMD(0xB1, 0x01, 0x2C, 0x2D);
    ST7735_CMD(0xB2, 0x01, 0x2C, 0x2D);
    ST7735_CMD(0xB3, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D);

    // Display inversion control
    ST7735_CMD(0xB4, 0x07);

    // Power control
    ST7735_CMD(0xC0, 0xA2, 0x02, 0x84);
    ST7735_CMD(0xC1, 0xC5);
    ST7735_CMD(0xC2, 0x0A, 0x00);
    ST7735_CMD(0xC3, 0x8A, 0x2A);
    ST7735_CMD(0xC4, 0x8A, 0xEE);
    ST7735_CMD(0xC5, 0x0E);

    // Display inversion (0x20 = off, 0x21 = on)
#if defined(RG_SCREEN_INVERT)
    ST7735_CMD(0x21);
#else
    ST7735_CMD(0x20);
#endif

    // Gamma correction
    ST7735_CMD(0xE0, 0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22, 0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10);
    ST7735_CMD(0xE1, 0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E, 0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10);

    // Color format: RGB565 (0x05 = 16-bit)
    ST7735_CMD(0x3A, 0x05);

    // Memory access control (MADCTL)
    ST7735_CMD(0x36, RG_SCREEN_MADCTL);

    // Normal display mode on
    ST7735_CMD(0x13);

    // Display on
    ST7735_CMD(0x29);
}

static void lcd_deinit(void)
{
    spi_deinit();
    gpio_reset_pin(RG_GPIO_LCD_DC);
#if defined(RG_GPIO_LCD_RST)
    gpio_reset_pin(RG_GPIO_LCD_RST);
#endif
}

static void lcd_sync(void)
{
    // Unused for SPI LCD
}

static void lcd_set_backlight(float percent)
{
    float level = RG_MIN(RG_MAX(percent / 100.f, 0), 1.f);
    int error_code = 0;

#if defined(RG_GPIO_LCD_BCKL)
    error_code = ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0x1FFF * level, 50, 0);
#endif

    if (error_code)
        RG_LOGE("failed setting backlight to %d%% (0x%02X)\n", (int)(100 * level), error_code);
    else
        RG_LOGI("backlight set to %d%%\n", (int)(100 * level));
}

static void lcd_set_window(int left, int top, int width, int height)
{
    int right = left + width - 1;
    int bottom = top + height - 1;

    if (left < 0 || top < 0 || right >= display.screen.real_width || bottom >= display.screen.real_height)
        RG_LOGW("Bad lcd window (x0=%d, y0=%d, x1=%d, y1=%d)\n", left, top, right, bottom);

    ST7735_CMD(0x2A, left >> 8, left & 0xFF, right >> 8, right & 0xFF); // Column address
    ST7735_CMD(0x2B, top >> 8, top & 0xFF, bottom >> 8, bottom & 0xFF); // Row address
    ST7735_CMD(0x2C);                                                    // Memory write
}

static inline uint16_t *lcd_get_buffer(size_t length)
{
    return spi_take_buffer();
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    if (length > 0)
        spi_queue_transaction(buffer, length * sizeof(*buffer), 3);
    else
        spi_give_buffer(buffer);
}

const rg_display_driver_t rg_display_driver_st7735 = {
    .name = "st7735",
};
