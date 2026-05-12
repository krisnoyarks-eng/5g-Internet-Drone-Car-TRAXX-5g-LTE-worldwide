#include <Servo.h>
#include <EEPROM.h>

Servo steeringServo;
const int servoPin = 4; // D2 on NodeMCU/Wemos

// ==========================================
// THE IRON WALLS - DYNAMICALLY SET BY PI
// ==========================================
float ABS_MIN_LEFT = 50.0;  // Default locked at safe 50
float ABS_MAX_RIGHT = 130.0; // Default locked at safe 130

// The perfect mathematical middle of your steering rack
float currentCenter = 90.0; 
float currentAngle = 90.0; 

// --- NEW ANTI-ROLLOVER & FAILSAFE VARIABLES ---
bool racingMode = false;
float currentSpeedKmh = 0.0;
float autoCenterSec = 0.0; 
unsigned long lastDriveCmd = 0;
unsigned long bootTime = 0; // ADDED: Captures exact start time to fix soft reset

void saveToEEPROM() {
  EEPROM.put(0, ABS_MIN_LEFT);
  EEPROM.put(4, ABS_MAX_RIGHT);
  EEPROM.commit();
  Serial.println("WALLS SAVED TO ESP EEPROM");
}

void loadFromEEPROM() {
  float tempLeft, tempRight;
  EEPROM.get(0, tempLeft);
  EEPROM.get(4, tempRight);
  
  if (tempLeft >= 50.0 && tempLeft <= 89.0) ABS_MIN_LEFT = tempLeft;
  if (tempRight >= 91.0 && tempRight <= 130.0) ABS_MAX_RIGHT = tempRight;
}

void setPreciseAngle(float angle) {
  if (angle < ABS_MIN_LEFT) angle = ABS_MIN_LEFT;
  if (angle > ABS_MAX_RIGHT) angle = ABS_MAX_RIGHT;
  
  if (angle < 50.0) angle = 50.0;
  if (angle > 130.0) angle = 130.0;
  
  int pulse = 544 + (int)(angle * 10.3111);
  steeringServo.writeMicroseconds(pulse);
}

void setup() {
  Serial.begin(115200);
  bootTime = millis(); // Captures start time to offset soft reboot timer
  EEPROM.begin(512); 
  loadFromEEPROM();  
  
  steeringServo.attach(servoPin);
  setPreciseAngle(currentCenter);

  Serial.println("ID:ESP12_STEER");
}

void loop() {
  // --- AUTO CENTER FAILSAFE LOGIC ---
  if (autoCenterSec > 0.01) {
    if (millis() - lastDriveCmd > (autoCenterSec * 1000)) {
      if (currentAngle != currentCenter) {
        currentAngle = currentCenter;
        setPreciseAngle(currentAngle);
      }
    }
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "PING_ID") { Serial.println("ID:ESP12_STEER"); return; }
    if (command == "restart" || command == "RESTART") { Serial.println("REBOOTING ESP..."); delay(100); ESP.restart(); return; }
    
    // UPDATED: Now subtracts the bootTime offset to always start at 0 seconds
    if (command == "runningtime" || command == "RUNNINGTIME") { Serial.print("UPTIME_SECONDS:"); Serial.println((millis() - bootTime) / 1000); return; }

    if (command == "limits") {
      Serial.print("LEFT WALL: "); Serial.print(ABS_MIN_LEFT);
      Serial.print(" | RIGHT WALL: "); Serial.println(ABS_MAX_RIGHT);
      Serial.print("CENTER: "); Serial.println(currentCenter);
      return;
    }

    if (command.length() < 2) return;

    char dir = command.charAt(0);
    float magnitude = command.substring(1).toFloat(); 

    // LIVE TELEMETRY FROM PI
    if (dir == 'k' || dir == 'K') { currentSpeedKmh = magnitude; return; }
    if (dir == 't' || dir == 'T') { autoCenterSec = magnitude; return; }
    if (dir == 'R') { racingMode = (magnitude > 0.5); return; }

    if (dir == 'x' || dir == 'X') { if (magnitude < 50.0) magnitude = 50.0; ABS_MIN_LEFT = magnitude; saveToEEPROM(); return; }
    if (dir == 'y' || dir == 'Y') { if (magnitude > 130.0) magnitude = 130.0; ABS_MAX_RIGHT = magnitude; saveToEEPROM(); return; }

    if (dir == 'c' || dir == 'C') {
        if (magnitude < ABS_MIN_LEFT) magnitude = ABS_MIN_LEFT;
        if (magnitude > ABS_MAX_RIGHT) magnitude = ABS_MAX_RIGHT;
        currentCenter = magnitude;
        currentAngle = currentCenter; 
        setPreciseAngle(currentAngle);
        return;
    }

    if (dir == 'a' || dir == 'A') { currentAngle = magnitude; setPreciseAngle(currentAngle); return; }

    // --- DRIVE RECORDING ---
    if (dir == 'l' || dir == 'L' || dir == 'r' || dir == 'R' || dir == 's' || dir == 'S') {
        lastDriveCmd = millis();
    }

    if (magnitude < 0.0) magnitude = 0.0;
    if (magnitude > 10.0) magnitude = 10.0;

    // --- ANTI-ROLLOVER RACING LOGIC ---
    if (racingMode && (dir == 'l' || dir == 'L' || dir == 'r' || dir == 'R')) {
        float maxAllowedTurn = 10.0;
        if (currentSpeedKmh >= 5.0)  maxAllowedTurn = 8.0;
        if (currentSpeedKmh >= 10.0) maxAllowedTurn = 5.0;
        if (currentSpeedKmh >= 15.0) maxAllowedTurn = 3.5;
        if (currentSpeedKmh >= 25.0) maxAllowedTurn = 2.0;

        if (magnitude > maxAllowedTurn) {
            magnitude = maxAllowedTurn;
        }
    }

    if (dir == 'l' || dir == 'L') {
      float maxLeftTravel = currentCenter - ABS_MIN_LEFT;
      currentAngle = currentCenter - (magnitude * (maxLeftTravel / 10.0));
    } 
    else if (dir == 'r' || dir == 'R') {
      float maxRightTravel = ABS_MAX_RIGHT - currentCenter;
      currentAngle = currentCenter + (magnitude * (maxRightTravel / 10.0));
    } 
    else if (dir == 's' || dir == 'S') {
      currentAngle = currentCenter;
    }
    else { return; }

    setPreciseAngle(currentAngle);
  }
}