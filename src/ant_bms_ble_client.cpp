#include "ant_bms_ble_client.h"

namespace ant_bms_ble {

namespace {
AntBmsBleClient *g_instance = nullptr;
}

static const char *variant_name(AntVariant v) {
  switch (v) {
    case AntVariant::V2_7E: return "V2(7E)";
    case AntVariant::V1_AA55AA: return "V1(AA55AA)";
    default: return "UNKNOWN";
  }
}

uint16_t AntBmsBleClient::crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

uint16_t AntBmsBleClient::chksum_v1_(const uint8_t *data, size_t len) {
  uint16_t checksum = 0;
  for (uint16_t i = 4; i < len; i++) {
    checksum = checksum + data[i];
  }
  return checksum;
}

bool AntBmsBleClient::begin(const NimBLEAddress &addr) {
  addr_ = addr;
  g_instance = this;
  return connect_();
}

bool AntBmsBleClient::connect_() {
  disconnect_();

  client_ = NimBLEDevice::createClient();
  if (!client_) return false;

  if (!client_->connect(addr_)) {
    disconnect_();
    return false;
  }

  state_ = DetectState::CONNECTED;
  connected_ = ensure_characteristic_();
  if (!connected_) {
    disconnect_();
  }
  return connected_;
}

void AntBmsBleClient::disconnect_() {
  connected_ = false;
  chr_ = nullptr;
  frame_buf_.clear();
  rx_buf_.clear();
  status_ = {};
  for (float &v : status_.cell_v) v = NAN;
  for (float &t : status_.temp_c) t = NAN;
  variant_ = AntVariant::UNKNOWN;
  state_ = DetectState::DISCONNECTED;
  if (client_) {
    if (client_->isConnected()) client_->disconnect();
    NimBLEDevice::deleteClient(client_);
    client_ = nullptr;
  }
}

bool AntBmsBleClient::ensure_characteristic_() {
  if (!client_ || !client_->isConnected()) return false;

  NimBLERemoteService *svc = client_->getService(kServiceUuid);
  if (!svc) return false;

  chr_ = svc->getCharacteristic(kCharUuid);
  if (!chr_) return false;

  if (chr_->canNotify()) {
    if (!chr_->subscribe(true, notify_cb_)) {
      chr_ = nullptr;
      return false;
    }
  }

  state_ = DetectState::SUBSCRIBED;
  detect_start_ms_ = millis();
  last_rx_ms_ = 0;
  probe_stage_ = 0;
  last_probe_ms_ = 0;
  variant_ = AntVariant::UNKNOWN;
  state_ = DetectState::DETECTING;
  return true;
}

void AntBmsBleClient::notify_cb_(NimBLERemoteCharacteristic *chr, uint8_t *data, size_t len, bool) {
  (void)chr;
  if (g_instance) g_instance->on_notify_(data, len);
}

void AntBmsBleClient::on_notify_(const uint8_t *data, size_t len) {
  assemble_and_detect_(data, len);
}

bool AntBmsBleClient::send_frame_(uint8_t function, uint16_t address, uint8_t value) {
  if (!connected_ || !chr_ || !client_ || !client_->isConnected()) return false;

  uint8_t frame[10];
  frame[0] = kStart1;
  frame[1] = kStart2;
  frame[2] = function;
  frame[3] = (uint8_t)(address & 0xFF);
  frame[4] = (uint8_t)((address >> 8) & 0xFF);
  frame[5] = value;
  uint16_t crc = crc16_(frame + 1, 5);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  frame[8] = kEnd1;
  frame[9] = kEnd2;

  return chr_->writeValue(frame, sizeof(frame), false);
}

bool AntBmsBleClient::send_raw_(const uint8_t *data, size_t len) {
  if (!connected_ || !chr_ || !client_ || !client_->isConnected()) return false;
  if (!data || len == 0) return false;
  return chr_->writeValue(data, len, false);
}

bool AntBmsBleClient::request_status() {
  return send_frame_(kCmdStatus, 0x0000, 0xBE);
}

bool AntBmsBleClient::request_device_info() {
  return send_frame_(kCmdDeviceInfo, 0x026C, 0x20);
}

bool AntBmsBleClient::assemble_and_detect_(const uint8_t *data, size_t len) {
  if (!data || len == 0) return false;

  last_rx_ms_ = millis();
  rx_buf_.insert(rx_buf_.end(), data, data + len);

  static constexpr size_t kMaxFrame = 512;
  if (rx_buf_.size() > kMaxFrame) {
    rx_buf_.clear();
    return false;
  }

  if (state_ == DetectState::DETECTING) {
    return detect_from_buffer_();
  }
  if (state_ == DetectState::ACTIVE_LOCKED) {
    return parse_locked_from_buffer_();
  }

  return false;
}

bool AntBmsBleClient::detect_from_buffer_() {
  size_t start = 0;
  size_t flen = 0;

  for (size_t i = 0; i + 1 < rx_buf_.size(); ++i) {
    AntVariant v = AntVariant::UNKNOWN;
    if (rx_buf_[i] == 0x7E) v = AntVariant::V2_7E;
    else if (i + 2 < rx_buf_.size() &&
             rx_buf_[i] == 0xAA && rx_buf_[i + 1] == 0x55 && rx_buf_[i + 2] == 0xAA) {
      v = AntVariant::V1_AA55AA;
    }

    if (v == AntVariant::UNKNOWN) continue;

    if (!try_extract_frame_(v, &start, &flen)) {
      continue;
    }
    if (start + flen > rx_buf_.size()) {
      continue;
    }
    if (!validate_frame_(v, rx_buf_.data() + start, flen)) {
      continue;
    }

    variant_ = v;
    state_ = DetectState::ACTIVE_LOCKED;
    Serial.printf("[ANT] Detected variant: %s\n", variant_name(variant_));
    parse_frame_(variant_, rx_buf_.data() + start, flen);
    rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + start + flen);
    return true;
  }

