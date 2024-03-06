#include <stdio.h>
#include <dimmer.h>

static const char *TAG = "dimmer_example";


void app_main(void) {
  printf("Dimmer Example\n");
  dimmer_t dimmer;
  ESP_ERROR_CHECK(create_dimmer(&dimmer, 2, 5));
  ESP_ERROR_CHECK(start_dimmer(&dimmer));
  while(1) {
    for( uint16_t i=0; i < 1000; i+=100) {
      ESP_ERROR_CHECK(set_duty( &dimmer, i));
      printf("PWM dutty: %d\n", dimmer.dutty);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for( uint16_t i=1000; i > 0; i-=100) {
      ESP_ERROR_CHECK(set_duty( &dimmer, i));
      printf("PWM dutty: %d\n", dimmer.dutty);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
      vTaskDelay(pdMS_TO_TICKS(10));

  }
}
