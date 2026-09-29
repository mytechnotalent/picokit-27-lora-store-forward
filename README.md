![picokit-27-lora-store-forward](https://raw.githubusercontent.com/mytechnotalent/picokit-27-lora-store-forward/main/picokit-27-lora-store-forward.png)

<br>

## FREE Reverse Engineering Self-Study Course [HERE](https://github.com/mytechnotalent/reverse-engineering)
## FREE Embedded Hacking Course [HERE](https://github.com/mytechnotalent/Embedded-Hacking)

<br>

# PICOKIT-27 LORA STORE FORWARD

### Buffered Readings and Flush on Link Recovery and Authenticated Heartbeat
#### Lesson 27 of the Picokit Series

<br>

***
**LEGAL DISCLAIMER:**
The information, tools, and code provided in this repository and course are strictly for educational, research, and defensive purposes only.

You are explicitly prohibited from using any materials contained herein to access, test, modify, or exploit any device, network, or system that you do not own 100% or for which you do not have explicit, documented, and legally binding authorization to interact with.

By using this repository and course, you acknowledge and agree that:

1. Any illegal, unauthorized, or malicious use of this information is solely your responsibility.
2. The author(s) and contributor(s) of this repository and course shall not be held liable for any damages, legal repercussions, criminal charges, or unauthorized actions resulting from the use, misuse, or abuse of the contents herein.
3. You will comply with all applicable local, state, national, and international laws regarding cybersecurity and computer fraud.

**IF YOU DO NOT AGREE WITH THESE TERMS, DO NOT USE THIS REPOSITORY AND COURSE.**
***

<br>
<br>

## Overview

The twenty-seventh Picokit lesson. The node samples the DHT11 on a two second
cadence, and while the downlink is up it uplinks each reading as an
authenticated heartbeat over LoRa. The gateway can mark the link down with a
downlink command, and from that moment the node stores every reading in a
bounded backlog; when the gateway marks the link up again the node flushes the
whole backlog in order before returning to live heartbeats.

<br>

## What it teaches

- Detecting link state from a plaintext downlink command.
- Storing readings in a bounded backlog while the link is down.
- Flushing the backlog in sequence when the link recovers.
- Reporting the backlog depth in the authenticated heartbeat body.

<br>

## Hardware

| Peripheral | Pico 2 pin | Role |
| --- | --- | --- |
| DHT11 | GP4 | buffered readings |
| Red / Yellow / Green | GP16 / GP18 / GP17 | link and sample status |
| Onboard LED | GP25 | heartbeat, one blink per transmit |
| RYLR998 | GP8 TX / GP9 RX | LoRa heartbeat |
| Debug Probe | SWCLK/SWDIO/GND, GP0/GP1 | SWD and the console |

<br>

## How it works

The node runs `monitor_step` in a loop. Every 2 seconds it samples the DHT11
and reflects the sample on the status LEDs. Every 5 seconds it transmits an
authenticated heartbeat; while the link is up the body is live, and while the
link is down the reading is counted in the backlog up to 255. A downlink
`LINK 0` marks the link down and a `LINK 1` marks it up and flushes every
buffered reading. The heartbeat body is
`{"n":27,"s":<seq>,"b":<backlog>}` sealed with the field key.

<br>

## Build and flash

```bash
cd firmware
cmake -S . -B build -G Ninja -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "program build/picokit_27_lora_store_forward.elf verify reset exit"
```

<br>

## Watch the node

Open the console at 115200 and reset:

```text
BOOT
=== PICOKIT-27 LORA STORE FORWARD // BUFFER + FLUSH + AUTHENTICATED HEARTBEAT ===
DHT t=230 h=610
STORE b=1
STORE b=2
LINK UP
FLUSH b=0
RX from 0x0001, N bytes
```

<br>

## The gateway

```bash
cd gateway
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 listen.py --port /dev/cu.usbserial-A50285BI --hub 0001 --network 18 --db gateway.db
```

It prints `OK node=27 rssi=...` per authenticated heartbeat. The terminal
dashboard `python3 tui.py --db gateway.db` and the web dashboard
`python3 web/app.py --db gateway.db` show the same rows.

<br>

## Verify

```bash
python3 .opencode/skill/embedded-c-standard/audit_c_standard.py
python3 .opencode/skill/embedded-python-standard/audit_python_standard.py
python3 .opencode/skill/iot-readme-standard/validate_readme.py
python3 .opencode/skill/iot-banner-standard/validate_banner.py
python3 scripts/run_tests.py
python3 scripts/check_coverage.py
```

<br>

# Next
[picokit-28-lcd-menu-button](https://github.com/mytechnotalent/picokit-28-lcd-menu-button)

<br>

# License
[MIT License](https://github.com/mytechnotalent/picokit-27-lora-store-forward/blob/main/LICENSE)
