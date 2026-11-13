# OFM-Dali – RGBW-Erweiterung (Branch `v1dev` dieses Forks)

Dieser Fork ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse pro Leuchte (RGBW-Modus). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (Commit `6e7aca5`). Das Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY bleibt unverändert.

> **Status: auf Hardware in Test.** Gebaut, geflasht und mit einem echten Treiber (DA4-M, 4-Kanal-Konstantspannungs-Dimmer im Modus RGBW) am `GW-REG1-Dali` erprobt (13.11.2026): ETS-Import, Programmierung, DALI-Adressierung, Farbtelegramme, Gültigkeitsbits, Statusrückmeldung, Dimmen. Siehe [Testfälle](#5-testfälle) für den genauen Stand.

Details zu allen Änderungen im Vergleich zum Ausgangscode: [`doc/RGBW-Gegenueberstellung.md`](doc/RGBW-Gegenueberstellung.md).

## Was ist neu?

- Neuer Eintrag **„RGBW“** in der ETS beim Parameter *„Farbe ansteuern per“* (Einzeladressen **und** Gruppenadressen).
- Bei RGBW erscheinen **zwei Kommunikationsobjekte** (die gleichen Objektnummern wie bei den anderen Farbtypen, z. B. 306/307 für Kanal A0):
  - **Farbe RGBW** (Eingang, DPT 251.600, 6 Byte)
  - **Farbe RGBW Status** (Ausgang, DPT 251.600, 6 Byte)
- Das Gateway wertet die **Gültigkeitsbits** aus: Es werden nur die Kanäle geändert, die als gültig markiert sind (z. B. nur Weiß ändern, ohne RGB anzufassen).
- Ein Farbtelegramm **schaltet einen ausgeschalteten Kanal ein**, und **R = G = B = W = 0 schaltet ihn aus** (siehe [Ein-/Ausschaltverhalten](#ein-ausschaltverhalten)).
- Ansteuerung des Treibers über DALI DT8: **RGB-Pegel + Weiß-Pegel + `ACTIVATE`**. Die Einstellung *„Farbe übertragen per“* (RGB/xy) spielt bei RGBW keine Rolle.

### Telegramm DPT 251.600

| Byte | Inhalt |
|------|--------|
| 0 | Rot (0–255, in der ETS 0–100 %) |
| 1 | Grün |
| 2 | Blau |
| 3 | Weiß |
| 4 | reserviert (0x00) |
| 5 | Bit 3 = R gültig, Bit 2 = G gültig, Bit 1 = B gültig, Bit 0 = W gültig |

In der ETS zeigt *„Wert schreiben“* dafür je Kanal ein Häkchen (= Gültigkeitsbit) und ein Prozentfeld.

### DALI-Sequenz (je Farbänderung)

1. `DTR0=R, DTR1=G, DTR2=B` → `SET TEMPORARY RGB DIMLEVEL` (DT8, Befehl 235)
2. `DTR0=W, DTR1=255, DTR2=255` → `SET TEMPORARY WAF DIMLEVEL` (DT8, Befehl 236; 255 = MASK = Amber/Freecolour unverändert)
3. `ACTIVATE` (Befehl 226) – beide temporären Werte werden gemeinsam übernommen

Der Wert 255 wird – wie schon bei RGB – auf 254 begrenzt, weil 255 bei DALI „MASK“ (nicht ändern) bedeutet. Die Helligkeit läuft weiterhin über die normalen Objekte *Schalten* und *Dimmen*.

### Ein-/Ausschaltverhalten

| Ereignis | Verhalten bei RGBW-Kanälen |
|----------|----------------------------|
| **Ausschalten** eines RGBW-Kanals | Normales `DAPC 0`, die Leuchte fadet mit der Fade-Zeit des Treibers aus. Ein Umweg über die Farbe 0/0/0/0 ist **nicht** sinnvoll: der DA4-M zeigt dafür Weiß statt Schwarz (nur noch per Konsole `blackoff 1` zuschaltbar, Standard aus). Die zuletzt gesetzte Farbe bleibt im Gateway gespeichert. |
| **Einschalten** (*Schalten* Ein, *Dimmen absolut* > 0, Farbtelegramm an einen ausgeschalteten Kanal) | Zuerst wird die Farbe gesendet, solange die Leuchte noch aus ist. Der Treiber blendet sie bei Pegel 0 unsichtbar über. Sobald `QUERY_STATUS` (Bit 4 „Fade läuft“) meldet, dass der Farb-Fade beendet ist, wird der Einschaltwert gesendet. Der Treiber hat nur **eine** Fade-Engine: ein Pegelbefehl während des Farb-Fades bricht ihn ab und lässt eine Mischfarbe stehen. Antwortet der Treiber nicht, gilt als Rückfall Übertragungszeit (310 ms) + Fade-Zeit (per `QUERY_FADE_SPEEDS`, Standard 1000 ms) + Reserve (`margin`, 100 ms) + 300 ms. Ein weiteres Telegramm (Schalten, Dimmen, Sperre, Szene) verwirft ausstehende Schritte. |
| Farbtelegramm an einen **eingeschalteten** Kanal | Nur die Farbe ändert sich, die Helligkeit bleibt. |
| *Schalten* / *Dimmen absolut* | wie bisher. Die Farbe bleibt dabei erhalten (Rot, dann 25 % = Rot mit 25 %). |

Zusätzlich wurde dafür `koHandleDimmAbs` korrigiert: Der interne Ein/Aus-Merker (`currentState`) folgt jetzt dem Wert von *Dimmen absolut* (0 % = aus). Vorher blieb er nach „Dimmen auf 0“ auf *Ein* stehen, und ein folgendes Farbtelegramm wurde ignoriert. (Treppenhauslicht-Kanäle sind ausgenommen.)

Bewusst nicht angefasst (bekannte Lücken dieser Version): relatives Dimmen von aus (`RECALL_MIN`) bei gespeicherter Farbe Schwarz, Treppenhauslicht-Kanäle, Funktionstasten/Identify/ETS-Funktionen des Moduls, Szenen, die im Treiber selbst ausgeführt werden (die Farbe gilt danach als unbekannt).

**Serielle Konsole (Feintuning und Diagnose):** `settle nnnn` (Pause im Fallback, 0–5000 ms), `margin nnn` (Reserve auf die Fade-Zeit, 0–2000 ms), `onmode m [nnnn]` (`1` = Farbe zuerst, Standard; `0` = Pegel zuerst; nnnn = feste Pause vor dem Pegel, 0 = automatisch per `QUERY_STATUS`), `blackoff 0|1` (Farbe vor dem Ausschalten auf 0/0/0/0, Standard 0). Mit `#define RGBW_DIAG` in `DaliChannel.h` (Standard aus, nur Test) zusätzlich Log der DALI-Frames und der Abfragen `QUERY_ACTUAL_LEVEL`/`QUERY_STATUS`; diese Diagnose entfällt vor einem Pull Request.

Hinweis zum Ausgangscode: Bei den anderen Farbtypen (HSV, RGB, TW, xyY) aktualisiert ein Farbtelegramm an einen ausgeschalteten Kanal nur die KNX-Statusobjekte, sendet aber keinen DALI-Einschaltbefehl. Das wurde hier bewusst **nicht** angefasst, nur für RGBW korrigiert (siehe offene Punkte für den Maintainer).

## Bekannte Einschränkungen

- **Szenen/DCA-Konfiguration:** Die Szenen-Farbe (Konfigurationstransfer) kennt nur RGB. Bei RGBW-Kanälen wird dort das Weiß nicht mitgeschrieben. Szenen per KNX-Telegramm (Szenenobjekt) funktionieren über die im Treiber gespeicherten Szenen.
- **Getrennte Objekte** (Rot/Grün/Blau/Weiß einzeln, 8 Bit) gibt es bei RGBW *nicht* – nur das kombinierte 6-Byte-Objekt. Das kann später nachgerüstet werden.
- **HCL** (Human Centric Lighting) wirkt bei RGBW wie bisher nur auf die Helligkeit.
- Manche Treiber erwarten für Weiß statt `SET TEMPORARY WAF DIMLEVEL` den Befehl `SET TEMPORARY RGBWAF CONTROL`/`RGBWAF DIMLEVEL` (237) bzw. ein anderes Kanal-Mapping. Beim DA4-M (Modus RGBW) wird das Weiß über Befehl 236 angesteuert; andere Treiber sind nicht geprüft.
- **ETS-Applikation:** Der Parameter *colorType* wurde von 2 auf 3 Bit verbreitert und deshalb an eine freie Stelle im Parameterspeicher verschoben (Einzelkanal: Byte 21, Bit 5–7; Gruppe: Byte 17, Bit 2–4). Die Applikation ist dadurch **nicht** in-place zu Geräten mit der alten Version updatebar (`ReplacesVersions` enthält die neue Version nicht): Gerät neu anlegen bzw. neu parametrieren.

## Testanleitung

### Voraussetzungen

- Windows mit Git, VS Code + PlatformIO, ETS (für die Signatur des Producers) und **OpenKNXproducer ≥ 3.11.6** (für dreiteilige `ApplicationVersion`, z. B. `0.12.0`).
- DALI-Gateway-Hardware `GW-REG1-Dali` (RP2040), DALI-Netzteil und ein DT8-RGBW-Treiber (hier getestet: DA4-M, Kippschalter auf *RGBW*).

### 1. Firmware-Projekt mit diesem Fork einrichten

```powershell
# Firmware-Repo (Zweig v1dev) und Abhängigkeiten
git clone -b v1dev https://github.com/OpenKNX/GW-REG1-Dali.git
cd GW-REG1-Dali
git config --global url."https://github.com/".insteadOf "git@github.com:"   # dependencies.txt nutzt SSH-URLs
restore/Restore-Dependencies.ps1    # legt die Geschwisterordner an und verlinkt sie nach lib/

# Geschwisterordner OFM-Dali auf diesen Fork umstellen
cd ../OFM-Dali
git remote add rgbw https://github.com/Rangrous86/OFM-Dali.git
git fetch rgbw
git checkout -B v1dev rgbw/v1dev
```

`lib/OFM-Dali` im Firmware-Projekt ist ein Link auf diesen Geschwisterordner – damit nutzen ETS-Generator und Firmware automatisch den RGBW-Stand. Spätere Updates: im Ordner `OFM-Dali` `git pull rgbw v1dev`.

### 2. ETS-Applikation und Header erzeugen

In `GW-REG1-Dali/src/DaliGateway.xml` die `ApplicationVersion` auf `0.12.0` erhöhen (Teil `x.y` = ETS-Version, `z` = Firmware-Revision), dann:

```powershell
cd ..\GW-REG1-Dali
OpenKNXproducer create --Debug -h include/knxprod.h src/DaliGateway
```

Dabei entsteht `include/knxprod.h` mit `PT_colorType_RGBW` (= 4) und den neu berechneten Parameter-Offsets sowie `src/DaliGateway.knxprod` für die ETS. `include/knxprod.h` nicht von Hand ändern, sie wird immer vom Producer erzeugt.

### 3. ETS importieren

`.knxprod` in die ETS importieren, Gerät **neu anlegen** (siehe Einschränkung oben) und parametrieren: Kanal auf *Gerätetyp DT8 – Farbe*, *Farbe ansteuern per: RGBW*.

### 4. Firmware flashen

Environment `develop_REG1_USB_V1` bauen (VS Code / PlatformIO: *Build*). Ist kein automatischer Upload möglich (picotool braucht unter Windows den WinUSB-Treiber via Zadig), das Gerät mit gedrückter BOOTSEL-Taste anstecken und `.pio/build/develop_REG1_USB_V1/firmware.uf2` auf das Laufwerk `RPI-RP2` kopieren. Danach das Gerät per ETS programmieren (Vollständiger Download).

### 5. Testfälle

1. **DALI-Adressierung** (ETS: *Dali-GW → Adressierung*): Treiber scannen, lange Adresse eintragen, Kurzadresse `0` setzen (Kurzadresse n gehört zu Kanal An).
2. Gruppenadressen auf *Schalten*, *Dimmen absolut*, *Farbe RGBW* (DPT 251.600) und die zugehörigen Status-Objekte legen, Applikation programmieren.
3. In der ETS im Gruppenmonitor per *Wert schreiben* auf *Farbe RGBW* senden. Die Häkchen vor R/G/B/W sind die Gültigkeitsbits.

| # | Senden (Kanal ist zu Beginn **aus**, sofern nicht anders angegeben) | Erwartung |
|---|----------------------------------------------------|-----------|
| 1 | *Schalten* Ein, *Dimmen absolut* 50 % / 100 %, *Schalten* Aus | Leuchte reagiert; Status-Objekte folgen |
| 2 | Alle vier Häkchen, R 100 %, G/B/W 0 % | Leuchte **schaltet ein**, rein Rot; Status `FF 00 00 00 00 0F`, Schalten Status *Ein* |
| 3 | Alle vier Häkchen, G 100 %, übrige 0 % | rein Grün |
| 4 | Alle vier Häkchen, B 100 %, übrige 0 % | rein Blau |
| 5 | Alle vier Häkchen, W 100 %, übrige 0 % (der wichtigste Test!) | rein Weiß; Status `00 00 00 FF 00 0F` |
| 6 | Nur Häkchen **W**, 30 % (Leuchte an) | nur Weiß ändert sich, RGB bleibt wie zuvor |
| 7 | Nur Häkchen **R**, 100 % | nur Rot ändert sich, Weiß/Grün/Blau bleiben |
| 8 | Kein Häkchen / `… 00` in Byte 5 | **keine** gültigen Kanäle: Licht darf sich nicht ändern |
| 9 | Alle vier Häkchen, alle 0 % | Leuchte **schaltet aus**; Schalten Status *Aus*, Dimmen Status 0 % |
| 10 | Nach 9: *Schalten* Ein | Leuchte geht mit der **zuletzt gesetzten Farbe** an (nicht schwarz) |
| 11 | Nach 9 (zuletzt war z. B. Weiß gesetzt): Farbtelegramm R 100 % | Leuchte schaltet mit **Rot** ein, nicht mit der alten Farbe (Weiß); ein zweites Senden ist nicht nötig |
| 12 | *Dimmen absolut* 0 %, danach ein Farbtelegramm | Leuchte schaltet wieder ein (Merker bleibt nicht auf *Ein* hängen) |
| 13 | Rot gesetzt, dann *Dimmen absolut* 25 % | Rot mit 25 % Helligkeit |
| 14 | RGB/HSV/TW/xyY-Kanäle nachtesten | verhalten sich wie vorher |

Rohdaten-Beispiele (Byte 0–5, z. B. für Home Assistant `knx.send` mit `payload: [0, 0, 0, 255, 0, 15]`): rein Weiß `00 00 00 FF 00 0F`, nur Weiß ändern (50 %) `00 00 00 80 00 01`, nur Rot ändern `FF 00 00 00 00 08`, alles aus `00 00 00 00 00 0F`.

Die Debug-Ausgabe (OpenKNX-Logging, Level *Debug*, serielle Konsole 115200 Baud) zeigt pro Telegramm `Got RGBW: …`, `Send RGBW: …`, `RGBW: switching on with …` bzw. `RGBW: all channels 0 -> switching off`. Falls Weiß nicht leuchtet: Treiber-Datenblatt prüfen (RGBW-Mapping, Anzahl Farbkanäle) und mit dem Log vergleichen.

Optional, ohne Hardware: Host-Test der Logik

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

## Pull Request

Nach erfolgreichem Test kann aus `v1dev` dieses Forks ein Pull Request gegen `OpenKNX/OFM-Dali` (Basis-Branch **`v1dev`**) erstellt werden. Vorschlag für die Beschreibung: Zusammenfassung dieser README + [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md) + Testergebnis (getesteter Treiber, Auffälligkeiten).
