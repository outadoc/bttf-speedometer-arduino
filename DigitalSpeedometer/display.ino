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

void setup_display() {
    byte numDigits = 2;
    byte digitPins[] = {PIN_DIG_1, PIN_DIG_2};
    byte segmentPins[] = {
        PIN_SEG_A,
        PIN_SEG_B,
        PIN_SEG_C,
        PIN_SEG_D,
        PIN_SEG_E,
        PIN_SEG_F,
        PIN_SEG_G,
        PIN_SEG_DP
    };

    // If you change these you might need to hack around in SevSeg.cpp to
    // replay my crappy hacks
    bool resistorsOnSegments = true;
    byte hardwareConfig = COMMON_CATHODE;
    bool updateWithDelays = false;
    bool leadingZeros = false;
    
    sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, 
        resistorsOnSegments, updateWithDelays, leadingZeros);

    sevseg.blank();
}

void setup_timers() {
    // Timer that will refresh the display (interpolation using SevSeg)
    Timer1.initialize(TIMER_INTERVAL_DISP_REFRESH_MS * 1000);
    Timer1.attachInterrupt(isr_refresh_display);

    // Timer that will display the speed read from OBD every xxx ms,
    // interpolating if needed
    MsTimer2::set(TIMER_INTERVAL_DISP_INC_MS, isr_display);
    MsTimer2::start();
}

speed_t adjust_speed(speed_t speed) {
    // Adjust read speed using modifier (set via potentiometer) and take
    // unit into account
    if (unit_select == UNIT_MPH) {
        // Convert from km/h to mph
        return round(modifier * (float)speed * 0.621371);
    }
    
    return round(modifier * (float)speed);
}

void set_displayed_speed(speed_t speed) {
    // Display number with a decimal point
    sevseg.setNumber(speed, 0);
}

void isr_display() {
    // Speed currently displayed; will be incremented to reach target speed
    static speed_t curr_disp_speed = 0;
    
    if (state != STATE_CONNECTED) {
        return;
    }
    
    // Make copy of target speed
    const speed_t target_speed = adjust_speed(target_read_speed);

    // We want to increment speed one by one until we hit the target speed
    // on a relatively short duration
    double interval_between_incs = TIMER_INTERVAL_DISP_INC_MS / (abs(target_speed - curr_disp_speed));

    // Enable interrupts for the rest of the procedure,
    // because we want the display to still be refreshed
    interrupts();

    if (curr_disp_speed == target_speed) {
        set_displayed_speed(curr_disp_speed);        
    }

    // Until we've hit the target speed, increment, display and pause
    while (curr_disp_speed != target_speed) {
        if (curr_disp_speed < target_speed) {
            curr_disp_speed++;
        } else {
            curr_disp_speed--;
        }
        
        // Display and pause execution for a fixed amount of time between each iteration
        set_displayed_speed(curr_disp_speed);
        delay(interval_between_incs);
    }
}

// Called every 10 ms
void isr_refresh_display() {
    sevseg.refreshDisplay();
}
