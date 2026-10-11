# IoT extension: plan

Branch `iot`. `main` stays the MIC5 hand-in; nothing here changes it until
the IoT work is finished and tested.

## Goal

Turn the pet feeder into a real connected device, and learn the basics that
IoT jobs ask about along the way: device-to-cloud and cloud-to-device
messaging, MQTT, TLS with device certificates, a device shadow, provisioning,
OTA updates, reliability, and data and alerts in the cloud.

## Architecture

```
 ┌──────────────── pet feeder ────────────────┐            ┌──────── AWS ─────────┐
 │                                            │            │                      │
 │  FRDM-MCXA153 (FreeRTOS)    UART link      │   Wi-Fi    │  IoT Core            │
 │  servo, button, LEDs   ◄──────────────►  ESP32 (ESP-IDF) ─── MQTT/TLS ──► │  ├ device shadow     │
 │  safety rules (Feeder)  LPUART1 ↔ UART2    │   8883     │  ├ rules engine ──► CloudWatch / SNS
 │  console (USB)                             │            │  └ Jobs (OTA)        │
 └────────────────────────────────────────────┘            └──────────────────────┘
```

- **MCXA153 = real-time controller.** It keeps the servo, the button, the
  LEDs and all safety rules (cooldown, daily limit). It doesn't know about
  Wi-Fi or the cloud.
- **ESP32 = connectivity co-processor.** Our own ESP-IDF firmware: Wi-Fi,
  time sync, MQTT over TLS to AWS IoT Core, shadow, OTA. It doesn't drive any
  hardware of the feeder.
- **The cloud can ask, the device decides.** A feed command from the cloud
  goes into FeedQ like a button press (`FEED_REMOTE`), so the Feeder's rules
  still apply. A bug, a replayed message or an attacker can't overfeed the
  pet. This split (safety at the edge, convenience in the cloud) is a common
  interview topic.

This host-MCU + Wi-Fi-module split is how many commercial products are built
(the MCU does the product, a certified module does the radio).

## Wiring

| ESP32 DevKit (classic) | FRDM-MCXA153 | Signal |
|---|---|---|
| GPIO17 (UART2 TX) | J3[11] = P3_8 (LPUART1 RX) | ESP32 → MCXA153 |
| GPIO16 (UART2 RX) | J3[9] = P3_9 (LPUART1 TX) | MCXA153 → ESP32 |
| GND | GND | common ground (required) |

Both boards use 3.3 V logic, so no level shifter is needed. Power each board
from its own USB cable. Pins to be confirmed against the FRDM-MCXA153 user
manual and the MCXA153 reference manual (pin mux ALT number) in phase 3.

## MQTT topics

`<thing>` = `petfeeder-01`

| Topic | Direction | QoS | Retained | Content |
|---|---|---|---|---|
| `petfeeder/<thing>/event` | device → cloud | 1 | no | `{"type":"fed","source":"button","portions":1,"today":3,"ts":...}` |
| `petfeeder/<thing>/telemetry` | device → cloud | 0 | no | heap, stack, uptime, RSSI, every 60 s |
| `petfeeder/<thing>/cmd` | cloud → device | 1 | no | `{"cmd":"feed","portions":1,"id":"..."}` |
| `petfeeder/<thing>/cmd/ack` | device → cloud | 1 | no | `{"id":"...","result":"fed"/"rejected:cooldown"}` |
| `petfeeder/<thing>/status` | device → cloud | 1 | yes | `online` / `offline` (Last Will) |
| `$aws/things/<thing>/shadow/...` | both | 1 | — | reported/desired state: interval, today, last meal, firmware version |

## UART link protocol (MCXA153 ↔ ESP32)

Line-based text, so it can be read on a logic analyser or a terminal, with a
checksum so corrupted lines are dropped:

