#include "ds2482.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h" 
#include <cstring>

namespace esphome {
namespace ds2482 {

static const char *const TAG = "ds2482";

// DS2482 Commands
static const uint8_t CMD_DRST = 0xF0;
static const uint8_t CMD_SRP  = 0xE1;
static const uint8_t CMD_WCFG = 0xD2;
static const uint8_t CMD_1WRS = 0xB4;
static const uint8_t CMD_1WWB = 0xA5;
static const uint8_t CMD_1WRB = 0x96;
static const uint8_t CMD_1WT  = 0x78;

// Pointers
static const uint8_t PTR_STATUS = 0xF0;
static const uint8_t PTR_DATA   = 0xE1;
static const uint8_t PTR_CONFIG = 0xC3;

// Status Bits
static const uint8_t STS_1WB = 0x01;
static const uint8_t STS_PPD = 0x02;
static const uint8_t STS_SD  = 0x04;
static const uint8_t STS_RST = 0x10;
static const uint8_t STS_SBR = 0x20;
static const uint8_t STS_TSB = 0x40;
static const uint8_t STS_DIR = 0x80;

static const uint8_t CFG_APU = 0x01;
static const uint8_t CFG_SPU = 0x04;

// ==========================================================
// Setup
// ==========================================================

void DS2482Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS2482...");
  delay(10);

  if (!this->ds2482WriteCmd(CMD_DRST)) {
      ESP_LOGE(TAG, "SETUP FAILED: DS2482 Reset NACKed.");
      this->mark_failed();
      return;
  }
  delay(10);

  // Write Config (APU=1)
  uint8_t cfg_val = (uint8_t)((~CFG_APU << 4) & 0xF0) | CFG_APU;
  if (!this->ds2482WriteCmdParam(CMD_WCFG, cfg_val)) {
      ESP_LOGE(TAG, "SETUP FAILED: Config Write Failed.");
      this->mark_failed();
      return;
  }

  // Verify Config
  uint8_t read_back = 0;
  if (!this->ds2482SetReadPtr(PTR_CONFIG) || this->read(&read_back, 1) != i2c::ERROR_OK) {
     ESP_LOGE(TAG, "SETUP FAILED: Read Config Failed.");
     this->mark_failed();
     return;
  }

  ESP_LOGI(TAG, "DS2482 initialized. Config: 0x%02X", read_back);
  this->init_success_ = true;
  this->lastDeviceFlag_ = false;
  this->lastDiscrepancy_ = 0;
}

// ==========================================================
// Update Loop
// ==========================================================

void DS2482Component::update() {
  if (!this->init_success_) return;
  
  ESP_LOGD(TAG, "Update Loop: %d sensors configured.", this->sensors_.size());

  // If no sensors are hardcoded, allow auto-scan
  if (this->sensors_.empty()) {
      if (this->found_devices_.empty()) this->manual_scan();
      return;
  }

  // 1. Reset Bus
  if (!this->owReset()) {
      ESP_LOGW(TAG, "Bus Reset Failed (No Presence Pulse).");
      return; 
  }
  
  // 2. Start Conversion (Skip ROM -> Convert T)
  this->owWriteByte(0xCC);
  this->owWriteByte(0x44);
  
  // 3. Wait 750ms
  delay(750); 

  // 4. Read Sensors
  for (auto &s : this->sensors_) {
    // Reset before addressing specific sensor
    if (!this->owReset()) {
        ESP_LOGW(TAG, "Read Aborted: Bus Reset failed for sensor 0x%016llX", s.address);
        continue;
    }
    
    // Match ROM
    this->owWriteByte(0x55); 
    uint64_t addr = s.address;
    for(int i=0; i<8; i++) {
        this->owWriteByte((uint8_t)(addr & 0xFF));
        addr >>= 8;
    }

    // Read Scratchpad
    this->owWriteByte(0xBE); 

    uint8_t data[2] = {0, 0};
    if (this->owReadBytes(data, 2)) {
        this->owReset(); // Stop reading

        int16_t raw = (data[1] << 8) | data[0];
        float temp = raw / 16.0f;

        ESP_LOGI(TAG, "Sensor 0x%016llX -> Temp: %.2f C", s.address, temp);

        if (temp > -55.0 && temp < 125.0) {
            s.sensor_obj->publish_state(temp);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read bytes from sensor 0x%016llX", s.address);
    }
  }
}

// ==========================================================
// Low Level Helpers
// ==========================================================

bool DS2482Component::ds2482WriteCmd(uint8_t cmd) {
    return this->write(&cmd, 1) == i2c::ERROR_OK;
}

bool DS2482Component::ds2482WriteCmdParam(uint8_t cmd, uint8_t param) {
    uint8_t buf[2] = {cmd, param};
    return this->write(buf, 2) == i2c::ERROR_OK;
}

bool DS2482Component::ds2482SetReadPtr(uint8_t ptr) {
    return this->ds2482WriteCmdParam(CMD_SRP, ptr);
}

bool DS2482Component::readStatus(uint8_t &status) {
    if (!this->ds2482SetReadPtr(PTR_STATUS)) return false;
    return this->read(&status, 1) == i2c::ERROR_OK;
}

bool DS2482Component::readData(uint8_t &data) {
    if (!this->ds2482SetReadPtr(PTR_DATA)) return false;
    return this->read(&data, 1) == i2c::ERROR_OK;
}

bool DS2482Component::waitIdle(uint8_t &status) {
    for (int i = 0; i < 100; i++) {
        if (!this->readStatus(status)) return false;
        if (!(status & STS_1WB)) return true; // Busy bit clear
        delay(1);
    }
    return false;
}

// ==========================================================
// 1-Wire Primitives
// ==========================================================

bool DS2482Component::owReset() {
    uint8_t status = 0;
    if (!this->ds2482WriteCmd(CMD_1WRS)) return false;
    if (!this->waitIdle(status)) return false;
    return (status & STS_PPD); 
}

bool DS2482Component::owWriteByte(uint8_t b) {
    uint8_t status = 0;
    if (!this->ds2482WriteCmdParam(CMD_1WWB, b)) return false;
    return this->waitIdle(status);
}

bool DS2482Component::owReadBytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t status = 0;
        if (!this->ds2482WriteCmd(CMD_1WRB)) return false;
        if (!this->waitIdle(status)) return false;
        if (!this->readData(buf[i])) return false;
    }
    return true;
}

