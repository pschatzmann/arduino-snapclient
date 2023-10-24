#ifndef _RTPRX_H_
#define _RTPRX_H_

#include "config.h"

void rtp_rx_start(void);
void rtp_rx_stop(void);
void rtp_rx_task(void *pvParameters);
void setup_rtp_i2s();

#endif /* _RTPRX_H_  */
