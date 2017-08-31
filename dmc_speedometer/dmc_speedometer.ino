#include <avr/sleep.h>
#include <TimerOne.h>
#include <MsTimer2.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#define STATE_DISCONNECTED 0x0
#define STATE_CONNECTED    0x2
#define STATE_SLEEPING     0x3

#define TIMER_INTERVAL_DISP_REFRESH_MS 10
#define TIMER_INTERVAL_DISP_INC_MS   500

#define PIN_SEG_A  5
#define PIN_SEG_B  6
#define PIN_SEG_C  7
#define PIN_SEG_D  8
#define PIN_SEG_E  9
#define PIN_SEG_F  10
#define PIN_SEG_G  11
#define PIN_SEG_DP 12

#define PIN_DIG_1 A1
#define PIN_DIG_2 A2

#define PIN_USE_IMPERIAL 2
#define PIN_USE_METRIC 3

#define PIN_SPEED_ADJUST 0

//#define MODE_SIMULATION

typedef uint8_t speed_t;

SevSeg sevseg;
COBD obd;

volatile byte state;
volatile speed_t target_read_speed;

float modifier;
volatile bool should_display_imperial;
volatile bool should_display_metric;

void setup() {
    state = STATE_DISCONNECTED;
    target_read_speed = 0;

    pinMode(PIN_USE_IMPERIAL, INPUT);
    pinMode(PIN_USE_METRIC, INPUT);

    // Read speed modifier (1.0 keeps raw speed read from OBD)
    // Play with the potentiometer to adjust to real speed or switch to mph
    modifier = (float)map(analogReadAvg(PIN_SPEED_ADJUST, 20, 20), 
                            0, 1024, 9000, 12000) / (float)10000.0;

    #ifdef MODE_SIMULATION
        Serial.begin(9600);
        Serial.println(modifier, 4);
    #endif

    setup_display();
    setup_timers();
    setup_interrupts();

    // Initialize display unit
    isr_read_display_unit();

    // Wait a little for everything to settle before we move on
    delay(2000);
}

void setup_timers() {
    Timer1.initialize(TIMER_INTERVAL_DISP_REFRESH_MS * 1000);
    Timer1.attachInterrupt(isr_refresh_display);

    MsTimer2::set(TIMER_INTERVAL_DISP_INC_MS, isr_display);
    MsTimer2::start();
}

void setup_interrupts() {
    // When switching kph/OFF/mph, update the switch states in memory 
    attachInterrupt(digitalPinToInterrupt(PIN_USE_IMPERIAL), isr_read_display_unit, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_USE_METRIC), isr_read_display_unit, CHANGE);
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
    bool resistorsOnSegments = true;
    byte hardwareConfig = COMMON_CATHODE;
    bool updateWithDelays = false;
    bool leadingZeros = false;
    
    sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, 
        resistorsOnSegments, updateWithDelays, leadingZeros);
    
    sevseg.blank();
}

void setup_obd_connection() {
    sevseg.blank();    
#ifndef MODE_SIMULATION
    obd.begin();

    // initialize OBD-II adapter
    for (;;) {
        int value;
        // Try to init and read speed; if we can't do either of them, sleep for a while
        if (obd.init() && obd.readPID(PID_SPEED, value)) {
            // Connection established!
            break;            
        }

        state = STATE_DISCONNECTED;
        
        // Enter deep sleep; disable all timers, serial comm., interrupts, etc.
        obd.enterLowPowerMode();
        Narcoleptic.delay(8000);
        obd.leaveLowPowerMode();
    }
#else
    delay(1000);
#endif

    state = STATE_CONNECTED;
}

void loop() {
    if (state == STATE_DISCONNECTED) {
        // Clear display if we couldn't read the speed, and try reconnecting
        setup_obd_connection();
    }

    if (state == STATE_SLEEPING) {
        enter_sleep_mode();
        return;
    }

    probe_current_speed();
}

// INTERRUPTS

void isr_display() {
    // Speed currently displayed; will be incremented to reach target speed
    static speed_t curr_disp_speed = 0;
    
    if (state == STATE_DISCONNECTED) {
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

void isr_read_display_unit() {
    // If we were sleeping, wake up
    if (state == STATE_SLEEPING) {
        leave_sleep_mode();
        state = STATE_DISCONNECTED;
    }

    // Read display mode from switch
    should_display_imperial = digitalRead(PIN_USE_IMPERIAL);
    should_display_metric = digitalRead(PIN_USE_METRIC);

    if (!should_display_imperial && !should_display_metric) {
        state = STATE_SLEEPING;
    }
}

speed_t adjust_speed(speed_t speed) {
    if (should_display_imperial) {
        return round(modifier * (float)speed * 0.621371);
    }
    
    return round(modifier * (float)speed);
}

void set_displayed_speed(speed_t speed) {
    if (speed < 100) {
        sevseg.setNumber(speed, 0);
    } else {
        // If the speed is >= 100, truncate display and don't show the hundreds 
        // (useful when reading in km/h)
        sevseg.setNumber(speed - 100, 0);
    }
}

void probe_current_speed() {
    if (state == STATE_DISCONNECTED)
        return;

#ifndef MODE_SIMULATION
    int value;
    if (obd.readPID(PID_SPEED, value)) {
        noInterrupts();
        target_read_speed = value;
        interrupts();
        return;
    }

    noInterrupts();
    state = STATE_DISCONNECTED;
    target_read_speed = 0;
    interrupts();
#else
    delay(50);
    
    noInterrupts();
    target_read_speed = (millis() / 1000 * 5) % 99;
    interrupts();
#endif
}

int analogReadAvg(const int sensorPin, const int numberOfSamples, const long timeGap) {
    static int currentSample;
    static int currentValue = 0;

    for (int i = 0; i < numberOfSamples; i++) {
        currentSample = analogRead(sensorPin);
        currentValue += currentSample;
        delay(timeGap);
    }

    return (currentValue / numberOfSamples);
}

void enter_sleep_mode() {
    sevseg.blank();
    sevseg.refreshDisplay();
    
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
    sleep_disable();
#ifndef MODE_SIMULATION
    obd.leaveLowPowerMode();
#endif
}
