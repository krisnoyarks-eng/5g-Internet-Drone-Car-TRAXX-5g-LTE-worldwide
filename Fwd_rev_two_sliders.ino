#include <Servo.h>
#include <EEPROM.h>
#include <TM1637Display.h>

// --- Pins ---
#define CLK_PIN 2   
#define DIO_PIN 3   
const int AmpMeter = A0;   
const int VoltsMeter = A1; 
const int escPin = 9; 

TM1637Display display(CLK_PIN, DIO_PIN);
Servo myESC;

// --- Custom Letter Segments ---
const uint8_t char_V = SEG_C | SEG_D | SEG_E;          
const uint8_t char_A = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G; 
const uint8_t char_P = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;         
const uint8_t char_F = SEG_A | SEG_E | SEG_F | SEG_G; 
const uint8_t char_r = SEG_E | SEG_G;                 
const uint8_t char_C = SEG_A | SEG_D | SEG_E | SEG_F; 
const uint8_t msg_CON[] = { SEG_A|SEG_D|SEG_E|SEG_F, SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F, SEG_C|SEG_E|SEG_G };
const uint8_t circle[] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F };

// --- Variables ---
String inputString = "";
String fullIP = "";           
String networkName = "";      
unsigned long lastCommandTime = 0, lastIPRequest = 0, lastModeSwitch = 0, lastScrollMillis = 0;
bool isBraked = true, hasExternalIP = false; 
bool piWifiError = false; 
int lastPulse = 1500, displayMode = 0, scrollPos = 0, subPhase = 0;
uint8_t animStep = 0;

int max_fwd = 1;
int max_rev = 1;
float v_offset = 0.0; 
int saved_cal_p = 0; 
int batt_type = 0; 
float failsafe_sec = 3.0; 

int batt_count = 1;
int batt_mah = 5800;

unsigned long last_fwd_time = -10000; 
unsigned long last_rev_time = -10000; 

// --- TUNING RATIOS ---
const float vDividerRatio = 3.45; // If using 0-25V sensor, change to 5.0
const float aDividerRatio = 12.0; 
const float sensitivity = 0.100;  
const float distV = 5.0 / 1023.0; 
float zeroPoint = 0;

const int ee_max_f = 0, ee_max_r = 2, ee_v_off = 4, ee_cal_p = 8;
const int ee_b_type = 10, ee_fs = 12, ee_b_cnt = 16, ee_b_mah = 18, ee_init = 22;

extern volatile unsigned long timer0_millis;
void(* resetFunc) (void) = 0;

// NEW: Ratchet System Variables
float filtered_v = -1.0;
int current_pct = -1;

float getVolts() {
  long rawSum = 0;
  for(int i=0; i<30; i++) rawSum += analogRead(VoltsMeter);
  float raw_v = ((rawSum / 30.0) * distV) * vDividerRatio;
  return raw_v + v_offset; 
}

float getAmps() {
  long rawSum = 0;
  int samples = 100; 
  for(int i=0; i < samples; i++) rawSum += analogRead(AmpMeter);
  
  float currentRawV = (rawSum / (float)samples) * distV;
  float voltageDifference = currentRawV - zeroPoint;
  float currentAmps = fabs(voltageDifference / sensitivity) * aDividerRatio;
  
  static float filteredAmps = 0;
  filteredAmps = (currentAmps * 0.1) + (filteredAmps * 0.9);
  return (filteredAmps < 0.01) ? 0.00 : filteredAmps;
}

