// Original code by
/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

// Modified code after here

#include <dimmer.h>

const static char *TAG = "dimmer";

int8_t global_dimmer_groups[SOC_MCPWM_GROUPS] = { -1, -1 };
uint32_t global_dimmer_generators = 0UL;

// internal use functions

// float auto_frequency(uint8_t sync_gpio) {
//     return 60.0f; // hardcoded for now
// }

uint8_t set_group_id( uint8_t sync_gpio) {
    ESP_LOGI(TAG, "Selecting group ID automatically");

    int8_t group_id = -1;
    for( uint8_t i = 0; i < SOC_MCPWM_GROUPS; i++ ) {
        if( global_dimmer_groups[i] == sync_gpio ) {
            group_id = i;
            break;
        }
    }

    // If no group was found, select the first available
    if( group_id == -1 ) {
        for( uint8_t i = 0; i < SOC_MCPWM_GROUPS; i++ ) {
            if( global_dimmer_groups[i] == -1 ) {
                global_dimmer_groups[i] = sync_gpio;
                group_id = i;
                break;
            }
        }
    }

    // If no group was found, return an error
    if( group_id == -1 ) {
        ESP_LOGE(TAG, "No group available for dimmer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Group ID: %d", group_id);
    return group_id;
}

esp_err_t validate_generator( uint8_t gen_gpio) {

ESP_LOGI(TAG, "Validating generator GPIO");

    uint32_t gen_mask = 1UL << gen_gpio; // mask for the generator GPIO
    if( global_dimmer_generators & gen_mask ) {
        ESP_LOGE(TAG, "Generator GPIO already in use");
        return ESP_FAIL;
    }

    global_dimmer_generators |= gen_mask; // set the generator GPIO as used
    
    ESP_LOGI(TAG, "generator GPIO is valid");
    return ESP_OK;
}

/** -------------------------( Manual Dimmer Related )------------------------- */

/**
 * This function will initialize a dimmer struct with the given GPIOs
 * @param *dimmer a pointer to the dimmer the struct
 * @param gen_gpio the GPIO number to generate the PWM signal
 * @param sync_gpio the GPIO number to sync the zero-crossing signal
 * @return esp_err_t ESP_OK
*/
esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio )
{
    ESP_LOGI(TAG, "Initialize dimmer values");
    dimmer->gen_gpio = gen_gpio;
    dimmer->sync_gpio = sync_gpio;
    dimmer->dutty = 0;
   
    #ifdef CONFIG_FREQUENCY_60HZ
        dimmer->heartz = 60;
    #elif defined(CONFIG_FREQUENCY_50HZ)
        dimmer->heartz = 50;
    #else
        #error "Please execute menuconfig and select a frequency"
    #endif

    ESP_ERROR_CHECK(validate_generator(gen_gpio));
    uint8_t group_id = set_group_id(sync_gpio);
    
    ESP_LOGI(TAG, "Create timer");
    mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .group_id = group_id,
        .resolution_hz = dimmer->heartz * 2000, // multiply by 2000 to get 1000 ticks per (half) period
        .period_ticks = 1000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &dimmer->timer));

    ESP_LOGI(TAG, "Create operator");
    mcpwm_oper_handle_t operator;
    mcpwm_operator_config_t operator_config = {
        .group_id = group_id, // operator should be in the same group of the above timers
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator));

    ESP_LOGI(TAG, "Connect timers and operators with each other");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator, dimmer->timer));

    ESP_LOGI(TAG, "Create comparators");
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &compare_config, &dimmer->comparator));
    // init compare for each comparator
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(dimmer->comparator, 1000)); // start with zero power

    ESP_LOGI(TAG, "Create generators");
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = gen_gpio,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &gen_config, &dimmer->generator));

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(dimmer->generator,
                                                                // when the timer value is zero, and is counting up, set output to low
                                                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(dimmer->generator,
                                                                // when compare event happens, and timer is counting up, set output to high
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, dimmer->comparator, MCPWM_GEN_ACTION_HIGH)));

    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(dimmer->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(dimmer->timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, 0, true));

    ESP_LOGI(TAG, "Setup sync strategy");
    mcpwm_sync_handle_t gpio_sync_source = NULL;
    mcpwm_gpio_sync_src_config_t gpio_sync_config = {
        .group_id = group_id,  // GPIO fault should be in the same group of the above timers
        .gpio_num = dimmer->sync_gpio,
        .flags.pull_down = false,
        .flags.pull_up = false,
        .flags.active_neg = false,
        .flags.io_loop_back = false,
    };

    ESP_ERROR_CHECK(mcpwm_new_gpio_sync_src(&gpio_sync_config, &gpio_sync_source));

    ESP_LOGI(TAG, "Set timer to sync on the GPIO");
    mcpwm_timer_sync_phase_config_t sync_phase_config = {
        .count_value = 0,
        .direction = MCPWM_TIMER_DIRECTION_UP,
        .sync_src = gpio_sync_source,
    };
    ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(dimmer->timer, &sync_phase_config));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, -1, true)); // start_dimmer is optional
    ESP_LOGI(TAG, "Dimmer created successfully");

    return ESP_OK;
}

