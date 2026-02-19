#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/ring_buffer.h"

#include <cstdint>
#include <memory>
#include <string>

namespace esphome {
namespace udp_audio_streamer {

class UDPAudioStreamer : public Component {
public:
  ~UDPAudioStreamer();

  void set_microphone_source(microphone::MicrophoneSource *mic_source) {
    this->mic_source_ = mic_source;
  }
  void set_endpoint(const std::string &host, uint16_t port);
  void set_chunk_duration(uint32_t chunk_duration_ms) {
    this->chunk_duration_ms_ = chunk_duration_ms;
  }
  void set_buffer_duration(uint32_t buffer_duration_ms) {
    this->buffer_duration_ms_ = buffer_duration_ms;
  }
  void set_passive(bool passive) { this->passive_ = passive; }

  void setup() override;
  void loop() override;
  void dump_config() override;

protected:
  bool allocate_buffers_();
  void deallocate_buffers_();
  bool ensure_socket_();

  microphone::MicrophoneSource *mic_source_{nullptr};
  audio::AudioStreamInfo audio_stream_info_;

  std::shared_ptr<RingBuffer> ring_buffer_;
  uint8_t *send_buffer_{nullptr};
  size_t send_buffer_size_{0};
  size_t ring_buffer_size_{0};

  std::unique_ptr<socket::Socket> socket_;
  struct sockaddr_storage dest_addr_{};

  std::string host_;
  uint16_t port_{0};
  uint32_t chunk_duration_ms_{32};
  uint32_t buffer_duration_ms_{512};
  bool passive_{false};
  bool endpoint_valid_{false};
  bool warned_full_{false};
  bool socket_logged_{false};
  bool streaming_logged_{false};
  uint32_t bytes_since_log_{0};
  uint32_t packets_since_log_{0};
  uint32_t last_rate_log_ms_{0};
};

} // namespace udp_audio_streamer
} // namespace esphome

#endif // USE_ESP32
