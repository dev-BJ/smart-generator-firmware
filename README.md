# Smart Generator Controller

ESP32-based firmware for SMS/IoT generator monitoring and control system.

## Hardware Mapping

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| **SD/MMC** | | SPI interface |
| MISO | 19 | |
| MOSI | 23 | |
| SCK | 18 | |
| CS | 5 | |
| **Relays (Active-Low)** | | |
| Gen Control (Relay 2) | 32 | Generator start/stop |
| Battery Charge (Relay 3) | 13 | Battery charging control |
| Load Transfer (Relay 4) | 14 | Mains/Generator load switching |
| **Voltage Sensors (Input-only GPIOs)** | | |
| Mains Sense | 34 | ADC input only |
| Gen Sense | 35 | ADC input only |
| Battery Sense | 15 | Voltage divider |
| **LEDs** | | |
| SMS Activity | 4 | Blinks on SMS activity |
| Low Fuel Warning | 2 | Lights on low fuel |
| **Ultrasonic (HC-SR04)** | | |
| Trig | 25 | |
| Echo | 26 | |
| **Servo (SG90)** | | |
| Choke | 27 | 0° = choke engaged, 90° = released |
| **Buzzer** | 33 | |
| **SIM800L (UART2)** | | |
| SIM TX (ESP32 RX) | 16 | |
| SIM RX (ESP32 TX) | 17 | |
| **I2C (LCD + DS3231)** | | |
| SDA | 21 | |
| SCL | 22 | |

## Build Instructions

### Requirements
- PlatformIO CLI or VS Code with PlatformIO extension
- ESP32 Dev Module (esp32dev board)

### Build
```bash
pio run -e esp32dev
```

### Upload
```bash
pio run -e esp32dev -t upload
```

### Monitor
```bash
pio device monitor -b 115200
```

## SMS Commands

All commands are case-insensitive. Send as plain SMS to the SIM800L number.

| Command | Description | Authorization |
|---------|-------------|---------------|
| `ONGEN` | Start generator manually | Admin only |
| `OFFGEN` | Stop generator manually | Admin only |
| `MSTATUS` | Get mains/gen voltage status + fuel level | Admin only |
| `DSTATUS` | Get daily runtime + fuel consumption | Admin only |
| `RSTALL` | Reset all stats (runtime, fuel) | Admin + Default |
| `RSTNUMBER` | Reset registered phone to default | Admin + Default |
| `CHP<old>N<new>` | Change registered phone number | Admin only |

### Phone Number Format
- Nigerian numbers normalized automatically: `08012345678` → `+2348012345678`
- Default admin: `+2348012345678` (CHANGE IN config.h!)

## Generator State Machine

```
OFF → STARTING → RUNNING → STOPPING → OFF
```

- **OFF**: Generator stopped, relays off, choke released
- **STARTING**: Relay ON, choke engaged for 5s, waiting for stable voltage (3s)
- **RUNNING**: Generator stable, relay ON, choke released
- **STOPPING**: Relay OFF, waiting 5s for voltage to drop

## Battery Charging

- Starts charging when battery < 11.0V
- Stops charging when battery > 13.4V
- Hysteresis prevents relay chatter

## Load Transfer

- Mains present → Load on mains (Relay 4 OFF)
- Mains fail + Gen running → Load on generator (Relay 4 ON)
- Mains return → 60s delay before switching back to mains

## Fuel Monitoring

- Ultrasonic sensor (HC-SR04) measures distance to fuel surface
- 5-sample moving average filter
- Tank: 50L, Empty=40cm, Full=5cm (adjustable in config.h)
- Alerts: Low (30%), Critical (10%), Shutdown (7%)

## Data Persistence

Stored in NVS (Preferences):
- Registered phone number
- Daily/total generator runtime
- Daily/total fuel consumed
- Last day (for midnight rollover)

## Configuration

All hardware pins, thresholds, and timing in `include/config.h` — modify for your setup.

## Safety Notes

- Voltage sensors on GPIO 34/35 are INPUT ONLY — no output capability
- Relays are active-low (most modules)
- Always test with load disconnected first
- Set DEFAULT_ADMIN_NUMBER in config.h before deployment

## License

MIT