# Scott's Personal ESPHome Components

A collection of custom ESPHome components for various hardware and integrations.

## Available Components

- **[cst3240](#cst3240-touchscreen)**: CST3240 capacitive touchscreen controller driver

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
```
