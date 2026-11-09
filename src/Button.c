#include "button.h"
#include "config.h"

#include <stddef.h>

#include "esp_attr.h"
#include "esp_timer.h"

#define DEBOUNCE_MS    50u
#define POLL_MS        5u
#define LONG_PRESS_MS  1000u

static volatile uint32_t interruptCount = 0;
static volatile uint32_t lastInterruptMs = 0;

static uint32_t handledCount = 0;

uint32_t millis(void)
{
	return (uint32_t)(esp_timer_get_time() / 1000);
}

static void IRAM_ATTR onButtonInterrupt(void *arg)
{
	interruptCount++;
	lastInterruptMs = millis();
}

void buttonStart(void)
{
	gpio_int_type_t edge;

    if (MODE == MODE_POLLING) {
		edge = GPIO_INTR_DISABLE;   // Task 4: no interrupts
	} else if (MODE <= MODE_TIME_DEBOUNCE) {
		edge = GPIO_INTR_NEGEDGE;   // Tasks 1-2: falling edge only
	} else {
		edge = GPIO_INTR_ANYEDGE;   // state machine: press and release
	}

	gpio_config_t button = {
		.pin_bit_mask = 1ULL << BUTTON_PIN,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.intr_type = edge,
	};
	ESP_ERROR_CHECK(gpio_config(&button));

	if (edge != GPIO_INTR_DISABLE) {
		ESP_ERROR_CHECK(gpio_install_isr_service(0));
		ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, onButtonInterrupt, NULL));
	}
}

uint32_t buttonInterruptCount(void)
{
	return interruptCount;
}

static bool isButtonDown(void)
{
	return gpio_get_level(BUTTON_PIN) == 0;
}

static bool hasNewInterrupt(void)
{
	return handledCount != interruptCount;
}

// ---------- Task 1: every interrupt is a press ----------

static bool noDebounce(void)
{
	if (!hasNewInterrupt()) {
		return false;
	}

	handledCount++;
	return true;
}

// ---------- Task 2: ignore interrupts closer than 50 ms to the last accepted one ----------

static bool timeDebounce(void)
{
	static uint32_t lastAcceptedMs = 0;

	if (!hasNewInterrupt()) {
		return false;
	}
	handledCount = interruptCount;

	if (lastInterruptMs - lastAcceptedMs < DEBOUNCE_MS) {
		return false;
	}

	lastAcceptedMs = lastInterruptMs;
	return true;
}

// ---------- State machine: tasks 3-4 and extra homework ----------

typedef enum {
	STATE_IDLE,      // stable, not pressed
	STATE_PRESSED,   // went down, waiting 50 ms to be sure
	STATE_HELD,      // stable, pressed
	STATE_RELEASE    // went up, waiting 50 ms to be sure
} ButtonState;

typedef enum {
	EVENT_NONE,
	EVENT_PRESS,       // press confirmed
	EVENT_LONG_PRESS,  // held for LONG_PRESS_MS
	EVENT_RELEASE      // release confirmed
} ButtonEvent;

static const char *STATE_NAMES[] = { "IDLE", "PRESSED", "HELD", "RELEASE" };

static ButtonState state = STATE_IDLE;
static uint32_t changedMs = 0;     // when the state machine saw the last change
static uint32_t pressedMs = 0;     // when the current press started
static uint32_t heldMs = 0;        // how long the last press lasted
static bool longPressSent = false;

const char *buttonStateName(void)
{
	return STATE_NAMES[state];
}

static bool timeToRead(uint32_t now)
{
	static uint32_t lastPollMs = 0;

	if (MODE == MODE_POLLING) {
		if (now - lastPollMs < POLL_MS) {
			return false;
		}
		lastPollMs = now;
		return true;
	}

	const bool stable = (state == STATE_IDLE || state == STATE_HELD);
	if (stable && !hasNewInterrupt()) {
		return false;
	}
	handledCount = interruptCount;
	return true;
}

static ButtonEvent stepStateMachine(uint32_t now)
{
	if (state == STATE_HELD && !longPressSent && now - pressedMs >= LONG_PRESS_MS) {
		longPressSent = true;
		return EVENT_LONG_PRESS;
	}

	if (!timeToRead(now)) {
		return EVENT_NONE;
	}

	const bool down = isButtonDown();
	const bool stableLongEnough = now - changedMs >= DEBOUNCE_MS;

	switch (state) {
		case STATE_IDLE:
			if (down) {
				state = STATE_PRESSED;
				changedMs = now;
			}
			break;

		case STATE_PRESSED:
			if (!down) {
				state = STATE_IDLE;
			} else if (stableLongEnough) {
				state = STATE_HELD;
				pressedMs = changedMs;
				longPressSent = false;
				return EVENT_PRESS;
			}
			break;

		case STATE_HELD:
			if (!down) {
				state = STATE_RELEASE;
				changedMs = now;
			}
			break;

		case STATE_RELEASE:
			if (down) {
				state = STATE_HELD;
			} else if (stableLongEnough) {
				state = STATE_IDLE;
				heldMs = changedMs - pressedMs;
				return EVENT_RELEASE;
			}
			break;
	}
	return EVENT_NONE;
}

bool buttonPressed(uint32_t now)
{
	if (MODE == MODE_NO_DEBOUNCE) {
		return noDebounce();
	}
	if (MODE == MODE_TIME_DEBOUNCE) {
		return timeDebounce();
	}

	const ButtonEvent event = stepStateMachine(now);

	switch (MODE) {
		case MODE_STATE_DEBOUNCE:
		case MODE_POLLING:     return event == EVENT_PRESS;
		case MODE_SHORT_PRESS: return event == EVENT_RELEASE && heldMs < LONG_PRESS_MS;
		case MODE_LONG_PRESS:  return event == EVENT_LONG_PRESS;
		case MODE_ON_RELEASE:  return event == EVENT_RELEASE;
	}
	return false;
}
