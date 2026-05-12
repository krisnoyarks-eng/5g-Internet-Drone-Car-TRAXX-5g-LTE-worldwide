# 5g-Internet-Drone-Car-TRAXX-5g-LTE-worldwide
Internet Drone Car 5g LTE worldwide
🛸 ULTIMATE DRONE OS: TECHNICAL & OPERATOR MANUAL
System Version: V1.0 (Headless / Dual-MCU Architecture)
Target Hardware: Raspberry Pi (Master), Arduino Uno (Drive MCU), ESP8266 (Steer MCU)

1. SYSTEM ARCHITECTURE & DESIGN OVERVIEW
The Ultimate Drone OS is a robust, web-based teleoperation platform designed for high-speed RC vehicles. It uses a distributed, three-brain architecture to ensure safety, low latency, and hardware protection.
	•	The Master Brain (Raspberry Pi): Runs a Python Flask web server, processes camera/audio feeds using FFmpeg, handles GPS telemetry, serves the HTML frontend, and routes internet traffic via FRP tunneling.
	•	Drive Controller (Arduino Uno): Dedicated exclusively to handling traction (ESC control via PWM), voltage/current sensor monitoring, ratcheted battery percentage calculations, hardware dead-time failsafes, and the physical TM1637 4-digit display.
	•	Steer Controller (ESP8266): Dedicated to high-speed servo steering, maintaining "Iron Wall" physical travel limits, and enforcing anti-rollover speed constraints based on live GPS data.

2. FIRST-TIME SETUP & WHAT TO EXPECT
Initial Boot Sequence
	1	Supply power to the Raspberry Pi and both microcontrollers.
	2	The Pi takes approximately 45-60 seconds to boot the OS, initialize the Apache proxy, and start the background services.
	3	If the Pi cannot find a known Wi-Fi network within 50 seconds, it will automatically broadcast an emergency fallback hotspot.
	4	Connect your phone or laptop to the Wi-Fi network DRONE_SETUP using the password drone1234.
	5	Navigate your browser to the Pi's IP address (usually http://10.42.0.1 on the hotspot, or its assigned IP on a home network).
First Load Calibration
	•	Legal Waiver: On the very first load, the HUD will prompt a legal waiver acknowledging the dangers of high-speed operation. You must accept this to unlock the UI.
	•	USB Binding: The system will display a loading spinner as it automatically pings /dev/ttyUSB* and /dev/ttyACM* to identify which port is the Uno and which is the ESP8266 based on their hardcoded IDs.
	•	Battery Seed Prompt: The UI will ask for your current battery percentage. Entering this value seeds the "Smart Battery Ratchet" algorithm so it can accurately calculate voltage sag going forward.

3. FILE SYSTEM, DEFAULTS & PASSWORDS
All persistent memory is stored safely on the Pi's filesystem and the EEPROM of the respective microcontrollers.
System Credentials
Credential Type
Default Value
Description
Pi User Login
drone
The primary non-root user running the OS services.
Pi Root Password
drone786@@
Used in the UI for system-level actions (GUI toggles, reboots).
Fallback Wi-Fi AP
drone1234
Network key when router access is unavailable.
Configuration Files & Storage
	•	/home/drone/settings.json: The master configuration file. Saves your limits, center offsets, battery chemistry, network passwords, and UI configurations.
	•	/home/drone/odometer.txt: Stores the persistent total distance driven (in KM).
	•	/home/drone/mp3_horns/: Directory where user-uploaded .mp3 horn sounds are stored.
	•	/home/drone/frpc.ini: Configuration file for the external FRP fleet tunnel.
	•	/home/drone/setup_drone.sh: The master installation bash script.
System Factory Defaults
	•	Drive Failsafe Delay: 3.0 Seconds (prevents stripping gears by instantly slamming forward to reverse).
	•	Steering Limits: Absolute Left: 50.0 / Absolute Right: 130.0 / Center Offset: 90.0.
	•	Battery Configuration: LiPo (2S), 1 Parallel Pack, 5800 mAh.
	•	Fleet Tunnel Port: 8585.

