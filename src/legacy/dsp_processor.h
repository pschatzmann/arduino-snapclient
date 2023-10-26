#ifndef _DSP_PROCESSOR_H_
#define _DSP_PROCESSOR_H_
#include <stdint.h>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// enum dspFlows {
//   dspfStereo,
//   dspfBiamp,
//   dspf2DOT1,
//   dspfFunkyHonda,
//   dspfBassBoost
// };

void dsp_i2s_task_handler_legacy(void *arg);

void http_gettask(void *pvParameters);

void dsp_i2s_task_init(uint32_t sample_rate, bool slave);

size_t write_ringbuf(const uint8_t *data, size_t size);

void dsp_i2s_task_deinit(void);

// enum filtertypes {
//   LPF,
//   HPF,
//   BPF,
//   BPF0DB,
//   NOTCH,
//   ALLPASS360,
//   ALLPASS180,
//   PEAKINGEQ,
//   LOWSHELF,
//   HIGHSHELF
// };

// // Process node
// typedef struct ptype {
//   int filtertype;
//   float freq;
//   float gain;
//   float q;
//   float *in, *out;
//   float coeffs[5];
//   float w[2];
// } ptype_t;

// // Process flow
// typedef struct pnode {
//   ptype_t process;
//   struct pnode *next;
// } pnode_t;

// void dsp_setup_flow(double freq, uint32_t samplerate);
// void dsp_set_xoverfreq(uint8_t, uint8_t, uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* _DSP_PROCESSOR_H_  */
