#include <RadioLib.h>
#include <WiFi.h>
#include <SPI.h>

// -----------------------------------------------------------------------------
// HARDWARE SPI & RADIO PIN CONFIGURATION (TTGO LoRa32 Layout)
// -----------------------------------------------------------------------------
#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_NSS    18
#define LORA_RST    14
#define LORA_DIO0   26
#define LORA_DIO1   33

SPIClass loraSPI(VSPI);
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1, loraSPI);

// VERIFIED OPERATIONAL REGION ROUTING POINTER
LoRaWANNode node(&radio, &EU868);

// -----------------------------------------------------------------------------
// VERIFIED CRYPTOGRAPHIC KEYS (MSB FORMAT)
// -----------------------------------------------------------------------------
uint64_t joinEUI = 0x0000000000000000;
uint64_t devEUI  = 0x70B3D57ED0079C15;

uint8_t appKey[]  = { 0x8F, 0x16, 0xC8, 0x54, 0x6A, 0x3F, 0x7E, 0xE8, 0x4A, 0x37, 0x57, 0x9F, 0x51, 0xAE, 0x09, 0xDD };
uint8_t nwkKey[]  = { 0x8F, 0x16, 0xC8, 0x54, 0x6A, 0x3F, 0x7E, 0xE8, 0x4A, 0x37, 0x57, 0x9F, 0x51, 0xAE, 0x09, 0xDD };

void setup() {
  Serial.begin(115200);
  while(!Serial);
  delay(1000);
  
  Serial.println(F("========================================"));
  Serial.println(F("RadioLib TTGO EU868 Node - ONLINE"));
  Serial.println(F("========================================"));

  WiFi.mode(WIFI_OFF);
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  if (radio.begin() != RADIOLIB_ERR_NONE) {
    while (true);
  }

  node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  
  Serial.print(F("Activating session via OTAA handshake... "));
  int state = node.activateOTAA();
  
  if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println(F("SUCCESS! Device has joined TTN!"));
  } else {
    Serial.print(F("Join Failed, code: "));
    Serial.println(state);
  }
}

void loop() {
  if(node.isActivated()) {
    Serial.print(F("Sending verification telemetry packet... "));
    
    // 1. Generate Gimmick Sensor Telemetry Values
    // Multiplied by scale factors to turn decimals into integers for compact transfer
    uint16_t voltage  = 330;          // e.g., 3.30V (scaled x100)
    uint16_t current  = 150;          // e.g., 150mA 
    uint16_t luminous = 450;          // e.g., 450 Lux
    int16_t  tilt     = -15;          // e.g., -15 degrees (signed for direction)

    // 2. Pack data into an 8-byte array (MSB / Big-Endian)
    uint8_t telemetryData[8];
    
    telemetryData[0] = (voltage >> 8) & 0xFF;
    telemetryData[1] = voltage & 0xFF;
    
    telemetryData[2] = (current >> 8) & 0xFF;
    telemetryData[3] = current & 0xFF;
    
    telemetryData[4] = (luminous >> 8) & 0xFF;
    telemetryData[5] = luminous & 0xFF;
    
    telemetryData[6] = (tilt >> 8) & 0xFF;
    telemetryData[7] = tilt & 0xFF;

    // Print values locally for verification
    Serial.print(F("[V: ")); Serial.print(voltage);
    Serial.print(F(", I: ")); Serial.print(current);
    Serial.print(F(", L: ")); Serial.print(luminous);
    Serial.print(F(", T: ")); Serial.print(tilt);
    Serial.println(F("]"));
    
    // Parameter 3: Port = 1
    // Parameter 4: Is confirmed? = false
    int state = node.sendReceive(telemetryData, sizeof(telemetryData), 1, false);
    
    // Simplified clean status validation check
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("Uplink successful (Sent to Gateway)!"));
    } else {
      Serial.print(F("Uplink state update, code: "));
      Serial.println(state);
    }
  } else {
    Serial.println(F("Session broken. Attempting reconnection..."));
    node.activateOTAA();
  }

  // Sends packet every 60 seconds
  delay(60000);
}
