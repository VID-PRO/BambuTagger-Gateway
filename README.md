# BambuTagger-Gateway

Multi-client bridge for Bambu Lab printers — breaks the 3-connection limit by multiplexing MQTT, camera, and FTPS traffic through a single ESP8266 gateway.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/G8M220JASY)

---

## Features

| Category | Details |
|---|---|
| **MQTT multiplexer** | One upstream MQTT connection to the printer; up to 8 local clients can subscribe/publish simultaneously via the gateway |
| **Camera broadcast** | Single upstream connection to printer camera stream; broadcasts to all connected local clients |
| **FTPS forwarder** | Single upstream FTPS connection; forwards control/data to local clients |
| **WiFi AP mode** | Creates `Bambu-Gateway` access point — clients connect directly to the gateway, no LAN reconfiguration needed |
| **Station mode** | Optionally joins your existing WiFi network alongside the AP |
| **mDNS** | Advertises as `BambuTagger-Gateway.local` |
| **Auto-resubscribe** | Tracks all client subscriptions; re-subscribes upstream on reconnect |
| **Topic-aware routing** | Correctly handles `#`, `+` wildcards when forwarding printer reports to local clients |

---

## How It Works

Bambu Lab printers accept **3 simultaneous connections**. Once that limit is hit, new clients are rejected. This gateway connects as a **single client per service** and lets many local clients share those connections.

```
┌──────────────┐     ┌──────────────────────┐     ┌─────────────────┐
│ MQTT Client 1│────▶│                      │────▶│                 │
│ MQTT Client 2│────▶│  ESP8266 Gateway     │────▶│  Bambu Lab      │
│ MQTT Client 3│────▶│  ┌────────────────┐  │     │  Printer        │
│      ...      │     │  │ MQTT Bridge    │──┼────▶│  ┌───────────┐  │
│ MQTT Client 8 │     │  │ (1 upstream    │  │     │  │ MQTT 8883 │  │
├──────────────┤     │  │  connection)    │  │     │  ├───────────┤  │
│ Camera Client│────▶│  ├────────────────┤  │     │  │ Camera    │  │
│ Camera Client│────▶│  │ Camera Proxy   │──┼────▶│  │ Stream    │  │
│      ...      │     │  │ (1 upstream    │  │     │  ├───────────┤  │
│ Camera Client│     │  │  connection)    │  │     │  │ FTPS 990  │  │
├──────────────┤     │  ├────────────────┤  │     │  └───────────┘  │
│ FTP Client   │────▶│  │ FTPS Proxy     │──┼────▶│                 │
│ FTP Client   │────▶│  │ (1 upstream    │  │     │                 │
│      ...      │     │  │  connection)    │  │     │                 │
└──────────────┘     └──────────────────────┘     └─────────────────┘
```

> **Printer sees:** 1 MQTT + 1 camera + 1 FTPS = **3 connections total** (same limit, but now shared by unlimited clients).

---

## Hardware

### Bill of Materials

