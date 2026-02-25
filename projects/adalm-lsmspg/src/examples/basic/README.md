# ADALM-LSMSPG Basic Example

This basic example demonstrates how to use the AD5592R and AD5593R drivers directly without the IIO framework.

## Overview

The basic example showcases:

- **Device Initialization**: Initializes both AD5592R (SPI) and AD5593R (I2C) devices
- **Configuration Display**: Shows device configuration including reference voltage, ADC/DAC ranges, and channel modes
- **ADC Operations**: Reads analog values from configured ADC channels and converts them to voltages
- **DAC Operations**: Writes analog values to configured DAC channels and displays the output voltages
- **GPIO Operations**: Reads GPIO input states and toggles GPIO output states
- **Continuous Operation**: Runs in a loop, demonstrating real-time operation

## Channel Configuration

Both devices use the same channel configuration:
- **Channel 0**: DAC only
- **Channel 1**: ADC only
- **Channel 2**: DAC + ADC (bidirectional)
- **Channel 3**: DAC + ADC (bidirectional)
- **Channel 4**: GPIO Input (with pulldown)
- **Channel 5**: GPIO Output (default low)
- **Channel 6**: GPIO Input (with pulldown)
- **Channel 7**: GPIO Output (default low)

## Building

To build the basic example:

```bash
make EXAMPLE=basic
```

## Hardware Setup

The example expects:
- **AD5592R**: Connected via SPI interface
- **AD5593R**: Connected via I2C interface
- **UART**: For debug output and monitoring

## Expected Output

The example will continuously display:
1. Device configuration information (once at startup)
2. DAC output values and corresponding voltages
3. ADC input readings and corresponding voltages
4. GPIO status (input/output states)
5. Loop counter and timing information

The DAC values will ramp up gradually on each iteration, and GPIO outputs will toggle every 2 seconds.

## Key Features Demonstrated

- **Direct driver usage**: Uses ad5592r/ad5593r drivers without IIO abstraction
- **Mixed-signal operation**: Combines ADC, DAC, and GPIO functionality
- **Dual interface support**: Demonstrates both SPI (AD5592R) and I2C (AD5593R) communication
- **Real-time monitoring**: Continuous polling and display of all device states
- **Error handling**: Proper initialization and cleanup procedures

## Comparison with IIO Example

Unlike the IIO example which provides a network interface for remote control, this basic example:
- Runs standalone without network dependencies
- Provides direct access to driver functions
- Is simpler to understand for learning purposes
- Demonstrates basic device operation patterns
- Can be easily modified for custom applications

This makes it ideal for:
- Learning the AD5592R/AD5593R driver APIs
- Testing basic device functionality
- Developing custom applications
- Understanding the hardware capabilities