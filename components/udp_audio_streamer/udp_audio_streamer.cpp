#include "udp_audio_streamer.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

namespace esphome {
namespace udp_audio_streamer {

static const char *const TAG = "udp_audio_streamer";

UDPAudioStreamer::~UDPAudioStreamer() { this->deallocate_buffers_(); }

void UDPAudioStreamer::set_endpoint(const std::string &host, uint16_t port) {
  this->host_ = host;
  this->port_ = port;
}

void UDPAudioStreamer::setup() {
  if (this->mic_source_ == nullptr) {
    ESP_LOGE(TAG, "Microphone source not configured");
    this->mark_failed();
    return;
  }

  if (this->host_.empty() || this->port_ == 0) {
    ESP_LOGE(TAG, "Destination host and port must be provided");
    this->mark_failed();
    return;
  }

  socklen_t sl = socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&this->dest_addr_), sizeof(this->dest_addr_),
                                      this->host_, this->port_);
  if (sl == 0) {
    ESP_LOGE(TAG, "Invalid destination address '%s:%u'", this->host_.c_str(), this->port_);
    this->mark_failed();
    return;
  }
  this->endpoint_valid_ = true;

  this->audio_stream_info_ = this->mic_source_->get_audio_stream_info();
  ESP_LOGI(TAG, "Configuring UDP stream to %s:%u (%u Hz, %u channel(s), %u-bit samples)", this->host_.c_str(),
           this->port_, this->audio_stream_info_.get_sample_rate(), this->audio_stream_info_.get_channels(),
           this->audio_stream_info_.get_bits_per_sample());

  this->send_buffer_size_ = this->audio_stream_info_.ms_to_bytes(this->chunk_duration_ms_);
  if (this->send_buffer_size_ == 0) {
    this->send_buffer_size_ = this->audio_stream_info_.frames_to_bytes(1);
  }
  if (this->send_buffer_size_ == 0) {
    ESP_LOGE(TAG, "Unable to determine audio frame size");
    this->mark_failed();
    return;
  }

  this->ring_buffer_size_ = this->audio_stream_info_.ms_to_bytes(this->buffer_duration_ms_);
  if (this->ring_buffer_size_ < this->send_buffer_size_ * 2) {
    this->ring_buffer_size_ = this->send_buffer_size_ * 4;
  }

  if (!this->allocate_buffers_()) {
    ESP_LOGE(TAG, "Failed to allocate audio buffers");
    this->mark_failed();
    return;
  }

  this->mic_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    std::shared_ptr<RingBuffer> ring = this->ring_buffer_;
    if (!ring) {
      return;
    }
    size_t written = ring->write(data.data(), data.size());
    if (written < data.size()) {
      if (!this->warned_full_) {
        ESP_LOGW(TAG, "Ring buffer full, dropping %zu bytes", data.size() - written);
        this->warned_full_ = true;
      }
    } else {
      this->warned_full_ = false;
    }
  });

  if (!this->passive_ && !this->mic_source_->is_running()) {
    ESP_LOGD(TAG, "Starting microphone source");
    this->mic_source_->start();
  }
}

void UDPAudioStreamer::loop() {
  if (this->is_failed() || !this->endpoint_valid_) {
    return;
  }

  if (!this->allocate_buffers_()) {
    this->status_momentary_error("buffer_alloc", 1000);
    return;
  }

  if (!this->ensure_socket_()) {
    this->status_set_warning();
    return;
  }

  if (!this->passive_ && !this->mic_source_->is_running()) {
    ESP_LOGD(TAG, "Starting microphone source");
    this->mic_source_->start();
  }

  std::shared_ptr<RingBuffer> ring = this->ring_buffer_;
  if (!ring || this->send_buffer_size_ == 0) {
    return;
  }

  size_t available = ring->available();
  while (available >= this->send_buffer_size_) {
    size_t read_bytes = ring->read(this->send_buffer_, this->send_buffer_size_, 0);
    if (read_bytes == 0) {
      break;
    }

    if (this->audio_stream_info_.get_bits_per_sample() == 16) {
      for (size_t i = 0; i + 1 < read_bytes; i += 2) {
        std::swap(this->send_buffer_[i], this->send_buffer_[i + 1]);
      }
    }

    ssize_t sent = this->socket_->sendto(this->send_buffer_, read_bytes, 0,
                                         reinterpret_cast<struct sockaddr *>(&this->dest_addr_),
                                         sizeof(this->dest_addr_));
    if (sent < 0) {
      if (!this->status_has_warning()) {
        ESP_LOGW(TAG, "sendto failed: errno=%d", errno);
      }
      this->status_set_warning();
      break;
    }
    if (static_cast<size_t>(sent) != read_bytes) {
      if (!this->status_has_warning()) {
        ESP_LOGW(TAG, "Partial UDP write: %d/%zu bytes", static_cast<int>(sent), read_bytes);
      }
      this->status_set_warning();
      break;
    }
    this->status_clear_warning();
    if (!this->streaming_logged_) {
      ESP_LOGI(TAG, "Streaming audio packets (%zu bytes) to %s:%u", read_bytes, this->host_.c_str(), this->port_);
      this->streaming_logged_ = true;
    }
    this->bytes_since_log_ += read_bytes;
    this->packets_since_log_ += 1;
    uint32_t now = millis();
    if (this->last_rate_log_ms_ == 0) {
      this->last_rate_log_ms_ = now;
    }
    uint32_t elapsed = now - this->last_rate_log_ms_;
    if ((elapsed >= 1000) && (this->bytes_since_log_ > 0)) {
      uint32_t bytes_per_sec = (this->bytes_since_log_ * 1000U) / elapsed;
      ESP_LOGD(TAG, "Throughput: %u B/s across %u packets", bytes_per_sec, this->packets_since_log_);
      this->bytes_since_log_ = 0;
      this->packets_since_log_ = 0;
      this->last_rate_log_ms_ = now;
    }
    available = ring->available();
  }
}

