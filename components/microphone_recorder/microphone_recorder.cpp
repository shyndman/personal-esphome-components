#include "microphone_recorder.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_defs.h>

namespace esphome {
namespace microphone_recorder {

static const char *const TAG = "microphone_recorder";

void MicrophoneRecorder::setup() {
  if (this->mic_source_ == nullptr) {
    ESP_LOGE(TAG, "Microphone source not configured");
    this->mark_failed();
    return;
  }

  if (this->clk_pin_ < 0 || this->cmd_pin_ < 0 || this->d0_pin_ < 0) {
    ESP_LOGE(TAG, "SD card pins not fully specified");
    this->mark_failed();
    return;
  }

  if (!this->mount_sdcard_()) {
    ESP_LOGE(TAG, "Failed to mount SD card");
    this->mark_failed();
    return;
  }

  auto recorder_callback = [this](const std::vector<uint8_t> &data) { this->handle_audio_data_(data); };
  this->mic_source_->add_data_callback(std::move(recorder_callback));
}

void MicrophoneRecorder::loop() {
  if (!this->recording_) {
    return;
  }

  if (this->max_duration_ms_ > 0) {
    uint32_t elapsed = millis() - this->recording_start_ms_;
    if (elapsed >= this->max_duration_ms_) {
      this->pending_stop_ = true;
    }
  }

  if (this->pending_stop_) {
    this->stop_recording();
  }
}

void MicrophoneRecorder::dump_config() {
  ESP_LOGCONFIG(TAG, "Microphone Recorder:");
  ESP_LOGCONFIG(TAG, "  Mount point: %s", this->mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  File prefix: %s", this->filename_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Max duration: %u ms", this->max_duration_ms_);
  ESP_LOGCONFIG(TAG, "  Pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d", this->clk_pin_, this->cmd_pin_, this->d0_pin_,
                this->d1_pin_, this->d2_pin_, this->d3_pin_);
}

bool MicrophoneRecorder::mount_sdcard_() {
  if (this->mounted_) {
    return true;
  }
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = this->format_if_failed_,
      .max_files = 8,
      .allocation_unit_size = 0,
  };

  bool use_spi = (this->d1_pin_ < 0 && this->d2_pin_ < 0 && this->d3_pin_ >= 0);

  if (use_spi) {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = this->cmd_pin_,
        .miso_io_num = this->d0_pin_,
        .sclk_io_num = this->clk_pin_,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "spi_bus_initialize failed (%s)", esp_err_to_name(ret));
      return false;
    }
    this->spi_bus_initialized_ = (ret == ESP_OK);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = this->d3_pin_;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(this->mount_point_.c_str(), &host, &slot_config, &mount_config, &this->card_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "esp_vfs_fat_sdspi_mount failed (%s)", esp_err_to_name(ret));
      if (this->spi_bus_initialized_) {
        spi_bus_free(host.slot);
        this->spi_bus_initialized_ = false;
      }
      return false;
    }

    this->using_spi_host_ = true;
    this->spi_host_id_ = host.slot;
  } else {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = (this->d3_pin_ >= 0 && this->d2_pin_ >= 0 && this->d1_pin_ >= 0) ? SDMMC_HOST_FLAG_4BIT : SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = (this->d3_pin_ >= 0 && this->d2_pin_ >= 0 && this->d1_pin_ >= 0) ? 4 : 1;
    slot_config.clk = (gpio_num_t) this->clk_pin_;
    slot_config.cmd = (gpio_num_t) this->cmd_pin_;
    slot_config.d0 = (gpio_num_t) this->d0_pin_;
    slot_config.d1 = (gpio_num_t) ((this->d1_pin_ >= 0) ? this->d1_pin_ : -1);
    slot_config.d2 = (gpio_num_t) ((this->d2_pin_ >= 0) ? this->d2_pin_ : -1);
    slot_config.d3 = (gpio_num_t) ((this->d3_pin_ >= 0) ? this->d3_pin_ : -1);
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ret = esp_vfs_fat_sdmmc_mount(this->mount_point_.c_str(), &host, &slot_config, &mount_config, &this->card_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "esp_vfs_fat_sdmmc_mount failed (%s)", esp_err_to_name(ret));
      return false;
    }
    this->using_spi_host_ = false;
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "Mounted SD card at %s", this->mount_point_.c_str());
  return true;
}

void MicrophoneRecorder::unmount_sdcard_() {
  if (!this->mounted_) {
    return;
  }
  esp_vfs_fat_sdcard_unmount(this->mount_point_.c_str(), this->card_);
  if (this->using_spi_host_ && this->spi_bus_initialized_) {
    spi_bus_free(this->spi_host_id_);
    this->spi_bus_initialized_ = false;
  }
  this->mounted_ = false;
  this->card_ = nullptr;
}

