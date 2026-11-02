//==============================RA08-H-Node-Tw0============================================
#include <Arduino.h>
#include <Wire.h>
#include <PZEM004Tv30.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <hp_BH1750.h>

// Pin mapping definitions for ESP32
#define RX2_PIN 16 // Ra-08H RX
#define TX2_PIN 17 // Ra-08H TX
#define RX1_PIN 32 // PZEM TX
#define TX1_PIN 33 // PZEM RX

// Sensor Instances
PZEM004Tv30 pzem(Serial1, RX1_PIN, TX1_PIN);
Adafruit_MPU6050 mpu;
hp_BH1750 bh1750;

// VERIFIED TARGET INDUSTRIAL NETWORK CREDENTIALS
const char* TARGET_DEVEUI = "70B3D57ED0079C36";
const char* TARGET_APPEUI = "0000000000000000";
const char* TARGET_APPKEY = "064D30E5977AE829F9A933A1AC579F80";

String sendCommandGetResponse(const char* cmd, unsigned long timeoutMs = 2000) {
  String response = "";
  Serial2.print(cmd);
  Serial2.print("\r\n");
  
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
  }
  return response;
}

void provisionNode() {
  Serial.println(F("\n Pinging current LoRa module..."));
  sendCommandGetResponse("AT", 1000);
  
  String devEuiCheck = sendCommandGetResponse("AT+CDEVEUI?", 1500);
  String appEuiCheck = sendCommandGetResponse("AT+CAPPEUI?", 1500);
  String appKeyCheck = sendCommandGetResponse("AT+CAPPKEY?", 1500);

  bool needsProvisioning = false;
  char cmd[100]; // Single resilient buffer for dynamic command building

  if (devEuiCheck.indexOf(TARGET_DEVEUI) == -1) {
    sprintf(cmd, "AT+CDEVEUI=%s", TARGET_DEVEUI);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }
  if (appEuiCheck.indexOf(TARGET_APPEUI) == -1) {
    sprintf(cmd, "AT+CAPPEUI=%s", TARGET_APPEUI);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }
  if (appKeyCheck.indexOf(TARGET_APPKEY) == -1) {
    sprintf(cmd, "AT+CAPPKEY=%s", TARGET_APPKEY);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }

  if (needsProvisioning) {
    Serial.println(F(" Enforcing Regional Frequency Parameter Constraints..."));
    sendCommandGetResponse("AT+CFREQBANDMASK=0001", 1000);
    sendCommandGetResponse("AT+CULDLMODE=2", 1000);

    Serial.println(F(" Writing credentials permanently to NVM..."));
    String saveResp = sendCommandGetResponse("AT+CSAVE", 2000);
    
    if (saveResp.indexOf("OK") != -1) {
      Serial.println(F(" >>> SUCCESS: Node successfully provisioned! <<<"));
    } else {
      Serial.println(F(" >>> ERROR: Flash save verification failure. <<<"));
    }
    sendCommandGetResponse("AT+IREBOOT=0", 2000);
    delay(2000);
  } else {
    Serial.println(F(" >>> NOTICE: Connected node already configured. Skipping flash write. <<<"));
  }

  sendCommandGetResponse("AT+CTXP=7", 1000);
  Serial.println(F(" Initializing network join sequence stream..."));
  Serial.println(sendCommandGetResponse("AT+CJOIN=1,0,10,8", 5000));
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  delay(2000);
  
  Serial.println(F("====================================================="));
  Serial.println(F("      ESP32 TELEMETRY PROVISIONING UTILITY           "));
  Serial.println(F("====================================================="));

  // Init I2C
  Wire.begin(21, 22);

  // Init MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  }

  // Init BH1750
  bh1750.begin(BH1750_TO_GROUND);

  // Init Serial Pipelines
  Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN); // *** FIX: Initialize PZEM pipeline
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN); // Initialize Ra-08H pipeline
  
  provisionNode();
}

void loop() {
  Serial.println(F("\n--- Fetching Real Sensor Data & Telemetry Packet ---"));

  // 1. Read PZEM-004T
  float pzemVoltage = pzem.voltage();
  float pzemCurrent = pzem.current();
  float pzemPower   = pzem.power();
  float pzemEnergy  = pzem.energy();
  float pzemFreq    = pzem.frequency();
  float pzemPf      = pzem.pf();

  // Fallbacks for disconnected AC lines
  if (isnan(pzemVoltage)) pzemVoltage = 0.0;
  if (isnan(pzemCurrent)) pzemCurrent = 0.0;
  if (isnan(pzemPower))   pzemPower   = 0.0;
  if (isnan(pzemEnergy))  pzemEnergy  = 0.0;
  if (isnan(pzemFreq))    pzemFreq    = 0.0;
  if (isnan(pzemPf))      pzemPf      = 0.0;

  // Print local diagnostics to USB Serial Monitor
  Serial.printf("V: %.1fV | A: %.3fA | W: %.1fW | kWh: %.3fkWh | Hz: %.1fHz | PF: %.2f\n", 
                pzemVoltage, pzemCurrent, pzemPower, pzemEnergy, pzemFreq, pzemPf);

  // 2. Read BH1750
  bh1750.start();
  float lux = bh1750.getLux();
  if (lux < 0) lux = 0.0;
  Serial.printf("Light: %.2f lx\n", lux);

  // 3. Read MPU6050 
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float tiltDeg = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
  Serial.printf("Tilt: %.2f°\n", tiltDeg);

  // 4. Scale parameters into fixed integer representations
  uint16_t encVoltage = (uint16_t)(pzemVoltage * 10);  
  uint32_t encCurrent = (uint32_t)(pzemCurrent * 1000); 
  uint32_t encPower   = (uint32_t)(pzemPower * 10);    
  uint32_t encEnergy  = (uint32_t)(pzemEnergy * 1000); 
  uint16_t encFreq    = (uint16_t)(pzemFreq * 10);     
  uint16_t encPf      = (uint16_t)(pzemPf * 100);     
  uint32_t encLux     = (uint32_t)(lux * 100);         
  int16_t  encTilt    = (int16_t)(tiltDeg * 100);      

  // 5. Construct Compact Binary Payload (24 Bytes -> 48 Hex characters)
  char hexPayload[64]; // *** FIX: Safe expanded buffer bounds
  sprintf(hexPayload, "%04X%08X%08X%08X%04X%04X%08X%04X", 
          encVoltage, encCurrent, encPower, encEnergy, encFreq, encPf, encLux, (uint16_t)encTilt);

  char atBuffer[100]; // *** FIX: Safe expanded buffer bounds
  sprintf(atBuffer, "AT+DTRX=0,1,24,%s", hexPayload);

  Serial.print(F("Sending AT Command: "));
  Serial.println(atBuffer);

  String txLog = sendCommandGetResponse(atBuffer, 5000);
  Serial.println(txLog);

  delay(60000);
}

