# Scott's Personal ESPHome Components

A collection of custom ESPHome components for various hardware and integrations.

## Available Components

- **[cst3240](#cst3240-touchscreen)**: CST3240 capacitive touchscreen controller driver
- **[udp_audio_streamer](#udp-audio-streamer)**: Always-on UDP microphone audio streaming

## Installation

Add this external component repository to your ESPHome configuration and specify which component(s) you want to use:

```yaml
external_components:
  - source: github://shyndman/personal-esphome-components
    components: [cst3240]  # Specify which component(s) to load
```

---

## CST3240 Touchscreen

CST3240 capacitive touchscreen controller driver with multi-touch support and virtual button capabilities.

### Features

- **Multi-touch Support**: Up to 5 simultaneous touch points
- **Interrupt-driven**: Efficient GPIO interrupt-based touch detection
- **Virtual Buttons**: Define touch regions as binary sensors
- **Hardware Reset**: Proper initialization sequence with reset pin support
- **Cross-platform**: Compatible with ESP32, ESP32-C3, ESP32-S2, ESP32-S3
- **Framework Support**: Works with both Arduino and ESP-IDF frameworks

### Complete Configuration

See [ESPHome Touchscreen Component](https://esphome.io/components/touchscreen) for more information.

```yaml
# I2C Bus
i2c:
  sda: GPIO21
  scl: GPIO22

# SPI Bus for Display
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23

# Display Configuration
display:
  - platform: ili9xxx
    id: display0
    model: ILI9341
    cs_pin: GPIO5
    dc_pin: GPIO2
    reset_pin: GPIO4
    dimensions: 320x240

# Touchscreen with Calibration
touchscreen:
  - platform: cst3240
    id: touch0
    display: display0
    transform:
      mirror_x: false
      mirror_y: false
      swap_xy: false
    interrupt_pin: GPIO15
    reset_pin: GPIO16
    calibration:
      x_min: 0
      x_max: 320
      y_min: 0
      y_max: 240
    on_touch:
      then:
        ...
    on_update:
      then:
        ...
    on_release:
      then:
        ...
```

### Configuration Options

#### CST3240 Touchscreen

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `id` | ID | | Identifier for the touchscreen component |
| `display` | ID | | Identifier for the display component (required) |
| `interrupt_pin` | Pin | | GPIO pin connected to touchscreen interrupt (optional) |
| `reset_pin` | Pin | | GPIO pin connected to touchscreen reset (optional but recommended) |
| `i2c_id` | ID | | I2C bus to use (if multiple buses) |
| `address` | Hex | `0x5A` | I2C address of the CST3240 |
| `update_interval` | Time | `50ms` | How often to poll for touches |
| `transform` | Transform | | Coordinate transformation settings |

#### Transform Options

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `calibration` | Calibration | | Touch coordinate calibration |
| `swap_xy` | Boolean | `false` | Swap X and Y coordinates |
| `mirror_x` | Boolean | `false` | Mirror X coordinates |
| `mirror_y` | Boolean | `false` | Mirror Y coordinates |

#### Calibration Options

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `x_min` | Integer | `0` | Minimum X coordinate |
| `x_max` | Integer | Display width | Maximum X coordinate |
| `y_min` | Integer | `0` | Minimum Y coordinate |
| `y_max` | Integer | Display height | Maximum Y coordinate |

### Virtual Buttons

Create touch-sensitive regions that act as binary sensors:

```yaml
binary_sensor:
  # Regular touchscreen button
  - platform: touchscreen
    name: "Home Button"
    touchscreen_id: my_touchscreen
    x_min: 0
    x_max: 100
    y_min: 200
    y_max: 240

  # CST3240-specific virtual button
  - platform: cst3240
    cst3240_id: my_touchscreen
    name: "CST3240 Virtual Button"
```

### Wiring Diagram

```
ESP32          CST3240
GPIO21  ---->  SDA
GPIO22  ---->  SCL
GPIO15  ---->  INT (interrupt)
GPIO16  ---->  RST (reset)
3.3V    ---->  VCC
GND     ---->  GND

---

## UDP Audio Streamer

Always-on UDP audio publisher that taps any ESPHome microphone source and forwards PCM frames to a remote listener. Useful for prototyping wake-word engines or raw audio captures outside of the voice assistant pipeline.

### Features

- Streams 16–32 bit PCM, up to two channels, at the microphone’s native sample rate
- Configurable send cadence (`chunk_duration`) and ring buffer depth (`buffer_duration`)
- Optional passive mode that only relays audio when another component starts the microphone

### Basic Configuration

```yaml
external_components:
  - source: github://shyndman/personal-esphome-components
    components: [udp_audio_streamer]

i2s_audio:
  - id: i2s0
    i2s_lrclk_pin: GPIO42
    i2s_bclk_pin: GPIO41
    i2s_mclk_pin: GPIO40

microphone:
  - platform: i2s_audio
    id: i2s_mic
    adc_type: external
    i2s_audio_id: i2s0
    i2s_din_pin: GPIO2
    sample_rate: 16000

udp_audio_streamer:
  host: 192.168.1.50
  port: 7000
  chunk_duration: 40ms
  buffer_duration: 640ms
  microphone:
    microphone: i2s_mic
    bits_per_sample: 16
    channels: 0
```

### Configuration Options

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `host` | String | — | IPv4/IPv6 destination for UDP packets |
| `port` | Integer | — | Destination UDP port |
| `chunk_duration` | Time | `32ms` | Audio slice sent per packet |
| `buffer_duration` | Time | `512ms` | Total ring buffer depth before dropping samples |
| `microphone` | Microphone Source | — | See [ESPHome microphone source schema](https://esphome.io/components/microphone/index.html) |
| `passive` | Boolean | `false` | Do not start/stop the microphone automatically |

### Debugging Tips

- For quick verification, use `socat -u UDP-RECV:7000,reuseaddr,fork - | hexdump -Cv` on a desktop.
- If packets stop, check ESPHome logs for `udp_audio_streamer` warnings about socket send failures or buffer overruns.
```
