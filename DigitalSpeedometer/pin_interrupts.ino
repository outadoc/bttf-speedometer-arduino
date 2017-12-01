/* Arduino-based replica of the Back to the Future DeLorean Speedometer
   Copyright (C) 2017  Baptiste Candellier

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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