```
$EVT,FED,button,1,3*5A\n        MCXA153 → ESP32: a meal was given (source, portions, today)
$EVT,REJ,remote,cooldown*1C\n   MCXA153 → ESP32: a request was rejected
$STA,3,6,60000,0*2B\n           MCXA153 → ESP32: status (today, max, interval ms, limit flag)
$CMD,FEED,1,42*0F\n             ESP32 → MCXA153: feed 1 portion, request id 42
$CMD,INTERVAL,600000,43*..\n    ESP32 → MCXA153: new interval
$ACK,42,OK*..\n / $ACK,42,REJ,cooldown*..\n
```

`*XX` = XOR of all characters between `$` and `*`, in hex (as in NMEA GPS
sentences). On the MCXA153: a 3rd ISR (`LPUART1_IRQHandler` → stream buffer),
a new **Link** task (low priority), and remote requests go into FeedQ.

## Phases

Each phase ends with something that runs and a short note in
`docs/iot/LOG.md` (what was done, what went wrong, what was learned). That log
is the proof of work for job applications.

| # | Phase | Result | IoT skill |
|---|---|---|---|
| 0 | Setup | ESP-IDF in VS Code, AWS account with a budget alarm, AWS CLI | tooling, cloud account hygiene |
| 1 | ESP32 online | Wi-Fi station with reconnect, SNTP time, logs; Wi-Fi password not in git (`sdkconfig` / NVS) | ESP-IDF, FreeRTOS on ESP32, event loop |
| 2 | ESP32 ↔ AWS IoT Core | Thing, X.509 certificate, least-privilege policy; MQTT over TLS (port 8883); heartbeat, Last Will; tested with the AWS MQTT test client | mutual TLS, IoT policies, MQTT QoS/retain/LWT |
| 3 | Board link | UART protocol both ways; Link task on the MCXA153; feed from the AWS console | host/co-processor design, protocol design, ISR + RTOS |
| 4 | Device shadow | reported state (today, interval, last meal, version); desired `interval` from the cloud → delta → MCXA153 | digital twin, desired vs reported state |
| 5 | Reliability | reconnect with exponential backoff, offline buffer for events, task watchdog, command acks with ids | resilient devices, at-least-once delivery |
| 6 | Provisioning | Wi-Fi set up from a phone (ESP-IDF provisioning over BLE or SoftAP); certificate in its own flash partition, not in the firmware | provisioning, secrets handling |
| 7 | OTA | firmware update from AWS IoT Jobs (or S3 via HTTPS), dual partitions, automatic rollback if the new image doesn't confirm | secure OTA, A/B updates |
| 8 | Data and alerts | IoT rule → CloudWatch metrics and dashboard; SNS e-mail on "daily limit reached" and "feeder offline" | rules engine, monitoring, alerting |
| 9 | Portfolio | architecture diagram, README for the IoT part, threat model (one page), demo video | presenting the work |

Phases 1–3 are the core: after phase 3 the feeder is a working IoT device.
4–8 each add one skill and can be done in any order.

## Cost and safety

- AWS IoT Core free tier (first 12 months) covers this project: about 1 device
  and a few thousand messages a day. After that it costs cents per month.
  Phase 0 sets a **budget alarm** (for example 1 USD) so nothing runs up
  unnoticed.
- Certificates, private keys and Wi-Fi passwords are **never committed**.
  `.gitignore` covers them, and phase 2 adds a check.
- The repo is private while MIC5 is being graded. For job applications, make
  it public (or copy the IoT part to its own repo) after grading, once the
  secrets check has passed.

## What to say in an interview (the short version)

"I built a connected pet feeder. A FreeRTOS MCU does the real-time control
and the safety rules; an ESP32 that I programmed in ESP-IDF is the
connectivity co-processor. It talks MQTT over mutual TLS to AWS IoT Core,
keeps a device shadow, buffers events while offline, and updates itself over
the air with rollback. Cloud commands go through the same safety rules as
the physical button, so the cloud can never overfeed the pet."