/**
 * This function will free all pointers in the dimmer struct
 * @param *dimmer a pointer to the dimmer the struct
 * @return esp_err_t ESP_OK
*/
esp_err_t delete_dimmer( dimmer_t *dimmer) {
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, 0, true));
    ESP_ERROR_CHECK(mcpwm_timer_disable(dimmer->timer));
    dimmer->timer = NULL;
    dimmer->comparator = NULL;
    dimmer->generator = NULL;
    return ESP_OK;
}

/**
 * This function will set the dutty cycle of the dimmer
 * @param *dimmer a pointer to the dimmer the struct
 * @param dutty the dutty cycle to set the dimmer to. Must be between 0 and 1000
 * @return esp_err_t ESP_OK
*/
esp_err_t set_dutty( dimmer_t *dimmer, uint16_t dutty ) {

    // Validate dutty
    if( dutty > 1000) {
        dutty = 1000;
    }

    dimmer->dutty = dutty;

    dutty = 1000 - dutty; // Invert signal

    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(dimmer->comparator, dutty));

    return ESP_OK;

}

/**
 * This function will set the power of the dimmer
 * @param *dimmer a pointer to the dimmer the struct 
 * @param power the power to set the dimmer to. Must be between 0 and 1
 * @return esp_err_t ESP_OK
*/
esp_err_t set_power(dimmer_t *dimmer, double power) {

    uint16_t dutty;
    if( power <= 0 ) {
        dutty = 0;
    }
    else if( power >= 1) {
        dutty = 1000; // avoid floating point errors
    }
    else {
        /** 
         * Calculate the dutty cycle
         * t = acos(1 - 2 * power) / (2 * pi * freq)
         * dutty = 1000 * t * 2 * freq
         **/
        dutty = (uint16_t) round(1000 * acos(1 - 2*power) / (M_PI)); // result is in ticks 0 - 1000
    }

    // Convert power to dutty
    set_dutty(dimmer, dutty);

    return ESP_OK;
}

/**
 * This function will start emmiting pwm signal regardless of zero-crossing.
 * But it will sync as soon as zero cross signal is applied
 * @param *dimmer a pointer to the dimmer the struct 
 * @return esp_err_t ESP_OK
*/
esp_err_t start_dimmer( dimmer_t *dimmer ) {
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, -1, true));
    return ESP_OK;
}

/**
 * This function will stop pwm signal from being generated but only when next count reaches zero
 * @param *dimmer a pointer to the dimmer the struct 
 * @return esp_err_t ESP_OK
*/
esp_err_t stop_dimmer( dimmer_t *dimmer ) {
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, 0, true));
    return ESP_OK;
}

/**
 * This function will return the power of the dimmer in percentage
 * @param *dimmer a pointer to the dimmer the struct 
 * @return double the power of the dimmer in percentage 0 - 1
*/
float get_power(dimmer_t *dimmer) {
    return ( 0.5 * (1 - cos(M_PI * dimmer->dutty / 1000.0)));
}

/** -------------------------( Task Dimmer Related )------------------------- */

/**
 * This function will create a task to control the dimmer
 * @param *arg a pointer to the task_dimmer_t struct wich contains necessary information to create the task
 * @return void
*/
void task_dimmer(void *arg) {
    
    // Retrieve the arguments
    task_dimmer_t *args = (task_dimmer_t *)arg;

    ESP_ERROR_CHECK(validate_generator(args->gen_gpio));
    uint8_t group_id = set_group_id(args->sync_gpio);
    
    #ifdef CONFIG_FREQUENCY_60HZ
        float heartz = 60;
    #elif defined(CONFIG_FREQUENCY_50HZ)
        float heartz = 50;
    #else
        #error "Please execute menuconfig and select a frequency"
    #endif

    ESP_LOGI(TAG, "Create timer");
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .group_id = group_id,
        .resolution_hz = heartz * 2000, // multiply by 2000 to get 1000 ticks per (half) period
        .period_ticks = 1000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    ESP_LOGI(TAG, "Create operator");
    mcpwm_oper_handle_t operator;
    mcpwm_operator_config_t operator_config = {
        .group_id = group_id, // operator should be in the same group of the above timers
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator));

    ESP_LOGI(TAG, "Connect timers and operators with each other");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator, timer));

    ESP_LOGI(TAG, "Create comparators");
    mcpwm_cmpr_handle_t comparator;
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &compare_config, &comparator));
    // init compare for each comparator
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 0));

    ESP_LOGI(TAG, "Create generators");
    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = args->gen_gpio,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &gen_config, &generator));

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                                // when the timer value is zero, and is counting up, set output to low
                                                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                // when compare event happens, and timer is counting up, set output to high
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_HIGH)));


    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(generator, 0, true));

    ESP_LOGI(TAG, "Setup sync strategy");
    mcpwm_sync_handle_t gpio_sync_source = NULL;
    mcpwm_gpio_sync_src_config_t gpio_sync_config = {
        .group_id = group_id,  // GPIO fault should be in the same group of the above timers
        .gpio_num = args->sync_gpio,
        .flags.pull_down = false,
        .flags.pull_up = false,
        .flags.active_neg = false,
        .flags.io_loop_back = false,
    };

    ESP_ERROR_CHECK(mcpwm_new_gpio_sync_src(&gpio_sync_config, &gpio_sync_source));

    ESP_LOGI(TAG, "Set timer to sync on the GPIO");
    mcpwm_timer_sync_phase_config_t sync_phase_config = {
        .count_value = 0,
        .direction = MCPWM_TIMER_DIRECTION_UP,
        .sync_src = gpio_sync_source,
    };
    ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(timer, &sync_phase_config));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(generator, -1, true));
    ESP_LOGI(TAG, "Dimmer created successfully");

    uint32_t dutty = 0;
    while(1) {
        if ( xTaskNotifyWait(0, ULONG_MAX, &dutty, portMAX_DELAY) == pdTRUE ) {
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, dutty));
        }
    }
}

