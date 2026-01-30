# Generic Board Configuration

## Description

This is a generic board configuration where all pins are undefined and need to be configured according to actual hardware.

## Usage

### 1. Configure Pins

Edit `board/generic_board/config.h` and configure all pins according to actual hardware:

```c
// LED Status Indicator
#define PIN_LED             9       // Modify according to actual hardware
#define PIN_LED_PORT        GPIOA   // Modify according to actual hardware

// User Button
#define PIN_SW0             20      // Modify according to actual hardware
#define PIN_SW0_PORT        GPIOB   // Modify according to actual hardware

// IMU SPI Interface
#define PIN_SPI_CS          12      // Modify according to actual hardware
#define PIN_SPI_CS_PORT     GPIOA   // Modify according to actual hardware
#define PIN_SPI_CLK         13      // Modify according to actual hardware
#define PIN_SPI_MOSI        14      // Modify according to actual hardware
#define PIN_SPI_MISO        15      // Modify according to actual hardware

// IMU Interrupt
#define PIN_IMU_INT1        11      // Modify according to actual hardware
#define PIN_IMU_INT1_PORT   GPIOA   // Modify according to actual hardware

// Charging Detection
#define PIN_CHRG_DET        10      // Modify according to actual hardware
#define PIN_CHRG_DET_PORT   GPIOA   // Modify according to actual hardware

// Battery Detection
#define PIN_VBAT_ADC        8       // Modify according to actual hardware
#define PIN_VBAT_ADC_PORT   GPIOA   // Modify according to actual hardware
#define PIN_VBAT_ADC_CHANNEL 12     // Modify according to actual hardware
```

### 2. Configure Pin Mappings

Edit `board/generic_board/pins.h` and configure ADC, PWM, UART mappings:

```c
// ADC Channel Mapping
#define ADC_CH_PA5_AIN1     1

// PWM Channel Mapping
#define PWM_CH_PA6_PWM4     4

// UART Channel Mapping
#define UART0_TX            PB7
#define UART0_RX            PB4
```

### 3. Compile

```bash
# Compile generic board
make TARGET=tracker BOARD=generic_board

# Or use build script
./build.sh tracker BOARD=generic_board
```

## Configuration Check

The compiler will check if critical pins are configured, and display warnings if not:

```
warning: PIN_LED is not defined! Please configure it in board/generic_board/config.h
warning: PIN_SW0 is not defined! Please configure it in board/generic_board/config.h
```

## Notes

1. **All pins must be configured**: All pin definitions need to be configured according to actual hardware
2. **Pin consistency**: Ensure pin definitions match the actual hardware schematic
3. **Chip selection**: Default is CH592, can be changed to CH591 in `config.h`
4. **Function verification**: After configuration, it is recommended to perform functional testing

## Configuration Checklist

Pins that need to be configured:

- [ ] PIN_LED - LED Status Indicator
- [ ] PIN_SW0 - User Button
- [ ] PIN_SPI_CS - IMU SPI Chip Select
- [ ] PIN_SPI_CLK - IMU SPI Clock
- [ ] PIN_SPI_MOSI - IMU SPI Data Output
- [ ] PIN_SPI_MISO - IMU SPI Data Input
- [ ] PIN_IMU_INT1 - IMU Interrupt 1
- [ ] PIN_CHRG_DET - Charging Detection
- [ ] PIN_VBAT_ADC - Battery Detection ADC
- [ ] PIN_BOOT - BOOT Button (optional)
- [ ] PIN_RST - Reset Pin (optional)
- [ ] PIN_I2C_SCL - I2C Clock (if using I2C)
- [ ] PIN_I2C_SDA - I2C Data (if using I2C)

## Reference

You can refer to other board configurations as examples:
- `board/mingyue_slimevr/ch591d/config.h` - CH591D Configuration Example
- `board/mingyue_slimevr/ch592x/config.h` - CH592X Configuration Example
