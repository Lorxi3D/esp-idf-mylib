#include <dimmer.h>

#define DIMMER_0_GEN_GPIO  2
#define DIMMER_0_SYNC_GPIO 5

#define DIMMER_1_GEN_GPIO  4
#define DIMMER_1_SYNC_GPIO 5

static const char *TAG = "manual_dimmer_example";

void app_main(void) {
  
  printf("Manual Dimmer Example\n");

  dimmer_t dimmer_0;
  create_dimmer( &dimmer_0, DIMMER_0_GEN_GPIO, DIMMER_0_SYNC_GPIO );

  dimmer_t dimmer_1;
  create_dimmer( &dimmer_1, DIMMER_1_GEN_GPIO, DIMMER_1_SYNC_GPIO );
  set_dutty( &dimmer_1, 250 );

  while (1)
  {
    for( double i=0; i < 1; i+=.05) {
      ESP_ERROR_CHECK(set_power( &dimmer_0, i));
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    for( double i=1; i > 0; i-=.05) {
      ESP_ERROR_CHECK(set_power( &dimmer_0, i));
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

}
