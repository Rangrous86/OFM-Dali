# OFM-Dali – RGBW-Erweiterung

*[English version](README.en.md)*

Dieser Fork erweitert das OpenKNX-DALI-Gateway (`OFM-Dali`, Gerät `GW-REG1-Dali`) um **RGBW**: Rot, Grün, Blau und Weiß über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`), wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse je Leuchte (RGBW-Modus).

| | |
|---|---|
| **Stand** | `v14b` (Branch `v1dev` dieses Forks) |
| **Basis** | `OpenKNX/OFM-Dali`, Branch `v1dev`, Commit `a120578` |
| **ETS-Applikation** | 0.13 (eine Version über dem offiziellen Release 0.12; ein Gerät mit 0.12 lässt sich direkt aktualisieren) |
| **Getestet** | am 21.11.2026 mit zwei DA4-M (RGBW) am `GW-REG1-Dali`: Einzel-EVG, Gruppen, Broadcast, Sperre, Ausfall des Treibers, Neustart, bestanden ([Testprotokoll](doc/RGBW-Testprotokoll.md)) |
| **Unverändert** | die Farbtypen HSV, RGB, TW und xyY sowie alle anderen Kanaltypen (bis auf drei kleine Korrekturen, siehe 3) |

## Inhalt

1. [Was ist das?](#1-was-ist-das)
2. [Was ist neu?](#2-was-ist-neu)
3. [Was ist geändert?](#3-was-ist-geändert)
4. [Einrichtung in der ETS](#4-einrichtung-in-der-ets)
5. [Verhalten im Betrieb](#5-verhalten-im-betrieb)
6. [Einschränkungen und Hinweise](#6-einschränkungen-und-hinweise)
7. [Installation](#7-installation)
8. [Test](#8-test)
9. [Konsole](#9-konsole)
10. [Offen und Fragen](#10-offen-und-fragen)
11. [Weitere Dokumente](#11-weitere-dokumente)

## 1. Was ist das?

Das Gateway kann bisher Farbe als HSV, RGB, Farbtemperatur (TW) und xyY ansteuern, aber keinen eigenen Weißkanal. Viele RGBW-Leuchten und Visualisierungen (z. B. Home Assistant) arbeiten mit vier Kanälen. Die Erweiterung ergänzt dafür den Farbtyp **RGBW**:

- **Ein Objekt** *Farbe RGBW* mit R, G, B, W und Gültigkeitsbits je Kanal (DPT 251.600), dazu ein Statusobjekt.
- **DALI DT8, ein Treiber, eine Kurzadresse:** RGB-Pegel und Weiß-Pegel gehen als temporäre Farbe an den Treiber (`SET TEMPORARY RGB DIMLEVEL`, `SET TEMPORARY WAF DIMLEVEL`). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.
- **Farbe und Helligkeit in einem Fade:** Der Treiber hat nur eine Fade-Engine; jeder neue Befehl bricht einen laufenden Fade ab. Deshalb gehen Farbe und Helligkeit, wo es nötig ist, als **ein** Befehl raus (temporäre Farbe + `DAPC`, automatische Aktivierung nach IEC 62386-209).

Telegramm DPT 251.600:

| Byte | Inhalt |
|---|---|
| 0 | Rot (0–255, in der ETS 0–100 %) |
| 1 | Grün |
| 2 | Blau |
| 3 | Weiß |
| 4 | reserviert (0x00) |
| 5 | Bit 3 = R gültig, Bit 2 = G gültig, Bit 1 = B gültig, Bit 0 = W gültig |

Beispiele: rein Weiß `00 00 00 FF 00 0F`, nur Rot ändern `FF 00 00 00 00 08` (bei Einzeladressen; die anderen Kanäle bleiben), alles aus `00 00 00 00 00 0F`.

Ob Werte unter 100 % die Helligkeit ändern, hängt vom Parameter *Helligkeit bei RGBW* ab (Abschnitt 4):
- **Über Objekt „Dimmen absolut“** (Standard): Die Werte bestimmen nur die **Mischung**. `00 00 00 80 00 0F` ist wie `00 00 00 FF 00 0F` reines Weiß. Die Helligkeit kommt über *Dimmen absolut*, *Schalten* oder *Dimmen relativ*.
- **Aus dem RGBW-Wert:** Der größte Kanal ist die **Helligkeit**. `00 00 00 80 00 0F` ist dann Weiß mit etwa 50 %.

## 2. Was ist neu?

| Funktion | Kurz |
|---|---|
| **Farbtyp RGBW** für Einzeladressen und Gruppen | Objekte *Farbe RGBW* und *Farbe RGBW Status* (6 Byte). Nur gültig markierte Kanäle werden geändert (bei Einzeladressen); alle Kanäle 0 schaltet aus. |
| **Helligkeit bei RGBW** (Parameter) | *Über Objekt „Dimmen absolut“* (Standard: Farbe = nur Mischung) oder *Aus dem RGBW-Wert* (größter Kanal = Helligkeit, für Clients mit nur einem Objekt). |
| **Ein Befehl für Farbe und Helligkeit** | Telegramme innerhalb von 80 ms (Sammelfenster) gehen gemeinsam raus. Home Assistant schickt Farbe und Helligkeit etwa 20 ms auseinander; die Leuchte fadet in einem Zug. |
| **Gruppen geben den ganzen Zustand vor** | Jeder Gruppenbefehl bei brennender Gruppe trägt Farbe **und** Pegel; einzeln umgestellte Mitglieder kehren zum Gruppenzustand zurück. |
| **Farbe nach relativem Dimmen** | Hat ein Taster direkt nach einem Farbwechsel gedimmt, wird die Farbe nach dem Dimmen noch einmal gesendet. |
| **Broadcast – Farbe RGBW** (neues Modul-Objekt) | Farbe und Pegel an alle Leuchten in einem Befehl, ohne Status und ohne den gespeicherten Zustand zu ändern, z. B. für Alarm- oder Blinklicht. |
| **Broadcast – Zurück** (neues Modul-Objekt) | Alle RGBW-Gruppen und RGBW-Kanäle ohne Gruppe bekommen ihren gespeicherten Zustand zurück. |
| **Sperre bei RGBW** mit **„Bei Entsperren vorherigen Zustand wiederherstellen“** (Parameter) | Korrigiertes Sperrverhalten; mit „Ja“ werden Befehle während der Sperre gespeichert und beim Entsperren als ein Befehl ausgeführt. |
| **Konsolenbefehle** | `getCol`, `aastat`, `aaon`, `aawin` (Abschnitt 9). |

## 3. Was ist geändert?

**Für alle Kanaltypen** (kleine Korrekturen am Ausgangscode):

- *Dimmen absolut* 0 % setzt den internen Ein/Aus-Merker auf *aus*. Vorher blieb er auf *ein*, und ein folgendes Farbtelegramm wurde übergangen.
- Nach dem Stopp eines relativen Dimmens wird der Pegel ≈ 300 ms später einmal abgefragt und gemeldet (der letzte Dimmschritt läuft im Treiber noch).
- Die Startwerte für „Einschalten mit letztem Wert“ sind DALI-Pegel für 100 % bzw. 10 % (vorher die Zahlen 100 und 10, also ≈ 1,5 % bzw. 0,1 %).

**Nur bei RGBW** (der bisherige Weg bleibt für alle anderen Typen):

- Schalten, Dimmen und Farbe laufen über das Sammelfenster; Ausschalten ist sofort ein `DAPC 0`.
- *Schalten Status* wird nur bei einer Änderung gesendet und bei *Heller* aus „aus“ sofort.
- *Dunkler* bis zum Minimum schaltet aus und beendet das Dimmen.
- Sperre: eigener Zweig mit korrigiertem Verhalten (siehe 5).
- Broadcast *Schalten* sendet nur den Pegel, keine Farbe.

**ETS und Speicher:** Der Parameter *Farbe ansteuern per* hat jetzt 3 Bit und liegt deshalb an anderer Stelle im Parameterspeicher (Einzeladresse Byte 21, Gruppe Byte 17); die neuen Parameter nutzen frei gewordene Bits. Die Blockgröße ist unverändert. KNX-Speicher: 63 Byte frei. Alle Einzelheiten: [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md).

## 4. Einrichtung in der ETS

**Kanal oder Gruppe:**

1. *Gerätetyp* „DT8 – Farbe“ (Einzeladresse) bzw. „Farbig“ (Gruppe).
2. *Farbe ansteuern per*: **RGBW**. Es erscheinen *Farbe RGBW* (Eingang) und *Farbe RGBW Status* (Ausgang), bei A0 z. B. die Objekte 306/307.
3. *Helligkeit bei RGBW*: **Über Objekt „Dimmen absolut“** für Visualisierungen mit eigener Helligkeitsadresse (z. B. Home Assistant mit `brightness_address`), sonst **Aus dem RGBW-Wert**.
4. Optional *Sperrobjekt anzeigen* = Ja, dann *Verhalten bei Sperren/Entsperren* und **Bei Entsperren vorherigen Zustand wiederherstellen**.
5. *Farbe übertragen per* (RGB/xy) spielt bei RGBW keine Rolle.

**Allgemein (Modul):** Objekte *Dali: Broadcast – Farbe RGBW* (Objekt 6 des Moduls, in der ETS z. B. Nr. 26) und *Dali: Broadcast – Zurück* (Objekt 7, z. B. Nr. 27).

**Empfehlung für den Treiber** (Reiter *DALI Einstellungen* des Kanals): *Power On Level* und *Failure On Level* auf **„Inaktiv“** setzen und „Auf Gerät übertragen“. Dann kommt der Treiber nach einem Ausfall seiner Versorgung mit dem **letzten Pegel und der letzten Farbe** zurück (mit dem Werkswert: voll weiß) und bleibt bei einem Ausfall des DALI-Busses unverändert. Die Einstellung ist im Treiber gespeichert, also nur einmal übertragen.

## 5. Verhalten im Betrieb

| Telegramm | Am Treiber | Status |
|---|---|---|
| *Schalten* Ein / *Dimmen absolut* > 0 | gespeicherte Farbe + Pegel in **einem** Fade (Einschaltwert Tag/Nacht bzw. letzter Wert) | Schalten Ein, Dimmen % |
| *Schalten* Aus / *Dimmen absolut* 0 / Farbe alle 0 | `DAPC 0`, die Farbe bleibt im Treiber und im Gateway gespeichert | Aus, 0 % |
| Farbe an einen ausgeschalteten Kanal | Farbe wird im Treiber gesetzt, die Lampe **bleibt aus**; kommt im Fenster auch *Ein* oder eine Helligkeit, ein gemeinsamer Befehl | nur *Farbe RGBW Status* |
| Farbe allein, Lampe an | Einzel-EVG: Farbe, der Pegel bleibt. Gruppe: Farbe + Gruppenpegel | Farbe |
| Helligkeit allein, Lampe an | Einzel-EVG: Pegel, die Farbe bleibt. Gruppe: Gruppenfarbe + Pegel | Dimmen % |
| Farbe + Helligkeit innerhalb von 80 ms | ein Befehl, ein Fade | beide |
| *Dimmen relativ* | wie bisher (`UP`/`DOWN`); danach die Farbe noch einmal und Pegelabfrage. *Heller* aus „aus“ startet beim Minimum | Schalten Ein sofort, Dimmen % am Ende |
| Szene | unverändert: der Treiber fährt seine gespeicherte Szene an | nicht nachgeführt |
| Sperren | nur Pegel: *Anschalten* = Einschaltwert, *Ausschalten* = aus, *Fester Wert* in Prozent; Gruppen nutzen ihre eigenen Parameter | folgt |
| Während der Sperre | *Schalten*, *Dimmen*, *Farbe* werden verworfen, mit „wiederherstellen“ = Ja gespeichert | – |
| Entsperren | *Verhalten bei Entsperren*; mit „wiederherstellen“ = Ja der gespeicherte Zustand als ein Befehl | folgt |
| Broadcast *Schalten* | nur Pegel an alle (100 % bzw. aus), jede RGBW-Leuchte in der Farbe aus ihrem Treiber | Schalten aller Kanäle und Gruppen |
| *Broadcast – Farbe RGBW* | Farbe an alle RGBW-Treiber, Pegel (größter Kanal) an **alle** Treiber; alle 0 = aus | kein Status |
| *Broadcast – Zurück* | gespeicherter Zustand je RGBW-Gruppe und RGBW-Kanal ohne Gruppe | – |

**Blinklicht mit Rückkehr:** z. B. *Broadcast – Farbe RGBW* im Wechsel Rot 100 % und „alles 0“, am Ende *Broadcast – Zurück*. Jede RGBW-Gruppe kehrt in ihren Zustand von vorher zurück.

## 6. Einschränkungen und Hinweise

- **Überlappende Gruppen:** Liegt ein Treiber in mehreren Gruppen, gilt der zuletzt gesendete Befehl. Die Statusobjekte der anderen Gruppen und der Einzelkanäle werden nicht nachgeführt (Ausgangscode). Wer mehrere Gruppen auf denselben Zustand bringen will, sendet an jede Gruppe den passenden Befehl. Bei *Broadcast – Zurück* gilt deshalb die Gruppe mit der höchsten Nummer.
- **Gruppen bekommen immer die vollständige Farbe:** Kanäle ohne Gültigkeitsbit werden bei Gruppen 0.
- **Einzel-EVG:** Zwei getrennte Befehle (mehr als 80 ms auseinander) können sich den Fade gegenseitig abbrechen. Farbe und Helligkeit gemeinsam ändern = beides innerhalb von 80 ms senden.
- **Einschalten mit neuer Farbe aus „aus“:** Die Farbe blendet von der im Treiber gespeicherten Farbe über; bei niedrigem Pegel ist kurz eine Mischfarbe zu sehen (normgerecht). Ohne Mischfarbe: neue Farbe zuerst bei ausgeschalteter Lampe senden, dann einschalten.
- **Nach einem Neustart des Gateways** ist die Farbe bis zum ersten Farbtelegramm unbekannt (Ein/Aus und Pegel werden aus dem Treiber gelesen). Bis dahin geht beim Einschalten nur der Pegel.
- **Broadcast bei gemischten Anlagen:** Der Pegel von *Broadcast – Farbe RGBW* wirkt auch auf normale Dimm- und Schalttreiber; *Broadcast – Zurück* stellt nur RGBW zurück. *Broadcast Schalten/Dimmen* überschreiben den gespeicherten Zustand aller Kanäle; danach bringt *Zurück* diesen Zustand.
- **Szenen:** Szenenfarbe per Konfigurationstransfer ohne Weiß; Szenen im Treiber mit Pegel **und** Farbe speichern.
- **HCL:** Die Farbtemperatur der HCL auf einem RGBW-Kanal nicht aktivieren (wird noch nicht übersprungen); die Helligkeit funktioniert.
- **Treiber:** getestet nur mit dem DA4-M (RGBW-Modus). Der Treiber muss die automatische Aktivierung können (DT8, ab Werk an; prüfen mit `aastat`).
- **Taster am Gerät** (`RECALL MAX`/`OFF`) ohne RGBW-Behandlung, wie im Ausgangscode.

## 7. Installation

**Voraussetzungen:** Windows mit Git, VS Code + PlatformIO, ETS, OpenKNXproducer ≥ 3.11.6. Hardware `GW-REG1-Dali` (RP2040), DALI-Netzteil, DT8-RGBW-Treiber (z. B. DA4-M, Schalter auf *RGBW*).

**1. Firmware-Projekt mit diesem Fork einrichten**

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

`lib/OFM-Dali` im Firmware-Projekt ist ein Link auf diesen Ordner. Updates: im Ordner `OFM-Dali` `git pull rgbw v1dev`.

**2. Version setzen und ETS-Applikation erzeugen**

In `GW-REG1-Dali/src/DaliGateway.xml`:

```xml
ApplicationVersion="0.13.0"
ReplacesVersions="0.0 0.10 0.11 0.12"
```

```powershell
cd ..\GW-REG1-Dali
& "$HOME\bin\OpenKNXproducer.exe" create --Debug -h include/knxprod.h src/DaliGateway
```

Der Producer erzeugt `include/knxprod.h` und `src/DaliGateway.knxprod`. Der PlatformIO-Build erzeugt den Header nicht neu, deshalb nach jeder Änderung an den XML-Dateien zuerst den Producer laufen lassen.

**3. Build und Flashen**

```powershell
& "$HOME\.platformio\penv\Scripts\platformio.exe" run -e develop_REG1_USB_V1
```

Das Gateway in den Bootloader bringen (BOOTSEL-Taste oder Konsolenbefehl `bootloader`). Es meldet sich als Laufwerk `RPI-RP2`; `.pio/build/develop_REG1_USB_V1/firmware.uf2` dorthin kopieren. Laufwerk vorher prüfen:

```powershell
Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'
```

**4. ETS**

`DaliGateway.knxprod` importieren. Ein Gerät mit 0.12 per *Aktualisieren* umstellen, sonst neu einfügen. Parametrieren (Abschnitt 4), Download. Bis zum Download blinken die LEDs, wenn die Konfiguration nicht zur Version passt.

**Prüfen:** Konsole (115200 Baud, aus dem Projektordner starten):

```powershell
& "$HOME\.platformio\penv\Scripts\platformio.exe" device monitor --environment develop_REG1_USB_V1
```

`i` zeigt *Firmware Version 0.13.0* und *Configured*; `aastat` zeigt für jeden RGBW-Kanal *automatic activation ON*.

## 8. Test

### 8.1 Aufbau

| | |
|---|---|
| Hardware | `GW-REG1-Dali`, zwei DT8-RGBW-Treiber (DA4-M) an Kurzadresse 0 (A0) und 1 (A1) |
| DALI-Gruppen | A0 in Gruppe 0 und 1, A1 in Gruppe 0 und 2 → **G0** = A0 + A1, **G1** = nur A0, **G2** = nur A1 |
| ETS | A0, A1 als „DT8 – Farbe“, G0–G2 als „Farbig“, alle *Farbe ansteuern per* = RGBW, *Helligkeit bei RGBW* = über Objekt; Tag-Einschaltwert 100 %, Fade-Zeit 1 s |
| Senden | ETS-Gruppenmonitor (*Wert schreiben*) oder Home Assistant (`knx.send`, KNX-Licht mit `rgbw_color` und `brightness`) |
| Beobachten | Leuchte, ETS-Gruppenmonitor, serielle Konsole (Log-Level *Debug*) |

Gruppenadressen im Testaufbau:

| Objekt | A0 | A1 | G0 | G1 | G2 |
|---|---|---|---|---|---|
| Schalten | 20/0/0 | 20/0/8 | 20/0/15 | 20/0/22 | 20/0/29 |
| Schalten Status | 20/0/4 | 20/0/12 | 20/0/16 | 20/0/23 | 20/0/30 |
| Dimmen relativ | 20/0/6 | 20/0/14 | 20/0/17 | 20/0/24 | 20/0/31 |
| Dimmen absolut | 20/0/1 | 20/0/9 | 20/0/18 | 20/0/25 | 20/0/32 |
| Dimmen Status | 20/0/5 | 20/0/13 | 20/0/19 | 20/0/26 | 20/0/33 |
| Farbe RGBW | 20/0/2 | 20/0/10 | 20/0/20 | 20/0/27 | 20/0/34 |
| Farbe RGBW Status | 20/0/3 | 20/0/11 | 20/0/21 | 20/0/28 | 20/0/35 |
| Sperren | 20/0/40 | 20/0/41 | 20/0/42 | 20/0/43 | 20/0/44 |

Broadcast: Schalten 20/0/36, Dimmen absolut 20/0/37, Farbe RGBW 20/0/38, Zurück 20/0/39.

### 8.2 Testablauf

Jeder Test: vorher beschreiben, was gesendet wird und was als bestanden gilt; dann senden. Erwartete Logzeilen in `code`.

| # | Senden | Bestanden, wenn |
|---|---|---|
| E1 | A0 aus; Rot, 20 ms später 50 % | **eine** Zeile `RGBW: level 229 with temporary colour (one fade)`, Lampe rot ≈ 50 % in einem Fade |
| E2 | Farbe allein, Lampe an | `RGBW: colour (ACTIVATE)`, Pegel bleibt |
| E3 | Helligkeit allein, Lampe an | `RGBW: level N`, Farbe bleibt |
| E4 | Aus, dann Farbe | `colour for a switched-off channel, lamp stays off (ACTIVATE)`, Lampe bleibt dunkel, *Schalten Status* bleibt Aus; danach *Ein*: direkt in der neuen Farbe |
| E5 | Farbe, sofort *Heller*, Stopp | nach dem Dimmen `RGBW: colour again after relative dimming`, am Ende die neue Farbe; Stopp erst nach dem Maximum: `RGBW: dimming already ended, stop ignored` |
| E6 | Aus, dann *Heller* | *Schalten Status* Ein sofort (nicht erst beim Stopp) |
| E7 | Home Assistant: aus, dann Farbe + 50 % | **eine** Zeile `level 229 with temporary colour (one fade)` |
| G1 | G0 Ein, G0 Farbe allein, G0 Helligkeit allein | Ein ohne Farbe (noch unbekannt); dann jeweils **eine** Zeile mit Farbe **und** Pegel |
| G2 | Farbe an G2, danach Farbe an G0 | A1 folgt zuerst G2, dann wieder G0 |
| G3 | A0 einzeln umstellen, danach Helligkeit an G0 | A0 kehrt zur Gruppenfarbe zurück |
| G4 | G0 aus, Farbe, Ein | dunkel beim Farbwechsel, beim Einschalten direkt die neue Farbe |
| B1 | *Broadcast Schalten* Aus/Ein, *Broadcast Dimmen* 30 % | nur `DAPC` an alle, keine Farbe |
| B2 | *Broadcast – Farbe RGBW* Rot | Farbe an `0xFF`, `DAPC` an `0xFE`, **kein** Status |
| B3 | Blinken (Rot / alles 0), dann *Zurück* | während des Blinkens kein Status; danach alle im Zustand von vorher, ohne Mischfarbe |
| B4 | *Broadcast – Farbe RGBW*, *Broadcast Schalten* Aus, *Zurück* | Lampen bleiben aus (*Schalten* ist endgültig) |
| S1 | Sperre mit *Ausschalten*/*Anschalten*, wiederherstellen = Nein | aus beim Sperren, *Ein* während der Sperre wird ignoriert (`is locked`), Einschaltwert beim Entsperren |
| S2 | Sperre mit *Fester Wert* 50 %, wiederherstellen = Ja; während der Sperre Farbe + Helligkeit | 50 % beim Sperren; während der Sperre nur `locked, stored for the unlock`; beim Entsperren **ein** Befehl mit dem gespeicherten Zustand |
| S3 | Gruppensperre mit eigenen Werten | die Gruppe nutzt ihre eigenen Parameter |
| A1 | Power On / Failure „Inaktiv“; Versorgung bzw. DALI-Bus des Treibers kurz trennen | Lampe kommt mit letztem Pegel und letzter Farbe zurück bzw. bleibt unverändert |
| R1 | Gateway neu starten (`restart`) bei brennender Lampe, danach Farbe | Lampe unverändert; Ein/Aus und Pegel gemeldet; erste Farbe mit `ACTIVATE`, Pegel bleibt |

**Ergebnis 21.11.2026:** alle Tests bestanden, auch der Schlusstest mit der Firmware ohne Diagnose. Logs und Einzelheiten: [Testprotokoll](doc/RGBW-Testprotokoll.md).

**Host-Test ohne Hardware:**

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

## 9. Konsole

| Befehl | Wirkung |
|---|---|
| `getCol yy [sel]` | fragt Farbtyp-Merkmale, Farbstatus und Farbwerte des Treibers `yy` ab, ändert nichts; ohne `sel`: Report (233–236), aktuell (9–12), temporär (201–204) je R, G, B, W |
| `aastat` | liest bei allen RGBW-Kanälen die DT8-Einstellung *automatische Aktivierung* |
| `aaon [yy]` | schaltet die automatische Aktivierung dauerhaft im Treiber ein (ohne `yy` bei allen RGBW-Kanälen); am DA4-M ab Werk an, deshalb dort ungetestet |
| `aawin nnn` | Sammelfenster in ms (0–1000, Standard 80, 0 = sofort senden); gilt bis zum Neustart |

**Funktioniert RGBW nicht wie beschrieben** (z. B. Farbe und Helligkeit kommen nicht in einem Fade): zuerst `aastat`, bei *OFF* `aaon`.

Wichtige Logzeilen: `Color absolute [v14b]`, `RGBW: level N with temporary colour (one fade)`, `RGBW: group colour with the group level N (one fade)`, `RGBW: colour (ACTIVATE)`, `RGBW: colour for a switched-off channel, lamp stays off (ACTIVATE)`, `RGBW: off`, `RGBW: colour again after relative dimming`, `RGBW: locking`/`unlocking`, `RGBW: stored state sent again`, `Broadcast RGBW: …`, `Broadcast zurueck: …`.

## 10. Offen und Fragen

**Offen:**
- Regression der anderen Farbtypen (HSV, RGB, TW, xyY) und Kanaltypen auf Hardware.
- Szenen mit Farbe, Tag/Nacht-Einschaltwerte, Treppenhauslicht und HCL mit RGBW.
- Andere Treiber als der DA4-M.
- Versionskennung im Log (`[v14b]`) vor dem Pull Request entfernen oder behalten.

**Fragen an den Maintainer:** zur RGBW-Erweiterung in der [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md), Abschnitt 7; zu Stellen im Ausgangscode und in der DALI-Bibliothek in [RGBW-Befunde-Ausgangscode.md](doc/RGBW-Befunde-Ausgangscode.md).

## 11. Weitere Dokumente

| Dokument | Inhalt |
|---|---|
| [doc/RGBW-Gegenueberstellung.md](doc/RGBW-Gegenueberstellung.md) | Änderungen am Code: ersetzte Stellen, Neues, Vorher/Nachher, wieder Entferntes, ETS und Speicher, vollständiges Diff |
| [doc/RGBW-Testprotokoll.md](doc/RGBW-Testprotokoll.md) | Versionsverlauf, Befunde zum Treiber (B1–B29), alle Testrunden mit Logs |
| [doc/RGBW-Befunde-Ausgangscode.md](doc/RGBW-Befunde-Ausgangscode.md) | Befunde im Ausgangscode und in der Bibliothek, für den Maintainer |
| [doc/README-Arbeitsfassung-2026-11-21.md](doc/README-Arbeitsfassung-2026-11-21.md) | Archiv: Arbeitsfassung dieser README vom 21.11.2026 |
| [doc/README-Rohfassung-2026-11-16.md](doc/README-Rohfassung-2026-11-16.md) | Archiv: Rohfassung vom 16.11.2026 |
| [doc/Applikationsbeschreibung-Dali.md](doc/Applikationsbeschreibung-Dali.md) | Applikationsbeschreibung des Gateways (Ausgangscode) |

**Pull Request** gegen `OpenKNX/OFM-Dali` (Basis `v1dev`) erst nach ausdrücklicher Freigabe.