4. HUD BUTTON FUNCTIONS & UI BREAKDOWN
The HUD is designed for mobile touchscreens or desktop mouse usage.
	•	Top Data Bar: Displays Ping (Latency), Live Voltage, Live Amperage (with estimated runtime left based on capacity), Wi-Fi Signal Strength, and Battery Percentage.
	•	Mute Toggle (🔇): Toggles the live stream microphone audio from the drone to your device.
	•	Settings Gear (⚙️): Opens the master configuration modal (detailed below).
	•	Left Joystick (Drive): Two vertical sliders. Push the top slider for Forward. Push the bottom slider for Reverse. Letting go snaps to 0 and triggers the smart brake to halt the motor.
	•	Right Joystick (Steer): A horizontal slider. Slide left to turn wheels left, slide right to turn wheels right. Letting go snaps to 0 and centers the wheels.
	•	🚨 HORN Button: Pressing this triggers the selected MP3 or WAV file over the drone's Bluetooth speaker. Swiping left/right on this button skips to the next/previous horn track. Double-tapping stops audio.
	•	Bottom Data Bar: Displays GPS telemetry: Speed, Odometer, Altitude, Satellites locked, and current Street Name. A mini-map actively tracks your location.

5. SERVER MANAGEMENT & DEBUGGING
The drone runs its core processes in the background using systemd. You can manage these via the in-app Web Terminal (Settings > Terminal) or via SSH.
Master Systemd Services
Service Name
Function
drone-pilot.service
The main Python Flask backend processing serial commands and the UI.
drone-cam.service
The FFmpeg video streaming feed.
drone-audio.service
The ALSA-to-MP3 microphone streaming feed.
drone-ap.service
The emergency hotspot fallback generator.
drone-keepalive.service
Pings Google every 3 seconds to prevent Wi-Fi power-saving sleep.
drone-bt-speaker.service
Auto-connects to a saved Bluetooth speaker on boot.
Important Server Commands
Run these in the Web Terminal or SSH:
	•	Restart Main Server: sudo systemctl restart drone-pilot
	•	Stop Camera (Save CPU): sudo systemctl stop drone-cam
	•	Check Server Logs: journalctl -u drone-pilot -n 50 --no-pager
	•	Check Camera Errors: journalctl -u drone-cam -f
	•	Check USB Connections: dmesg | grep tty
	•	Reboot Drone: sudo reboot

6. TESTING SERIAL COMMANDS VIA SSH (DIRECT MCU COMMUNICATION)
If you need to manually test commands (like setting the max steering walls) directly over SSH without the Python web server interfering, you must bypass the drone-pilot service.
Step 1: Stop the Python Backend
The Python script constantly polls the USB ports. You must stop it so you can take control of the serial line.
sudo systemctl stop drone-pilot
Step 2: Identify Your USB Ports
Find out which port the ESP8266 (Steering) and Uno (Drive) are connected to.
ls /dev/ttyUSB* /dev/ttyACM*
Step 3: Connect via Screen
Use the screen utility to open a direct serial monitor to the ESP8266. Assuming the ESP8266 is on /dev/ttyUSB1:
screen /dev/ttyUSB1 115200
Note: Once inside screen, whatever you type is sent directly to the MCU. Press Enter to send the command.
To exit screen, press Ctrl+A, then press K, then press Y.
Step 4: Examples of Commands to Send via SSH
Once connected to the ESP8266 via screen, you can type these exactly and hit Enter:
	•	Check Identity: PING_ID (It will reply ID:ESP12_STEER)
	•	Check Current Limits: limits (It will print the current EEPROM walls)
	•	Set Absolute Left Wall to 45: x45.0
	•	Set Absolute Right Wall to 135: y135.0
	•	Set Center to 88.5: c88.5
	•	Test Steer Left: l5 (Steers left at 50% power)
	•	Test Snap Center: s
Step 5: Restart the Backend
When you are done testing, restart the system to give control back to the UI.
sudo systemctl start drone-pilot

