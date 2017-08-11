#include <Timer.h>
#include <OBD2UART.h>
#include <SevSeg.h>

SevSeg sevseg;
COBD obd;
Timer t;

int lastReadSpeed = -1;

void setup() {
  setupDisplay();
  setupObdConnection();

  t.every(100, readCurrentSpeed);
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
  delay(1000);

  byte version = obd.begin();

  delay(1000);
  
  // initialize OBD-II adapter
  while (!obd.init());
}

void loop() {
  if (lastReadSpeed < 0) {
    // Clear display if we couldn't read the speed
    sevseg.blank();
  } else {
    // Display last read speed if things are ok
    displaySpeed(lastReadSpeed);
  }
  
  sevseg.refreshDisplay();
  t.update();
}

// Call with an interval of [reasonable] ms
int readCurrentSpeed() {
  int value;

  if (obd.readPID(PID_SPEED, value)) {
    float modifier = (analogRead(0) / 1024.0) * 100.0;
    lastReadSpeed = value;
    return lastReadSpeed;
  }

  lastReadSpeed = -1;
  return lastReadSpeed;
}

void displaySpeed(int speed) {
  sevseg.setNumber(speed, 0);
}

