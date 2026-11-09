#pragma once

#include <stdbool.h>
#include <stdint.h>

// Tasks 1-4
#define MODE_NO_DEBOUNCE     1  // every interrupt counts
#define MODE_TIME_DEBOUNCE   2  // ignore interrupts closer than 50 ms
#define MODE_STATE_DEBOUNCE  3  // state machine woken by interrupts
#define MODE_POLLING         4  // state machine, pin read every 5 ms

// Extra homework
#define MODE_SHORT_PRESS     5  // released before 1 s
#define MODE_LONG_PRESS      6  // held for 1 s
#define MODE_ON_RELEASE      7  // action after release

// Current mode
#define MODE  MODE_ON_RELEASE

void buttonStart(void);

bool buttonPressed(uint32_t now);

const char *buttonStateName(void);

uint32_t buttonInterruptCount(void);
uint32_t millis(void);
