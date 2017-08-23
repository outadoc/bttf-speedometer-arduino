#include <TimerOne.h>
#include <MsTimer2.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>
#include <NeoSWSerial.h>

#define STATE_DISCONNECTED 0x0
#define STATE_CONNECTED    0x2

#define TIMER_INTERVAL_DISPLAY_MS 10
#define TIMER_INTERVAL_PROBE_MS   500

#define DISP_STATUS_CODES
//#define MODE_SIMULATION
//#define LOG_TRACE

#ifdef MODE_SIMULATION
    NeoSWSerial debugSerial(2, 3);
#endif

typedef uint8_t speed_t;

SevSeg sevseg;
COBD obd;

volatile byte state;
volatile speed_t target_read_speed;

float modifier = 1.0;

void setup() {
#ifdef MODE_SIMULATION  
    debugSerial.begin(9600);
    debugSerial.println("initializing speedometer");
#endif
    
    state = STATE_DISCONNECTED;
    target_read_speed = 0;

    // Read speed modifier (1.0 keeps raw speed read from OBD)
    // Play with the potentiometer to adjust to real speed or switch to mph
    modifier = (float)map(analogRead(0), 0, 1024, 2000, 18000) / (float)10000.0;

#ifdef MODE_SIMULATION
    debugSerial.print("modifier value: ");
    debugSerial.println(modifier);
#endif

    setup_display();

    // Wait a little for everything to settle before we move on
    delay(2000);

    setup_timers();

#ifdef DISP_STATUS_CODES
    display_status_code(1);
#endif
}

void setup_display() {
    byte numDigits = 2;
    byte digitPins[] = {A1, A2};
    byte segmentPins[] = {5, 6, 7, 8, 9, 10, 11, 12};
    bool resistorsOnSegments = true;
    byte hardwareConfig = COMMON_CATHODE;
    bool updateWithDelays = false;
    bool leadingZeros = false;
    
    sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
    sevseg.blank();
}

void setup_obd_connection() {
#ifndef MODE_SIMULATION
    obd.begin();

#ifdef DISP_STATUS_CODES
    display_status_code(2);
#endif

    // initialize OBD-II adapter
    for (;;) {
#ifdef DISP_STATUS_CODES
        display_status_code(5);
#endif
        int value;
        // Try to init and read speed; if we can't do either of them, sleep for a while
        if (obd.init() && obd.readPID(PID_SPEED, value))
                break;
                
#ifdef DISP_STATUS_CODES
        display_status_code(3);
#endif

        state = STATE_DISCONNECTED;

#ifdef DISP_STATUS_CODES
        display_status_code(4);
#endif
        
        // Enter deep sleep; disable all timers, serial comm., interrupts, etc.
        obd.enterLowPowerMode();    
        Narcoleptic.delay(8000);
        obd.leaveLowPowerMode();
    }
#else
    delay(1000);
#endif

#ifdef DISP_STATUS_CODES
    display_status_code(6);
#endif

    state = STATE_CONNECTED;
}

void setup_timers() {
    // Every 10 ms
    Timer1.initialize(TIMER_INTERVAL_DISPLAY_MS * 1000);
    Timer1.attachInterrupt(refresh_display);

    // Every 200 ms
    MsTimer2::set(TIMER_INTERVAL_PROBE_MS, probe_current_speed);
    MsTimer2::start();
}

void loop() {
    // Speed currently displayed; will be incremented to reach target speed
    static speed_t curr_disp_speed = 0;

#ifdef DISP_STATUS_CODES
    display_status_code(7);
#endif
    
    noInterrupts();
    // Make copies of current speed and target speed
    const speed_t target_speed = adjust_speed(target_read_speed);
    interrupts();
    
    if (state == STATE_DISCONNECTED) {
#ifdef MODE_SIMULATION
        debugSerial.println("disconnected from obd");
#endif

        // Clear display if we couldn't read the speed, and try reconnecting
        //sevseg.blank();
        setup_obd_connection();
    }

#ifdef DISP_STATUS_CODES
    display_status_code(8);
#endif

    // We want to increment speed one by one until we hit the target speed
    // on a relatively short duration
    double interval_between_incs = TIMER_INTERVAL_PROBE_MS / (abs(target_speed - curr_disp_speed));

#ifdef MODE_SIMULATION
    debugSerial.print(curr_disp_speed);
    debugSerial.print(" -> ");
    debugSerial.println(target_speed);
#endif

    if (curr_disp_speed == target_speed) {
        delay(50);
    }

#ifdef DISP_STATUS_CODES
    display_status_code(9);
#endif
    
    // Until we've hit the target speed, increment, display and pause
    while (curr_disp_speed != target_speed) {
            if (curr_disp_speed < target_speed) {
                curr_disp_speed++;
            } else {
                curr_disp_speed--;
            }
            
            // Display and pause execution for a fixed amount of time between each iteration
            display_speed(curr_disp_speed);
            delay(interval_between_incs);
    }
}

// Called every 10 ms
void refresh_display() {
#ifdef LOG_TRACE
    debugSerial.println("entered refresh_display");
#endif

    sevseg.refreshDisplay();
}

speed_t adjust_speed(speed_t speed) {
    return round(modifier * (float)speed);
}

void display_speed(speed_t speed) {
    if (speed < 100) {
            sevseg.setNumber(speed, 0);
    } else {
            // If the speed is >= 100, truncate display and don't show the hundreds 
            // (useful when reading in km/h)
            sevseg.setNumber(speed - 100, 0);
    }
}

void probe_current_speed() {
#ifdef LOG_TRACE
    debugSerial.println("entered probe_current_speed");
#endif

    if (state == STATE_DISCONNECTED)
        return;

#ifdef DISP_STATUS_CODES
    display_status_code(10);
#endif

#ifndef MODE_SIMULATION
    int value;
    if (obd.readPID(PID_SPEED, value)) {
#ifdef DISP_STATUS_CODES
        display_status_code(11);
#endif

        noInterrupts();
        target_read_speed = value;
        interrupts();
        return;
    }

#ifdef DISP_STATUS_CODES
    display_status_code(11);
#endif

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

void display_status_code(byte code) {
    sevseg.setNumber(0xf0 + code, -1, true);
    delay(200);
}


