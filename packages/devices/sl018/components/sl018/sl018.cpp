/**
 * SL018/SL030 RFID reader ESPHome external component (ESP-IDF backend)
 *
 * Ported from the Arduino library by Marc Boon <http://www.marcboon.com>
 * https://github.com/marcboon/RFIDuino/tree/master/SL018
 *
 * Reads NDEF Text records written by NFC Tools (Android).
 * Sequence: SELECT → LOGIN (sector 1, transport key) → READ block 4 → parse NDEF
 */

#include "sl018.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace sl018 {

static const char *const TAG = "sl018";
static const uint32_t TX_RX_GAP_MS = 20;

// ── Setup ─────────────────────────────────────────────────────────────────────

void SL018Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SL018...");
  if (tag_pin_ != nullptr)
    tag_pin_->setup();

  uint8_t reset_pkt[2] = {1, CMD_RESET};
  this->write(reset_pkt, 2);
  delay(200);

  queue_seek_();
}

void SL018Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SL018 RFID Reader:");
  LOG_I2C_DEVICE(this);
  if (tag_pin_ != nullptr)
    LOG_PIN("  TAG Pin: ", tag_pin_);
  if (tag_sensor_ != nullptr)
    LOG_TEXT_SENSOR("  ", "Tag Text Sensor", tag_sensor_);
  ESP_LOGCONFIG(TAG, "  NDEF block: %d", ndef_block_);
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void SL018Component::update() {
  if (tag_pin_ != nullptr) {
    bool tag_present = !tag_pin_->digital_read();  // active LOW
    if (!tag_present) {
      if (!last_published_uid_.empty()) {
        last_published_uid_ = "";
        tag_uid_ = "";
        tag_type_ = 0;
        if (tag_sensor_ != nullptr)
          tag_sensor_->publish_state("");
        ESP_LOGD(TAG, "Tag removed");
      }
      reset_to_seek_();
      return;
    }
  }

  uint32_t now = millis();

  // ── SEND ──────────────────────────────────────────────────────────────────
  if (state_ == State::SEND) {
    if (!transmit_pending_())
      return;  // write failed, retry next tick
    last_tx_ms_ = millis();
    state_ = State::RECEIVE;
    return;
  }

  // ── RECEIVE ───────────────────────────────────────────────────────────────
  if (now - last_tx_ms_ < TX_RX_GAP_MS)
    return;

  uint8_t len;
  switch (cmd_) {
    case CMD_LOGIN:                    len = 3;  break;
    case CMD_SEEK: case CMD_SELECT:    len = 11; break;
    case CMD_READ16:                   len = SIZE_PACKET; break;
    default:                           len = SIZE_PACKET; break;
  }

  if (!receive_data_(len))
    return;  // not ready yet, retry next tick

  process_response_();
}

// ── Command queueing ──────────────────────────────────────────────────────────

void SL018Component::queue_command_(uint8_t *pkt, uint8_t total_len) {
  memcpy(pending_data_, pkt, total_len);
  pending_len_ = total_len;
  state_ = State::SEND;
}

void SL018Component::queue_seek_() {
  uint8_t pkt[2] = {1, CMD_SELECT};
  queue_command_(pkt, 2);
  // After transmit, cmd_ will be overridden to CMD_SEEK in transmit_pending_
  read_step_ = ReadStep::SEEK;
}