7. DRIVE MCU (ARDUINO UNO) COMMAND REFERENCE
This is the complete API dictionary for the traction/battery microcontroller. Communication is at 115200 baud.
Commands Sent FROM Pi TO Uno
Command
Action
PING_ID
Requests the hardware identity of the board.
restart
Triggers a hard software reset of the Uno.
runningtime
Requests the MCU uptime in seconds.
limits
Requests the saved EEPROM limits and failsafes.
lim_f:[1-10]
Sets max allowed forward throttle (Saves to EEPROM).
lim_r:[1-10]
Sets max allowed reverse throttle (Saves to EEPROM).
b_cfg:[0/1]
Sets battery chemistry. 0 = LiPo, 1 = NiMH.
fs:[Seconds]
Sets the dead-time delay between directional changes (e.g., fs:3.0).
b_cnt:[Num]
Sets the number of batteries wired in parallel.
b_mah:[Num]
Sets battery capacity per pack in mAh.
cal_p:[0-100]
Calibrates voltage sensor based on known percentage. cal_p:0 resets it.
ip:[IP]
Sends the Pi's local IP to be scrolled on the physical TM1637 display.
net:[SSID]
Sends the Wi-Fi network name to the TM1637 display.
piwifidown
Tells Uno the Pi lost connection; displays "WIFI DOWN" on the screen.
volt
Polls current voltage and the calculated ratcheted percentage.
amp
Polls current amperage draw.
s or s0
Applies "Smart Brakes" (brief opposite ESC pulse to halt motor).
a[1-10]
Drive FORWARD at power level 1 through 10 (e.g., a5).
b[1-10]
Drive REVERSE at power level 1 through 10 (e.g., b10).
Responses Sent FROM Uno TO Pi
Output
Meaning
ID:ESP_DRIVE
Identification handshake.
REBOOTING UNO...
Acknowledgment of reset command.
UPTIME_SECONDS:[Num]
Time since MCU boot.
Limits - FWD: X | REV: X | CALP: X | BT: X | FS: X
Current operating parameters feedback.
Battery: [V]V | Pct: [%]
Live battery telemetry.
Current: [A] A
Live amp draw.

8. STEER MCU (ESP8266) COMMAND REFERENCE
This is the complete API dictionary for the steering microcontroller.
Commands Sent FROM Pi TO ESP8266
Command
Action
PING_ID
Requests the hardware identity.
restart
Soft resets the ESP8266.
runningtime
Requests the MCU uptime (adjusted for soft reboots).
limits
Requests the Iron Wall EEPROM limits and center point.
k[Float]
Live telemetry: Current GPS speed in KM/H (used for anti-rollover logic).
t[Float]
Sets Auto-Center failsafe timeout in seconds (e.g., t1.5).
R[0/1]
Toggles Racing Mode. R1 restricts steering angle at high GPS speeds.
x[Angle]
Sets the Absolute Left Physical Wall (e.g., x55.0). Saves to EEPROM.
y[Angle]
Sets the Absolute Right Physical Wall (e.g., y125.0). Saves to EEPROM.
c[Angle]
Sets the mechanical center trim (e.g., c92.5).
a[Angle]
Commands servo to an absolute, specific angle bypassing joystick math.
l[1-10]
Steer LEFT from center at a magnitude of 1 to 10 (e.g., l5).
r[1-10]
Steer RIGHT from center at a magnitude of 1 to 10 (e.g., r10).
s
Snap steering immediately to exact center.
Responses Sent FROM ESP8266 TO Pi
Output
Meaning
ID:ESP12_STEER
Identification handshake.
REBOOTING ESP...
Acknowledgment of reset.
UPTIME_SECONDS:[Num]
Time since MCU boot.
LEFT WALL: [X] | RIGHT WALL: [X]
Feedback of EEPROM hard limits.
CENTER: [X]
Feedback of current trim offset.

9. TROUBLESHOOTING GUIDE
Problem: The HUD loads, but Drive/Steer says "Disconnected".
	•	Cause: The Python backend cannot read the USB ports, or the hardware disconnected.
	•	Solution: Open Settings > Microcontrollers > Click "FORCE RE-SCAN USBs". If this fails, open the Web Terminal and run dmesg | grep tty to see if the USB cable vibrated loose.
Problem: Steering is twitching or acting erratic.
	•	Cause: The ESP8266 is browning out due to the servo drawing too much power, or the auto-center failsafe is fighting your commands.
	•	Solution: Go to Settings > Microcontrollers and set "Centering Time" to 0.0s to test if the failsafe is triggering prematurely. Ensure the servo is powered by a dedicated BEC, not the ESP8266 5V pin.
Problem: Drone instantly drops to 0% battery when throttling up.
	•	Cause: Voltage sag is dipping below the limit, and your Battery Settings are incorrect.
	•	Solution: Go to Settings > Battery. Ensure your parallel packs and mAh are correctly entered. The system calculates sag via (Amps / Pack Count) * 0.015. If the total mAh is set too low, the math assumes massive failure.
