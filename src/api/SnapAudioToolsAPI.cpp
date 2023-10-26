#include "SnapAudioToolsAPI.h"
#include "SnapClient.h"

SnapClient *selfSnapClient = nullptr;
static const char*TAG = "API";


bool audio_begin(uint32_t sample_rate, uint8_t bits) {
  ESP_LOGI(TAG, "sample_rate: %d", sample_rate);
  return selfSnapClient->snap_audio_begin(sample_rate, bits);
}

void audio_end() { 
  ESP_LOGI(TAG);
  selfSnapClient->snap_audio_end();
}

bool audio_write(const void *src, size_t size, size_t *bytes_written) {
  ESP_LOGI(TAG, "%d", size);
  return selfSnapClient->snap_audio_write(src, size, bytes_written);
}

void audio_mute(bool mute) { 
  ESP_LOGI(TAG, "mute: %s", mute ? "true": "false");

  selfSnapClient->setMute(mute); 
}

void audio_set_volume(float vol) { selfSnapClient->setVolume(vol); }
