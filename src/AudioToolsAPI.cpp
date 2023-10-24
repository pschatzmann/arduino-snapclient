#include "AudioToolsAPI.h"
#include "SnapClient.h"

SnapClient *selfSnapClient=nullptr; 

bool audio_begin(uint32_t sample_rate, uint8_t bits){
    return selfSnapClient->snap_audio_begin(sample_rate, bits);
}

void audio_end() { selfSnapClient->snap_audio_end(); }

bool audio_write(const void *src, size_t size, size_t *bytes_written) {
    return selfSnapClient->snap_audio_write(src, size, bytes_written);
}

bool audio_write_expand(const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written){
    return selfSnapClient->snap_audio_write(src, size, bytes_written);
}