Problem: Camera feed is a black screen, but HUD loads.
	•	Cause: FFmpeg crashed or the Logitech camera disconnected.
	•	Solution: Open the Web Terminal and run sudo systemctl restart drone-cam. Refresh the browser after 5 seconds.
Problem: Reverse doesn't work immediately after driving forward.
	•	Cause: This is a safety feature to prevent gear shredding.
	•	Solution: The system enforces a default 3.0-second lockout between direction changes. To disable this (Not Recommended), go to Settings > Microcontrollers > Failsafe Seconds, and set it to 0.

🧠 PART 1: DRIVE CONTROLLER (ARDUINO UNO)
The Arduino Uno is strictly responsible for handling the electronic speed controller (ESC), monitoring power consumption (Voltage/Amperage), driving the physical TM1637 LCD display, and managing hardware-level failsafes (like preventing the gears from stripping when shifting from forward to reverse).
1. Hardware & Pin Definitions
	•	Pins 2 & 3: Connected to the TM1637 4-digit display (Clock and Data).
	•	Analog A0: Connected to a Current (Amperage) sensor.
	•	Analog A1: Connected to a Voltage sensor via a voltage divider.
	•	Pin 9: Generates the PWM signal for the motor's Electronic Speed Controller (ESC).
2. Core Measurement Functions
	•	getVolts()
	◦	What it does: Reads the raw analog value from pin A1 30 times and averages it to smooth out electrical noise.
	◦	How it works: It multiplies the raw value by the sensor's distance multiplier (distV) and the hardware voltage divider ratio (vDividerRatio). It then adds the software calibration offset (v_offset) to ensure hyper-accurate readings.
	•	getAmps()
	◦	What it does: Calculates the live current draw of the motors.
	◦	How it works: It takes 100 rapid samples from pin A0. It compares this against the zeroPoint (calibrated at boot). It calculates the difference, applies the sensor's sensitivity multiplier, and finally passes the result through an aggressive Low-Pass Filter ((currentAmps * 0.1) + (filteredAmps * 0.9)). This prevents the amperage spikes from looking jittery on the HUD.
	•	getBatteryPercentage()
	◦	What it does: Calculates a highly accurate, sag-corrected battery percentage that doesn't "bounce" around when driving.
	◦	How it works (The Ratchet Algorithm): 1. It calculates Voltage Sag by looking at the current Amps drawn divided by the number of parallel batteries. 2. It applies a heavy smoothing filter to the voltage to ignore sudden throttle punches. 3. It maps the voltage to a 0–100 scale depending on if you selected LiPo or NiMH in the settings. 4. The Ratchet: It uses a variable called current_pct. The code states: if (calc_pct < current_pct) current_pct = calc_pct;. This means the battery percentage on the HUD can only go down, never up. This prevents the battery from dropping to 40% under hard acceleration and bouncing back to 80% when you stop.
3. Display & Animation Functions
	•	scrollText(String text)
	◦	What it does: Creates a scrolling marquee effect on the 4-digit TM1637 display.
	◦	How it works: It pads the target text with spaces, converts standard characters into 7-segment hex codes, and shifts them across the 4 digits every 300 milliseconds. Used for displaying the IP address and Wi-Fi network name.
	•	runAnimation()
	◦	What it does: Creates a circular spinning "loading" animation on the display when the drone has no Wi-Fi or IP address.
4. Motor Control & Safety
	•	applySmartBrakes()
	◦	What it does: Stops the drone rapidly instead of letting it coast.
	◦	How it works: Standard ESCs coast when returning to neutral (1500ms pulse). If the drone was going forward (pulse > 1505), this function instantly blasts a full-reverse pulse (1000ms) for 350 milliseconds to brake the motor, then snaps back to neutral (1500ms).
	•	executeCommand(String cmd)
	◦	What it does: The master "brain" of the Uno. It interprets the text commands sent over USB from the Raspberry Pi.
	◦	Settings Commands: Commands like lim_f:, lim_r:, and fs: update the variables in the active memory and immediately write them to the physical EEPROM so they survive power losses.
	◦	Calibration (cal_p:): If you send a known battery percentage, it calculates the difference between the physical pin reading and the expected voltage, saving an offset to EEPROM.
	◦	Drive Commands (a and b): * a is forward, b is reverse.
	▪	It receives a power level (1-10). It maps this to the ESC pulse ranges (1550-2000 for forward, 1400-1200 for reverse).
	▪	Failsafe Lockout: Before executing a direction change, it checks millis() - last_rev_time < failsafe_sec. If you try to switch directions faster than the allowed failsafe time, it ignores the command, saving your transmission gears.
