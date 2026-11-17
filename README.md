# Embed Kit – UART Frame Parser

**Author:** Gaurav Hinge  
**Language:** Embedded C (C99)  
**Compiler:** GCC

## Project Overview

This project implements a standalone UART frame parser using standard C. It processes incoming UART data byte by byte and identifies valid frames using a Start of Frame (SOF) byte and XOR checksum validation.

## UART Frame Format

| Field | Size |
|---|---|
| SOF | 1 byte (`0xAA`) |
| CMD | 1 byte |
| LEN | 1 byte |
| PAYLOAD | 0–16 bytes |
| CHECKSUM | 1 byte |

**Checksum:** XOR of CMD, LEN, and all payload bytes.

## Features

- Byte-by-byte UART frame parsing.
- Configurable inter-byte timeout.
- Timeout detection and recovery.
- XOR checksum validation.
- Payload length support from 0 to 16 bytes.
- Back-to-back frame processing.
- Timeout can be disabled by setting it to zero.
- Standard C99 implementation without external libraries.

## Parser Return Values

| Return value | Description |
|---|---|
| `1` | Valid frame received |
| `0` | Frame processing in progress |
| `-1` | Checksum mismatch |
| `-2` | Inter-byte timeout |

## Build Instructions

Compile using GCC:

```bash
gcc -Wall -std=c99 uart_parser.c -o uart_parser
```

Run:

```bash
./uart_parser
```

On Windows PowerShell:

```powershell
.\uart_parser.exe
```

## Test Cases

1. Valid UART frame.
2. Inter-byte timeout and recovery.
3. Two valid frames received back-to-back.
4. Timeout disabled.

## Checksum Note

The assignment's first example specifies checksum `0x22`. However, XOR calculation gives:

`0x01 ^ 0x03 ^ 0x10 ^ 0x20 ^ 0x30 = 0x02`

Therefore, the implementation uses `0x02`, following the specified XOR checksum algorithm.

## Project Files

- `uart_parser.c` – UART parser implementation and test cases.
- `README.md` – Project documentation.