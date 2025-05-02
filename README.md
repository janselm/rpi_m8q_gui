# u-blox PVT Viewer for Raspberry Pi

A multithreaded C application for Raspberry Pi that reads real-time NAV-PVT data from a u-blox GPS module (e.g., ZOE-M8Q) over SPI using the UBX protocol, and displays location and time data via a GTK-based GUI.

## Features

- Real-time GPS parsing via UBX NAV-PVT
- GTK GUI showing latitude, longitude, and UTC time
- Thread-safe design with graceful shutdown
- SPI communication using bcm2835 library

## Requirements

- Raspberry Pi with SPI enabled
- u-blox GNSS module (ZOE-M8Q or similar)
- [bcm2835 library](http://www.airspayce.com/mikem/bcm2835/)
- GTK 3
- `make`, `gcc`, `pthread`

## Build

```sh
make