int getBatteryPercentage() {
  float v = getVolts();
  float a = getAmps();
  
  float sag_v = (a / (float)batt_count) * 0.015;
  float corrected_v = v + sag_v; 
  
  // HEAVY LOW-PASS FILTER: Prevents instant drops when you punch the throttle
  if (filtered_v < 0.0) {
     filtered_v = corrected_v;
  } else {
     filtered_v = (corrected_v * 0.005) + (filtered_v * 0.995); 
  }

  int calc_pct = 0;
  if (batt_type == 1) { calc_pct = map((int)(filtered_v * 100), 600, 840, 0, 100); } 
  else { calc_pct = map((int)(filtered_v * 100), 640, 840, 0, 100); }
  calc_pct = constrain(calc_pct, 0, 100);

  // ONE-WAY RATCHET: Battery can only stay the same or drop. Never rise.
  if (current_pct == -1) {
     current_pct = calc_pct;
  } else if (calc_pct < current_pct) {
     current_pct = calc_pct; 
  }
  
  return current_pct;
}

void scrollText(String text) {
  if (millis() - lastScrollMillis > 300) {
    lastScrollMillis = millis();
    String cleanText = text;
    cleanText.replace(".", "-"); 
    String paddedText = "    " + cleanText + "    "; 
    uint8_t segments[4];
    for (int i = 0; i < 4; i++) {
      char c = paddedText[scrollPos + i];
      if (c == ' ') segments[i] = 0;
      else if (c == '-') segments[i] = SEG_G;
      else if (c >= '0' && c <= '9') segments[i] = display.encodeDigit(c - '0');
      else segments[i] = display.encodeDigit(c);
    }
    display.setSegments(segments);
    scrollPos++;
    if (scrollPos > (int)paddedText.length() - 4) scrollPos = 0;
  }
}

void runAnimation() {
  if (millis() - lastScrollMillis > 100) { 
    lastScrollMillis = millis();
    uint8_t data[] = { circle[animStep], circle[animStep], circle[animStep], circle[animStep] };
    display.setSegments(data);
    animStep = (animStep + 1) % 6; 
  }
}

void applySmartBrakes() {
  if (isBraked) return;
  if (lastPulse > 1505) myESC.writeMicroseconds(1000); 
  else if (lastPulse < 1495) myESC.writeMicroseconds(2000);
  delay(350); 
  myESC.writeMicroseconds(1500); 
  isBraked = true; 
  lastPulse = 1500;
}