void SL018Component::queue_auth_() {
  // NFC Forum NDEF-formatted cards (written by NFC Tools) use key D3F7D3F7D3F7
  // for sectors 1+ (key A, type 0xAA)
  uint8_t sector = ndef_block_ / 4;
  uint8_t pkt[10] = {9, CMD_LOGIN, sector, 0xAA, 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
  queue_command_(pkt, 10);
  read_step_ = ReadStep::AUTH;
}

void SL018Component::queue_read_block_() {
  uint8_t block = ndef_block_ + ndef_blocks_read_;
  uint8_t pkt[3] = {2, CMD_READ16, block};
  queue_command_(pkt, 3);
  read_step_ = ReadStep::READ_BLOCK;
}

void SL018Component::reset_to_seek_() {
  pending_len_ = 0;
  queue_seek_();
}

void SL018Component::seek_tag() { reset_to_seek_(); }

void SL018Component::led(bool on) {
  uint8_t pkt[3] = {2, CMD_SET_LED, (uint8_t)(on ? 1 : 0)};
  queue_command_(pkt, 3);
}

// ── I2C helpers ───────────────────────────────────────────────────────────────

bool SL018Component::transmit_pending_() {
  if (pending_len_ == 0) {
    queue_seek_();
  }
  ESP_LOGV(TAG, "TX step=%d cmd=%02X", (int) read_step_, pending_data_[1]);
  auto err = this->write(pending_data_, pending_len_);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write failed (err %d)", (int) err);
    return false;
  }
  cmd_ = pending_data_[1];
  // SELECT sent as seek: track as CMD_SEEK
  if (read_step_ == ReadStep::SEEK)
    cmd_ = CMD_SEEK;
  pending_len_ = 0;
  return true;
}

bool SL018Component::receive_data_(uint8_t length) {
  if (length > SIZE_PACKET)
    length = SIZE_PACKET;

  auto err = this->read(data_, length);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed (err %d)", (int) err);
    return false;
  }

  if (data_[0] == 0)
    return false;  // module not ready

  return true;
}

// ── Response processing ───────────────────────────────────────────────────────

void SL018Component::process_response_() {
  error_code_ = data_[2];

  switch (read_step_) {

    case ReadStep::SEEK: {
      if (error_code_ == ERR_OK && data_[0] >= 7) {
        tag_length_ = data_[0] - 3;
        tag_type_   = data_[data_[0]];
        memcpy(tag_number_, data_ + 3, tag_length_);
        array_to_hex_(tag_string_, tag_number_, tag_length_);
        tag_uid_ = std::string(tag_string_);
        ESP_LOGD(TAG, "Tag selected: %s (%s)", tag_string_, get_tag_name());
        queue_auth_();
      } else {
        // No tag yet
        queue_seek_();
      }
      break;
    }

    case ReadStep::AUTH: {
      if (error_code_ == ERR_OK || error_code_ == ERR_LOGIN_OK) {
        ESP_LOGD(TAG, "Auth OK, reading %d blocks from block %d", NDEF_BLOCKS, ndef_block_);
        ndef_blocks_read_ = 0;
        memset(ndef_buf_, 0, sizeof(ndef_buf_));
        queue_read_block_();
      } else {
        ESP_LOGW(TAG, "Auth failed (err %02X), retrying seek", error_code_);
        queue_seek_();
      }
      break;
    }

    case ReadStep::READ_BLOCK: {
      if (error_code_ == ERR_OK && data_[0] >= 3) {
        // Append the 16 block bytes into ndef_buf_
        memcpy(ndef_buf_ + ndef_blocks_read_ * 16, data_ + 3, 16);
        ndef_blocks_read_++;

        if (ndef_blocks_read_ < NDEF_BLOCKS) {
          // Read next block
          queue_read_block_();
        } else {
          // All blocks read — parse NDEF
          std::string text = parse_ndef_text_(ndef_buf_, NDEF_BUF_LEN);
          if (!text.empty() && text != last_published_uid_) {
            last_published_uid_ = text;
            tag_uid_ = text;
            ESP_LOGI(TAG, "NDEF text: '%s'", text.c_str());
            if (tag_sensor_ != nullptr)
              tag_sensor_->publish_state(text);
          } else if (text.empty()) {
            ESP_LOGW(TAG, "No NDEF text record found");
          }
          read_step_ = ReadStep::DONE;
        }
      } else {
        ESP_LOGW(TAG, "Block read failed (err %02X)", error_code_);
        queue_seek_();
      }
      break;
    }

    case ReadStep::DONE:
      // Tag still present, nothing to do — wait for removal via tag_pin_
      // If no tag pin, re-seek so we detect a card swap
      if (tag_pin_ == nullptr)
        queue_seek_();
      else
        state_ = State::RECEIVE;  // stay in receive, do nothing
      break;
  }
}

