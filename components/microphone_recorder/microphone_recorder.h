#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/microphone/microphone_source.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include <cstdio>
#include <mutex>
#include <string>

#include <driver/sdmmc_types.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>

namespace esphome {
namespace microphone_recorder {

class MicrophoneRecorder : public Component {
 public:
  void set_microphone_source(microphone::MicrophoneSource *mic_source) { this->mic_source_ = mic_source; }

  void set_sd_pins(int clk_pin, int cmd_pin, int d0_pin, int d1_pin, int d2_pin, int d3_pin) {
    this->clk_pin_ = clk_pin;
    this->cmd_pin_ = cmd_pin;
    this->d0_pin_ = d0_pin;
    this->d1_pin_ = d1_pin;
    this->d2_pin_ = d2_pin;
    this->d3_pin_ = d3_pin;
  }

  void set_mount_point(const std::string &mount_point) { this->mount_point_ = mount_point; }
  void set_filename_prefix(const std::string &prefix) { this->filename_prefix_ = prefix; }
  void set_max_duration_ms(uint32_t duration_ms) { this->max_duration_ms_ = duration_ms; }
  void set_format_if_mount_failed(bool format_if_failed) { this->format_if_failed_ = format_if_failed; }

  bool start_recording();
  void stop_recording();
  bool is_recording() const { return this->recording_; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  bool mount_sdcard_();
  void unmount_sdcard_();
  bool open_new_file_();
  void close_file_();

  void handle_audio_data_(const std::vector<uint8_t> &data);
  void write_wav_header_(std::FILE *file, uint32_t data_length);
  void update_wav_sizes_();

  microphone::MicrophoneSource *mic_source_{nullptr};
  std::string mount_point_{"/sdcard"};
  std::string filename_prefix_{"rec"};

  int clk_pin_{-1};
  int cmd_pin_{-1};
  int d0_pin_{-1};
  int d1_pin_{-1};
  int d2_pin_{-1};
  int d3_pin_{-1};

  bool format_if_failed_{false};
  bool mounted_{false};

  std::FILE *file_{nullptr};
  std::string active_path_;

  uint32_t data_bytes_written_{0};
  uint32_t recording_start_ms_{0};
  uint32_t max_duration_ms_{10000};

  bool recording_{false};
  bool pending_stop_{false};

  std::mutex write_mutex_;

  sdmmc_card_t *card_{nullptr};
  bool using_spi_host_{false};
  spi_host_device_t spi_host_id_{SPI2_HOST};
  bool spi_bus_initialized_{false};
};

class StartRecordingAction : public esphome::automation::Action,
                             public esphome::automation::Parented<MicrophoneRecorder> {
 public:
  void play(esphome::automation::ActionContext &ctx) override;
};

class StopRecordingAction : public esphome::automation::Action,
                            public esphome::automation::Parented<MicrophoneRecorder> {
 public:
  void play(esphome::automation::ActionContext &ctx) override;
};

}  // namespace microphone_recorder
}  // namespace esphome

#endif  // USE_ESP32
