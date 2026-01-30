# Generic Receiver Board Configuration

## Description

This is a generic receiver board configuration where all pins are undefined and need to be configured according to actual hardware.

## Usage

### 1. Configure Pins

Edit `board/generic_receiver/config.h` and configure all pins according to actual hardware:

```c
// LED Status Indicator
#define PIN_LED             9       // Modify according to actual hardware
#define PIN_LED_PORT        GPIOA   // Modify according to actual hardware

// Pairing Button
#define PIN_PAIR_BTN        4       // Modify according to actual hardware
#define PIN_PAIR_BTN_PORT   GPIOB   // Modify according to actual hardware

// Debug UART (optional)
#define PIN_DEBUG_TX        7       // Modify according to actual hardware
#define PIN_DEBUG_TX_PORT   GPIOB   // Modify according to actual hardware
#define PIN_DEBUG_RX        4       // Modify according to actual hardware
#define PIN_DEBUG_RX_PORT   GPIOB   // Modify according to actual hardware
```

### 2. Configure Pin Mappings

Edit `board/generic_receiver/pins.h` and configure UART mappings if needed:

```c
// UART Channel Mapping
#define UART0_TX            PB7
#define UART0_RX            PB4
```

### 3. Compile

```bash
# Compile generic receiver
make TARGET=receiver BOARD=generic_receiver

# Or use build script
./build.sh receiver generic_receiver
```

## Configuration Check

The compiler will check if critical pins are configured, and display warnings if not:

```
warning: PIN_LED is not defined! Please configure it in board/generic_receiver/config.h
warning: PIN_PAIR_BTN is not defined! Please configure it in board/generic_receiver/config.h
```

## Notes

1. **All pins must be configured**: All pin definitions need to be configured according to actual hardware
2. **Pin consistency**: Ensure pin definitions match the actual hardware schematic
3. **Chip selection**: Default is CH592, can be changed to CH591 in `config.h`
4. **Function verification**: After configuration, it is recommended to perform functional testing

## Configuration Checklist

Pins that need to be configured:

- [ ] PIN_LED - LED Status Indicator
- [ ] PIN_PAIR_BTN - Pairing Button
- [ ] PIN_DEBUG_TX - Debug UART TX (optional)
- [ ] PIN_DEBUG_RX - Debug UART RX (optional)
- [ ] PIN_RF_ANT - RF Antenna (usually hardware fixed)

## Reference

You can refer to other board configurations as examples:
- `board/mingyue_slimevr/ch592x/config.h` - CH592X Receiver Configuration Example

## Differences from Tracker

Receiver configuration is simpler than tracker:
- No IMU sensor pins (SPI/I2C)
- No battery detection pins
- No charging detection pins
- Focus on USB, LED, and pairing button

