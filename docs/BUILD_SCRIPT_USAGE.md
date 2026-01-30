# Build Script Usage Guide

## Overview

This document describes the build system usage. **The recommended way is to use `make` command directly.**

## Standard Build Commands (Recommended)

### Basic Usage

```bash
# Compile Tracker (default CH591D board)
make TARGET=tracker

# Compile Receiver (default CH591D board)
make TARGET=receiver

# Clean build files
make clean
```

### Specify Board Type

```bash
# CH591D board
make TARGET=tracker BOARD=ch591d

# CH592X board
make TARGET=tracker BOARD=ch592x
make TARGET=receiver BOARD=ch592x

# Generic board (need to configure pins)
make TARGET=tracker BOARD=generic_board
```

### Examples

```bash
# Compile Tracker for CH591D
make TARGET=tracker BOARD=ch591d

# Compile Tracker for CH592X
make TARGET=tracker BOARD=ch592x

# Compile Receiver for CH592X
make TARGET=receiver BOARD=ch592x

# Clean and rebuild
make clean
make TARGET=tracker BOARD=ch592x
```

## Alternative: build.sh Script

**Note**: `build.sh` is a convenience script that depends on `scripts/build/build_complete.sh`. If that file is missing, `build.sh` will automatically fall back to `make` command.

### Usage

```bash
# If build_complete.sh exists, use it
./build.sh tracker

# Otherwise, it will automatically use make command
```

### Limitations

- Requires `scripts/build/build_complete.sh` to be present
- May not work in all environments
- **Recommendation**: Use `make` command directly

## Makefile Options

### Target Selection

| Option | Description | Example |
|--------|-------------|---------|
| `TARGET=tracker` | Compile tracker firmware | `make TARGET=tracker` |
| `TARGET=receiver` | Compile receiver firmware | `make TARGET=receiver` |

### Board Selection

| Option | Description | Example |
|--------|-------------|---------|
| `BOARD=ch591d` | CH591D board (20-pin QFN) | `make TARGET=tracker BOARD=ch591d` |
| `BOARD=ch592x` | CH592X board (28-pin QFN) | `make TARGET=tracker BOARD=ch592x` |
| `BOARD=generic_board` | Generic board (pins undefined) | `make TARGET=tracker BOARD=generic_board` |

### Other Options

| Option | Description | Example |
|--------|-------------|---------|
| `DEBUG=1` | Enable debug mode | `make TARGET=tracker DEBUG=1` |
| `OTA=1` | Enable OTA support | `make TARGET=tracker OTA=1` |
| `clean` | Clean build files | `make clean` |

## Build Output

### Output Directory

All build outputs are in the `output/` directory:

```
output/
├── tracker_ch591.elf
├── tracker_ch591.hex
├── tracker_ch591.bin
├── receiver_ch592.elf
├── receiver_ch592.hex
└── receiver_ch592.bin
```

### Build Directory

Intermediate build files are in the `build/` directory:

```
build/
├── tracker/
│   ├── *.o files
│   └── *.d files
└── receiver/
    ├── *.o files
    └── *.d files
```

## Troubleshooting

### Common Issues

1. **"make: command not found"**
   - Install GNU Make
   - On Windows: Install MSYS2 or use WSL

2. **"riscv-none-elf-gcc: command not found"**
   - Install RISC-V GCC toolchain
   - Add toolchain to PATH

3. **"Board not defined" error**
   - Specify BOARD parameter: `make TARGET=tracker BOARD=ch591d`

4. **Build fails with pin warnings (generic_board)**
   - This is expected for generic_board
   - Configure pins in `board/generic_board/config.h`

## Best Practices

1. **Always specify BOARD parameter** for clarity:
   ```bash
   make TARGET=tracker BOARD=ch591d
   ```

2. **Clean before rebuilding** if you change configuration:
   ```bash
   make clean
   make TARGET=tracker BOARD=ch592x
   ```

3. **Use make command directly** instead of build.sh for reliability

4. **Check build output** in `output/` directory after compilation

## Migration from build.sh

If you were using `./build.sh`, migrate to `make`:

**Old way:**
```bash
./build.sh tracker
./build.sh receiver
./build.sh all
```

**New way:**
```bash
make TARGET=tracker BOARD=ch591d
make TARGET=receiver BOARD=ch592x
```
