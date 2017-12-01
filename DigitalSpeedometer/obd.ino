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

void setup_obd_connection() {
    if (state == STATE_CONNECTED) {
        return;
    }

    sevseg.blankWithDp();

#ifndef MODE_SIMULATION
    obd.begin();

    // Initialize OBD-II adapter
    if (!obd.init()) {
        // We couldn't even connect to the adapter; sleep
        noInterrupts();
        state = STATE_DISCONNECTED;
        interrupts();

        Narcoleptic.delay(IDLE_SLEEP_DELAY_MS);
        return;
    }

    // We could init the communication; try reading from the ECU
    int value;    
    if (!obd.readPID(PID_SPEED, value)) {
        // Couldn't connect or read speed
        noInterrupts();
        state = STATE_DISCONNECTED;
        interrupts();

        // Enter deep sleep; disable all timers, serial comm., interrupts, etc.
        // for 8 seconds
        obd.enterLowPowerMode();
        Narcoleptic.delay(IDLE_SLEEP_DELAY_MS);
        obd.leaveLowPowerMode();
        return;
    }

    // Connection established!
    noInterrupts();
    state = STATE_CONNECTED;
    interrupts();
#else
    delay(1000);
    state = STATE_CONNECTED;
#endif
}

void probe_current_speed() {
    if (state != STATE_CONNECTED) {
        return;
    }

#ifndef MODE_SIMULATION
    int value;

    // Read speed from OBD, and update target_read_speed
    if (obd.readPID(PID_SPEED, value)) {
        noInterrupts();
        target_read_speed = value;
        interrupts();
    } else {
        // Couldn't read; we're disconnected, change state and we'll care about
        // that in the main loop
        noInterrupts();
        state = STATE_DISCONNECTED;
        target_read_speed = 0;
        interrupts();
    }
#else
    delay(50);
    
    noInterrupts();
    target_read_speed = (millis() / 1000 * 5) % 145;
    interrupts();
#endif
}