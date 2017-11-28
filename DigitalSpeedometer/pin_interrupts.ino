#include <Arduino.h>
#include "DigitalSpeedometer.h"

void setup_interrupts() {
    // Update the switch states in memory when they're changed
    attachInterrupt(digitalPinToInterrupt(PIN_UNIT_SELECT), isr_check_display_unit, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLEEP_ENABLE), isr_check_sleep_mode, CHANGE);
}

void isr_check_display_unit() {
    // Read unit mode from switch
    unit_select = digitalRead(PIN_UNIT_SELECT);
}

void isr_check_sleep_mode() {
    // Check if we've enabled the sleep switch
    int sleep_enable = digitalRead(PIN_SLEEP_ENABLE);

    // If we were sleeping, wake up
    if (state == STATE_SLEEPING && !sleep_enable) {
        leave_sleep_mode();
    }

    // Go to sleep
    if (sleep_enable) {
        state = STATE_SLEEPING;
    }
}
