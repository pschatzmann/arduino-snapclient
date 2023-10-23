#include "AudioToolsAPI.h"
#include "SnapClient.h"

SnapClient *selfSnapClient=nullptr; 

bool audio_begin(uint32_t sample_rate, uint8_t bits){
    AudioInfo info;
    info.sample_rate = sample_rate;
    info.bits_per_sample = bits;
    info.channels = 2;
    selfSnapClient->output().setAudioInfo(info);
    return selfSnapClient->output().begin();
}

void audio_end() { selfSnapClient->output().end(); }

bool audio_write(const void *src, size_t size, size_t *bytes_written, int ticks_to_wait) {
    *bytes_written = selfSnapClient->output().write((const uint8_t*)src, size);
    return *bytes_written > 0;
}

bool audio_write_expand(const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written, int ticks_to_wait){
    *bytes_written = selfSnapClient->output().write((const uint8_t*)src, size);
    return *bytes_written > 0;
}