// ── NDEF Text record parser ───────────────────────────────────────────────────
//
// NFC Tools writes a standard NDEF message in the Mifare data block:
//
//   [03][len][D1][01][text_payload_len][54][lang_len][lang...][text...]...[FE]
//    ^    ^    ^   ^         ^          ^      ^         ^        ^
//    |    |    |   |         |          |      |         |        actual text
//    |    |    |   |         |      TNF=T  lang code  e.g. "en"
//    |    |    |   type_len=01
//    |    |   MB+ME+SR+TNF=0x01 (Well Known)
//    | NDEF msg len
//   TLV tag 0x03
//
// We scan for TLV tag 0x03, then walk the NDEF record header to find the text.

std::string SL018Component::parse_ndef_text_(const uint8_t *buf, uint8_t buf_len) {
  uint8_t i = 0;

  // Walk TLV blocks
  while (i < buf_len) {
    uint8_t tlv_tag = buf[i++];
    if (tlv_tag == 0xFE || i >= buf_len) break;  // Terminator
    if (tlv_tag == 0x00) continue;                // Null TLV, no length

    uint8_t tlv_len = buf[i++];
    uint8_t tlv_end = i + tlv_len;
    if (tlv_end > buf_len) tlv_end = buf_len;

    if (tlv_tag == 0x03 && tlv_len > 0) {
      // NDEF Message TLV
      // Record header: [flags][type_len][payload_len][type...][payload...]
      if (i + 3 > buf_len) break;
      // uint8_t flags   = buf[i];   // not needed
      uint8_t type_len    = buf[i + 1];
      uint8_t payload_len = buf[i + 2];
      i += 3;

      // Type field
      if (i + type_len > buf_len) break;
      bool is_text = (type_len == 1 && buf[i] == 0x54);  // 'T'
      i += type_len;

      if (is_text) {
        if (i >= buf_len) break;
        uint8_t lang_len = buf[i++] & 0x3F;  // status byte, low 6 bits = lang length
        i += lang_len;                         // skip language code e.g. "en"

        uint8_t text_len = payload_len - 1 - lang_len;
        if (i + text_len > buf_len) text_len = buf_len - i;
        if (text_len == 0) break;

        return std::string(reinterpret_cast<const char *>(buf + i), text_len);
      }
      // Not a Text record — skip to end of this TLV
      i = tlv_end;
      continue;
    }

    // Unknown TLV — skip
    i = tlv_end;
  }

  return "";
}

void SL018Component::array_to_hex_(char *s, const uint8_t *array, uint8_t len) {
  static const char hex[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < len; i++) {
    *s++ = hex[array[i] >> 4];
    *s++ = hex[array[i] & 0x0F];
  }
  *s = '\0';
}

const char *SL018Component::get_tag_name() const {
  switch (tag_type_) {
    case 1: return "Mifare 1K";
    case 2: return "Mifare Pro";
    case 3: return "Mifare UltraLight";
    case 4: return "Mifare 4K";
    case 5: return "Mifare ProX";
    case 6: return "Mifare DesFire";
    default: return "Unknown";
  }
}

const char *SL018Component::get_error_message() const {
  switch (error_code_) {
    case 0x00: return "OK";
    case 0x01: return "No tag present";
    case 0x02: return "Login OK";
    case 0x03: return "Login failed";
    case 0x04: return "Read failed";
    case 0x05: return "Write failed";
    case 0x06: return "Unable to read after write";
    case 0x0A: return "Collision detected";
    case 0x0C: return "Load key failed";
    case 0x0D: return "Not authenticated";
    case 0x0E: return "Not a value block";
    default:   return "Unknown error";
  }
}

}  // namespace sl018
}  // namespace esphome
