#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include <math.h>

extern uint8_t global_dimmer_count;

typedef struct dimmer
{
    uint8_t              id;        // internal management
    mcpwm_timer_handle_t timer;     // internal management
    mcpwm_cmpr_handle_t  comparator;// internal management
    mcpwm_gen_handle_t   generator; // internal management
    uint8_t              gen_gpio;  // generator gpio
    uint8_t              sync_gpio; // zero-crossing gpio
    float                heartz;    // zero-crossing frequency
    uint16_t             dutty;     // duty cycle 0-1000
} dimmer_t;

esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio);
esp_err_t auto_frequency(dimmer_t *dimmer); // automatic frequency detection
esp_err_t set_frequency(dimmer_t *dimmer, float freq); // manual frequency detection
esp_err_t set_duty(dimmer_t *dimmer, uint16_t dutty);
esp_err_t set_power(dimmer_t *dimmer, float power);
uint8_t get_power(dimmer_t *dimmer);
esp_err_t start_dimmer(dimmer_t *dimmer);
esp_err_t stop_dimmer(dimmer_t *dimmer);