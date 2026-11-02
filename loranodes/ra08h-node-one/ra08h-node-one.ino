#include <Arduino.h>

// Pin mapping definitions for ESP32 Hardware Serial 2
#define RX2_PIN 16
#define TX2_PIN 17

// =============================================================================
// VERIFIED TARGET INDUSTRIAL NETWORK CREDENTIALS
// =============================================================================
const char* TARGET_DEVEUI = "70B3D57ED0079C17";
const char* TARGET_APPEUI = "0000000000000000";
const char* TARGET_APPKEY = "A82B8E45A40497999D4981DADA52C5C7";

// Helper function to send AT commands and monitor the incoming string lines
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
  String resp = sendCommandGetResponse("AT", 1000);
  
  // Verify firmware version mapping presence
  Serial.println(F(" Querying internal key structure..."));
  String devEuiCheck = sendCommandGetResponse("AT+CDEVEUI?", 1500);
  String appEuiCheck = sendCommandGetResponse("AT+CAPPEUI?", 1500);
  String appKeyCheck = sendCommandGetResponse("AT+CAPPKEY?", 1500);

  bool needsProvisioning = false;

  // Check if current DevEUI matches the desired target string profile
  if (devEuiCheck.indexOf(TARGET_DEVEUI) == -1) {
    Serial.println(F(" -> Warning: DevEUI mismatch detected. Overwriting..."));
    char cmd[40];
    sprintf(cmd, "AT+CDEVEUI=%s", TARGET_DEVEUI);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }

  // Check if AppEUI matches
  if (appEuiCheck.indexOf(TARGET_APPEUI) == -1) {
    Serial.println(F(" -> Warning: AppEUI mismatch detected. Overwriting..."));
    char cmd[40];
    sprintf(cmd, "AT+CAPPEUI=%s", TARGET_APPEUI);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }

  // Check if AppKey matches
  if (appKeyCheck.indexOf(TARGET_APPKEY) == -1) {
    Serial.println(F(" -> Warning: AppKey mismatch detected. Overwriting..."));
    char cmd[60];
    sprintf(cmd, "AT+CAPPKEY=%s", TARGET_APPKEY);
    sendCommandGetResponse(cmd, 1500);
    needsProvisioning = true;
  }

  // Force system setting configuration locks if changes were made
  if (needsProvisioning) {
    Serial.println(F(" Enforcing EU868 Regional Frequency Parameter Constraints..."));
    sendCommandGetResponse("AT+CFREQBANDMASK=0001", 1000);
    sendCommandGetResponse("AT+CULDLMODE=2", 1000);

    Serial.println(F(" Writing credentials permanently to Non-Volatile Memory (NVM)..."));
    String saveResp = sendCommandGetResponse("AT+CSAVE", 2000);
    
    if (saveResp.indexOf("OK") != -1) {
      Serial.println(F(" >>> SUCCESS: Node successfully provisioned and locked! <<<"));
    } else {
      Serial.println(F(" >>> ERROR: Flash save verification failure. <<<"));
    }
    
    // Reboot the module to execute cleanly on the new profile boundary layout
    sendCommandGetResponse("AT+IREBOOT=0", 2000);
    delay(2000);
  } else {
    Serial.println(F(" >>> NOTICE: Connected node is already perfectly configured. Skipping flash write. <<<"));
  }

  Serial.println(F(" Lowering Radio Spill Envelope (Indoor Safe Mode Enforcement)..."));
  sendCommandGetResponse("AT+CTXP=7", 1000);

  Serial.println(F(" Initializing network join sequence stream..."));
  Serial.println(sendCommandGetResponse("AT+CJOIN=1,0,10,8", 5000));
}

void setup() {
  Serial.begin(115200); // Diagnostic output line to your PC Monitor
  while(!Serial);
  delay(2000);
  
  Serial.println(F("====================================================="));
  Serial.println(F(" ESP32 RA-08H MULTI-NODE MASS PROVISIONING UTILITY   "));
  Serial.println(F("====================================================="));

  // Open low-power UART pipeline communication framework to the node at 9600 baud using Serial2
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  
  // Run provisioning lifecycle
  provisionNode();
}

void loop() {
  // Telemetry loop for regular operational checking
  Serial.println(F("\n--- Broadcasting Sensor Telemetry Packet ---"));
  
  uint16_t voltage  = 330;          
  uint16_t current  = 150;          
  uint16_t luminous = 450;          
  int16_t  tilt     = -15;          

  char hexPayload[20];
  sprintf(hexPayload, "%04X%04X%04X%04X", voltage, current, luminous, (uint16_t)tilt);

  char atBuffer[50];
  sprintf(atBuffer, "AT+DTRX=0,1,8,%s", hexPayload);

  String txLog = sendCommandGetResponse(atBuffer, 5000);
  Serial.println(txLog);

  // Send updates every 60 seconds
  delay(60000);
}
