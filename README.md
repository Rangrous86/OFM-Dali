# OFM-Dali – RGBW-Erweiterung (Branch `feature/rgbw`)

Dieser Branch ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (Commit `6e7aca5`). Es ändert sich nichts am Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY.

> **Status: ungetestet.** Der Stand wurde ohne PlatformIO, ohne OpenKNXproducer und ohne DALI-Hardware erstellt. Geprüft wurden nur die reine Logik (Host-Test) sowie die XML-Vorlagen per Skript (wohlgeformt, keine Bit-Überlappungen, Referenzen konsistent). Ob Firmware und ETS-Applikation bauen und ob die Treiber das Weiß wie erwartet ansteuern, zeigt erst der Test auf der Hardware – siehe [Testanleitung](#testanleitung).

Details zu allen Änderungen im Vergleich zum Ausgangscode: [`doc/RGBW-Gegenueberstellung.md`](doc/RGBW-Gegenueberstellung.md).

## Was ist neu?

- Neuer Eintrag **„RGBW“** in der ETS beim Parameter *„Farbe ansteuern per“* (Einzeladressen **und** Gruppenadressen).
- Bei RGBW erscheinen **zwei Kommunikationsobjekte** (die gleichen Objektnummern wie bei den anderen Farbtypen):
  - **Farbe RGBW** (Eingang, DPT 251.600, 6 Byte)
  - **Farbe RGBW Status** (Ausgang, DPT 251.600, 6 Byte)
- Das Gateway wertet die **Gültigkeitsbits** des Telegramms aus: Es werden nur die Kanäle geändert, die als gültig markiert sind (z. B. nur Weiß ändern, ohne RGB anzufassen).
- Ansteuerung des Treibers über DALI DT8: **RGB-Pegel + Weiß-Pegel + `ACTIVATE`** (siehe unten). Die Einstellung *„Farbe übertragen per“* (RGB/xy) spielt bei RGBW keine Rolle.

### Telegramm DPT 251.600

| Byte | Inhalt |
|------|--------|
| 0 | Rot (0–255, in der ETS 0–100 %) |
| 1 | Grün |
| 2 | Blau |
| 3 | Weiß |
| 4 | reserviert (0x00) |
| 5 | Bit 3 = R gültig, Bit 2 = G gültig, Bit 1 = B gültig, Bit 0 = W gültig |

### DALI-Sequenz (je Farbänderung)

1. `DTR0=R, DTR1=G, DTR2=B` → `SET TEMPORARY RGB DIMLEVEL` (DT8, Befehl 235)
2. `DTR0=W, DTR1=255, DTR2=255` → `SET TEMPORARY WAF DIMLEVEL` (DT8, Befehl 236; 255 = MASK = Amber/Freecolour unverändert)
3. `ACTIVATE` (Befehl 226) – beide temporären Werte werden gemeinsam übernommen

Der Wert 255 wird – wie schon bei RGB – auf 254 begrenzt, weil 255 bei DALI „MASK“ (nicht ändern) bedeutet. Die Helligkeit läuft weiterhin über die normalen Dimm-Objekte.

### Einschaltverhalten

Ein Farbtelegramm an einen **ausgeschalteten** RGBW-Kanal schaltet ihn ein (auf den normalen Einschaltwert Tag/Nacht, wie bei *Schalten = Ein*). Ein reines Weiter-Dimmen ändert die Farbe nicht: Die Farbe bleibt im Treiber erhalten.

Dafür wurde zusätzlich `koHandleDimmAbs` angepasst: Der interne Ein/Aus-Merker (`currentState`) folgt jetzt dem Wert von *Dimmen absolut* (0 % = aus). Ohne diese Korrektur blieb der Merker nach „Dimmen auf 0“ auf *Ein* stehen, und ein anschließendes Farbtelegramm wurde ignoriert. (Hardware-Test mit DA4-M am 13.11.2026.)

## Bekannte Einschränkungen

- **Szenen/DCA-Konfiguration:** Die Szenen-Farbe (Konfigurationstransfer) kennt nur RGB. Bei RGBW-Kanälen wird dort das Weiß nicht mitgeschrieben. Szenen per KNX-Telegramm (Szenenobjekt) funktionieren über die im Treiber gespeicherten Szenen.
- **Getrennte Objekte** (Rot/Grün/Blau/Weiß einzeln, 8 Bit) gibt es bei RGBW *nicht* – nur das kombinierte 6-Byte-Objekt (Variante 1 der Diskussion). Das kann später nachgerüstet werden.
- **HCL** (Human Centric Lighting) wirkt bei RGBW wie bisher nur auf die Helligkeit.
- Manche Treiber erwarten für Weiß statt `SET TEMPORARY WAF DIMLEVEL` den Befehl `SET TEMPORARY RGBWAF CONTROL`/`RGBWAF DIMLEVEL` (237) bzw. ein anderes Kanal-Mapping. Das ist ein typischer Punkt, der nur mit echter Hardware klärbar ist.
- **ETS-Applikation:** Der Parameter *colorType* wurde von 2 auf 3 Bit verbreitert und deshalb an eine freie Stelle im Parameterspeicher verschoben (Einzelkanal: Byte 21, Bit 5–7; Gruppe: Byte 17, Bit 2–4). Bestehende ETS-Projekte müssen daher auf die neue Applikationsversion aktualisiert werden (siehe Testanleitung, Schritt 3).

## Testanleitung

### Voraussetzungen

- OpenKNX-Entwicklungsumgebung wie für das Gateway üblich: VS Code + PlatformIO, **OpenKNXproducer ≥ 2.1.10** (unter `~/bin`), ETS.
- DALI-Gateway-Hardware `GW-REG1-Dali` (RP2040) samt DALI-Netzteil und DT8-RGBW-Treiber.

### 1. Firmware-Projekt mit diesem Branch einrichten

```bash
# Firmware-Repo
git clone -b v1dev https://github.com/OpenKNX/GW-REG1-Dali.git
cd GW-REG1-Dali

# Abhängigkeiten holen (legt die Geschwisterordner an und verlinkt sie nach lib/)
#   Windows/PowerShell:  restore/Restore-Dependencies.ps1
#   (Ablauf: siehe dependencies.txt – die Abhängigkeiten liegen als Geschwister neben dem Projekt)

# Dann in den Geschwisterordner OFM-Dali wechseln und auf diesen Branch umstellen:
cd ../OFM-Dali
git remote add rgbw https://github.com/Rangrous86/OFM-Dali.git
git fetch rgbw
git checkout -b feature/rgbw rgbw/feature/rgbw
```

`lib/OFM-Dali` im Firmware-Projekt ist ein Link auf diesen Geschwisterordner – damit nutzen ETS-Generator und Firmware automatisch den RGBW-Stand.

### 2. ETS-Applikation und Header erzeugen

Im Firmware-Projekt (VS-Code-Task **„OpenKNXproducer“** oder manuell):

```bash
~/bin/OpenKNXproducer create --Debug -h include/knxprod.h src/DaliGateway
```

Dabei entsteht `include/knxprod.h` mit `PT_colorType_RGBW` (= 4) und den neu berechneten Parameter-Offsets, außerdem die `.knxprod`-Datei für die ETS (`scripts/_Build-knxprod.ps1`).
**Wichtig:** `include/knxprod.h` nicht von Hand ändern, sie wird immer vom Producer erzeugt.

Wenn der Producer Fehler meldet (z. B. zu `DPST-251-600` oder zu Bit-Offsets), bitte die Meldung notieren – das ist der erste Punkt, an dem dieser Stand scheitern könnte.

### 3. ETS importieren

- In `src/DaliGateway.xml` bei `op:ETS` die `ApplicationVersion` erhöhen (z. B. `0.11.0` → `0.11.1`), damit die ETS die Applikation sauber als neue Version erkennt und beim Aktualisieren die Parameter neu ablegt.
- `.knxprod` in die ETS importieren, Gerät aktualisieren bzw. neu anlegen.

### 4. Firmware flashen

Mit dem gewohnten PlatformIO-Environment für die RP2040-Hardware bauen (VS-Code-Task *„Develop: GW-UP1-DG“*) und flashen. Anschließend per ETS programmieren.

### 5. Testfälle

In der ETS einen Kanal (und danach eine Gruppe) auf *„Farbe ansteuern per: RGBW“* stellen, Gruppenadressen für *Farbe RGBW* (Eingang) und *Farbe RGBW Status* verbinden, Kanal einschalten. Dann Telegramme senden (z. B. über ETS-Gruppenmonitor oder Home Assistant / Visu mit DPT 251.600):

| # | Telegramm (Byte 0–5) | Erwartung |
|---|----------------------|-----------|
| 1 | `FF 00 00 00 00 0F` | rein Rot |
| 2 | `00 FF 00 00 00 0F` | rein Grün |
| 3 | `00 00 FF 00 00 0F` | rein Blau |
| 4 | `00 00 00 FF 00 0F` | rein Weiß (der wichtigste Test!) |
| 5 | `FF FF FF FF 00 0F` | alles an |
| 6 | `00 00 00 80 00 01` | nur Weiß ändern (50 %), RGB bleibt wie zuvor |
| 7 | `FF 00 00 00 00 08` | nur Rot ändern, Weiß/Grün/Blau bleiben |
| 8 | `FF FF FF FF 00 00` | **keine** gültigen Kanäle → Licht darf sich nicht ändern |
| 9 | wie 4, danach Status-Objekt lesen | Status = `00 00 00 FF 00 0F` |
| 10 | RGB/HSV/TW/xyY-Kanäle nachtesten | verhalten sich wie vorher |

Die Debug-Ausgabe (OpenKNX-Logging, Level *Debug*) zeigt pro Telegramm `Got RGBW: …` und `Send RGBW: …`. Falls Weiß nicht leuchtet: Treiber-Datenblatt prüfen (RGBW-Mapping, Anzahl Farbkanäle) und mit dem Log vergleichen, ob die Werte korrekt ankommen.

Optional, ohne Hardware: Host-Test der Logik

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

## Pull Request

Nach erfolgreichem Test kann aus diesem Branch ein Pull Request gegen `OpenKNX/OFM-Dali` (Basis-Branch **`v1dev`**) erstellt werden. Vorschlag für die Beschreibung: Zusammenfassung dieser README + [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md) + Testergebnis (getesteter Treiber, Auffälligkeiten).