void UDPAudioStreamer::dump_config() {
  ESP_LOGCONFIG(TAG, "UDP Audio Streamer:");
  ESP_LOGCONFIG(TAG, "  Destination: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Passive: %s", YESNO(this->passive_));
  ESP_LOGCONFIG(TAG, "  Chunk duration: %u ms (%zu bytes)", this->chunk_duration_ms_, this->send_buffer_size_);
  ESP_LOGCONFIG(TAG, "  Buffer duration: %u ms (%zu bytes)", this->buffer_duration_ms_, this->ring_buffer_size_);
  if (this->mic_source_ != nullptr) {
    const auto info = this->mic_source_->get_audio_stream_info();
    ESP_LOGCONFIG(TAG, "  Audio stream:");
    ESP_LOGCONFIG(TAG, "    Sample rate: %u Hz", info.get_sample_rate());
    ESP_LOGCONFIG(TAG, "    Channels: %u", info.get_channels());
    ESP_LOGCONFIG(TAG, "    Bits per sample: %u", info.get_bits_per_sample());
  }
}

bool UDPAudioStreamer::allocate_buffers_() {
  if (this->send_buffer_size_ == 0 || this->ring_buffer_size_ == 0) {
    return false;
  }

  if (!this->send_buffer_) {
    RAMAllocator<uint8_t> allocator;
    this->send_buffer_ = allocator.allocate(this->send_buffer_size_);
    if (this->send_buffer_ == nullptr) {
      ESP_LOGW(TAG, "Failed to allocate send buffer (%zu bytes)", this->send_buffer_size_);
      return false;
    }
  }

  if (this->ring_buffer_.use_count() == 0) {
    auto buffer = RingBuffer::create(this->ring_buffer_size_);
    if (!buffer) {
      ESP_LOGW(TAG, "Failed to create ring buffer (%zu bytes)", this->ring_buffer_size_);
      return false;
    }
    this->ring_buffer_ = std::shared_ptr<RingBuffer>(std::move(buffer));
    this->warned_full_ = false;
  }

  return true;
}

void UDPAudioStreamer::deallocate_buffers_() {
  if (this->send_buffer_) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->send_buffer_, this->send_buffer_size_);
    this->send_buffer_ = nullptr;
  }

  if (this->ring_buffer_.use_count() > 0) {
    this->ring_buffer_.reset();
  }
}

bool UDPAudioStreamer::ensure_socket_() {
  if (this->socket_ != nullptr) {
    return true;
  }

  auto sock = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
  if (sock == nullptr) {
    ESP_LOGW(TAG, "Failed to create UDP socket");
    return false;
  }

  if (sock->setblocking(false) != 0) {
    ESP_LOGW(TAG, "Failed to set socket non-blocking mode");
    return false;
  }

  this->socket_ = std::move(sock);
  if (!this->socket_logged_) {
    ESP_LOGD(TAG, "UDP socket created for %s:%u", this->host_.c_str(), this->port_);
    this->socket_logged_ = true;
  }
  return true;
}

}  // namespace udp_audio_streamer
}  // namespace esphome

#endif  // USE_ESP32
