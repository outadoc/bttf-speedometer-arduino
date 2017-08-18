#include <TimerOne.h>
#include <MsTimer2.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#define STATE_DISCONNECTED 0x0
#define STATE_CONNECTED    0x2

#define TIMER_INTERVAL_DISPLAY_MS 10
#define TIMER_INTERVAL_PROBE_MS   200

#define DEBUG

typedef int speed_t;

SevSeg sevseg;
COBD obd;

volatile byte state;
volatile speed_t target_read_speed;

void setup() {
  state = STATE_DISCONNECTED;
  target_read_speed = 0;

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("initializing speedometer");
#endif
  
  setup_display();

  // Every 10 ms
  Timer1.initialize(TIMER_INTERVAL_DISPLAY_MS * 1000);
  Timer1.attachInterrupt(refresh_display);

  // Every 200 ms
  MsTimer2::set(TIMER_INTERVAL_PROBE_MS, probe_current_speed);
}

void setup_display() {
  byte numDigits = 2;
  byte digitPins[] = {3, 4};
  byte segmentPins[] = {5, 6, 7, 8, 9, 10, 11, 12};
  bool resistorsOnSegments = true; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
}

void setup_obd_connection() {
#ifndef DEBUG
  obd.begin();

  // initialize OBD-II adapter
  for (;;) {
    int value;
    // Try to init and read speed; if we can't do either of them, sleep for a while
    if (obd.init() && obd.readPID(PID_SPEED, value))
        break;

    state = STATE_DISCONNECTED;
    
    obd.enterLowPowerMode();
    Narcoleptic.delay(7000);
    obd.leaveLowPowerMode();
  }
#else
  delay(1000);
#endif
  
  state = STATE_CONNECTED;
}

void loop() {
  // Speed currently displayed; will be incremented to reach target speed
  static speed_t curr_disp_speed = 0;

  noInterrupts();
  // Make copies of current speed and target speed
  const speed_t target_speed = adjust_speed(target_read_speed);
  interrupts();
  
  if (state == STATE_DISCONNECTED) {
    // Clear display if we couldn't read the speed, and try reconnecting
    sevseg.blank();
    setup_obd_connection();
  }

  // We want to increment speed one by one until we hit the target speed
  // on a relatively short duration
  double interval_between_incs = TIMER_INTERVAL_PROBE_MS / (target_speed - curr_disp_speed);

  // Until we've hit the target speed, increment, display and pause
  while (curr_disp_speed < target_speed) {
      display_speed(curr_disp_speed++);

      // Pause execution for a fixed amount of time between each iteration
      delay(interval_between_incs);
  }
}

// Called every 10 ms
void refresh_display() {
  sevseg.refreshDisplay();
}

speed_t adjust_speed(speed_t speed) {
  float modifier = (float)map(analogRead(0), 0, 1024, 2000, 18000) / (float)10000.0;
  speed_t res = round(modifier * (float)speed);
  
#ifdef DEBUG
  Serial.print("modifier: ");
  Serial.println(modifier, 4);

  Serial.print("speed: ");
  Serial.println(res);
#endif
  
  return res;
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
#ifdef DEBUG
  Serial.println("entered probe_current_speed");
#endif

  if (state == STATE_DISCONNECTED)
    return;

#ifndef DEBUG
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
  delay(100);
  
  noInterrupts();
  target_read_speed = (millis() / 1000) % 99;
  interrupts();
#endif
}


