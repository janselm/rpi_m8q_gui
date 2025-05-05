# u-blox PVT Viewer for Raspberry Pi 

A multithreaded C application for Raspberry Pi that reads real-time GPS data (NAV-PVT) from a u-blox GNSS module over SPI using the UBX protocol and displays system and GPS status via a GTK-based GUI.

## Features

- Real-time GPS parsing from UBX NAV-PVT messages over SPI
- Offline map display with auto-centering on current GPS location
- Custom location marker using icon overlay
- GTK-based GUI with:
    - Time display (timezone adjustable)
    - Speed display in MPH
    - Air tank pressure indicators (simulated, sensor-ready)
    - Scrollable and dynamically updating map view
- Thread-safe design with graceful shutdown
- Structured for integration with ADC-based pressure sensors

## Requirements

- Raspberry Pi with SPI enabled
- u-blox GNSS module (ZOE-M8Q)
- PNG map with known bounding box (defined in code)
- [bcm2835 library](http://www.airspayce.com/mikem/bcm2835/)
- GTK 3
- `make`, `gcc`, `pthread`

## Build

```sh
make
```

## Run
```
./guiTest
```
# u-blox PVT Viewer for Raspberry Pi

A multithreaded C application for Raspberry Pi that reads real-time NAV-PVT data from a u-blox GPS module (ZOE-M8Q) over SPI using the UBX protocol, and displays location and time data via a GTK-based GUI.

## Features

- Real-time GPS parsing via UBX NAV-PVT
- GTK GUI showing latitude, longitude, and UTC time
- Thread-safe design with graceful shutdown
- SPI communication using bcm2835 library

## Requirements

- Raspberry Pi with SPI enabled
- u-blox GNSS module (ZOE-M8Q)
- [bcm2835 library](http://www.airspayce.com/mikem/bcm2835/)
- GTK 3
- png map with bounding box json data (geojson.io)
- `make`, `gcc`, `pthread`

## Build

```sh
make
```

## Run
```
./guiTest
```
## Wiring

| u-blox Pin | Raspberry Pi Pin       |
|------------|------------------------|
| VCC        | 3.3V (Pin 1)           |
| GND        | GND (Pin 6)            |
| SCK        | SPI0 SCLK (Pin 23)     |
| MISO       | SPI0 MISO (Pin 21)     |
| MOSI       | SPI0 MOSI (Pin 19)     |
| CS         | SPI0 CE0 (Pin 24)      |

> Ensure SPI is enabled via `raspi-config`.

## Future Plans
- Live ADC integration for air tank pressure monitoring
- Scalable tile-based map loading and switching
- Logging and diagnostics
- UI refinements and mobile deployment

## License

MIT License
