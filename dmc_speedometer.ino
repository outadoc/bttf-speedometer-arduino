#include <TimerOne.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

#define STATE_DISCONNECTED 0
#define STATE_CONNECTED 1

SevSeg sevseg;
COBD obd;

volatile unsigned int lastReadSpeed = 0;
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
  /*byte version = obd.begin();
  
  // initialize OBD-II adapter
  for (;;) {
      if (obd.init())
          break;

      state = STATE_DISCONNECTED;
      
      obd.enterLowPowerMode();
      Narcoleptic.delay(7000);
      obd.leaveLowPowerMode();
  }*/

  state = STATE_CONNECTED;
  delay(5000);
}

void loop() {  
  if (state == STATE_DISCONNECTED) {
    // Clear display if we couldn't read the speed
    sevseg.blank();
    
    setupObdConnection();
  }
  
  // Display last read speed if things are ok
  sevseg.setNumber(lastReadSpeed, 0);
  
  readCurrentSpeed();
}

void refreshDisplay() {
  sevseg.refreshDisplay();
}

// Call with an interval of [reasonable] ms
void readCurrentSpeed() {
  int value;
  /*if (obd.readPID(PID_SPEED, value)) {
    //float modifier = (analogRead(0) / 1024.0) * 100.0;
    lastReadSpeed = value;
  }*/

  lastReadSpeed = random(1, 99);
  delay(300);

  //state = STATE_DISCONNECTED;
}