bool DS2482Component::oneWireTriplet(uint8_t dirBit, uint8_t &status) {
    uint8_t param = dirBit ? 0x80 : 0x00;
    if (!this->ds2482WriteCmdParam(CMD_1WT, param)) return false;
    return this->waitIdle(status);
}

// ==========================================================
// Search Algorithm
// ==========================================================

void DS2482Component::manual_scan() {
    if (!this->init_success_) return;
    ESP_LOGI(TAG, "--- Scanning 1-Wire Bus ---");
    this->search_bus();
    ESP_LOGI(TAG, "Scan Complete.");
}

void DS2482Component::search_bus() {
    this->found_devices_.clear();
    this->lastDiscrepancy_ = 0;
    this->lastDeviceFlag_ = false;
    std::fill(this->rom_.begin(), this->rom_.end(), 0);

    std::array<uint8_t, 8> rom_out;
    bool first = true;
    int safety = 0;

    while (safety++ < 64) { 
        if (!this->owSearch(first, rom_out)) break;
        
        first = false;
        uint64_t addr = 0;
        for (int i=7; i>=0; i--) {
            addr = (addr << 8) | rom_out[i];
        }
        
        this->found_devices_.push_back(addr);
        this->log_yaml_suggestion(addr);

        if (this->lastDeviceFlag_) break;
    }
}

bool DS2482Component::owSearch(bool first, std::array<uint8_t, 8> &romOut) {
    uint8_t idBitNumber = 1;
    uint8_t lastZero = 0;
    uint8_t romByteNumber = 0;
    uint8_t romByteMask = 1;
    uint8_t status = 0;

    if (!this->owReset()) {
        this->lastDiscrepancy_ = 0;
        this->lastDeviceFlag_ = false;
        return false;
    }
    if (!this->owWriteByte(0xF0)) return false; 

    do {
        uint8_t searchDirection = 0;
        if (idBitNumber < this->lastDiscrepancy_) {
            searchDirection = (this->rom_[romByteNumber] & romByteMask) ? 1 : 0;
        } else {
            searchDirection = (idBitNumber == this->lastDiscrepancy_);
        }

        if (!this->oneWireTriplet(searchDirection, status)) return false;

        uint8_t idBit = (status & STS_SBR) ? 1 : 0;
        uint8_t cmpIdBit = (status & STS_TSB) ? 1 : 0;
        uint8_t dirTaken = (status & STS_DIR) ? 1 : 0;

        if (idBit && cmpIdBit) break; 

        if (!idBit && !cmpIdBit && !dirTaken) {
            lastZero = idBitNumber;
        }

        if (dirTaken) {
            this->rom_[romByteNumber] |= romByteMask;
        } else {
            this->rom_[romByteNumber] &= ~romByteMask;
        }

        idBitNumber++;
        romByteMask <<= 1;
        if (romByteMask == 0) {
            romByteNumber++;
            romByteMask = 1;
        }
    } while (romByteNumber < 8);

    if (idBitNumber < 65) {
        this->lastDiscrepancy_ = 0;
        return false;
    }
    this->lastDiscrepancy_ = lastZero;
    if (this->lastDiscrepancy_ == 0) this->lastDeviceFlag_ = true;
    romOut = this->rom_;
    return true;
}

void DS2482Component::log_yaml_suggestion(uint64_t address) {
    ESP_LOGI(TAG, "Found Device: 0x%016llX", address);
    ESP_LOGI(TAG, "  - platform: ds2482");
    ESP_LOGI(TAG, "    address: 0x%016llX", address);
    ESP_LOGI(TAG, "    name: \"Sensor %04llX\"", address & 0xFFFF);
}

void DS2482Component::set_strong_pullup(bool enable) {
  uint8_t cfg_val = (uint8_t)((~CFG_APU << 4) & 0xF0) | CFG_APU;
  if (enable) cfg_val = (uint8_t)((~(CFG_APU|CFG_SPU) << 4) & 0xF0) | (CFG_APU|CFG_SPU);
  this->ds2482WriteCmdParam(CMD_WCFG, cfg_val);
}

void DS2482Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS2482:");
  LOG_I2C_DEVICE(this);
}

} // namespace ds2482
} // namespace esphome