  // Trim garbage until a possible preamble.
  if (rx_buf_.size() > 2) {
    size_t keep = rx_buf_.size();
    for (size_t i = 0; i + 1 < rx_buf_.size(); ++i) {
      if (rx_buf_[i] == 0x7E ||
          (i + 2 < rx_buf_.size() && rx_buf_[i] == 0xAA && rx_buf_[i + 1] == 0x55 && rx_buf_[i + 2] == 0xAA)) {
        keep = i;
        break;
      }
    }
    if (keep > 0 && keep < rx_buf_.size()) {
      rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + keep);
    }
  }
  return false;
}

bool AntBmsBleClient::parse_locked_from_buffer_() {
  if (variant_ == AntVariant::UNKNOWN) return false;

  size_t start = 0;
  size_t flen = 0;
  if (!try_extract_frame_(variant_, &start, &flen)) {
    return false;
  }
  if (start + flen > rx_buf_.size()) return false;
  if (!validate_frame_(variant_, rx_buf_.data() + start, flen)) {
    // Drop one byte to resync.
    rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 1);
    return false;
  }

  bool ok = parse_frame_(variant_, rx_buf_.data() + start, flen);
  rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + start + flen);
  return ok;
}

bool AntBmsBleClient::try_extract_frame_(AntVariant v, size_t *frame_start, size_t *frame_len) {
  if (!frame_start || !frame_len) return false;

  for (size_t i = 0; i + 1 < rx_buf_.size(); ++i) {
    if (v == AntVariant::V2_7E) {
      if (rx_buf_[i] != 0x7E) continue;
      if (i + 6 >= rx_buf_.size()) return false;
      uint8_t data_len = rx_buf_[i + 5];
      size_t total = 6u + (size_t)data_len + 4u;
      if (i + total <= rx_buf_.size()) {
        *frame_start = i;
        *frame_len = total;
        return true;
      }
      return false;
    }

    if (v == AntVariant::V1_AA55AA) {
      if (i + 2 >= rx_buf_.size()) return false;
      if (rx_buf_[i] != 0xAA || rx_buf_[i + 1] != 0x55 || rx_buf_[i + 2] != 0xAA) continue;
      static constexpr size_t kV1Len = 140;
      if (i + kV1Len <= rx_buf_.size()) {
        *frame_start = i;
        *frame_len = kV1Len;
        return true;
      }
      return false;
    }
  }
  return false;
}

