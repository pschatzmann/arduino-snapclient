# Snapcast client for Arduino ESP32

### [Snapcast](https://github.com/badaix/snapcast) Audio Streaming Client for ESP32

## Feature list

- Opus and PCM decoding are currently supported
- Auto connect to snapcast server on network
- Buffers up to 150 ms on Wroom modules
- Buffers more then enough on Wrover modules using PSRAM
- Multiroom sync delay controlled from Snapcast server 400ms - 2000ms

## Description

I have converted the [snapclient](https://github.com/jorgenkraghjakobsen/snapclient) from Jørgen Kragh Jakobsen to an Arduino Library and integrated my AudioTools project to be used to define the output devices.

## Dependencies

- [Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)
- [LibOpus](https://github.com/pschatzmann/arduino-libopus)

