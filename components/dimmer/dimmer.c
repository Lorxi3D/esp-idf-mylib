#include <dimmer.h>

const static char *TAG = "dimmer";

uint8_t global_dimmer_count = 0; // initialize the global dimmer count

/**
 * This function will initialize a dimmer struct with the given GPIOs
 * @param *dimmer a pointer to the dimmer the struct
 * @param gen_gpio the GPIO number to generate the PWM signal
 * @param sync_gpio the GPIO number to sync the zero-crossing signal
 * @return esp_err_t ESP_OK
*/
esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio)
{
    // Check if the GPIOs are valid
    // ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(gen_gpio), ESP_ERR_INVALID_ARG, TAG, "GPIO (%d) is invalid for generate PWM", gen_gpio);
    // ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(sync_gpio), ESP_ERR_INVALID_ARG, TAG, "GPIO (%d) is invalid for syncing", gen_gpio);

    // Initialize the dimmer values
    dimmer->id = global_dimmer_count;
    dimmer->gen_gpio = gen_gpio;
    dimmer->sync_gpio = sync_gpio;
    dimmer->dutty = 0;
    #ifdef CONFIG_AUTO_FREQUENCY
        auto_frequency(dimmer);
    #else
        #ifdef CONFIG_FREQUENCY_60HZ
            dimmer->heartz = 60; // multiply by 2000 to obtain half wave and 1000 ticks
        #elif defined(CONFIG_FREQUENCY_50HZ)
            dimmer->heartz = 50; // multiply by 2000 to obtain half wave and 1000 ticks
        #endif
    #endif

    ESP_LOGI(TAG, "Create timer");
    mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .group_id = dimmer->id,
        .resolution_hz = dimmer->heartz * 2000, // multiply by 2000 to get 1000 ticks per (half) period
        .period_ticks = 1000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &dimmer->timer));
    
    ESP_LOGI(TAG, "Create operator");
    mcpwm_oper_handle_t operator;
    mcpwm_operator_config_t operator_config = {
        .group_id = dimmer->id, // operator should be in the same group of the above timers
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
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(dimmer->comparator, dimmer->dutty));
    
    ESP_LOGI(TAG, "Create generators");
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = gen_gpio,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &gen_config, &dimmer->generator));
    

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(dimmer->generator,
                                                                // when the timer value is zero, and is counting up, set output to high
                                                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(dimmer->generator,
                                                                // when compare event happens, and timer is counting up, set output to low
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, dimmer->comparator, MCPWM_GEN_ACTION_LOW)));
    
    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(dimmer->timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(dimmer->timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "Force the output level to low, timer still running");
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, 0, true));
    

    ESP_LOGI(TAG, "Setup sync strategy");
    ESP_LOGI(TAG, "Create GPIO sync source");
    mcpwm_sync_handle_t gpio_sync_source = NULL;
    mcpwm_gpio_sync_src_config_t gpio_sync_config = {
        .group_id = dimmer->id,  // GPIO fault should be in the same group of the above timers
        .gpio_num = dimmer->sync_gpio,
        .flags.pull_down = false,
        .flags.pull_up = false,
        .flags.active_neg = false,
        .flags.io_loop_back = false,
    };

    ESP_ERROR_CHECK(mcpwm_new_gpio_sync_src(&gpio_sync_config, &gpio_sync_source));

    ESP_LOGI(TAG, "Set timers to sync on the GPIO");
    mcpwm_timer_sync_phase_config_t sync_phase_config = {
        .count_value = 0,
        .direction = MCPWM_TIMER_DIRECTION_UP,
        .sync_src = gpio_sync_source,
    };
    ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(dimmer->timer, &sync_phase_config));
    ESP_LOGI(TAG, "Dimmer created successfully");
    
    // Increment the global dimmer count
    global_dimmer_count++;

    return ESP_OK;
}

esp_err_t auto_frequency(dimmer_t *dimmer) {
    dimmer->heartz = 60 * 2000; // hardcoded for now
    return ESP_OK;
}

esp_err_t set_duty( dimmer_t *dimmer, uint16_t dutty ) {

    // Validate dutty
    if( dutty > 1000) {
        dutty = 1000;
    }

    dimmer->dutty = dutty;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(dimmer->comparator, dutty));

    return ESP_OK;

}

esp_err_t set_power(dimmer_t *dimmer, float power) {
    // Validate dutty
    if( power < 0 ) {
        power = 0;
    }
    if( power > 100) {
        power = 100;
    }

    
    power /= 100; // normalize power 0 - 1

    // convert power to dutty
    // V(t) = 127 * sin( 2*pi*f t )
    // integral = - 127 cos( 2*pi*f)
    // half wave area = 127 / 60 * pi
    uint16_t dutty = sin( 2*M_PI* dimmer->heartz ) * 1000;

    set_duty(dimmer, dutty);

    return ESP_OK;
}

/**
 * This function will start emmiting pwm signal regardless of zero-crossing. But it will sync as soon as zero cross signal is applied
 * @param *dimmer a pointer to the dimmer the struct 
 * @return esp_err_t ESP_OK
*/
esp_err_t start_dimmer( dimmer_t *dimmer ) {
    // ESP_ERROR_CHECK(mcpwm_timer_start_stop(dimmer->timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, -1, true));
    return ESP_OK;

}

/**
 * This function will stop pwm signal from being generated but only when next count reaches zero
 * @param *dimmer a pointer to the dimmer the struct 
 * @return esp_err_t ESP_OK
*/
esp_err_t stop_dimmer( dimmer_t *dimmer ) {
    // ESP_ERROR_CHECK(mcpwm_timer_start_stop(dimmer->timer, MCPWM_TIMER_STOP_EMPTY));
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(dimmer->generator, 0, true));

    return ESP_OK;
}