bool AntBmsBleClient::validate_frame_(AntVariant v, const uint8_t *data, size_t len) {
  if (!data || len < 8) return false;

  if (v == AntVariant::V2_7E) {
    if (data[0] != kStart1 || data[1] != kStart2) return false;
    if (data[len - 2] != kEnd1 || data[len - 1] != kEnd2) return false;
    const uint16_t remote_crc = (uint16_t)data[len - 3] << 8 | (uint16_t)data[len - 4];
    const uint16_t computed_crc = crc16_(data + 1, len - 5);
    return remote_crc == computed_crc;
  }

  if (v == AntVariant::V1_AA55AA) {
    if (len != 140) return false;
    if (data[0] != 0xAA || data[1] != 0x55 || data[2] != 0xAA) return false;
    const uint16_t remote_crc =
        (uint16_t)data[len - 2] << 8 | (uint16_t)data[len - 1];
    const uint16_t computed_crc = chksum_v1_(data, (uint16_t)(len - 2));
    return remote_crc == computed_crc;
  }

  return false;
}

bool AntBmsBleClient::parse_frame_(AntVariant v, const uint8_t *data, size_t len) {
  if (v == AntVariant::V2_7E) {
    const uint8_t function = data[2];
    switch (function) {
      case kFrameStatus:
        parse_status_(data, len);
        return true;
      case kFrameDeviceInfo:
        return true;
      default:
        return false;
    }
  }
  if (v == AntVariant::V1_AA55AA) {
    if (len != 140) return false;

    auto get16 = [&](size_t i) -> uint16_t {
      return (uint16_t)data[i + 0] << 8 | (uint16_t)data[i + 1];
    };
    auto get32 = [&](size_t i) -> uint32_t {
      return ((uint32_t)get16(i) << 16) | (uint32_t)get16(i + 2);
    };

    status_.valid = true;
    status_.permissions = 0;
    status_.battery_status = 0;

    status_.total_voltage_v = get16(4) * 0.1f;

    const uint8_t cell_count = data[123];
    status_.cell_count = cell_count;
    for (uint8_t i = 0; i < 32; ++i) status_.cell_v[i] = NAN;
    for (uint8_t i = 0; i < cell_count && i < 32; ++i) {
      status_.cell_v[i] = get16(6 + (i * 2)) * 0.001f;
    }

    status_.current_a = (int32_t)get32(70) * 0.1f;
    status_.soc_pct = data[74] * 1.0f;

    status_.capacity_ah = get32(75) * 0.000001f;
    status_.capacity_remaining_ah = get32(79) * 0.000001f;
    status_.cycle_capacity_ah = get32(83) * 0.001f;
    status_.total_runtime_s = get32(87);

    status_.temp_sensor_count = 6;
    for (uint8_t i = 0; i < 8; ++i) status_.temp_c[i] = NAN;
    for (uint8_t i = 0; i < 6; ++i) {
      status_.temp_c[i] = (int16_t)get16(91 + (i * 2)) * 1.0f;
    }

    status_.charge_mosfet_status = data[103];
    status_.discharge_mosfet_status = data[104];
    status_.balancer_status = data[105];

    status_.power_w = (int32_t)get32(111) * 1.0f;

    status_.max_cell_idx = data[115];
    status_.max_cell_v = get16(116) * 0.001f;
    status_.min_cell_idx = data[118];
    status_.min_cell_v = get16(119) * 0.001f;
    status_.delta_cell_v = status_.max_cell_v - status_.min_cell_v;
    status_.avg_cell_v = get16(121) * 0.001f;

    status_.balancing_mask = (uint64_t)get32(132);
    status_.warning_mask = (uint64_t)get16(136);
    status_.protection_mask = 0;
    return true;
  }
  return false;
}