5. Main Loops
	•	setup()
	◦	Initializes the display. Checks EEPROM address 22. If it is not 99, it assumes this is a factory-new Arduino and formats the EEPROM with safe default values (e.g., Failsafe = 3.0s, Max Speed = 1). Calculates the Amp meter zero-point while the motor is off. Attaches the ESC to pin 9.
	•	loop()
	◦	Phase Timer: Cycles the physical LCD display through different "modes" (IP Address -> Voltage -> Amperage -> Battery % -> Max Speeds) using a timing algorithm.
	◦	Serial Listener: Listens to the USB port char-by-char, builds a String, and feeds it to executeCommand() when it detects a newline \n.

⚙️ PART 2: STEER CONTROLLER (ESP8266)
The ESP8266 is explicitly tasked with steering. Because steering servos draw massive current spikes and require hyper-fast update rates, offloading this from the Arduino prevents brown-outs and keeps steering latency extremely low.
1. Hardware & Core Variables
	•	Pin 4 (D2): The PWM output pin driving the steering servo.
	•	ABS_MIN_LEFT & ABS_MAX_RIGHT: The "Iron Walls". The exact angles where your physical steering rack maxes out before the servo binds and burns out.
	•	currentCenter: The trim offset to ensure the car drives perfectly straight.
2. EEPROM Memory Functions
	•	saveToEEPROM() / loadFromEEPROM()
	◦	What it does: Saves and loads the Iron Walls (Left and Right limits) to the ESP's flash memory.
	◦	How it works: It uses EEPROM.put to store the floats. In loadFromEEPROM, it validates the data. If the saved data is corrupted (e.g., less than 50 or greater than 130), it ignores it and uses the hardcoded safe values.
3. The Core Safety Function
	•	setPreciseAngle(float angle)
	◦	What it does: The ultimate hardware gatekeeper. No matter what the Pi or the user requests, this function decides where the servo actually goes.
	◦	How it works: 1. It compares the requested angle against ABS_MIN_LEFT and ABS_MAX_RIGHT. If the request exceeds these walls, it clamps the value. 2. It has a hardcoded secondary clamp (50.0 and 130.0) to prevent EEPROM corruption from destroying the servo. 3. It converts the 0-180 degree angle into precise Microsecond pulses (544 + (int)(angle * 10.3111)) for high-resolution servo control.
4. Main Loops & Algorithms
	•	setup()
	◦	Starts the serial port, loads memory, attaches the servo, sets it to dead-center, and records the exact bootTime to keep uptime reporting accurate even if the chip restarts itself.
	•	loop()
	◦	Auto-Center Failsafe: Continuously monitors lastDriveCmd. If the Pi stops sending commands (due to Wi-Fi drop or software crash) for longer than autoCenterSec, it forces the steering back to currentCenter so the drone doesn't drive in endless circles.
	◦	Serial Parser: Listens for commands from the Pi.
	▪	k[Speed]: The Pi continuously feeds the ESP the live GPS speed in KM/H.
	▪	x, y, c: Adjusts the Left Wall, Right Wall, and Center Trim mathematically.
	◦	Drive Logic (l and r): * The Pi sends a requested turn magnitude from 1 to 10 (e.g., l5 for 50% left).
	▪	The ESP calculates the maximum physical throw available (currentCenter - ABS_MIN_LEFT).
	▪	It maps the 1-10 request perfectly within that physical throw.
	◦	Anti-Rollover Algorithm (Racing Mode):
	▪	If Racing Mode is enabled (R1), the ESP intercepts the steering command before sending it to the servo.
	▪	It looks at the live GPS speed (currentSpeedKmh).
	▪	If speed > 5 km/h, max turn is artificially limited to 80%.
	▪	If speed > 15 km/h, max turn is limited to 35%.
	▪	If speed > 25 km/h, max turn is limited to 20%.
	▪	Result: At high speeds, even if the user violently jerks the joystick 100% to the left, the ESP will only slightly turn the wheels, physically preventing the car from flipping over.

