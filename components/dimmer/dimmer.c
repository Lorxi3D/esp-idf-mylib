#include <dimmer.h>

const static char *TAG = "dimmer";

uint8_t global_dimmer_count = 0; // initialize the global dimmer count


esp_err_t create_dimmer( dimmer_t *dimmer, uint8_t gen_gpio, uint8_t sync_gpio)
{
    // Check if the GPIOs are valid
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(gen_gpio), ESP_ERR_INVALID_ARG, TAG, "GPIO (%d) is invalid for generate PWM", gen_gpio);
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(sync_gpio), ESP_ERR_INVALID_ARG, TAG, "GPIO (%d) is invalid for syncing", gen_gpio);

    // Initialize the dimmer values
    dimmer->id = global_dimmer_count;
    dimmer->dimmer_task = NULL;
    dimmer->gen_gpio = gen_gpio;
    dimmer->sync_gpio = sync_gpio;
    dimmer->duty = 0;
    #ifdef CONFIG_AUTO_FREQUENCY
        auto_frequency(dimmer);
    #else
        #ifdef CONFIG_FREQUENCY_60HZ
            dimmer->heartz = 60;
        #elif defined(CONFIG_FREQUENCY_50HZ)
            dimmer->heartz = 50;
        #endif
    #endif
    

    ESP_LOGI(TAG, "Create timer");
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .group_id = dimmer->id,
        .resolution_hz = dimmer->heartz * 1000, // multiply by 1000 to get 1000 ticks per period
        .period_ticks = 1000,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    
    ESP_LOGI(TAG, "Create operator");
    mcpwm_oper_handle_t operator;
    mcpwm_operator_config_t operator_config = {
        .group_id = dimmer->id, // operator should be in the same group of the above timers
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
    // ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 0));
    

    ESP_LOGI(TAG, "Create generators");
    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = gen_gpio,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &gen_config, &generator));
    

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                                // when the timer value is zero, and is counting up, set output to high
                                                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                // when compare event happens, and timer is counting up, set output to low
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));
    
    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    vTaskDelay(pdMS_TO_TICKS(10));

    // Manually added this "IDLE" phase, which can help us distinguish the wave forms before and after sync
    ESP_LOGI(TAG, "Force the output level to low, timer still running");
    ESP_ERROR_CHECK(mcpwm_generator_set_force_level(generator, 0, true));
    

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
    ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(timer, &sync_phase_config));
    
    

    // Create Task responsible for dimmer
    // xTaskCreate(dimmer_task, "dimmer_${global_dimmer_count_task}", 2048, dimmer, 10, &dimmer->dimmer_task);

    // Increment the global dimmer count
    global_dimmer_count++;

    return ESP_OK;
}

esp_err_t auto_frequency(dimmer_t *dimmer) {
    dimmer->heartz = 60; // hardcoded for now
    return ESP_OK;
}