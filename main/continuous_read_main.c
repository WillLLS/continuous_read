/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Includes */

#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"

#include <stdlib.h>
#include <inttypes.h>
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "driver/gptimer.h"


/**
 * @TODO:
 * - Add a led to show the state of the ADC
 * - Test different EXAMPLE_READ_LEN
 * - Use a On/Off button to start/stop the ADC instead of the pushbutton
 * - Condition d'unicité pour : 
 *                      - printf("timer_value is %lld \n", timer_value); (ligne 184)
 * - Essayer de sauver les données dans un buffer
 * - Essayer de différente taille de buffer
 */


/* TIMER */

uint64_t timer_value;
gptimer_handle_t gptimer_handle = NULL;

gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 20000
};

typedef struct arg_t
{
    adc_continuous_handle_t adc_handle;
    gptimer_handle_t gptimer_handle;
    uint64_t *timer_value;
    uint8_t flag;
} arg_t;

/* GPIO */

#define GPIO_BTN (4)

int btn_state;
uint8_t flag;

const gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_ANYEDGE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = (1ULL<<GPIO_BTN),
    .pull_down_en = 1,
    .pull_up_en = 0
};

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    arg_t *arguments = (arg_t *) arg;

    btn_state = 1 - btn_state;
    if(btn_state == 1){
        //gptimer_enable(arguments->gptimer_handle);
        gptimer_start(arguments->gptimer_handle);
        ESP_ERROR_CHECK(adc_continuous_start(arguments->adc_handle));
    }
    else{
        ESP_ERROR_CHECK(adc_continuous_stop(arguments->adc_handle));
        //ESP_ERROR_CHECK(adc_continuous_deinit(arguments->adc_handle);
        gptimer_get_raw_count(arguments->gptimer_handle, arguments->timer_value);
        gptimer_stop(arguments->gptimer_handle);
    }

    flag = 1;
}

/* ADC DMA */

#define MAX_BUF_SIZE        1024
#define EXAMPLE_READ_LEN    256

#ifdef ONE_ADC
static adc_channel_t channel[1] = {ADC_CHANNEL_7};
#else
static adc_channel_t channel[6] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7}; 
#endif

static TaskHandle_t s_task_handle;
static const char *TAG = "EXAMPLE";

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = MAX_BUF_SIZE,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 1000000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        uint8_t unit = ADC_UNIT_1;
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_0;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

static bool check_valid_data(const adc_digi_output_data_t *data)
{
    if (data->type1.channel >= SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
        return false;
    }

    return true;
}

void app_main(void)
{
    btn_state = 0;
    timer_value = 0;
    flag = 0;

    gptimer_new_timer(&timer_config, &gptimer_handle);
    gptimer_enable(gptimer_handle);
    
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    arg_t argument = {
        .adc_handle = handle, 
        .gptimer_handle = gptimer_handle, 
        .timer_value = &timer_value,
        .flag = flag
    };

    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_BTN, gpio_isr_handler, (void*) &argument));

    while(1) {
        if(flag){
            printf("timer_value is %lld \n", timer_value);
            flag = 0;
        }
        
        
        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        while (btn_state) {
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) {
                //ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32, ret, ret_num);
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (void*)&result[i];
                    if (check_valid_data(p)) {
                        printf("%d %x\n", p->type1.channel, p->type1.data);
                    }
                }
            
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
            vTaskDelay(1);
        }
    
        vTaskDelay(1);

    }
}
