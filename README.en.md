# OFM-Dali – RGBW extension

*[Deutsche Fassung](README.md)* (the German version is the reference; the documents in `doc/` are in German)

This fork extends the OpenKNX DALI gateway (`OFM-Dali`, device `GW-REG1-Dali`) with **RGBW**: red, green, blue and white through **one 6-byte object according to KNX DPT 251.600** (`DPT_Colour_RGBW`), as offered for example by the MDT DALI Control gateway. It controls DALI DT8 drivers with **one** short address per luminaire (RGBW mode).

| | |
|---|---|
| **Version** | `v14b` (branch `v1dev` of this fork) |
| **Base** | `OpenKNX/OFM-Dali`, branch `v1dev`, commit `a120578` |
| **ETS application** | 0.13 (one version above the official release 0.12; a device on 0.12 can be updated directly) |
| **Tested** | on 21 Nov 2026 with two DA4-M (RGBW) on a `GW-REG1-Dali`: single gear, groups, broadcast, lock, driver power loss, restart, all passed ([test log, German](doc/RGBW-Testprotokoll.md)) |
| **Unchanged** | the colour types HSV, RGB, TW and xyY and all other channel types (except three small fixes, see 3) |

## Contents

1. [What is it?](#1-what-is-it)
2. [What is new?](#2-what-is-new)
3. [What has changed?](#3-what-has-changed)
4. [ETS setup](#4-ets-setup)
5. [Behaviour in operation](#5-behaviour-in-operation)
6. [Limitations and notes](#6-limitations-and-notes)
7. [Installation](#7-installation)
8. [Test](#8-test)
9. [Console](#9-console)
10. [Open points and questions](#10-open-points-and-questions)
11. [Further documents](#11-further-documents)

## 1. What is it?

So far the gateway can control colour as HSV, RGB, colour temperature (TW) and xyY, but has no separate white channel. Many RGBW luminaires and visualisations (e.g. Home Assistant) work with four channels. The extension adds the colour type **RGBW**:

- **One object** *Farbe RGBW* (colour RGBW) with R, G, B, W and a valid bit per channel (DPT 251.600), plus a status object.
- **DALI DT8, one driver, one short address:** the RGB level and the white level are sent to the driver as a temporary colour (`SET TEMPORARY RGB DIMLEVEL`, `SET TEMPORARY WAF DIMLEVEL`). The DT6 approach "four addresses for four channels" is not supported.
- **Colour and brightness in one fade:** the driver has only one fade engine; every new command aborts a running fade. Where needed, colour and brightness are therefore sent as **one** command (temporary colour + `DAPC`, automatic activation according to IEC 62386-209).

Telegram DPT 251.600:

| Byte | Content |
|---|---|
| 0 | Red (0–255, 0–100 % in the ETS) |
| 1 | Green |
| 2 | Blue |
| 3 | White |
| 4 | reserved (0x00) |
| 5 | bit 3 = R valid, bit 2 = G valid, bit 1 = B valid, bit 0 = W valid |

Examples: pure white `00 00 00 FF 00 0F`, white only at 50 % `00 00 00 80 00 01`, change red only `FF 00 00 00 00 08`, all off `00 00 00 00 00 0F`.

## 2. What is new?

| Feature | In short |
|---|---|
| **Colour type RGBW** for single addresses and groups | Objects *Farbe RGBW* and *Farbe RGBW Status* (6 bytes). Only channels marked valid are changed (single addresses); all channels 0 switches off. |
| **Brightness with RGBW** (parameter) | *Via object "Dimmen absolut"* (default: colour = mix only) or *From the RGBW value* (largest channel = brightness, for clients with only one object). |
| **One command for colour and brightness** | Telegrams within 80 ms (collection window) are sent together. Home Assistant sends colour and brightness about 20 ms apart; the luminaire fades in one go. |
| **Groups set the whole state** | Every group command to a lit group carries colour **and** level; members that were changed individually return to the group state. |
| **Colour after relative dimming** | If a push button dimmed right after a colour change, the colour is sent once more after dimming. |
| **Broadcast – colour RGBW** (new module object) | Colour and level to all luminaires in one command, without status and without changing the stored state, e.g. for an alarm or flashing light. |
| **Broadcast – back** (new module object) | All RGBW groups and all RGBW channels without a group get their stored state back. |
| **Lock with RGBW** with **"restore previous state on unlock"** (parameter) | Corrected lock behaviour; with "yes", commands during the lock are stored and executed as one command on unlock. |
| **Console commands** | `getCol`, `aastat`, `aaon`, `aawin` (section 9). |

## 3. What has changed?

**For all channel types** (small fixes to the original code):

- *Dimmen absolut* 0 % sets the internal on/off flag to *off*. Before, it stayed *on* and a following colour telegram was skipped.
- After the stop of a relative dimming, the level is queried and reported once ≈ 300 ms later (the last dimming step is still running in the driver).
- The start values for "switch on with last value" are DALI levels for 100 % and 10 % (before: the numbers 100 and 10, i.e. ≈ 1.5 % and 0.1 %).

**RGBW only** (the existing path is kept for all other types):

- Switching, dimming and colour go through the collection window; switching off is an immediate `DAPC 0`.
- *Schalten Status* (switch status) is only sent on change, and immediately on *brighter* from "off".
- *Darker* down to the minimum switches off and ends dimming.
- Lock: own branch with corrected behaviour (see 5).
- Broadcast *switch* sends the level only, no colour.

**ETS and memory:** the parameter *Farbe ansteuern per* (colour control) now has 3 bits and therefore moved within the parameter memory (single address byte 21, group byte 17); the new parameters use the freed bits. The block size is unchanged. KNX memory: 63 bytes free. All details: [comparison (German)](doc/RGBW-Gegenueberstellung.md).

## 4. ETS setup

**Channel or group:**

1. *Gerätetyp* (device type) "DT8 – Farbe" (single address) or "Farbig" (group).
2. *Farbe ansteuern per*: **RGBW**. The objects *Farbe RGBW* (input) and *Farbe RGBW Status* (output) appear, for A0 e.g. objects 306/307.
3. *Helligkeit bei RGBW* (brightness with RGBW): **via object "Dimmen absolut"** for visualisations with their own brightness address (e.g. Home Assistant with `brightness_address`), otherwise **from the RGBW value**.
4. Optional *Sperrobjekt anzeigen* (show lock object) = yes, then *behaviour on lock/unlock* and **Bei Entsperren vorherigen Zustand wiederherstellen** (restore previous state on unlock).
5. *Farbe übertragen per* (RGB/xy) does not apply to RGBW.

**General (module):** objects *Dali: Broadcast – Farbe RGBW* (module object 6, e.g. no. 26 in the ETS) and *Dali: Broadcast – Zurück* (object 7, e.g. no. 27).

**Recommendation for the driver** (tab *DALI Einstellungen* of the channel): set *Power On Level* and *Failure On Level* to **"Inaktiv"** (inactive) and press "Auf Gerät übertragen" (write to device). The driver then comes back after a loss of its supply with the **last level and the last colour** (with the factory value: full white) and stays unchanged on a loss of the DALI bus. The setting is stored in the driver, so it is written once.

## 5. Behaviour in operation

| Telegram | At the driver | Status |
|---|---|---|
| *Schalten* on / *Dimmen absolut* > 0 | stored colour + level in **one** fade (day/night switch-on value or last value) | switch on, dim % |
| *Schalten* off / *Dimmen absolut* 0 / colour all 0 | `DAPC 0`; the colour stays stored in the driver and in the gateway | off, 0 % |
| Colour to a channel that is off | the colour is set in the driver, the lamp **stays off**; if *on* or a brightness also arrives within the window, one combined command | only *Farbe RGBW Status* |
| Colour only, lamp on | single gear: colour, the level stays. Group: colour + group level | colour |
| Brightness only, lamp on | single gear: level, the colour stays. Group: group colour + level | dim % |
| Colour + brightness within 80 ms | one command, one fade | both |
| *Dimmen relativ* | as before (`UP`/`DOWN`); afterwards the colour once more and a level query. *Brighter* from "off" starts at the minimum | switch on immediately, dim % at the end |
| Scene | unchanged: the driver recalls its stored scene | not updated |
| Lock | level only: *Anschalten* (switch on) = switch-on value, *Ausschalten* (switch off) = off, *Fester Wert* (fixed value) as a percentage; groups use their own parameters | follows |
| During the lock | *switch*, *dim*, *colour* are discarded; with "restore" = yes they are stored | – |
| Unlock | *behaviour on unlock*; with "restore" = yes the stored state as one command | follows |
| Broadcast *switch* | level only to all (100 % or off), each RGBW luminaire in the colour stored in its driver | switch status of all channels and groups |
| *Broadcast – Farbe RGBW* | colour to all RGBW drivers, level (largest channel) to **all** drivers; all 0 = off | no status |
| *Broadcast – Zurück* | stored state per RGBW group and RGBW channel without a group | – |

**Flashing light with return:** e.g. *Broadcast – Farbe RGBW* alternating red 100 % and "all 0", finally *Broadcast – Zurück*. Every RGBW group returns to its previous state.

## 6. Limitations and notes

- **Overlapping groups:** if a driver is in several groups, the last command wins. The status objects of the other groups and of the single channels are not updated (original code). To bring several groups to the same state, send the matching command to each group. With *Broadcast – Zurück* the group with the highest number therefore wins.
- **Groups always get the complete colour:** channels without a valid bit become 0 for groups.
- **Single gear:** two separate commands (more than 80 ms apart) can abort each other's fade. To change colour and brightness together, send both within 80 ms.
- **Switching on with a new colour from "off":** the colour fades from the colour stored in the driver; at a low level a mixed colour is briefly visible (as specified by the standard). Without a mixed colour: send the new colour first while the lamp is off, then switch on.
- **After a restart of the gateway** the colour is unknown until the first colour telegram (on/off and level are read from the driver). Until then, switching on sends the level only.
- **Broadcast in mixed installations:** the level of *Broadcast – Farbe RGBW* also affects normal dimming and switching drivers; *Broadcast – Zurück* restores RGBW only. *Broadcast switch/dim* overwrite the stored state of all channels; afterwards *Zurück* brings that state.
- **Scenes:** scene colour via configuration transfer has no white; store scenes in the driver with level **and** colour.
- **HCL:** do not activate the HCL colour temperature on an RGBW channel (it is not skipped yet); brightness works.
- **Drivers:** tested with the DA4-M (RGBW mode) only. The driver must support automatic activation (DT8, on by default; check with `aastat`).
- **Push button on the device** (`RECALL MAX`/`OFF`) without RGBW handling, as in the original code.

## 7. Installation

**Requirements:** Windows with Git, VS Code + PlatformIO, ETS, OpenKNXproducer ≥ 3.11.6. Hardware `GW-REG1-Dali` (RP2040), DALI power supply, DT8 RGBW driver (e.g. DA4-M, switch set to *RGBW*).

**1. Set up the firmware project with this fork**

```powershell
git clone -b v1dev https://github.com/OpenKNX/GW-REG1-Dali.git
cd GW-REG1-Dali
git config --global url."https://github.com/".insteadOf "git@github.com:"
restore/Restore-Dependencies.ps1

cd ../OFM-Dali
git remote add rgbw https://github.com/Rangrous86/OFM-Dali.git
git fetch rgbw
git checkout -B v1dev rgbw/v1dev
```

`lib/OFM-Dali` in the firmware project is a link to this folder. Updates: `git pull rgbw v1dev` in the `OFM-Dali` folder.

**2. Set the version and generate the ETS application**

In `GW-REG1-Dali/src/DaliGateway.xml`:

```xml
ApplicationVersion="0.13.0"
ReplacesVersions="0.0 0.10 0.11 0.12"
```

```powershell
cd ..\GW-REG1-Dali
& "$HOME\bin\OpenKNXproducer.exe" create --Debug -h include/knxprod.h src/DaliGateway
```

The producer generates `include/knxprod.h` and `src/DaliGateway.knxprod`. The PlatformIO build does not regenerate the header, so run the producer first after every change to the XML files.

**3. Build and flash**

```powershell
& "$HOME\.platformio\penv\Scripts\platformio.exe" run -e develop_REG1_USB_V1
```

Put the gateway into the bootloader (BOOTSEL button or console command `bootloader`). It appears as drive `RPI-RP2`; copy `.pio/build/develop_REG1_USB_V1/firmware.uf2` to it. Check the drive first:

```powershell
Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'
```

**4. ETS**

Import `DaliGateway.knxprod`. Update a device on 0.12, otherwise insert it again. Set the parameters (section 4), download. Until the download the LEDs blink if the configuration does not match the version.

**Check:** console (115200 baud, start it from the project folder):

```powershell
& "$HOME\.platformio\penv\Scripts\platformio.exe" device monitor --environment develop_REG1_USB_V1
```

`i` shows *Firmware Version 0.13.0* and *Configured*; `aastat` shows *automatic activation ON* for every RGBW channel.

## 8. Test

### 8.1 Setup

| | |
|---|---|
| Hardware | `GW-REG1-Dali`, two DT8 RGBW drivers (DA4-M) at short address 0 (A0) and 1 (A1) |
| DALI groups | A0 in groups 0 and 1, A1 in groups 0 and 2 → **G0** = A0 + A1, **G1** = A0 only, **G2** = A1 only |
| ETS | A0, A1 as "DT8 – Farbe", G0–G2 as "Farbig", all *Farbe ansteuern per* = RGBW, *Helligkeit bei RGBW* = via object; day switch-on value 100 %, fade time 1 s |
| Sending | ETS group monitor (*write value*) or Home Assistant (`knx.send`, KNX light with `rgbw_color` and `brightness`) |
| Observing | luminaire, ETS group monitor, serial console (log level *Debug*) |

Group addresses in the test setup:

| Object | A0 | A1 | G0 | G1 | G2 |
|---|---|---|---|---|---|
| Switch | 20/0/0 | 20/0/8 | 20/0/15 | 20/0/22 | 20/0/29 |
| Switch status | 20/0/4 | 20/0/12 | 20/0/16 | 20/0/23 | 20/0/30 |
| Dim relative | 20/0/6 | 20/0/14 | 20/0/17 | 20/0/24 | 20/0/31 |
| Dim absolute | 20/0/1 | 20/0/9 | 20/0/18 | 20/0/25 | 20/0/32 |
| Dim status | 20/0/5 | 20/0/13 | 20/0/19 | 20/0/26 | 20/0/33 |
| Colour RGBW | 20/0/2 | 20/0/10 | 20/0/20 | 20/0/27 | 20/0/34 |
| Colour RGBW status | 20/0/3 | 20/0/11 | 20/0/21 | 20/0/28 | 20/0/35 |
| Lock | 20/0/40 | 20/0/41 | 20/0/42 | 20/0/43 | 20/0/44 |

Broadcast: switch 20/0/36, dim absolute 20/0/37, colour RGBW 20/0/38, back 20/0/39.

### 8.2 Test sequence

For every test: first describe what is sent and what counts as passed, then send. Expected log lines in `code`.

| # | Send | Passed if |
|---|---|---|
| E1 | A0 off; red, 20 ms later 50 % | **one** line `RGBW: level 229 with temporary colour (one fade)`, lamp red ≈ 50 % in one fade |
| E2 | colour only, lamp on | `RGBW: colour (ACTIVATE)`, the level stays |
| E3 | brightness only, lamp on | `RGBW: level N`, the colour stays |
| E4 | off, then colour | `colour for a switched-off channel, lamp stays off (ACTIVATE)`, lamp stays dark, switch status stays off; then *on*: directly in the new colour |
| E5 | colour, immediately *brighter*, stop | after dimming `RGBW: colour again after relative dimming`, the new colour at the end; stop only after the maximum: `RGBW: dimming already ended, stop ignored` |
| E6 | off, then *brighter* | switch status on immediately (not only at the stop) |
| E7 | Home Assistant: off, then colour + 50 % | **one** line `level 229 with temporary colour (one fade)` |
| G1 | G0 on, G0 colour only, G0 brightness only | on without colour (still unknown); then **one** line each with colour **and** level |
| G2 | colour to G2, then colour to G0 | A1 follows G2 first, then G0 again |
| G3 | change A0 individually, then brightness to G0 | A0 returns to the group colour |
| G4 | G0 off, colour, on | dark during the colour change, directly the new colour on switching on |
| B1 | *broadcast switch* off/on, *broadcast dim* 30 % | only `DAPC` to all, no colour |
| B2 | *Broadcast – Farbe RGBW* red | colour to `0xFF`, `DAPC` to `0xFE`, **no** status |
| B3 | flashing (red / all 0), then *Zurück* | no status while flashing; afterwards everything in the previous state, without a mixed colour |
| B4 | *Broadcast – Farbe RGBW*, *broadcast switch* off, *Zurück* | lamps stay off (*switch* is final) |
| S1 | lock with *switch off*/*switch on*, restore = no | off on lock, *on* during the lock is ignored (`is locked`), switch-on value on unlock |
| S2 | lock with *fixed value* 50 %, restore = yes; colour + brightness during the lock | 50 % on lock; during the lock only `locked, stored for the unlock`; on unlock **one** command with the stored state |
| S3 | group lock with its own values | the group uses its own parameters |
| A1 | power on / failure "Inaktiv"; briefly disconnect the supply or the DALI bus of the driver | lamp comes back with last level and last colour, or stays unchanged |
| R1 | restart the gateway (`restart`) with the lamp on, then colour | lamp unchanged; on/off and level reported; first colour with `ACTIVATE`, level stays |

**Result 21 Nov 2026:** all tests passed, including the final test with the firmware without diagnostics. Logs and details: [test log (German)](doc/RGBW-Testprotokoll.md).

**Host test without hardware:**

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

## 9. Console

| Command | Effect |
|---|---|
| `getCol yy [sel]` | queries colour type features, colour status and colour values of driver `yy`, changes nothing; without `sel`: report (233–236), actual (9–12), temporary (201–204) for R, G, B, W |
| `aastat` | reads the DT8 setting *automatic activation* of all RGBW channels |
| `aaon [yy]` | switches automatic activation on permanently in the driver (without `yy`: all RGBW channels); on the DA4-M it is on by default, so it is untested there |
| `aawin nnn` | collection window in ms (0–1000, default 80, 0 = send immediately); valid until restart |

**If RGBW does not work as described** (e.g. colour and brightness do not come in one fade): first `aastat`, if *OFF* then `aaon`.

Important log lines: `Color absolute [v14b]`, `RGBW: level N with temporary colour (one fade)`, `RGBW: group colour with the group level N (one fade)`, `RGBW: colour (ACTIVATE)`, `RGBW: colour for a switched-off channel, lamp stays off (ACTIVATE)`, `RGBW: off`, `RGBW: colour again after relative dimming`, `RGBW: locking`/`unlocking`, `RGBW: stored state sent again`, `Broadcast RGBW: …`, `Broadcast zurueck: …`.

## 10. Open points and questions

**Open:**
- Regression test of the other colour types (HSV, RGB, TW, xyY) and channel types on hardware.
- Scenes with colour, day/night switch-on values, staircase light and HCL with RGBW.
- Drivers other than the DA4-M.
- Version tag in the log (`[v14b]`): remove or keep before the pull request.

**Questions to the maintainer:** about the RGBW extension in the [comparison](doc/RGBW-Gegenueberstellung.md), section 7; about places in the original code and in the DALI library in [RGBW-Befunde-Ausgangscode.md](doc/RGBW-Befunde-Ausgangscode.md) (German).

## 11. Further documents

All in German:

| Document | Content |
|---|---|
| [doc/RGBW-Gegenueberstellung.md](doc/RGBW-Gegenueberstellung.md) | code changes: replaced places, new parts, before/after, removed again, ETS and memory, full diff |
| [doc/RGBW-Testprotokoll.md](doc/RGBW-Testprotokoll.md) | version history, driver findings (B1–B29), all test rounds with logs |
| [doc/RGBW-Befunde-Ausgangscode.md](doc/RGBW-Befunde-Ausgangscode.md) | findings in the original code and the library, for the maintainer |
| [doc/README-Arbeitsfassung-2026-11-21.md](doc/README-Arbeitsfassung-2026-11-21.md) | archive: working version of this README from 21 Nov 2026 |
| [doc/README-Rohfassung-2026-11-16.md](doc/README-Rohfassung-2026-11-16.md) | archive: raw version from 16 Nov 2026 |
| [doc/Applikationsbeschreibung-Dali.md](doc/Applikationsbeschreibung-Dali.md) | application description of the gateway (original code) |

**Pull request** against `OpenKNX/OFM-Dali` (base `v1dev`) only after explicit approval.
