 #include <stdio.h>
#include <dimmer.h>

#define DIMMER_0_GEN_GPIO  2 
#define DIMMER_0_SYNC_GPIO 5

#define DIMMER_1_GEN_GPIO  4
#define DIMMER_1_SYNC_GPIO 5

static const char *TAG = "task_dimmer_example";

void app_main(void) {
  printf("Task Dimmer Example\n");
  task_dimmer_t dimmer_0 = create_task_dimmer( DIMMER_0_GEN_GPIO, DIMMER_0_SYNC_GPIO );

  task_dimmer_t dimmer_1 = create_task_dimmer( DIMMER_1_GEN_GPIO, DIMMER_1_SYNC_GPIO );
  ESP_ERROR_CHECK(set_task_dimmer_dutty( &dimmer_1, 500));

  while(1) {
    for( double i=0; i < 1; i+=.05) {
      ESP_ERROR_CHECK(set_task_dimmer_power( &dimmer_0, i));
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    for( double i=1; i > 0; i-=.05) {
      ESP_ERROR_CHECK(set_task_dimmer_power( &dimmer_0, i));
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}
