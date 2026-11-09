#include "button.h"
#include "config.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "button";

void app_main(void)
{
	gpio_config_t leds = {
		.pin_bit_mask = (1ULL << LED_1_PIN) | (1ULL << LED_2_PIN),
		.mode = GPIO_MODE_OUTPUT,
	};
	ESP_ERROR_CHECK(gpio_config(&leds));

	buttonStart();
	ESP_LOGI(TAG, "Mode %d", MODE);

	uint32_t pressCount = 0;
	bool led1 = false;
	bool led2 = false;
	uint32_t heartbeatMs = millis();

	while (1) {
		const uint32_t now = millis();

		if (buttonPressed(now)) {
			pressCount++;
			led1 = !led1;
			gpio_set_level(LED_1_PIN, led1);
			ESP_LOGI(TAG, "Press %lu, interrupts %lu, state %s",
			         (unsigned long)pressCount, (unsigned long)buttonInterruptCount(), buttonStateName());
		}

		if (now - heartbeatMs >= HEARTBEAT_MS) {
			led2 = !led2;
			gpio_set_level(LED_2_PIN, led2);
			heartbeatMs = now;
		}

		vTaskDelay(1);
	}
}
