#include "rg_system.h"

#if RG_AUDIO_USE_BUZZER_PIN
#include "rg_audio.h"

#ifndef ESP_PLATFORM
#error "Audio buzzer support can only be built inside esp-idf!"
#endif

#include <driver/ledc.h>
#include <driver/timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define BOOSTVOLUME        5
#define QUEUE_SIZE          800

#define LEDC_PWM_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define LEDC_PWM_CHANNEL     LEDC_CHANNEL_0
#define LEDC_PWM_TIMER       LEDC_TIMER_0

static QueueHandle_t sampleQueue;
static int pwm_bits = 10;
static int sampleRate;
static int timer_group = 0;
static int timer_idx = 1;

static bool IRAM_ATTR buzzer_timer_isr(void *arg)
{
    int16_t sample;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueReceiveFromISR(sampleQueue, &sample, &xHigherPriorityTaskWoken) == pdTRUE)
    {
        int32_t duty = (int32_t)sample + 32768;
        duty >>= (16 - pwm_bits);
        ledc_set_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL, duty);
        ledc_update_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL);
    }

    TIMERG0.int_clr_timers.t0 = 1;

    return xHigherPriorityTaskWoken;
}

static bool buzzer_init(int device, int rate)
{
    sampleRate = rate;
    RG_LOGI("Configuring speaker for %d Hz samplerate", sampleRate);

    sampleQueue = xQueueCreate(QUEUE_SIZE, sizeof(int16_t));
    if (!sampleQueue) {
        RG_LOGE("could not create sampleQueue");
        return false;
    }

    int freq = sampleRate;
    while (freq < 16000) freq *= 2;

    if (freq <= 19531)      pwm_bits = 12;
    else if (freq <= 39062) pwm_bits = 11;
    else if (freq <= 78125) pwm_bits = 10;
    else                    pwm_bits = 9;

    RG_LOGI("PWM frequency=%d Hz, resolution=%d bits", freq, pwm_bits);

    ledc_timer_config_t ledc_timer_conf = {
        .duty_resolution = pwm_bits,
        .freq_hz = freq,
        .speed_mode = LEDC_PWM_SPEED_MODE,
        .timer_num = LEDC_PWM_TIMER,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ledc_timer_config(&ledc_timer_conf);

    ledc_channel_config_t ledc_channel_conf = {
        .channel    = LEDC_PWM_CHANNEL,
        .duty       = 0,
        .gpio_num   = RG_AUDIO_USE_BUZZER_PIN,
        .speed_mode = LEDC_PWM_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_PWM_TIMER,
    };
    ledc_channel_config(&ledc_channel_conf);

    ledc_set_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL);

    timer_config_t timer_conf = {
        .divider     = 80,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en  = TIMER_PAUSE,
        .alarm_en    = TIMER_ALARM_EN,
        .auto_reload = true,
        .intr_type   = TIMER_INTR_LEVEL,
    };
    timer_init(timer_group, timer_idx, &timer_conf);
    timer_set_counter_value(timer_group, timer_idx, 0);
    timer_set_alarm_value(timer_group, timer_idx, 1000000 / sampleRate);
    timer_isr_callback_add(timer_group, timer_idx, buzzer_timer_isr, NULL, 0);
    timer_start(timer_group, timer_idx);

    return true;
}

static bool buzzer_deinit(void)
{
    timer_pause(timer_group, timer_idx);
    timer_isr_callback_remove(timer_group, timer_idx);
    timer_deinit(timer_group, timer_idx);

    if (sampleQueue) {
        vQueueDelete(sampleQueue);
        sampleQueue = NULL;
    }

    ledc_set_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL);
    return true;
}

static bool buzzer_submit(const rg_audio_frame_t *frames, size_t count)
{
    float volumeFactor = rg_audio_get_mute() ? 0.f : (rg_audio_get_volume() * 0.01f) * BOOSTVOLUME;

    // 队列空间不足就跳过这帧，防止爆音
    if (uxQueueSpacesAvailable(sampleQueue) < count)
        return true;

    for (size_t i = 0; i < count; ++i) {
        int32_t amplified = (int32_t)(frames[i].left * volumeFactor);
        if (amplified > 32767)  amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        int16_t left = (int16_t)amplified;
        xQueueSend(sampleQueue, &left, 0);
    }

    return true;
}

static bool buzzer_set_mute(bool mute)
{
    ledc_set_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_PWM_SPEED_MODE, LEDC_PWM_CHANNEL);
    return true;
}

const rg_audio_driver_t rg_audio_driver_buzzer = {
    .name = "buzzer",
    .init = buzzer_init,
    .deinit = buzzer_deinit,
    .submit = buzzer_submit,
    .set_mute = buzzer_set_mute,
    .set_volume = NULL,
    .set_sample_rate = NULL,
    .get_error = NULL,
};

#endif // RG_AUDIO_USE_BUZZER_PIN