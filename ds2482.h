#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h" // Always include
#include <vector>
#include <array>

#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

namespace esphome {
namespace ds2482 {

class DS2482Component;

// --- Child Classes ---

struct OneWireSensor {
  sensor::Sensor *sensor_obj;
  uint64_t address;
  uint8_t index;
  bool found;
};
class DS2482Sensor : public sensor::Sensor {};

#ifdef USE_OUTPUT
class DS2482Output : public output::FloatOutput {
 public:
  void set_parent(DS2482Component *parent) { parent_ = parent; }
  void write_state(float state) override;
 protected:
  DS2482Component *parent_{nullptr};
};
#endif

#ifdef USE_BUTTON
class DS2482ScanButton : public button::Button {
 public:
  void set_parent(DS2482Component *parent) { parent_ = parent; }
  void press_action() override;
 protected:
  DS2482Component *parent_{nullptr};
};
#endif

// --- Main Hub Component ---

class DS2482Component : public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void register_sensor(sensor::Sensor *obj, uint64_t address, uint8_t index) {
    this->sensors_.push_back({obj, address, index, false});
  }

  void manual_scan();
  void set_strong_pullup(bool enable);

 protected:
  // --- Low Level Primitives ---
  bool ds2482WriteCmd(uint8_t cmd);
  bool ds2482WriteCmdParam(uint8_t cmd, uint8_t param);
  bool ds2482SetReadPtr(uint8_t ptr);
  bool readStatus(uint8_t &status);
  bool readData(uint8_t &data);
  bool waitIdle(uint8_t &status);

  // --- 1-Wire Primitives ---
  bool owReset();
  bool owWriteByte(uint8_t b);
  bool owReadBytes(uint8_t *buf, size_t len);
  bool oneWireTriplet(uint8_t dirBit, uint8_t &status);
  bool owSearch(bool first, std::array<uint8_t, 8> &romOut);

  // --- State ---
  bool lastDeviceFlag_{false};
  uint8_t lastDiscrepancy_{0};
  std::array<uint8_t, 8> rom_{};
  bool init_success_{false};

  std::vector<OneWireSensor> sensors_;
  std::vector<uint64_t> found_devices_;

  void search_bus();
  void log_yaml_suggestion(uint64_t address);
};

// --- Inline Implementations ---

#ifdef USE_OUTPUT
inline void DS2482Output::write_state(float state) {
  if (this->parent_ != nullptr) this->parent_->set_strong_pullup(state > 0.5f);
}
#endif

#ifdef USE_BUTTON
inline void DS2482ScanButton::press_action() {
  if (this->parent_ != nullptr) this->parent_->manual_scan();
}
#endif

} // namespace ds2482
} // namespace esphome