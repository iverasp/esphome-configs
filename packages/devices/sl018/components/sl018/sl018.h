#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace sl018 {

static const uint8_t SIZE_PACKET = 19;

// Tag types
static const uint8_t MIFARE_1K          = 1;
static const uint8_t MIFARE_PRO         = 2;
static const uint8_t MIFARE_ULTRALIGHT  = 3;
static const uint8_t MIFARE_4K          = 4;
static const uint8_t MIFARE_PROX        = 5;
static const uint8_t MIFARE_DESFIRE     = 6;

// Commands
static const uint8_t CMD_IDLE       = 0x00;
static const uint8_t CMD_SELECT     = 0x01;
static const uint8_t CMD_LOGIN      = 0x02;
static const uint8_t CMD_READ16     = 0x03;
static const uint8_t CMD_WRITE16    = 0x04;
static const uint8_t CMD_READ_VALUE = 0x05;
static const uint8_t CMD_WRITE_VALUE= 0x06;
static const uint8_t CMD_WRITE_KEY  = 0x07;
static const uint8_t CMD_INC_VALUE  = 0x08;
static const uint8_t CMD_DEC_VALUE  = 0x09;
static const uint8_t CMD_COPY_VALUE = 0x0A;
static const uint8_t CMD_READ4      = 0x10;
static const uint8_t CMD_WRITE4     = 0x11;
static const uint8_t CMD_SEEK       = 0x20;
static const uint8_t CMD_SET_LED    = 0x40;
static const uint8_t CMD_SLEEP      = 0x50;
static const uint8_t CMD_RESET      = 0xFF;

// Error codes
static const uint8_t ERR_OK         = 0x00;
static const uint8_t ERR_NO_TAG     = 0x01;
static const uint8_t ERR_LOGIN_OK   = 0x02;
static const uint8_t ERR_LOGIN_FAIL = 0x03;
static const uint8_t ERR_READ_FAIL  = 0x04;
static const uint8_t ERR_WRITE_FAIL = 0x05;
static const uint8_t ERR_CANT_VERIFY= 0x06;
static const uint8_t ERR_COLLISION  = 0x0A;
static const uint8_t ERR_KEY_FAIL   = 0x0C;
static const uint8_t ERR_NO_LOGIN   = 0x0D;
static const uint8_t ERR_NO_VALUE   = 0x0E;

enum class State {
  SEND,     // Need to transmit a command
  RECEIVE,  // Command sent, waiting to read response
};

// Steps in the NDEF read sequence
enum class ReadStep {
  SEEK,
  AUTH,
  READ_BLOCK,   // reading one block at a time into ndef_buf_
  DONE,
};

static const uint8_t NDEF_BLOCKS = 3;         // blocks 4,5,6 = 48 bytes
static const uint8_t NDEF_BUF_LEN = NDEF_BLOCKS * 16;

class SL018Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_tag_pin(GPIOPin *pin) { tag_pin_ = pin; }
  void set_tag_sensor(text_sensor::TextSensor *sensor) { tag_sensor_ = sensor; }
  // Which block to read for NDEF data (default 4 = sector 1, block 0)
  void set_ndef_block(uint8_t block) { ndef_block_ = block; }

  const std::string &get_tag_uid() const { return tag_uid_; }
  uint8_t get_tag_type() const { return tag_type_; }
  const char *get_tag_name() const;
  uint8_t get_error_code() const { return error_code_; }
  const char *get_error_message() const;

  void seek_tag();
  void led(bool on);

 protected:
  void queue_command_(uint8_t *pkt, uint8_t total_len);
  void queue_seek_();
  void queue_auth_();
  void queue_read_block_();
  bool transmit_pending_();
  bool receive_data_(uint8_t length);
  void process_response_();
  std::string parse_ndef_text_(const uint8_t *block_data, uint8_t block_len);
  void array_to_hex_(char *s, const uint8_t *array, uint8_t len);
  void reset_to_seek_();

  GPIOPin *tag_pin_{nullptr};
  text_sensor::TextSensor *tag_sensor_{nullptr};

  uint8_t ndef_block_{4};  // Mifare 1K: first NDEF data block (sector 1, block 0)

  uint8_t data_[SIZE_PACKET]{};
  uint8_t pending_data_[SIZE_PACKET]{};
  uint8_t pending_len_{0};

  uint8_t ndef_buf_[NDEF_BUF_LEN]{};  // accumulated raw block bytes
  uint8_t ndef_blocks_read_{0};        // blocks collected so far

  uint8_t tag_number_[7]{};
  uint8_t tag_length_{0};
  char tag_string_[15]{};
  uint8_t tag_type_{0};
  uint8_t error_code_{0};
  uint8_t cmd_{CMD_IDLE};

  State state_{State::SEND};
  ReadStep read_step_{ReadStep::SEEK};
  uint32_t last_tx_ms_{0};

  std::string tag_uid_;
  std::string last_published_uid_;
};

}  // namespace sl018
}  // namespace esphome