bool MicrophoneRecorder::start_recording() {
  if (this->recording_) {
    ESP_LOGW(TAG, "Recording already in progress");
    return false;
  }
  if (!this->mounted_) {
    if (!this->mount_sdcard_()) {
      return false;
    }
  }

  if (!this->open_new_file_()) {
    return false;
  }

  this->data_bytes_written_ = 0;
  this->recording_start_ms_ = millis();
  this->recording_ = true;
  this->pending_stop_ = false;
  ESP_LOGI(TAG, "Recording started: %s", this->active_path_.c_str());
  return true;
}

void MicrophoneRecorder::stop_recording() {
  if (!this->recording_) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(this->write_mutex_);
    this->recording_ = false;
    this->pending_stop_ = false;
    this->update_wav_sizes_();
    this->close_file_();
  }

  ESP_LOGI(TAG, "Recording finished: %s (%u bytes)", this->active_path_.c_str(), this->data_bytes_written_);
}

bool MicrophoneRecorder::open_new_file_() {
  const auto info = this->mic_source_->get_audio_stream_info();
  if (info.get_bits_per_sample() != 16 || info.get_channels() == 0) {
    ESP_LOGE(TAG, "Unsupported audio format for recording");
    return false;
  }

  char filename[64];
  snprintf(filename, sizeof(filename), "%s/%s-%lu.wav", this->mount_point_.c_str(), this->filename_prefix_.c_str(),
           static_cast<unsigned long>(millis()));
  this->active_path_ = filename;

  this->file_ = std::fopen(filename, "wb");
  if (this->file_ == nullptr) {
    ESP_LOGE(TAG, "Failed to open %s for writing", filename);
    return false;
  }

  this->write_wav_header_(this->file_, 0);
  std::fflush(this->file_);
  return true;
}

void MicrophoneRecorder::close_file_() {
  if (this->file_ != nullptr) {
    std::fflush(this->file_);
    std::fclose(this->file_);
    this->file_ = nullptr;
  }
}

void MicrophoneRecorder::write_wav_header_(std::FILE *file, uint32_t data_length) {
  const auto info = this->mic_source_->get_audio_stream_info();
  const uint16_t channels = info.get_channels();
  const uint32_t sample_rate = info.get_sample_rate();
  const uint16_t bits_per_sample = info.get_bits_per_sample();
  const uint16_t block_align = channels * (bits_per_sample / 8);
  const uint32_t byte_rate = sample_rate * block_align;
  const uint32_t chunk_size = 36 + data_length;

  std::fwrite("RIFF", 1, 4, file);
  std::fwrite(&chunk_size, 4, 1, file);
  std::fwrite("WAVE", 1, 4, file);
  std::fwrite("fmt ", 1, 4, file);

  uint32_t subchunk1_size = 16;
  uint16_t audio_format = 1;
  std::fwrite(&subchunk1_size, 4, 1, file);
  std::fwrite(&audio_format, 2, 1, file);
  std::fwrite(&channels, 2, 1, file);
  std::fwrite(&sample_rate, 4, 1, file);
  std::fwrite(&byte_rate, 4, 1, file);
  std::fwrite(&block_align, 2, 1, file);
  std::fwrite(&bits_per_sample, 2, 1, file);

  std::fwrite("data", 1, 4, file);
  std::fwrite(&data_length, 4, 1, file);
}

void MicrophoneRecorder::update_wav_sizes_() {
  if (this->file_ == nullptr) {
    return;
  }

  uint32_t data_length = this->data_bytes_written_;
  uint32_t chunk_size = 36 + data_length;

  std::fseek(this->file_, 4, SEEK_SET);
  std::fwrite(&chunk_size, 4, 1, this->file_);
  std::fseek(this->file_, 40, SEEK_SET);
  std::fwrite(&data_length, 4, 1, this->file_);
  std::fflush(this->file_);
}

void MicrophoneRecorder::handle_audio_data_(const std::vector<uint8_t> &data) {
  if (!this->recording_) {
    return;
  }
  std::lock_guard<std::mutex> lock(this->write_mutex_);
  if (!this->recording_ || this->file_ == nullptr) {
    return;
  }

  size_t written = std::fwrite(data.data(), 1, data.size(), this->file_);
  if (written != data.size()) {
    ESP_LOGW(TAG, "Short write to recording file (%zu/%zu)", written, data.size());
    this->pending_stop_ = true;
    return;
  }
  this->data_bytes_written_ += written;
}

void StartRecordingAction::play(automation::ActionContext &ctx) {
  this->parent_->start_recording();
  this->play_next(ctx);
}

void StopRecordingAction::play(automation::ActionContext &ctx) {
  this->parent_->stop_recording();
  this->play_next(ctx);
}

}  // namespace microphone_recorder
}  // namespace esphome

#endif  // USE_ESP32