| Component | Notes | Buy |
|---|---|---|
| **ESP8266 board** | NodeMCU v3, Wemos D1 mini, or any ESP-12E module | [Aliexpress](https://www.aliexpress.com/) |
| 5V power supply | Micro-USB or 3.3V regulator (≥500mA) | — |

### Specifications

| Component | Details |
|---|---|
| MCU | ESP8266 (80–160 MHz, single-core) |
| RAM | 80 KB (usable) + 4 MB flash |
| WiFi | 802.11 b/g/n, AP + Station simultaneous |
| GPIO | Enough for status LED, reset button (optional) |

---

## Software

### Required Libraries (PlatformIO)

| Library | Version | Purpose |
|---|---|---|
| `PubSubClient` | ^2.8 | Upstream MQTT connection to printer |
| `ESP8266WiFi` | Built-in | WiFi AP + Station |
| `ESP8266mDNS` | Built-in | mDNS responder |

### Building

```bash
# Install PlatformIO (if not already)
pip install platformio

# Build
pio run -e esp12e

# Flash
pio run -e esp12e -t upload

# Monitor
pio device monitor -b 115200
```

### Board Settings (platformio.ini)

| Setting | Value |
|---|---|
| Board | `esp12e` |
| Flash Mode | `dout` |
| Framework | Arduino |
| Monitor Speed | 115200 |

---

## Configuration

Edit `src/config.h` before building:

| Setting | Default | Description |
|---|---|---|
| `PRINTER_HOST` | `bambu-printer.local` | Printer hostname or IP |
| `PRINTER_ACCESS_CODE` | `12345678` | Printer access code (from LCD/OR code) |
| `PRINTER_SERIAL` | `SERIAL001` | Printer serial number (from LCD) |
| `GATEWAY_AP_SSID` | `Bambu-Gateway` | Gateway WiFi name |
| `GATEWAY_AP_PASS` | `bambugateway` | Gateway WiFi password |
| `STATION_SSID` | `""` | Your LAN WiFi SSID (leave empty to skip) |
| `STATION_PASS` | `""` | Your LAN WiFi password |
| `MAX_MQTT_CLIENTS` | `8` | Max simultaneous local MQTT clients |

---

## Client Connection

### MQTT

| Parameter | Gateway Value |
|---|---|
| Broker | Gateway AP IP (`192.168.4.1`) |
| Port | `1883` (plain TCP) |
| Username | (none) |
| Password | (none) |
| Subscribe | Any topic — the bridge forwards matching printer reports |
| Publish | Publish JSON commands — forwarded to `device/<SERIAL>/request` |

### Camera

| Parameter | Gateway Value |
|---|---|
| Host | `192.168.4.1` |
| Port | `6000` |
| Protocol | Raw TCP stream (forwarded from printer) |

### FTPS

| Parameter | Gateway Value |
|---|---|
| Host | `192.168.4.1` |
| Port | `990` |
| Protocol | Raw TCP forwarder (TLS passthrough not implemented — see limitations) |

---

## Project Structure

```
BambuTagger-Gateway/
├── platformio.ini              # Build configuration
└── src/
    ├── config.h                # Printer credentials, WiFi, port limits
    ├── main.cpp                # Entry point, WiFi setup, bridge init
    ├── mqtt_bridge.h/.cpp      # MQTT multiplexer (upstream + downstream)
    └── tcp_proxy.h/.cpp        # Generic TCP forwarder (camera, FTPS)
```

---

## Protocol Details: MQTT Bridge

The MQTT bridge is the core component. It:

1. **Connects upstream** to the printer's MQTT broker (port 8883) as a single client using `PubSubClient`
2. **Listens locally** on port 1883 for plain TCP MQTT connections
3. **Parses incoming MQTT packets** (CONNECT, SUBSCRIBE, UNSUBSCRIBE, PUBLISH, PINGREQ, DISCONNECT) and responds appropriately
4. **Tracks subscriptions** per local client, including wildcard topics (`#`, `+`)
5. **Routes upstream messages** — when the printer publishes a report, the bridge matches all local client subscriptions and forwards the message
6. **Manages upstream subscriptions** — subscribes/unsubscribes on the printer connection based on aggregated local demand; only unsubscribes when no local client still wants a topic

### Upstream MQTT

| Parameter | Value |
|---|---|
| Port | 8883 (TLS) |
| Username | `bblp` |
| Password | Printer access code |
| Subscribe | `device/<SERIAL>/report` |
| Publish | `device/<SERIAL>/request` |
| Will topic | `device/<SERIAL>/status` |
| Will payload | `offline` (retained) |

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Gateway AP not visible | Check `GATEWAY_AP_SSID` is set; verify ESP8266 powers on (serial monitor) |
| MQTT clients can't connect | Ensure clients connect to `192.168.4.1:1883` and not the printer directly |
| Upstream MQTT fails | Verify `PRINTER_HOST`, `PRINTER_ACCESS_CODE`, `PRINTER_SERIAL`; printer must be on same network |
| Camera stream not working | Some printers use different camera ports; check your printer model's port and update `CAM_PRINTER_PORT` |
| FTPS not working | FTPS requires TLS which the ESP8266 TCP proxy does not terminate — use raw FTP or MQTT for file ops |
| Frequent disconnects | Reduce `MAX_MQTT_CLIENTS` if ESP8266 runs out of memory (check free heap in serial output) |

---

## Limitations

- **FTPS**: The ESP8266 TCP proxy forwards raw TCP bytes only. FTPS (FTP over TLS) requires TLS termination which is not implemented. For file operations, use the printer's MQTT API instead.
- **Camera**: The proxy assumes a plain TCP stream (e.g., MJPEG). If your printer uses a different protocol, adjust accordingly.
- **Memory**: ESP8266 has limited RAM (~80 KB heap). Each local client consumes ~4 KB. If you need many concurrent clients, consider an ESP32-based version.
- **Security**: Local MQTT connections are plain TCP (no TLS). The gateway is designed for isolated/trusted networks.

---

## Credits & References

- [Bambu-Research-Group](https://github.com/Bambu-Research-Group)
- MQTT client: [PubSubClient](https://github.com/knolleary/pubsubclient)
- PlatformIO: [https://platformio.org](https://platformio.org)

---

## License

MIT — free to use, modify, and share.
