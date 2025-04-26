#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <menu_manager.h>
#include <sdkconfig.h>
#include <stdbool.h>
#include <stdio.h>

static const char *TAG = "main";

void dumb(void *args) {
  ESP_LOGI(TAG, "I am dumb");
  while (true) {
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

menu_node_t submenu[3] = {
    {.label = "funcA", .function = &dumb},
    {.label = "funcB", .function = &dumb},
    {.label = "funcC", .function = &dumb},
};

menu_node_t root = {
    .label = "root",
    .num_options = 3,
    .submenus = (menu_node_t[3]){
        {.label = "submenu1", .submenus = submenu, .num_options = 3},
        {.label = "submenu2", .submenus = submenu, .num_options = 3},
        {.label = "submenu3", .submenus = submenu, .num_options = 3},
    }};

void simula_input(void *args) {
  Navigate_t teste = NAVIGATE_UP;
  vTaskDelay(6000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "NEXT");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  teste = NAVIGATE_SELECT;
  ESP_LOGI(TAG, "SELECT");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  teste = NAVIGATE_DOWN;
  ESP_LOGI(TAG, "DOWN");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  teste = NAVIGATE_SELECT;
  ESP_LOGI(TAG, "SELECT");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  teste = NAVIGATE_BACK;
  ESP_LOGI(TAG, "BACK");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "BACK");
  xQueueSend(qCommands, &teste, portMAX_DELAY);
  vTaskDelay(10000 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "Finalizada");
  vTaskDelete(NULL);
}

void display(menu_path_t *current_path) {

  ESP_LOGI(TAG, "title: %s, index_select: %d",
           current_path->current_menu->label, current_path->current_index);

  ESP_LOGI(
      TAG, "Option Selected %s",
      current_path->current_menu->submenus[current_path->current_index].label);
}

void app_main(void) {

  menu_config_t config = {
      .root = root,
      .loop = true,
      .display = &display,
  };

  xTaskCreatePinnedToCore(&menu_init, "menu_init", 2048, &config, 3, NULL, 0);
  vTaskDelay(5000 / portMAX_DELAY);
  xTaskCreatePinnedToCore(&simula_input, "simula", 2048, NULL, 1, NULL, 0);
  vTaskDelete(NULL);
}
