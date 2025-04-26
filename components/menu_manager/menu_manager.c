// TODO: Add comments
#include "menu_manager.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
const static char *TAG = "menu_menager";

TaskHandle_t tMenuFunction = NULL;
QueueHandle_t qCommands = NULL;

menu_path_t steckPath[CONFIG_MAX_DEPTH_PATH];
menu_path_t path;
uint8_t depth = 0;
Navigate_t inputCommand = NAVIGATE_NOTHING;
void (**show)(menu_path_t *current_menu);

static void NavigationUp(bool loop) {
  ESP_LOGI(TAG, "Command UP");

  if (path.current_index < path.current_menu->num_options - 1 || loop) {
    path.current_index++;
    path.current_index %= path.current_menu->num_options;
  }
}

static void NavigationDown(bool loop) {
  ESP_LOGI(TAG, "Command DOWN");

  if (path.current_index > 0 || loop) {
    path.current_index =
        (path.current_menu->num_options + path.current_index - 1) %
        path.current_menu->num_options;
  }
}

static void ExecFunction() {
  ESP_LOGI(TAG, "Execute Function: %s",
           path.current_menu->submenus[path.current_index].label);

  xTaskCreatePinnedToCore(
      path.current_menu->submenus[path.current_index].function,
      "Function_by_menu", 10240, NULL, 10, &tMenuFunction, 1);
}

static void SelectionOption() {
  ESP_LOGI(TAG, "Open Submenu");

  path.current_menu = &path.current_menu->submenus[path.current_index];
  path.current_index = 0;
  depth++;
  steckPath[depth] = path;
}

static void NavigationBack() {
  ESP_LOGI(TAG, "Command BACK");

  depth--;
  path = steckPath[depth];
#if !CONFIG_SALVE_INDEX
  path.current_index = 0;
#endif
}

void menu_init(void *args) {
  ESP_LOGI(TAG, "Start menu");
  menu_config_t *params = (menu_config_t *)args;

  qCommands = xQueueCreate(10, sizeof(Navigate_t));

  path.current_index = 0;
  path.current_menu = &params->root;
  steckPath[depth] = path;

  show = &params->display;
  (*show)(&path);

  ESP_LOGI(TAG, "Root Title: %s", path.current_menu->label);

  while (true) {

    xQueueReceive(qCommands, &inputCommand, portMAX_DELAY);

    if (tMenuFunction == NULL) {

      switch (inputCommand) {

      case NAVIGATE_UP:
        NavigationUp(params->loop);
        break;

      case NAVIGATE_DOWN:
        NavigationDown(params->loop);
        break;

      case NAVIGATE_SELECT:
        if (path.current_menu->submenus[path.current_index].function) {
          ExecFunction();
        } else {
          SelectionOption();
        }
        break;

      case NAVIGATE_BACK:
        if (depth) {
          NavigationBack();
        }
        break;

      default:
        ESP_LOGW(TAG, "Undenfined Input");
        break;
      }

    } else if (inputCommand == NAVIGATE_BACK) {
      vTaskDelete(tMenuFunction);
      tMenuFunction = NULL;
      ESP_LOGI(TAG, "Exit Function");
    }
    if (tMenuFunction == NULL)
      (*show)(&path);
  }
}

void exitFunction(void) {
  Navigate_t tempCommand = NAVIGATE_BACK;
  xQueueSend(qCommands, &tempCommand, portMAX_DELAY);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void execFunction(void (*function)(void *args)) {
  ESP_LOGI(TAG, "Execute Function");

  xTaskCreatePinnedToCore(function, "Function_by_menu", 10240, NULL, 10,
                          &tMenuFunction, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void setQuick_menuFunction(void) { (*show)(&path); }

#ifdef __cplusplus
}
#endif