void executeCommand(String cmd) {
  cmd.trim();
  if (cmd == "PING_ID") { Serial.println("ID:ESP_DRIVE"); return; }
  
  String lowerCmd = cmd; 
  lowerCmd.toLowerCase(); 

  if (lowerCmd == "restart") { 
    Serial.println("REBOOTING UNO..."); 
    delay(100); 
    noInterrupts();
    delay(100); 
    timer0_millis = 0; 
    delay(100); 
    interrupts();
    delay(100); 
    resetFunc(); 
    delay(100); 
    return; 
  }
  
  if (lowerCmd == "runningtime") { Serial.print("UPTIME_SECONDS:"); Serial.println(millis() / 1000); return; }

  if (lowerCmd == "limits") {
    Serial.print("Limits - FWD: "); Serial.print(max_fwd);
    Serial.print(" | REV: "); Serial.print(max_rev);
    Serial.print(" | CALP: "); Serial.print(saved_cal_p);
    Serial.print(" | BT: "); Serial.print(batt_type);
    Serial.print(" | FS: "); Serial.println(failsafe_sec);
    return;
  }

  if (lowerCmd.startsWith("lim_f:")) { max_fwd = lowerCmd.substring(6).toInt(); EEPROM.put(ee_max_f, max_fwd); return; }
  if (lowerCmd.startsWith("lim_r:")) { max_rev = lowerCmd.substring(6).toInt(); EEPROM.put(ee_max_r, max_rev); return; }
  if (lowerCmd.startsWith("b_cfg:")) { batt_type = lowerCmd.substring(6).toInt(); EEPROM.put(ee_b_type, batt_type); return; }
  if (lowerCmd.startsWith("fs:")) { failsafe_sec = lowerCmd.substring(3).toFloat(); EEPROM.put(ee_fs, failsafe_sec); return; }
  
  if (lowerCmd.startsWith("b_cnt:")) { batt_count = lowerCmd.substring(6).toInt(); EEPROM.put(ee_b_cnt, batt_count); return; }
  if (lowerCmd.startsWith("b_mah:")) { batt_mah = lowerCmd.substring(6).toInt(); EEPROM.put(ee_b_mah, batt_mah); return; }

  if (lowerCmd.startsWith("cal_p:")) {
    int targetPct = lowerCmd.substring(6).toInt();
    if (targetPct == 0) { v_offset = 0.0; saved_cal_p = 0; } 
    else {
        float expectedV = (batt_type == 1) ? 6.0 + ((8.4 - 6.0) * (targetPct / 100.0)) : 6.4 + ((8.4 - 6.4) * (targetPct / 100.0));
        long rawSum = 0;
        for(int i=0; i<30; i++) rawSum += analogRead(VoltsMeter);
        float currentRawV = ((rawSum / 30.0) * distV) * vDividerRatio; 
        v_offset = expectedV - currentRawV;
        saved_cal_p = targetPct;
    }
    EEPROM.put(ee_v_off, v_offset); EEPROM.put(ee_cal_p, saved_cal_p); 
    
    // INSTANTLY RESET THE RATCHET SO THE MANUAL ENTRY TAKES OVER
    current_pct = -1;
    filtered_v = -1.0;
    return;
  }

  if (lowerCmd == "piwifidown") { hasExternalIP = false; piWifiError = true; scrollPos = 0; return; }
  if (lowerCmd.startsWith("ip:")) { 
    hasExternalIP = true; piWifiError = false; 
    String newIP = cmd.substring(3); newIP.trim();
    if (newIP != fullIP) { fullIP = newIP; scrollPos = 0; }
    return; 
  }
  if (lowerCmd.startsWith("net:")) { networkName = cmd.substring(4); networkName.trim(); return; }
  
  if (lowerCmd == "volt") { 
    Serial.print("Battery: "); Serial.print(getVolts(), 2); 
    Serial.print("V | Pct: "); Serial.println(getBatteryPercentage());
    return; 
  }
  if (lowerCmd == "amp") { Serial.print("Current: "); Serial.print(getAmps(), 2); Serial.println(" A"); return; }
  if (lowerCmd == "s") { applySmartBrakes(); return; }

  char type = lowerCmd.charAt(0);
  int level = lowerCmd.substring(1).toInt();
  if (level == 0) { applySmartBrakes(); return; }
  
  if (type == 'a') { 
    if (millis() - last_rev_time < (unsigned long)(failsafe_sec * 1000.0)) return; 
    last_fwd_time = millis();
    if (level > max_fwd) level = max_fwd;
    lastPulse = map(level, 1, 10, 1550, 2000); 
    isBraked = false; lastCommandTime = millis(); 
  }
else if (type == 'b') { 
    if (millis() - last_fwd_time < (unsigned long)(failsafe_sec * 1000.0)) return; 
    last_rev_time = millis();
    if (level > max_rev) level = max_rev;
    
    // CHANGED: 1355 changed to 1000 to give reverse a full speed range
    lastPulse = map(level, 1, 10, 1400, 1200); 
    
    isBraked = false; lastCommandTime = millis(); 
  }
  myESC.writeMicroseconds(lastPulse);
}

