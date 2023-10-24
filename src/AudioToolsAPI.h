/**
 * The oringinal client library was implemented in C and has quite some C components left.
 * We provide a common audio output C api that is using the AudioTools C++ library.
*/
#pragma once
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

bool audio_begin(uint32_t sample_rate, uint8_t bits );
void audio_end();
bool audio_write(const void *src, size_t size, size_t *bytes_written);
bool audio_write_expand(const void *src, size_t size, size_t src_bits, size_t aim_bits, size_t *bytes_written);

#ifdef __cplusplus
}
#endif
