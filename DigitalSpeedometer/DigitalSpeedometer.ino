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

#include <avr/sleep.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#include "DigitalSpeedometer.h"

SevSeg sevseg;

#ifndef MODE_SIMULATION
COBD obd;
#endif

volatile state_t state;
volatile speed_t target_read_speed;

volatile float modifier;
volatile int unit_select;

void setup() {
    state = STATE_DISCONNECTED;
    target_read_speed = 0;

    pinMode(PIN_UNIT_SELECT, INPUT_PULLUP);
    pinMode(PIN_SLEEP_ENABLE, INPUT_PULLUP);

    // Read speed modifier (1.0 keeps raw speed read from OBD)
    // OBD speed can be a bit different from real life speed, so play with 
    // the potentiometer to adjust to real speed
    modifier = (float)map(analog_read_avg(PIN_SPEED_ADJUST, 20, 20), 
                            0, 1024, 9000, 12000) / (float)10000.0;

    #ifdef MODE_SIMULATION
        Serial.begin(9600);
        Serial.println(modifier, 4);
    #endif

    setup_display();
    setup_timers();
    setup_interrupts();

    // Initialize display unit switch and sleep mode interrupts
    isr_check_display_unit();
    isr_check_sleep_mode();
}

void loop() {
    noInterrupts();
    isr_check_sleep_mode();
    state_t loc_state = state;
    interrupts();

    // Yay, finite state machine!
    switch (loc_state) {
        case STATE_DISCONNECTED:
            // If we got disconnected (or on first loop), try reconnecting
            setup_obd_connection();
            break;        
        case STATE_SLEEPING:
            // If we switched the switch to OFF, time for sleepies
            enter_sleep_mode();
            break;
        case STATE_CONNECTED:
            // We should be connected. PROBE!
            probe_current_speed();
            break;
    }
}

void enter_sleep_mode() {
    noInterrupts();

    // Blank the display and wait a little before sleeping
    sevseg.blank();
    delay(500);

#ifndef MODE_SIMULATION
    if (state == STATE_CONNECTED) {
        obd.enterLowPowerMode();
    }
#endif

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable(); // enables the sleep bit in the mcucr register
    interrupts();   // interrupts allowed now, next instruction WILL be executed
    sleep_cpu();    // here the device is put to sleep
}

void leave_sleep_mode() {
    // Wakey wakey, eggs and bakey
    sleep_disable();
    
    noInterrupts();
    state = STATE_DISCONNECTED;
    interrupts();
}
