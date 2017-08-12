#include <TimerOne.h>
#include <OBD2UART.h>
#include <SevSeg.h>
#include <Narcoleptic.h>

SevSeg sevseg;
COBD obd;

volatile int lastReadSpeed = -1;

void setup() {
  setupDisplay();
  setupObdConnection();

  Timer1.initialize(13000);
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
  sevseg.setBrightness(90);
}

void setupObdConnection() {
  byte version = obd.begin();
  
  // initialize OBD-II adapter
  for (;;) {
      if (obd.init())
          break;
      
      obd.enterLowPowerMode();
      Narcoleptic.delay(7000);
      obd.leaveLowPowerMode();
  }
}

void loop() {  
  if (lastReadSpeed < 0) {
    // Clear display if we couldn't read the speed
    sevseg.blank();

    obd.enterLowPowerMode();
    Narcoleptic.delay(7000);
    obd.leaveLowPowerMode();
  } else {
    // Display last read speed if things are ok
    sevseg.setNumber(lastReadSpeed, 0);
  }

  readCurrentSpeed();
}

void refreshDisplay() {
  sevseg.refreshDisplay();
}

// Call with an interval of [reasonable] ms
void readCurrentSpeed() {
  int value;
  if (obd.readPID(PID_SPEED, value)) {
    //float modifier = (analogRead(0) / 1024.0) * 100.0;
    lastReadSpeed = value;
  }

  lastReadSpeed = -1;
}


