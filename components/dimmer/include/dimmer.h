#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include <math.h>

extern int8_t global_dimmer_groups[SOC_MCPWM_GROUPS];
extern uint32_t global_dimmer_generators;

typedef struct dimmer
{
    mcpwm_timer_handle_t timer;     // internal management
    mcpwm_cmpr_handle_t  comparator;// internal management
    mcpwm_gen_handle_t   generator; // internal management
    uint8_t              gen_gpio;  // generator gpio
    uint8_t              sync_gpio; // zero-crossing gpio
    float                heartz;    // zero-crossing frequency
    uint16_t             dutty;     // duty cycle 0-1000
} dimmer_t;

esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio);
esp_err_t delete_dimmer( dimmer_t *dimmer);

esp_err_t start_dimmer(dimmer_t *dimmer);
esp_err_t stop_dimmer(dimmer_t *dimmer);

esp_err_t set_dutty(dimmer_t *dimmer, uint16_t dutty);
esp_err_t set_power(dimmer_t *dimmer, double power);
float get_power(dimmer_t *dimmer);

// internal use functions
// float auto_frequency(uint8_t sync_gpio); // automatic frequency detection
uint8_t set_group_id( uint8_t sync_gpio);
esp_err_t validate_generator( uint8_t gen_gpio);

/** -------------------------( Task Related )------------------------- */

typedef struct task_dimmer
{
    uint8_t        gen_gpio;
    uint8_t        sync_gpio;
    uint16_t       dutty;
    TaskHandle_t   task;
} task_dimmer_t;

task_dimmer_t create_task_dimmer( uint8_t gen_gpio, uint8_t sync_gpio );
esp_err_t delete_task_dimmer( task_dimmer_t* dimmer );
esp_err_t set_task_dimmer_dutty( task_dimmer_t* dimmer, uint16_t dutty );
esp_err_t set_task_dimmer_power( task_dimmer_t* dimmer, double power );
float get_task_dimmer_power(task_dimmer_t* dimmer);