void setup() {
  Serial.begin(115200);
  display.setBrightness(0x0a);
  
  if (EEPROM.read(ee_init) != 99) {
    EEPROM.put(ee_max_f, 1); 
    EEPROM.put(ee_max_r, 1);
    EEPROM.put(ee_v_off, 0.0f); 
    EEPROM.put(ee_cal_p, 0);
    EEPROM.put(ee_b_type, 0); 
    EEPROM.put(ee_fs, 3.0f);
    EEPROM.put(ee_b_cnt, 1);
    EEPROM.put(ee_b_mah, 5800);
    EEPROM.write(ee_init, 99);
  }
  
  EEPROM.get(ee_max_f, max_fwd); EEPROM.get(ee_max_r, max_rev);
  EEPROM.get(ee_v_off, v_offset); EEPROM.get(ee_cal_p, saved_cal_p);
  EEPROM.get(ee_b_type, batt_type); EEPROM.get(ee_fs, failsafe_sec);
  EEPROM.get(ee_b_cnt, batt_count); EEPROM.get(ee_b_mah, batt_mah);
  
  if (max_fwd < 1 || max_fwd > 10) max_fwd = 1;
  if (max_rev < 1 || max_rev > 10) max_rev = 1;
  if (isnan(failsafe_sec) || failsafe_sec < 0.0 || failsafe_sec > 5.0) failsafe_sec = 3.0;
  if (batt_count < 1 || batt_count > 10) batt_count = 1;
  if (batt_mah < 500 || batt_mah > 30000) batt_mah = 5800;
  
  long total = 0;
  for(int i=0; i<300; i++) { total += analogRead(AmpMeter); delay(1); }
  zeroPoint = (total / 300.0) * distV;
  
  myESC.attach(escPin, 1000, 2000);
  myESC.writeMicroseconds(1500); 

  Serial.println("ID:ESP_DRIVE");
}

void loop() {
  if (millis() - lastIPRequest > 2000) { 
    lastIPRequest = millis(); 
    if (!hasExternalIP) Serial.println("IP?"); 
    else Serial.println("NETWORKNAME?");
  }
  
  unsigned long currentModeDuration = 8000; 
  unsigned long ipTime = 0;
  if (displayMode == 0 && hasExternalIP) {
      ipTime = (fullIP.length() + 5) * 600; 
      unsigned long netTime = ((networkName.length() > 0 ? networkName.length() : 2) + 5) * 300; 
      currentModeDuration = ipTime + 1500 + netTime; 
  }

  if (millis() - lastModeSwitch > currentModeDuration) {
    lastModeSwitch = millis();
    displayMode = (displayMode + 1) % 7; 
    display.clear(); scrollPos = 0; subPhase = 0; 
  }

  if (displayMode == 0) {
    if (piWifiError) { scrollText("WIFI DOWN"); } 
    else if (hasExternalIP) {
        unsigned long phase = (millis() - lastModeSwitch);
        if (phase < ipTime) { if (subPhase != 1) { subPhase = 1; scrollPos = 0; display.clear(); } scrollText(fullIP); }
        else if (phase < ipTime + 1500) { if (subPhase != 2) { subPhase = 2; display.clear(); } display.setSegments(msg_CON, 3, 0); }
        else { if (subPhase != 3) { subPhase = 3; scrollPos = 0; display.clear(); } scrollText(networkName.length() > 0 ? networkName : "PI"); }
    } else { runAnimation(); }
  } 
  else if (displayMode == 1) { display.showNumberDecEx((int)(getVolts()*10), 0b01000000, false, 3, 0); display.setSegments(&char_V, 1, 3); } 
  else if (displayMode == 2) { display.showNumberDecEx((int)(getAmps()*10), 0b01000000, false, 3, 0); display.setSegments(&char_A, 1, 3); }
  else if (displayMode == 3) { display.showNumberDec(getBatteryPercentage(), false, 3, 0); display.setSegments(&char_P, 1, 3); }
  else if (displayMode == 4) { display.showNumberDec(max_fwd, false, 2, 2); display.setSegments(&char_F, 1, 0); }
  else if (displayMode == 5) { display.showNumberDec(max_rev, false, 2, 2); display.setSegments(&char_r, 1, 0); }
  else if (displayMode == 6) {
    if (saved_cal_p > 0) { display.showNumberDec(saved_cal_p, false, 3, 0); display.setSegments(&char_C, 1, 3); } 
    else { displayMode = 0; lastModeSwitch = millis() - currentModeDuration; }
  }

  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) { executeCommand(inputString); inputString = ""; }
    } else { inputString += inChar; }
  }

  if (!isBraked && (millis() - lastCommandTime > 600)) { applySmartBrakes(); }
}