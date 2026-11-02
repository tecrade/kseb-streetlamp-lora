# Ra-08H LoRaWAN Module Configuration Guide

This guide provides step-by-step instructions to flash the official EU868 firmware onto the **Ai-Thinker Ra-08H LoRaWAN module** and configure it as an **OTAA End Node** using the ThinkNode Network Console.

---

## 🛠️ Prerequisites & Environment Setup

Before starting, ensure your development environment has the necessary permissions and Python dependencies installed to communicate with the hardware over serial.

### 1. Identify Your Serial Port
Plug your USB-to-UART bridge (or development board) into your Linux machine and run:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```
*Note down the correct interface (e.g., `/dev/ttyUSB0`).*

### 2. Grant Serial Port Permissions
To access serial interfaces without root privileges, add your user account to the `dialout` group:
```bash
sudo usermod -aG dialout \$USER
```
*(You may need to log out and log back in for this change to take effect).*

### 3. Install Core Dependencies
Install Python 3, `pip`, and the `pyserial` extension required by the Ai-Thinker flashing script:
```bash
sudo dnf install python3 python3-pip -y
pip3 install pyserial
```

---

## 💾 Firmware Preparation & Flashing

The Ra-08H requires a bootloader utility script located in the official SDK to flash the standard firmware binaries.

### 1. Clone the Flashing Utilities
Clone the official Ai-Thinker repository to fetch the `tremo_loader.py` script:
```bash
git clone https://github.com/Ai-Thinker-AIoT/Ai-Thinker-LoRaWAN-Ra-08.git
cd Ai-Thinker-LoRaWAN-Ra-08/build/scripts/
```
Verify that the flashing script is present in the directory:
```bash
ls tremo_loader.py
```

### 2. Download the Official Firmware
* **Official Firmware Download Page:** [Ai-Thinker Ra-08 Docs](https://wxkf.ai-thinker.com/en/Ra-08/)
* Download the standard **EU868 firmware binary**: `(1956)Ra-08H_EU868_V4.18_P1.4.0_20221025.bin`. Place this file three directories up from your terminal context, or adjust the path in the flashing command below to where you saved it.

### 3. Put the Hardware Into Flash Mode
1. Change the physical **jumper cap** orientation to the **`RXD0` marking side**.
2. **Hold down** the hardware **`IO2` (Boot)** button.
3. While holding `IO2`, **press and release** the **`Reset` (RST)** button.
4. Release the `IO2` button. The module is now waiting in flash mode.

### 4. Flash the Binary
Execute the `tremo_loader.py` script using the target serial interface and set a fast baud rate of `921600`:
```bash
python3 tremo_loader.py -p /dev/ttyUSB0 -b 921600 flash 0x08000000 "../../../(1956)Ra-08H_EU868_V4.18_P1.4.0_20221025.bin"
```

---

## ⚙️ Module Re-configuration

Once flashing is complete, you must transition the module back to an AT command execution context.

### 1. Post-Flash Jumpers
1. **Press and release** the physical **`Reset` (RST)** button to exit the bootloader execution environment.
2. Interchange the middle jumper cap, moving it to the **`LPRXD` side** to open standard AT terminal communications.

### 2. Initialize Serial Communication
Open the **Arduino IDE** (or your favorite serial terminal monitor) and configure the following connection rules:
* **Baud Rate:** `9600`
* **Line Endings:** Both `NL & CR` (Newline and Carriage Return)

---

## 📡 ThinkNode Network Console OTAA Node provisioning

Run the following AT commands inside the serial window to read system data, clear old configurations, and hardcode your unique ThinkNode end-node credentials.

### Step 1: Base Initialization
Verify system versioning and set standard regional operating masks:
```at
AT+CGMR?              # Queries current firmware version
AT+CFREQBANDMASK=0001 # Sets frequency channel masking properties 
AT+CULDLMODE=2        # Configures Uplink/Downlink alignment flags
AT+CSAVE              # Permanently commits changes to onboard EEPROM
AT+IREBOOT=0          # Requests immediate module hot reboot
```
*👉 **Action:** Manually press the physical **`RST`** button on the hardware after the reboot cycle completes.*

### Step 2: Provisioning ThinkNode Credentials
Replace the placeholders below with the keys generated directly inside your **ThinkNode Network Console**:
```at
AT+CDEVEUI=YOUR_16_DIGIT_DEV_EUI_HERE  # Define unique 16-hex Device EUI
AT+CAPPEUI=YOUR_16_DIGIT_APP_EUI_HERE  # Define unique 16-hex Application EUI
AT+CAPPKEY=YOUR_32_DIGIT_APP_KEY_HERE  # Define 32-hex Application Key for secure cryptographic generation
AT+CFREQBANDMASK=0001                  # Reinforce matching band rules
AT+CULDLMODE=2                        # Reinforce transmission modes
AT+CSAVE                               # Commit credentials safely into memory
AT+IREBOOT=0                           # Reboot module to activate credentials
```
*👉 **Action:** Manually press the physical **`RST`** button one last time.*

---

## 🧪 Uplink Transmission Verification

Use these diagnostic testing commands to verify successful registration and send a live network payload.

```at
AT+CTXP=7                     # Sets maximum Tx Transmit Power index 
AT+CJOIN=1,0,10,8             # Initiates OTAA network join sequence request
```
Once the console returns confirmation that the network join operation was successful (`+CJOIN:OK`), send an unconfirmed hex payload uplink:
```at
AT+DTRX=0,1,4,AABBCCDD        # Uplinks 4 bytes of test hex data (AABBCCDD)
```

