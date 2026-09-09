/*
 * =====================================================================================
 * TRAPNET - Edge-AI Physical Intrusion Response System (ESP32 Master Firmware)
 * Hardware Target: ESP32 DevKit V1 (30-Pin WROOM-32)
 * Environment: Arduino IDE 2.x / PlatformIO
 * 
 * Communication Topology:
 * - Serial (USB-OTG @ 115200 baud) <-> Android Mobile Phone (YOLOv8 Edge AI Node)
 * - Serial2 (GPIO16 RX / GPIO17 TX @ 256000 baud) <-> HLK-LD2410C 24GHz mmWave Radar
 * - SoftwareSerial (GPIO26 RX / GPIO27 TX @ 9600 baud) <-> SIM800L GSM Module
 * - Serial1 (GPIO35 RX / GPIO25 TX @ 9600 baud) <-> NEO-6M GPS Module
 * - I2C (GPIO21 SDA / GPIO22 SCL) <-> 0.96" SSD1306 OLED Display
 * - SPI (GPIO18 SCK, GPIO19 MISO, GPIO23 MOSI, GPIO5 CS) <-> MicroSD Card Module
 * - Digital Outputs: GPIO32 (Relay 1 - Solenoid Lock), GPIO33 (Relay 2 - Fog Generator)
 * - Digital Inputs: GPIO4 (Door IR), GPIO13 (Window IR), GPIO34 (Vib #1), GPIO39 (Vib #2)
 * =====================================================================================
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>

// -------------------------------------------------------------------------------------
// PIN DEFINITIONS
// -------------------------------------------------------------------------------------
// Relays (Active-LOW)
#define RELAY1_SOLENOID_PIN  32
#define RELAY2_FOG_PIN       33

// Perimeter Sensors
#define IR_DOOR_PIN          4
#define IR_WINDOW_PIN        13
#define VIBRATION1_PIN       34  // Input-Only (Requires 10k pull-down)
#define VIBRATION2_PIN       39  // Input-Only (Requires 10k pull-down)
#define PANIC_BUTTON_PIN     15  // Pulled-UP

// mmWave Radar UART2
#define RADAR_RX_PIN         16
#define RADAR_TX_PIN         17

// GSM SoftwareSerial
#define GSM_RX_PIN           26
#define GSM_TX_PIN           27

// GPS UART1
#define GPS_RX_PIN           35
#define GPS_TX_PIN           25

// OLED I2C & MicroSD SPI
#define OLED_SDA_PIN         21
#define OLED_SCL_PIN         22
#define SD_CS_PIN            5

#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        64

// -------------------------------------------------------------------------------------
// GLOBAL OBJECTS & VARIABLES
// -------------------------------------------------------------------------------------
HardwareSerial RadarSerial(2);  // UART2
HardwareSerial GpsSerial(1);    // UART1
SoftwareSerial GsmSerial(GSM_RX_PIN, GSM_TX_PIN);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensor States
uint16_t radarDistanceCm = 0;
uint8_t  radarSpeedKmh   = 0;
bool     radarMoving     = false;
bool     irDoorTriggered = false;
bool     irWindowTriggered = false;
bool     vib1Triggered   = false;
bool     vib2Triggered   = false;

// System Timers
unsigned long lastRadarPoll = 0;
unsigned long lastTelemetrySend = 0;
bool sdAvailable = false;

// Packet Protocol Definitions
const uint8_t HEADER_ESP32_TO_PHONE = 0xAA;
const uint8_t HEADER_PHONE_TO_ESP32 = 0xAF;

// -------------------------------------------------------------------------------------
// FUNCTION PROTOTYPES
// -------------------------------------------------------------------------------------
void initPerimeterSensors();
void initRelays();
void initOLED();
void initMicroSD();
void initGSM();
void readRadarData();
void readPerimeterSensors();
void sendPhoneTelemetryPacket();
void processPhoneCommands();
void triggerActuators(uint8_t relayMask);
void sendSMSAlert(String message);
void updateOLEDDisplay(String statusText);

// -------------------------------------------------------------------------------------
// SETUP FUNCTION
// -------------------------------------------------------------------------------------
void setup() {
    // 1. USB Serial to Mobile Phone (CDC-ACM / CP2102)
    Serial.begin(115200);
    delay(500);
    Serial.println(F("[TRAPNET] ESP32 System Booting..."));

    // 2. Hardware Relays Setup
    initRelays();

    // 3. Perimeter Input Sensors
    initPerimeterSensors();

    // 4. OLED Display Setup
    initOLED();
    updateOLEDDisplay("Booting System...");

    // 5. Radar Serial Setup (256000 baud)
    RadarSerial.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
    Serial.println(F("[TRAPNET] Radar Serial Initialized @ 256000 baud"));

    // 6. GSM Setup (9600 baud)
    GsmSerial.begin(9600);
    initGSM();

    // 7. GPS Setup (9600 baud)
    GpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // 8. MicroSD Card Initialization
    initMicroSD();

    updateOLEDDisplay("TRAPNET READY");
    Serial.println(F("[TRAPNET] All Systems Operational. Standing by for Phone & Radar data."));
}

// -------------------------------------------------------------------------------------
// MAIN LOOP FUNCTION
// -------------------------------------------------------------------------------------
void loop() {
    // 1. Read Sensors
    readPerimeterSensors();
    readRadarData();

    // 2. Transmit Telemetry Packet to Mobile Phone every 50ms (20 Hz rate)
    if (millis() - lastTelemetrySend >= 50) {
        lastTelemetrySend = millis();
        sendPhoneTelemetryPacket();
    }

    // 3. Check for Incoming Active Response Commands from Mobile Phone (YOLOv8 Triggers)
    processPhoneCommands();

    // 4. Update Status Display every 500ms
    static unsigned long lastOledUpdate = 0;
    if (millis() - lastOledUpdate >= 500) {
        lastOledUpdate = millis();
        String statusMsg = "RADAR: " + String(radarDistanceCm) + "cm";
        updateOLEDDisplay(statusMsg);
    }
}

// -------------------------------------------------------------------------------------
// HARDWARE INITIALIZATION HELPERS
// -------------------------------------------------------------------------------------
void initRelays() {
    pinMode(RELAY1_SOLENOID_PIN, OUTPUT);
    pinMode(RELAY2_FOG_PIN, OUTPUT);
    
    // Active-LOW Relays: HIGH = Deactivated (Safe)
    digitalWrite(RELAY1_SOLENOID_PIN, HIGH);
    digitalWrite(RELAY2_FOG_PIN, HIGH);
}

void initPerimeterSensors() {
    pinMode(IR_DOOR_PIN, INPUT_PULLUP);
    pinMode(IR_WINDOW_PIN, INPUT_PULLUP);
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
    
    // Pins 34 and 39 are Input-Only (Requires external 10k resistor)
    pinMode(VIBRATION1_PIN, INPUT);
    pinMode(VIBRATION2_PIN, INPUT);
}

void initOLED() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("[TRAPNET] WARNING: OLED Allocation Failed"));
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(F("TRAPNET ESP32 v2.0"));
        display.display();
    }
}

void initMicroSD() {
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(F("[TRAPNET] WARNING: MicroSD Card Mount Failed"));
        sdAvailable = false;
    } else {
        Serial.println(F("[TRAPNET] MicroSD Card Mounted Successfully"));
        sdAvailable = true;
    }
}

void initGSM() {
    delay(1000);
    GsmSerial.println("AT");
    delay(200);
    GsmSerial.println("AT+CMGF=1"); // Set SMS text mode
    delay(200);
    Serial.println(F("[TRAPNET] GSM Module Ready."));
}

// -------------------------------------------------------------------------------------
// SENSOR READING & PARSING
// -------------------------------------------------------------------------------------
void readPerimeterSensors() {
    // IR Sensors are Active-LOW (LOW when beam is broken)
    irDoorTriggered   = (digitalRead(IR_DOOR_PIN) == LOW);
    irWindowTriggered = (digitalRead(IR_WINDOW_PIN) == LOW);
    
    // Vibration Sensors are HIGH on motion pulse
    vib1Triggered = (digitalRead(VIBRATION1_PIN) == HIGH);
    vib2Triggered = (digitalRead(VIBRATION2_PIN) == HIGH);
}

void readRadarData() {
    // Parse HLK-LD2410C 24GHz mmWave Binary Packets
    while (RadarSerial.available() >= 8) {
        if (RadarSerial.read() == 0xF4 && RadarSerial.read() == 0xF3) {
            uint8_t payload[6];
            RadarSerial.readBytes(payload, 6);
            
            // Extract Distance & Motion Energy Gates
            radarMoving = payload[0];
            radarDistanceCm = payload[1] | (payload[2] << 8);
            radarSpeedKmh   = payload[3];
        }
    }
}

// -------------------------------------------------------------------------------------
// MOBILE PHONE PACKET COMMUNICATION (USB-OTG / SERIAL)
// -------------------------------------------------------------------------------------
void sendPhoneTelemetryPacket() {
    uint8_t packet[8];
    packet[0] = HEADER_ESP32_TO_PHONE;
    packet[1] = 0x01; // Telemetry Type
    packet[2] = (radarDistanceCm >> 8) & 0xFF;
    packet[3] = radarDistanceCm & 0xFF;
    packet[4] = radarSpeedKmh;
    
    // Pack Digital Sensor Flags into Bitmask
    uint8_t flags = 0;
    if (irDoorTriggered)   flags |= (1 << 0);
    if (irWindowTriggered) flags |= (1 << 1);
    if (vib1Triggered)     flags |= (1 << 2);
    if (vib2Triggered)     flags |= (1 << 3);
    packet[5] = flags;
    packet[6] = radarMoving ? 0x01 : 0x00;
    
    // XOR Checksum
    uint8_t checksum = 0;
    for (int i = 0; i < 7; i++) {
        checksum ^= packet[i];
    }
    packet[7] = checksum;

    // Send 8-byte Binary Packet over USB Serial to Mobile Phone
    Serial.write(packet, 8);
}

void processPhoneCommands() {
    // Read Command Byte Stream from Mobile Phone
    if (Serial.available() >= 4) {
        if (Serial.peek() == HEADER_PHONE_TO_ESP32) {
            uint8_t cmdPacket[4];
            Serial.readBytes(cmdPacket, 4);
            
            // Verify Checksum
            uint8_t expectedChecksum = cmdPacket[0] ^ cmdPacket[1] ^ cmdPacket[2];
            if (cmdPacket[3] == expectedChecksum) {
                uint8_t relayMask = cmdPacket[2];
                Serial.print(F("[TRAPNET] Threat Confirmed by YOLOv8! Relay Mask: 0x"));
                Serial.println(relayMask, HEX);
                
                triggerActuators(relayMask);
            }
        } else {
            Serial.read(); // Discard invalid header byte
        }
    }
}

// -------------------------------------------------------------------------------------
// ACTUATORS & ALERTS
// -------------------------------------------------------------------------------------
void triggerActuators(uint8_t relayMask) {
    // relayMask: Bit 0 = Solenoid Lock (GPIO32), Bit 1 = Fog Generator (GPIO33)
    if (relayMask & 0x01) {
        digitalWrite(RELAY1_SOLENOID_PIN, LOW); // Engage Lock
        Serial.println(F("[ACTUATOR] Solenoid Door Lock ENGAGED!"));
    }
    if (relayMask & 0x02) {
        digitalWrite(RELAY2_FOG_PIN, LOW); // Trigger Fog
        Serial.println(F("[ACTUATOR] Fog Generator RELEASED!"));
    }

    updateOLEDDisplay("INTRUDER TRAPPED!");
    
    // Fallback SMS alert
    sendSMSAlert("CRITICAL ALERT: Intruder Confirmed by YOLOv8 & Radar. Solenoid & Fog Engaged!");
}

void sendSMSAlert(String message) {
    GsmSerial.println("AT+CMGS=\"+919876543210\""); // Replace with target phone number
    delay(500);
    GsmSerial.print(message);
    delay(500);
    GsmSerial.write(26); // Ctrl+Z to send
    delay(500);
    Serial.println(F("[GSM] SMS Telemetry Dispatched."));
}

void updateOLEDDisplay(String statusText) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("== TRAPNET EDGE AI =="));
    display.println(F("---------------------"));
    display.setCursor(0, 20);
    display.println(statusText);
    display.setCursor(0, 45);
    display.print(F("IR Door: "));
    display.println(irDoorTriggered ? "TRIG" : "OK");
    display.display();
}
