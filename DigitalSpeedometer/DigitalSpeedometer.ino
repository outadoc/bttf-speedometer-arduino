#include <Arduino.h>

#include <avr/sleep.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#include "DigitalSpeedometer.h"

SevSeg sevseg;
COBD obd;

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

void setup_timers() {
    // Timer that will refresh the display (interpolation using SevSeg)
    Timer1.initialize(TIMER_INTERVAL_DISP_REFRESH_MS * 1000);
    Timer1.attachInterrupt(isr_refresh_display);

    // Timer that will display the speed read from OBD every xxx ms,
    // interpolating if needed
    MsTimer2::set(TIMER_INTERVAL_DISP_INC_MS, isr_display);
    MsTimer2::start();
}

void setup_interrupts() {
    // Update the switch states in memory when they're changed
    attachInterrupt(digitalPinToInterrupt(PIN_UNIT_SELECT), isr_check_display_unit, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLEEP_ENABLE), isr_check_sleep_mode, CHANGE);
}

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
    
    sevseg.blankWithDp();
}

void setup_obd_connection() {
    sevseg.blankWithDp();

#ifndef MODE_SIMULATION
    obd.begin();

    // initialize OBD-II adapter
    while (state == STATE_DISCONNECTED) {
        int value;
        // Try to init and read speed; if we can't do either of them, sleep for a while
        if (obd.init() && obd.readPID(PID_SPEED, value)) {
            // Connection established!
            state = STATE_CONNECTED;
        } else {
            // Couldn't connect or read speed
            state = STATE_DISCONNECTED;

            // Enter deep sleep; disable all timers, serial comm., interrupts, etc.
            sevseg.blankWithDp();
            obd.enterLowPowerMode();
            Narcoleptic.delay(8000);
            obd.leaveLowPowerMode();
        }
    }
#else
    delay(1000);
    state = STATE_CONNECTED;
#endif
}

void loop() {
    noInterrupts();
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

// INTERRUPTS

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
        state = STATE_DISCONNECTED;
    }

    // Go to sleep
    if (sleep_enable) {
        state = STATE_SLEEPING;
    }
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

int analog_read_avg(const int sensor_pin, const int nb_samples, const long time_gap) {
    static int currentSample;
    static int currentValue = 0;

    // Perform multiple reads on the given pin and return an average
    for (int i = 0; i < nb_samples; i++) {
        currentSample = analogRead(sensor_pin);
        currentValue += currentSample;
        delay(time_gap);
    }

    return (currentValue / nb_samples);
}

void enter_sleep_mode() {
    sevseg.blank();

#ifndef MODE_SIMULATION
    obd.enterLowPowerMode();
#endif
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    noInterrupts(); // make sure we don't get interrupted before we sleep
    sleep_enable(); // enables the sleep bit in the mcucr register
    interrupts();   // interrupts allowed now, next instruction WILL be executed
    sleep_cpu();    // here the device is put to sleep
}

void leave_sleep_mode() {
    // Wakey wakey, eggs and bakey
    sleep_disable();
#ifndef MODE_SIMULATION
    obd.leaveLowPowerMode();
#endif
}