void AntBmsBleClient::parse_status_(const uint8_t *data, size_t len) {
  if (len < 80) return;

  auto get16 = [&](size_t i) -> uint16_t {
    return (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
  };
  auto get32 = [&](size_t i) -> uint32_t {
    return (uint32_t)get16(i) | ((uint32_t)get16(i + 2) << 16);
  };

  const uint8_t temp_count = data[8];
  const uint8_t cell_count = data[9];
  const uint8_t max_cells = (cell_count > 32) ? 32 : cell_count;
  const uint8_t max_temps = (temp_count > 6) ? 6 : temp_count;

  uint8_t offset = (uint8_t)(cell_count * 2);
  offset = (uint8_t)(offset + (temp_count * 2));

  status_.permissions = data[6];
  status_.battery_status = data[7];
  status_.temp_sensor_count = temp_count;
  status_.cell_count = cell_count;

  status_.protection_mask =
      (uint64_t)get32(10) | ((uint64_t)get32(14) << 32);
  status_.warning_mask =
      (uint64_t)get32(18) | ((uint64_t)get32(22) << 32);
  status_.balancing_mask =
      (uint64_t)get32(26) | ((uint64_t)get32(30) << 32);

  for (uint8_t i = 0; i < 32; ++i) status_.cell_v[i] = NAN;
  for (uint8_t i = 0; i < max_cells; ++i) {
    status_.cell_v[i] = get16(34 + (i * 2)) * 0.001f;
  }

  for (uint8_t i = 0; i < 8; ++i) status_.temp_c[i] = NAN;
  for (uint8_t i = 0; i < max_temps; ++i) {
    status_.temp_c[i] = (int16_t)get16(34 + (i * 2) + (cell_count * 2)) * 1.0f;
  }
  status_.temp_c[max_temps] =
      (int16_t)get16(34 + offset) * 1.0f;        // MOSFET
  status_.temp_c[max_temps + 1] =
      (int16_t)get16(36 + offset) * 1.0f;        // Balancer

  const uint16_t raw_v = get16(38 + offset);   // 0.01 V
  const int16_t raw_i = (int16_t)get16(40 + offset);  // 0.1 A
  const uint16_t raw_soc = get16(42 + offset); // 1.0 %

  status_.total_voltage_v = raw_v * 0.01f;
  status_.current_a = raw_i * 0.1f;
  status_.soc_pct = raw_soc * 1.0f;

  status_.charge_mosfet_status = data[46 + offset];
  status_.discharge_mosfet_status = data[47 + offset];
  status_.balancer_status = data[48 + offset];

  status_.capacity_ah = get32(50 + offset) * 0.000001f;
  status_.capacity_remaining_ah = get32(54 + offset) * 0.000001f;
  status_.cycle_capacity_ah = get32(58 + offset) * 0.001f;
  status_.power_w = (int32_t)get32(62 + offset) * 1.0f;
  status_.total_runtime_s = get32(66 + offset);

  status_.max_cell_v = get16(74 + offset) * 0.001f;
  status_.max_cell_idx = (uint8_t)get16(76 + offset);
  status_.min_cell_v = get16(78 + offset) * 0.001f;
  status_.min_cell_idx = (uint8_t)get16(80 + offset);
  status_.delta_cell_v = get16(82 + offset) * 0.001f;
  status_.avg_cell_v = get16(84 + offset) * 0.001f;

  status_.valid = true;
}

void AntBmsBleClient::tick(uint32_t now_ms) {
  if (!client_ || !client_->isConnected()) {
    connected_ = false;
    return;
  }
  connected_ = true;

  if (state_ == DetectState::DETECTING) {
    const uint32_t since_sub = (detect_start_ms_ == 0) ? 0 : (now_ms - detect_start_ms_);
    if (since_sub >= 1200 && (now_ms - last_probe_ms_) >= 500) {
      last_probe_ms_ = now_ms;
      // Probe order: V2 -> V1 -> Legacy (read-only).
      if (probe_stage_ == 0) {
        (void)request_status();
        probe_stage_ = 1;
      } else if (probe_stage_ == 1) {
        const uint8_t v1_probe[6] = {0xDB, 0xDB, 0x00, 0x00, 0x00, 0x00};
        (void)send_raw_(v1_probe, sizeof(v1_probe));
        probe_stage_ = 2;
      }
    }
  }

  if (now_ms - last_status_req_ms_ >= 1000) {
    last_status_req_ms_ = now_ms;
    (void)request_status();
  }

  if (now_ms - last_devinfo_req_ms_ >= 5000) {
    last_devinfo_req_ms_ = now_ms;
    (void)request_device_info();
  }
}

}  // namespace ant_bms_ble
