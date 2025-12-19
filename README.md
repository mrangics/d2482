# DS2482 1-Wire Master for ESPHome

This is a custom ESPHome component for the **Maxim DS2482-100/800** I2C-to-1-Wire Bridge Controller. It allows you to offload the timing-critical 1-Wire protocol to a dedicated chip, which is far more reliable than bit-banging GPIOs on the ESP32/ESP8266, especially for long cable runs.

## Features
* **Offloaded Timing:** No interrupts blocked on the ESP32; perfect for complex projects.
* **Active Pullup (SPU):** Automatically enables Strong Pullup during temperature conversion (crucial for parasitic sensors or long cables).
* **Bus Scanning:** Built-in scanning tool to discover connected sensors.
* **Robust Error Handling:** Resets the 1-Wire bus automatically if communication fails.

---

## 1. Installation Structure

Create a folder named `ds2482` inside your project's `components` directory (or `my_components` depending on your config). The structure **must** look like this to ensure the Python loader works correctly:

```text
config/
  ├── esphome/
  │   ├── mate.yaml              <-- Your main firmware file
  │   └── components/            <-- Create this if missing
  │       └── ds2482/            <-- The component folder
  │           ├── __init__.py    <-- Hub registration
  │           ├── sensor.py      <-- Sensor platform registration
  │           ├── button.py      <-- Button platform registration
  │           ├── output.py      <-- Output platform registration
  │           ├── ds2482.h       <-- C++ Header
  │           └── ds2482.cpp     <-- C++ Implementation
```

## 2. Configuration
### Main Hub (ds2492)
This sets up the I2C connection to the chip.

```yaml
ds2482:
  id: my_hub_id
  address: 0x18 # Default I2C address (0x18, 0x19, etc.)
  update_interval: 60s # How often to read all sensors (Default: 60s)
```

### Sensor (sensor)
Defines the specific DS18B20 (or compatible) sensors you want to read.
```yaml
sensor:
  platform: ds2482
  name: "Living Room Temperature"
  address: 0x28AA1234560000FF # 64-bit ROM Code (See "Finding Addresses" below)
```

### Scan Button (button)
Adds a button to Home Assistant that, when pressed, scans the bus and prints connected devices to the ESPHome logs.
```yaml
button:
  platform: ds2482 name: "Scan 1-Wire Bus"
```

### Strong Pullup Control (output) - Optional
Allows you to manually toggle the "Strong Pullup" (SPU) mode for testing. Note: The component automatically manages SPU during reads, so this is rarely needed for normal operation.
```yaml
output:
  platform: ds2482 id: spu_control
```

## 3. Full Example YAML
```yaml
esphome:
  name: "onewire-gateway"

esp32:
  board: esp32dev

# Define I2C Bus
i2c:
  sda: 21
  scl: 22
  scan: true
  id: bus_a

# Load the DS2482 Component
ds2482:
 id: ow_hub
 i2c_id: bus_a
 address: 0x18
 update_interval: 30s

# Sensors (Add these after finding addresses)
sensor:
  - platform: ds2482
    name: "Boiler Return Temp"
    address: 0xB9C736C21E64FF28
    - platform: ds2482
    name: "Boiler Out Temp"
    address: 0x426309C21E64FF28

# Scan 1-Wire bus for sensors button
button:
  - platform: ds2482
    name: "Scan 1-Wire Bus"
```

## 4. How to Find Sensor Addresses
### If you do not know the 64-bit address of your sensors:

* Flash the ESP with just the Hub and Button configured (remove the sensor: entries temporarily if they are wrong).
* Open the ESPHome Logs (Wireless or USB).
* Press the "Scan 1-Wire Bus" button in Home Assistant.
* Look for output like this in the logs:

```log
[I][ds2482:201]: --- Scanning 1-Wire Bus --- 
[I][ds2482:215]: Found Device: 0x28AA1234560000FF 
[I][ds2482:216]: - platform: ds2482 
[I][ds2482:217]:   address: 0x28AA1234560000FF 
[I][ds2482:218]:   name: "Sensor 00FF"
```

Copy the address value and paste it into your YAML configuration.

## 5. Troubleshooting
### Logs say "No Sensors Enabled":

You forgot to add the sensor: section to your YAML, or you commented it out. The firmware optimizes out the reading logic if no sensors are defined.

### Value is "Unknown":

The sensor might be disconnected.

The sensor might be returning 85.0°C (Power-on reset value), which the component filters out as invalid to prevent data spikes. This usually indicates a wiring issue or insufficient power (parasitic mode failing).

### I2C Errors / NACK:

Check that your DS2482 SDA/SCL lines are connected to the ESP32 pins defined in the i2c: section.

Ensure you have physical 4.7kΩ pullup resistors on the SDA and SCL lines (the DS2482 does not provide these for the I2C side).
