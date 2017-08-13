#include <TimerOne.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#define STATE_DISCONNECTED 0
#define STATE_CONNECTED 1

#define DEBUG

typedef int speed_t;

SevSeg sevseg;
COBD obd;

volatile byte state;

void setup() {
  state = STATE_DISCONNECTED;
  
  setupDisplay();

  Timer1.initialize(10000);
  Timer1.attachInterrupt(refreshDisplay);
}

void setupDisplay() {
  byte numDigits = 2;
  byte digitPins[] = {3, 4};
  byte segmentPins[] = {5, 6, 7, 8, 9, 10, 11, 12};
  bool resistorsOnSegments = true; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
}

void setupObdConnection() {
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
  #endif

  #ifdef DEBUG
  delay(5000);
  #endif
  
  state = STATE_CONNECTED;
}

void loop() {  
  if (state == STATE_DISCONNECTED) {
    // Clear display if we couldn't read the speed, and try reconnecting
    sevseg.blank();
    setupObdConnection();
  }

  speed_t speed = readCurrentSpeed();
  speed_t adjustedSpeed = adjustSpeed(speed);
  
  // Display last read speed if things are ok
  if (adjustedSpeed < 100) {
      sevseg.setNumber(adjustedSpeed, 0);
  } else {
      // If the speed is >= 100, don't display the hundreds 
      // (useful when reading in km/h)
      sevseg.setNumber(adjustedSpeed - 100, 0);
  }
}

// Called every 10 ms
void refreshDisplay() {
  sevseg.refreshDisplay();
}

speed_t adjustSpeed(speed_t speed) {
  float modifier = (float)map(analogRead(0), 0, 1024, 50, 200) / (float)100.0;
  return modifier * speed;
}

speed_t readCurrentSpeed() {
  #ifndef DEBUG
  int value;
  if (obd.readPID(PID_SPEED, value)) {
    return value;
  }
  
  state = STATE_DISCONNECTED;
  return -1;
  #endif

  #ifdef DEBUG
  delay(300);
  return random(1, 99);
  #endif
}