/**
 * This function will create the dimmer struct and the task to control the dimmer
 * @param gen_gpio the GPIO number to generate the PWM signal
 * @param sync_gpio the GPIO number to sync the zero-crossing signal
 * @return task_dimmer_t the dimmer task struct
*/
task_dimmer_t create_task_dimmer( uint8_t gen_gpio, uint8_t sync_gpio) {
    task_dimmer_t dimmer = {
        .task = NULL,
        .gen_gpio = gen_gpio,
        .sync_gpio = sync_gpio,
        .dutty = 0,
        // .queue = xQueueCreate(3, sizeof(uint16_t)), // hardcoded to 3 elements
    };
    // if ( dimmer.queue == NULL) {
    //     ESP_LOGE(TAG, "Failed to create queue");
    // }
    xTaskCreate(task_dimmer, "task_dimmer", 4096, (void*)&dimmer, 5, &dimmer.task);
    return dimmer;
}

/**
 * This function will delete the dimmer task and the queue,
 * make sure to stop the dimmer before deleting it to avoid memory leaks
 * @param *dimmer a pointer to the task_dimmer_t struct
 * @return esp_err_t ESP_OK
*/
esp_err_t delete_task_dimmer ( task_dimmer_t* dimmer ) {
    vTaskDelete(dimmer->task);
    dimmer->task = NULL;
    return ESP_OK;
}

/**
 * This function will set the dutty cycle of the dimmer.
 * You can set the power to 0 to stop the dimmer
 * @param *dimmer a pointer to the task_dimmer_t struct
 * @param dutty the dutty cycle to set the dimmer to. Must be between 0 and 1000
 * @return esp_err_t ESP_OK
*/
esp_err_t set_task_dimmer_dutty( task_dimmer_t* dimmer,uint16_t dutty ) {
    
    if( dutty >= 1000) {
        dutty = 1000;
    }

    dimmer->dutty = dutty; // update dutty struct

    dutty = 1000 - dutty; // Invert signal

    // Notify to queue
    if( xTaskNotify(dimmer->task, dutty, eSetValueWithOverwrite) != pdTRUE ) {
        ESP_LOGW(TAG, "Failed to send notification.");
    }
    return ESP_OK;
}

/**
 * This function will set the power of the dimmer.
 * You can set the power to 0 to stop the dimmer
 * @param *dimmer a pointer to the task_dimmer_t struct
 * @param power the power to set the dimmer to. Must be between 0 and 1
 * @return esp_err_t ESP_OK
*/
esp_err_t set_task_dimmer_power( task_dimmer_t* dimmer, double power ) {
    uint16_t dutty;
    if( power <= 0 ) {
        dutty = 0;
    }
    else if( power >= 1) {
        dutty = 1000; // avoid floating point errors
    }
    else {
        /** 
         * Calculate dutty cycle
         * t = acos(1 - 2 * power) / (2 * pi * freq)
         * dutty = 1000 * t * 2 * freq
         **/
        dutty = (uint16_t) round(1000 * acos(1 - 2*power) / (M_PI)); // result is in ticks (0 - 1000)
    }

    // update dutty
    dimmer->dutty = dutty;

    dutty = 1000 - dutty; // Invert signal

    // Notify task
    if( xTaskNotify(dimmer->task, dutty, eSetValueWithOverwrite) != pdTRUE ) {
        ESP_LOGW(TAG, "Failed to send notification");
    }
    return ESP_OK;
}

/**
 * This function will return the power of the dimmer in percentage
 * @param *dimmer a pointer to the dimmer_task the struct 
 * @return double the power of the dimmer in percentage 0 - 1
*/
float get_task_dimmer_power(task_dimmer_t* dimmer) {
    return (float) ( 0.5 * (1 - cos(M_PI * dimmer->dutty / 1000.0)));
}
