#pragma once

#include "Arduino.h" // for ESP.getPsramSize()
#include "AudioTools.h"
#include "SnapCommon.h"
#include "SnapConfig.h"
#include "SnapLogger.h"
#include "SnapTime.h"
#include "SnapTimeSync.h"
#include <stdint.h>
#include <sys/time.h>

/**
 * @brief Simple Output Class which uses the AudioTools to build an output chain
 * with volume control and a resampler
 * @author Phil Schatzmann
 * @version 0.1
 * @date 2023-10-28
 * @copyright Copyright (c) 2023
 **/

class SnapOutput {

public:
  // singleton: only access via instance class method
  SnapOutput() {
    audio_info.sample_rate = 48000;
    audio_info.channels = 2;
    audio_info.bits_per_sample = 16;
  }

  /// Starts the processing which is also starting the the dsp_i2s_task_handler
  /// task
  virtual bool begin() {
    is_sync_started = false;
    return true;
  }

  /// Writes audio data to the queue
  virtual size_t write(const uint8_t *data, size_t size) {
    ESP_LOGD(TAG, "%zu", size);

    if (!synchronizePlayback()) {
      return size;
    }

    return audioWrite(data, size);
  }

  /// Provides info about the audio data
  virtual bool writeHeader(SnapAudioHeader &header) {
    this->header = header;
    return true;
  }

  /// Ends the processing and releases the memory
  virtual void end(void) {}

  /// Adjust the volume
  void setVolume(float vol) {
    this->vol = vol / 100.0;
    ESP_LOGI(TAG, "Volume: %f", this->vol);
    vol_stream.setVolume(this->vol * vol_factor);
  }

  /// provides the actual volume
  float volume() { return vol; }

  /// mute / unmute
  void setMute(bool mute) {
    is_mute = mute;
    vol_stream.setVolume(mute ? 0 : vol);
    audioWriteSilence();
  }

  /// checks if volume is mute
  bool isMute() { return is_mute; }

  /// Adjust volume by factor e.g. 1.5
  void setVolumeFactor(float fact) { vol_factor = fact; }

  /// Defines the audio output chain to the final output
  void setOutput(AudioOutput &output) {
    this->out = &output; // final output
    resample.setStream(output);
    vol_stream.setStream(resample);        // adjust volume
    decoder_stream.setStream(&vol_stream); // decode to pcm
  }

  /// Defines the decoder class
  void setDecoder(AudioDecoder &dec) { decoder_stream.setDecoder(&dec); }

  /// setup of all audio objects
  bool audioBegin(uint32_t sample_rate, uint8_t channels, uint8_t bits) {
    ESP_LOGI(TAG, "sample_rate: %d, channels: %d, bits: %d", sample_rate,
             channels, bits);
    if (out == nullptr)
      return false;

    audio_info.sample_rate = sample_rate;
    audio_info.bits_per_sample = bits;
    audio_info.channels = channels;

    // open volume control: allow amplification
    auto vol_cfg = vol_stream.defaultConfig();
    vol_cfg.copyFrom(audio_info);
    vol_cfg.allow_boost = true;
    vol_cfg.channels = 2;
    vol_cfg.bits_per_sample = 16;
    vol_stream.begin(vol_cfg);

    // open final output
    out->setAudioInfo(audio_info);
    out->begin();

    // open decoder
    auto dec_cfg = decoder_stream.defaultConfig();
    dec_cfg.copyFrom(audio_info);
    decoder_stream.begin(dec_cfg);

    // open resampler
    auto res_cfg = resample.defaultConfig();
    res_cfg.step_size = 1.009082;
    res_cfg.copyFrom(audio_info);
    resample.begin(res_cfg);

    ESP_LOGD(TAG, "end");
    return true;
  }

  /// Defines the time synchronization logic
  void setSnapTimeSync(SnapTimeSync &timeSync){
    p_snap_time_sync = &timeSync;
  }

  // do nothing
  virtual void doLoop() {}

  SnapTimeSync& snapTimeSync() {
    return *p_snap_time_sync;
  }

protected:
  const char *TAG = "SnapOutput";
  AudioOutput *out = nullptr;
  AudioInfo audio_info;
  EncodedAudioStream decoder_stream;
  VolumeStream vol_stream;
  ResampleStream resample;
  float vol = 1.0;        // volume in the range 0.0 - 1.0
  float vol_factor = 1.0; //
  bool is_mute = false;
  SnapAudioHeader header;
  SnapTime &snap_time = SnapTime::instance();
  SnapTimeSyncDynamic time_sync_default;
  SnapTimeSync *p_snap_time_sync = &time_sync_default;
  bool is_sync_started = false;

  /// to speed up or slow down playback
  void setPlaybackFactor(float fact) { resample.setStepSize(fact); }

  /// determine actual playback speed
  float playbackFactor() { return resample.getStepSize(); }

  void audioWriteSilence() {
    for (int j = 0; j < 50; j++) {
      out->writeSilence(1024);
    }
  }

  void audioEnd() {
    ESP_LOGD(TAG, "start");
    if (out == nullptr)
      return;
    out->end();
  }

  // writes the audio data to the decoder
  size_t audioWrite(const void *src, size_t size) {
    ESP_LOGD(TAG, "%zu", size);
    size_t result = decoder_stream.write((const uint8_t *)src, size);

    return result;
  }

  /// start to play audio only in valid server time: return false if to be
  /// ignored - update playback speed
  bool synchronizePlayback() {
    bool result = true;

    auto delay_ms = getDelayMs();
    SnapTimeSync& ts = *p_snap_time_sync;

    if (!is_sync_started) {

      ts.begin(audio_info.sample_rate);

      // start audio when first package in the future becomes valid
      result = synchronizeOnStart(delay_ms);
    } else {

      if (ts.isSync()) {
        // update speed
        float current_factor = playbackFactor();
        float new_factor = p_snap_time_sync->getFactor();
        if (new_factor != current_factor) {
          setPlaybackFactor(new_factor);
        }
      }
    }
    return result;
  }

  bool synchronizeOnStart(int delay_ms) {
    bool result = true;
    if (delay_ms < 0) {
      // ignore the data and report it as processed
      ESP_LOGW(TAG, "audio data expired: delay %d", delay_ms);
      result = false;
    } else if (delay_ms > 100000) {
      ESP_LOGW(TAG, "invalid delay: %d ms", delay_ms);
      result = false;
    } else {
      // wait for the audio to become valid
      ESP_LOGI(TAG, "starting after %d ms", delay_ms);
      delay(delay_ms);
      is_sync_started = true;
      result = true;
    }
    return result;
  }

  /// Calculate the delay in ms
  int getDelayMs() {
    auto msg_time = snap_time.toMillis(header.sec, header.usec);
    auto server_time = snap_time.serverMillis();
    // wait for the audio to become valid
    int diff_ms = msg_time - server_time;
    int delay_ms = diff_ms + p_snap_time_sync->getStartDelay();
    return delay_ms;
  }
};