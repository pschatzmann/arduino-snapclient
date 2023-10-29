# Snapcast Client for Arduino 

### Snapcast Audio Streaming Client for Arduino

[Snapcast](https://github.com/badaix/snapcast) is a multiroom client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio

### Feature list

- Opus and PCM decoding are currently supported
- Auto connect to snapcast server on network
- The functionality has been tested on an ESP32
- Memory efficient implementation, so that PSRAM is not needed
- Use any output device suppored by the [Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)

### Description

I have converted the [snapclient](https://github.com/jorgenkraghjakobsen/snapclient) from Jørgen Kragh Jakobsen to an Arduino Library and integrated my AudioTools project to be used to define the output devices.

### Dependencies

- [Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)
- [LibOpus](https://github.com/pschatzmann/arduino-libopus)


### Configuration

Configuration settings can be found in [SnapConfig.h](https://github.com/pschatzmann/arduino-snapcast/blob/main/src/SnapConfig.h)


### Documentation

For further information consult the [class documentation](https://pschatzmann.github.io/arduino-snapclient/html/annotated.html) or the [examples](examples/).


