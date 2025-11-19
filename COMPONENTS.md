# Component Catalog

This repository contains the following ESPHome components:

## Available Components

### cst3240

**Type**: Touchscreen Driver
**Status**: Stable
**Platforms**: ESP32, ESP32-C3, ESP32-S2, ESP32-S3
**Frameworks**: Arduino, ESP-IDF

Capacitive touchscreen controller driver for the CST3240 chip with multi-touch support, interrupt-driven operation, and virtual button capabilities.

**Documentation**: See [README.md](README.md#cst3240-touchscreen)
**Examples**:
- [touchscreen.yaml](examples/touchscreen.yaml) - Basic touchscreen setup
- [virtual-buttons.yaml](examples/virtual-buttons.yaml) - Touch regions as binary sensors

**Key Features**:
- Multi-touch support (up to 5 points)
- Interrupt-driven touch detection
- Virtual button regions
- Hardware reset support

---

### udp_audio_streamer

**Type**: Audio Utility
**Status**: Experimental
**Platforms**: ESP32-family
**Frameworks**: ESP-IDF, Arduino

Streams PCM microphone data over UDP on a configurable cadence. Ideal for prototyping wake-word backends or piping audio into custom DSP services without using the built-in voice assistant.

**Documentation**: See [README.md](README.md#udp-audio-streamer)
**Examples**:
- [udp-audio-streamer.yaml](examples/udp-audio-streamer.yaml) – Minimal end-to-end streaming setup

**Key Features**:
- 16–32 bit PCM forwarding at the microphone’s native sample rate
- Adjustable packet cadence and ring buffer depth
- Passive mode for sharing an already-running microphone session

---

### epaper_spi (Spectra 6 Enhancements)

**Type**: Display Driver Override  
**Status**: Experimental  
**Platforms**: ESP32-family  
**Frameworks**: ESP-IDF, Arduino

Override of ESPHome’s `epaper_spi` component that reproduces the Spectra-6 vendor initialization flow, including dual reset pulses and the post-`PON` `BTST2` booster tuning. Required for panels such as the Good Display GDEP040E01 and other E Ink Spectra 6 glass that otherwise stays blank.

**Documentation**: See [README.md](README.md#spectra-6-epaper-driver-enhancements)  
**Examples**: Use the sample configuration in the README as a starting point (adjust pins for your carrier).

**Key Features**:
- Configurable multi-pulse reset handling
- Automatic “second setting” (`BTST2`) immediately after `PON`
- Compatible with ESPHome’s custom `init_sequence` YAML option

---

## Adding New Components

When adding a new component to this repository:

1. **Create component directory** in `components/`:
   ```
   components/
   └── your_component_name/
       ├── __init__.py
       ├── your_component.cpp
       └── your_component.h
   ```

2. **Add component documentation** to README.md following the CST3240 pattern

3. **Create example configurations** in `examples/`

4. **Add test configurations** in `tests/` for CI validation

5. **Update this catalog** with component details:
   - Type and purpose
   - Supported platforms
   - Current status (experimental, stable, deprecated)
   - Link to documentation
   - Key features

6. **Update CI workflow** if needed to test the new component

## Component Status Definitions

- **Experimental**: Early development, API may change
- **Stable**: Production-ready, API is stable
- **Deprecated**: No longer maintained, use alternative component
