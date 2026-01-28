#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

namespace ant_bms_ble {

static constexpr uint16_t kServiceUuid = 0xFFE0;
static constexpr uint16_t kCharUuid = 0xFFE1;

static constexpr uint8_t kStart1 = 0x7E;
static constexpr uint8_t kStart2 = 0xA1;
static constexpr uint8_t kEnd1 = 0xAA;
static constexpr uint8_t kEnd2 = 0x55;

static constexpr uint8_t kCmdStatus = 0x01;
static constexpr uint8_t kCmdDeviceInfo = 0x02;

static constexpr uint8_t kFrameStatus = 0x11;
static constexpr uint8_t kFrameDeviceInfo = 0x12;

struct AntStatusSummary {
  bool valid = false;
  uint8_t cell_count = 0;
  uint8_t temp_sensor_count = 0;
  float total_voltage_v = NAN;   // raw uint16 / 10.0
  float current_a = NAN;         // raw int16 / 10.0
  float soc_pct = NAN;           // raw uint16 / 10.0
  uint8_t battery_status = 0;
  uint8_t permissions = 0;
  uint64_t protection_mask = 0;
  uint64_t warning_mask = 0;
  uint64_t balancing_mask = 0;

  float cell_v[32];
  float temp_c[8];  // temp sensors + mosfet + balancer

  float capacity_ah = NAN;
  float capacity_remaining_ah = NAN;
  float cycle_capacity_ah = NAN;
  float power_w = NAN;
  uint32_t total_runtime_s = 0;

  float max_cell_v = NAN;
  uint8_t max_cell_idx = 0;
  float min_cell_v = NAN;
  uint8_t min_cell_idx = 0;
  float delta_cell_v = NAN;
  float avg_cell_v = NAN;

  uint8_t charge_mosfet_status = 0;
  uint8_t discharge_mosfet_status = 0;
  uint8_t balancer_status = 0;
};

enum class AntVariant : uint8_t {
  UNKNOWN = 0,
  V2_7E = 1,
  V1_AA55AA = 2,
};

enum class DetectState : uint8_t {
  DISCONNECTED = 0,
  CONNECTED,
  SUBSCRIBED,
  DETECTING,
  ACTIVE_LOCKED,
};

class AntBmsBleClient {
 public:
  bool begin(const NimBLEAddress &addr);
  void tick(uint32_t now_ms);

  bool is_connected() const { return connected_; }
  bool has_status() const { return status_.valid; }
  const AntStatusSummary &status() const { return status_; }
  AntVariant variant() const { return variant_; }
  DetectState state() const { return state_; }
  uint32_t last_rx_ms() const { return last_rx_ms_; }

  bool request_status();
  bool request_device_info();

 private:
  NimBLEClient *client_ = nullptr;
  NimBLERemoteCharacteristic *chr_ = nullptr;
  NimBLEAddress addr_ = NimBLEAddress("");
  bool connected_ = false;

  std::vector<uint8_t> frame_buf_;
  std::vector<uint8_t> rx_buf_;
  uint32_t last_status_req_ms_ = 0;
  uint32_t last_devinfo_req_ms_ = 0;
  uint32_t last_rx_ms_ = 0;
  uint32_t detect_start_ms_ = 0;
  uint32_t last_probe_ms_ = 0;
  uint8_t probe_stage_ = 0;

  AntStatusSummary status_{};
  AntVariant variant_ = AntVariant::UNKNOWN;
  DetectState state_ = DetectState::DISCONNECTED;

  static void notify_cb_(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t len, bool is_notify);
  void on_notify_(const uint8_t *data, size_t len);

  bool connect_();
  void disconnect_();
  bool ensure_characteristic_();

  bool send_frame_(uint8_t function, uint16_t address, uint8_t value);
  bool send_raw_(const uint8_t *data, size_t len);
  bool assemble_and_detect_(const uint8_t *data, size_t len);
  bool detect_from_buffer_();
  bool parse_locked_from_buffer_();
  bool try_extract_frame_(AntVariant v, size_t *frame_start, size_t *frame_len);
  bool validate_frame_(AntVariant v, const uint8_t *data, size_t len);
  bool parse_frame_(AntVariant v, const uint8_t *data, size_t len);
  void parse_status_(const uint8_t *data, size_t len);

  static uint16_t crc16_(const uint8_t *data, size_t len);
  static uint16_t chksum_v1_(const uint8_t *data, size_t len);
};

}  // namespace ant_bms_ble
