# OFM-Dali – RGBW-Erweiterung (Branch `v1dev` dieses Forks)

Dieser Fork ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse pro Leuchte (RGBW-Modus). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (Commit `6e7aca5`). Das Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY bleibt unverändert.

> **Status: auf Hardware in Test.** Gebaut, geflasht und mit einem echten Treiber (DA4-M, 4-Kanal-Konstantspannungs-Dimmer im Modus RGBW) am `GW-REG1-Dali` erprobt (13./14.11.2026): ETS-Import, Programmierung, DALI-Adressierung, Farbtelegramme, Statusrückmeldung, Ein-/Ausschalten, Dimmen. Der RGBW-Kanal für sich läuft; die Wechselwirkungen mit anderen Objekten und Kanälen (Szenen, Gruppen, Broadcast, relatives Dimmen u. a.) sind noch offen. Siehe [Testfälle](#5-testfälle) und den dortigen Teststand.

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

**Zwei Arten, die Helligkeit zu steuern** (ETS-Parameter *Helligkeit bei RGBW* im Abschnitt *Farbe*, siehe unten): Standard ist *Über Objekt „Dimmen absolut“*, also *Farbe = Mischung, Helligkeit = Dimmwert*. Wie bei DALI-DT8 üblich legt das RGBW-Objekt dann nur das Mischungsverhältnis der Kanäle fest, die Gesamthelligkeit kommt ausschließlich vom DALI-Pegel (*Dimmen absolut*). Ein Farbtelegramm mit kleineren Beträgen (z. B. 50 % statt 100 % Rot) dimmt eine eingeschaltete Leuchte dann nicht (am DA4-M gemessen). Visualisierungen, die RGBW-Werte „helligkeitsskaliert“ senden, brauchen in diesem Modus zusätzlich eine Adresse für die Helligkeit (z. B. `brightness_address` in Home Assistant) auf *Dimmen absolut*. Alternativ gibt es den Modus *Aus dem RGBW-Wert*.

### Helligkeit aus dem RGBW-Wert („Slider-Modus“, v12)

Für Clients mit nur einem Objekt (z. B. ein Home-Assistant-KNX-Licht ohne `brightness_address`, dessen Helligkeitsregler den RGBW-Wert skaliert) gibt es den ETS-Parameter **Helligkeit bei RGBW** (Einzelkanal und Gruppe, nur sichtbar bei *Farbe ansteuern per = RGBW*):

| Einstellung | Verhalten |
|-------------|-----------|
| **Über Objekt „Dimmen absolut“** (Standard) | wie bisher: Farbe = nur Mischung, Helligkeit nur über *Dimmen absolut* / *Schalten* / *Dimmen relativ* |
| **Aus dem RGBW-Wert (größter Kanal = Helligkeit)** | Der größte der vier Kanäle ist die Helligkeit (255 = 100 %), das Verhältnis der Kanäle ist die Farbmischung. |

Im Slider-Modus gilt:

- **Farbtelegramm** `R G B W`: Helligkeit = größter Kanal ÷ 255 (linear in Prozent, dann wie *Dimmen absolut* auf die DALI-Kurve umgerechnet). Der Treiber bekommt die auf 255 normierte Mischung (er wertet die Amplitude ohnehin nicht aus) und die Helligkeit als eigenen Pegelbefehl, in der bekannten Reihenfolge ohne den laufenden Farb-Fade abzubrechen. Beispiel: `80 00 00 80` = Rot-Weiß mit etwa 50 %.
- **Aus → Farbtelegramm:** schaltet mit dem Pegel aus dem RGBW-Wert ein (nicht mit dem Einschaltwert Tag/Nacht).
- **alle Kanäle 0:** aus (wie bisher).
- **Status Farbe (RGBW)** zeigt Mischung × aktueller Pegel. Ändert sich der Pegel über *Dimmen absolut*, *Schalten* oder *Dimmen relativ*, folgt der Farbstatus (ein Client, der die Helligkeit aus dem Farbwert liest, sieht die richtige Helligkeit). Ein eben empfangener Farbwert wird unverändert zurückgemeldet.
- *Dimmen absolut* / *Schalten* / *Dimmen relativ* funktionieren weiter und stellen den Pegel direkt. Beide Wege gleichzeitig zu benutzen ist möglich, aber nicht nötig: ein Client mit Helligkeitsadresse sendet seine Farbe mit 100 % und regelt die Helligkeit separat, der Modus *Über Objekt* ist dafür der passende.
- Die Szenen im Treiber, HCL und die anderen Farbtypen sind nicht betroffen.

**Hardware-Befund, der dazu geführt hat (A1, v11):** Telegramm `80 00 00 80 00 0F` an eine brennende Lampe: Lampe unverändert (40 %), Status 20/0/3 meldet `80 00 00 80`, Home Assistant zeigt 50 %. Der DA4-M normiert die Farbe, die Amplitude geht verloren.

### Gruppen (v12)

- **Farbe beim Start:** Wie ein Einzelkanal liest auch ein RGBW-Gruppenkanal beim Start die Farbe aus dem Treiber (QUERY_COLOUR_VALUE, Selektoren 233–236), aber nur von **einem** EVG: dem Dimm-Referenz-EVG der Gruppe (dasselbe wie für Pegel und Fade-Zeit). Alle Mitglieder abzufragen würde 4 Abfragen je EVG kosten, und bei abweichenden Mitgliedern gäbe es keinen eindeutigen Wert. Ohne Referenz-EVG bleibt die Gruppenfarbe bis zum ersten Farbtelegramm unbekannt. Weicht ein Mitglied ab (z. B. einzeln verstellt), zeigt der Gruppenstatus die Farbe des Referenz-EVGs.
- **Immer die komplette Farbe:** Ein EVG kann in mehreren Gruppen liegen oder einzeln verstellt werden, die in der Gruppe gespeicherte Farbe ist deshalb nicht verlässlich. Ein Farbtelegramm an eine Gruppe wird darum immer als vollständige Farbe gesendet: Kanäle, die im Telegramm nicht als gültig markiert sind, gelten als **0** (beim Einzelkanal behalten sie dagegen ihren Wert). Beispiel: Rot an, dann Telegramm „nur Blau gültig“ → Blau, Rot wird 0. Ein Telegramm, dessen gültige Kanäle alle 0 sind, schaltet die Gruppe aus. Der Treiber bekommt weiterhin immer alle vier Kanäle (R, G, B, W) in einer Sequenz.
- Der Pegel ist unkritisch: ein Ziel-Pegel (z. B. 50 %) ist für jedes Mitglied gleich, egal von wo aus es startet, nur die Fade-Dauer unterscheidet sich.
- **Nicht abgedeckt (bekannte Lücke, nicht auf Hardware getestet):** Die Gruppe weiß nichts vom laufenden Farb-Fade ihrer Mitglieder, wenn dasselbe EVG über eine andere Gruppe oder einzeln angesteuert wird.

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
- **ETS-Applikation:** (v12: neuer Parameter *Helligkeit bei RGBW*, Einzelkanal Byte 22 Bit 0, Gruppe Byte 17 Bit 5; der Parameterblock des Einzelkanals wird dadurch um 1 Byte größer, die ETS-Applikation muss neu importiert und das Gerät neu programmiert werden.) Der Parameter *colorType* wurde von 2 auf 3 Bit verbreitert und deshalb an eine freie Stelle im Parameterspeicher verschoben (Einzelkanal: Byte 21, Bit 5–7; Gruppe: Byte 17, Bit 2–4). Die Applikation ist dadurch **nicht** in-place zu Geräten mit der alten Version updatebar (`ReplacesVersions` enthält die neue Version nicht): Gerät neu anlegen bzw. neu parametrieren.

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

#### Teststand (14./15.11.2026, Firmware-Tag `v6` bis `v11`, DA4-M im Modus RGBW)

Der **RGBW-Kanal für sich** ist ausgiebig getestet und läuft. Die Wechselwirkungen mit den übrigen Objekten und Kanälen sind noch offen und werden in der nächsten Sitzung abgehakt (gegebenenfalls mit Anpassungen).

**Auf Hardware bestätigt (v6):**

- Einschalten aus Aus mit Einzelfarben Rot/Grün/Blau/Weiß und halben Werten (Test 2–5, 11): sofort die neue Farbe, keine alte Farbe, kein Weiß-Blitz, weiches Hochfaden bis 100 %; Mischfarben (Lila = R + B) ebenso.
- Ausschalten (Test 9): sauber, kein Blitz; *Schalten Status* Aus, *Dimmen Status* 0 %.
- *Dimmen absolut* aus brennender Lampe (Test 13): 50 % und 10 % wirken, der DALI-Pegel stimmt (`getLvl`), Status-Objekte folgen.
- Status-Rückmeldung *Farbe RGBW Status*, *Schalten Status*, *Dimmen Status*.
- **Festgelegt:** Farbe = Mischung, Helligkeit = *Dimmen absolut*. Kleinere Beträge im RGBW-Telegramm dimmen eine brennende Lampe nicht (am DA4-M gemessen).
- Gültigkeitsbits (Test 6–8, mit v6): nur W (30 %) und nur R (100 %) ändern bei brennender Lampe nur diesen Kanal, die Helligkeit bleibt; nur W auf 0 % lässt Blau stehen und schaltet nicht aus; Telegramm ohne gültigen Kanal wird ignoriert (`RGBW: no valid channel in telegram, ignored`), auch bei Nullwerten.
- Test 9/10: alle vier Kanäle 0 % schaltet sauber aus; *Schalten* Ein bringt die zuletzt gesetzte Farbe mit dem letzten Dimmwert zurück (z. B. Magenta, 50 %).
- Test 11: Farbtelegramm auf eine ausgeschaltete Lampe schaltet mit der **neuen** Farbe ein (Rot statt gespeichertem Magenta), ohne zweites Senden; ebenso nach alle-0 mit Grün (Test 12b).
- Test 12: nach *Dimmen absolut* 0 % (Lampe aus, `RGBW: off`) schaltet ein Farbtelegramm (alle vier Häkchen, B 100 %) die Lampe wieder ein, in der neuen Farbe mit dem letzten Dimmwert; der Merker bleibt nicht auf *Ein* hängen.
- Test 1: *Schalten* Ein/Aus und *Dimmen absolut* 50 %/100 % (ETS und Home Assistant).

**Bekanntes Verhalten (DA4-M):** Ändert sich die Farbe bei **ausgeschalteter** Lampe (Einschalten mit neuer Farbe), braucht der Treiber für den Farbwechsel ≈ 1,4 s (gemessen 1352/1394 ms, Lampe bleibt dabei dunkel); die Lampe erscheint dann direkt in der neuen Farbe. Bleibt die Farbe gleich (Einschalten mit gespeicherter Farbe), ist die Verzögerung nur ≈ 0,3 s. Im Test akzeptabel, vom Treiber vorgegeben.

**Kosmetik:** Bei jedem *Dimmen absolut* auf eine brennende Lampe sendet das Gateway zusätzlich *Schalten Status* Ein, obwohl die Lampe schon an war. *(v8: bei RGBW-Kanälen wird der Schaltstatus nur noch bei Änderung gesendet, auf Hardware bestätigt, siehe „v8 auf Hardware“.)*

**Damit sind die Tests 1–12 mit v6 auf Hardware durch.** Test 13 (Rot, dann *Dimmen absolut* 25 %) ist durch die Dimm-Nachtests abgedeckt (getestet mit 50 %, 100 % und 10 %).

**Farbe und Helligkeit zusammen auf einer brennenden Lampe (Home Assistant), Fund und Korrektur (v6 → v7):**

- *Fund (v6):* Home Assistant sendet bei einer Leuchte mit Helligkeitsadresse zuerst die Farbe (*Farbe RGBW*) und ≈ 20 ms später die Helligkeit (*Dimmen absolut*). Auf einer **brennenden** Lampe ging der Dimmbefehl sofort auf den Bus und hat den noch laufenden Farbwechsel des DA4-M abgebrochen (eine Fade-Engine im Treiber, ein Pegelbefehl bricht den Farbfade ab). Ergebnis: falsche Farbe (Blau + Weiß statt Rot + Weiß; reines Weiß statt Blau), zweimal von zwei Versuchen. Ein einzelnes Farbtelegramm (nur ETS/knx.send ohne Helligkeit) war dagegen in Ordnung, ebenso das Paar beim Einschalten aus Aus.
- *Korrektur (v7):* `koHandleDimmAbs` wartet bei einer brennenden RGBW-Lampe, deren Farbsequenz noch läuft (`colourFadeMayRun()`, seit dem letzten `sendColorAsRGBW` weniger als `rgbwOffWaitMs()` + 300 ms), mit dem Pegelbefehl bis `QUERY_STATUS` das Ende des Farbfades meldet (derselbe Mechanismus wie beim Einschalten mit Farbe). Log: `RGBW: level … after the colour fade`.
- *Bestätigt (v7):* Rot-Weiß 20 % → Blau 50 % (HA: `rgbw_color [0,0,255,0]`, `brightness 128`): Weiß dimmt weg, geht über kurz Lila nach reinem Blau, dann 50 %; Blau 50 % → Rot-Weiß 50 % (`[255,0,0,255]`, `brightness 128`): Rot-Weiß ohne Blau-Rest, Pegel bleibt. Die Farbsequenz braucht ≈ 1,57 s, der Pegel folgt danach (Log: `colour fade over … 1569–1571 ms`, `final on value 229`).
- *Gegenrichtung (erst Helligkeit, dann Farbe):* gemessen mit v8, behoben in v9, siehe „Reihenfolge von Helligkeit und Farbe“ unten.
- *Relatives Dimmen unmittelbar nach einem Farbtelegramm:* mit v10 gemessen, Fehler, behoben in v11, siehe „Relatives Dimmen nach einem Farbtelegramm (v11)“ unten.

**Dimmen relativ (heller/dunkler):**

- Auf brennender Lampe sauber, die Farbe bleibt unverändert, es wird keine Farbe neu gesendet (R1a dunkler mit v6, R1b heller mit v7). *Heller* hält am Anschlag selbst an (`Dimm Stop at max 254`), das spätere Stopp-Telegramm ändert dann nichts mehr.
- *Heller* aus Aus (R2b, v7): das Gateway holt die Lampe mit `RECALL_MIN` auf den Minimalpegel (`starting with min 1`), die Lampe geht in der **gespeicherten Farbe** (Rot-Weiß) an und wird heller; das Stopp-Telegramm hält sie bei Pegel 72 an (0,7 %), der Pegel wandert danach nicht weiter.
- Beobachtung: Die Statusobjekte (*Dimmen Status*, *Schalten Status*) werden bei **jedem** relativen Schritt gesendet, obwohl „Dimmstatus Interval bei relativ“ auf *Deaktiviert (nur am Ende)* steht. Vermutlich kommt das von der Pegelabfrage `Got new actual level` während des Dimmens (Upstream, Mai 2026) und nicht vom Parameter; nicht geprüft. Die Schrittweite beträgt ≈ 9 Pegelstufen alle 200 ms (45 Stufen/s, DALI-Standard-Fade-Rate); in Prozent wirkt das im unteren Bereich der logarithmischen DALI-Kurve sehr langsam (von Pegel 1 bis 72 in ≈ 2 s, entspricht 0,7 %). Nicht verändert.

- **Fund: *Dunkler* bis zum Minimum (14.11.2026, v7):** Die Lampe wurde sauber dunkler, ging am Minimum aber **nicht aus** und das Gateway sendete etwa fünfmal pro Sekunde *Dunkler* + *Aus* (Log: `Dimm Stop at: 0`, `Turn off device`, `RGBW: off (plain level off)`, `Got new actual level 0%-1`; auf dem Bus *Schalten Status* Aus im 200-ms-Takt, Telegrammflut), bis das Stopp-Telegramm kam. Erst dann ging die Lampe aus; danach meldete das Gateway fälschlich *Ein* (0,4 %), weil die Pegelabfrage den Dimmwert auf 1 zurückgeschrieben hatte. Ursache: die Dimmschleife (`loopDimming`) beendet das Dimmen am Minimum nicht, sondern wiederholt *Dunkler* und *Aus* bis zum Stopp (Struktur aus dem Upstream-Code, `git blame`; im Normalbetrieb endet sie mit dem Loslassen der Taste, bei anderen Farbtypen ist dasselbe Verhalten zu erwarten, nicht geprüft). Dass das *Dunkler* nach dem *Aus* den DA4-M anhält, ist eine Vermutung.
- **v8 (Fix, auf Hardware bestätigt, siehe unten):** Der Schritt, der das Minimum erreicht, sendet kein weiteres *Dunkler* mehr, schaltet aus, beendet das Dimmen, verwirft die noch laufende Pegelabfrage und setzt die Status auf *Aus*/0 %. *Dunkler* auf einer ausgeschalteten Lampe wird ignoriert. Nur RGBW-Kanäle.

**v8 auf Hardware (14.11.2026, Gateway-Tag `v8`, Home Assistant `knx.send`/`light.turn_on`, ETS-Gruppenmonitor und serielles Log):**

- *Test 1, Einschalten mit neuem Standardwert:* Rot-Weiß 20 % aus Aus: Pegel 254 als Standard „letzter Wert" nach dem Neustart, Lampe geht in der neuen Farbe an (Weiß wirkt bei 20 % sehr hell).
- *Test 2, Schaltstatus nur bei Änderung:* *Dimmen absolut* auf eine brennende Lampe sendet kein zusätzliches *Schalten Status* Ein mehr (Lampe wird heller, nur *Dimmen Status*).
- *Test 3, Dunkler bis zum Minimum:* sauber bis aus, keine Sprünge, keine Wiederholung von *Dunkler*/*Aus*, kein Busflug; Status *Aus*/0 %, das spätere Stopp-Telegramm ändert nichts. Damit ist der v7-Fund behoben.
- *Test 4, Heller aus Aus:* Lampe geht mit `RECALL_MIN` in der gespeicherten Farbe (Rot-Weiß) an, wird gleichmäßig heller (Log `starting with min 1`, `Dimm Up Start 1/1`), kein `Send RGBW`; genau ein *Schalten Status* Ein. Das Stopp-Telegramm kam hier nach ≈ 5 s (Latenz des Testwerkzeugs) und nicht nach 2 s, deshalb stand die Lampe bei 21,6 % statt bei 0,7 %. Beobachtung: Das *Schalten Status* Ein kommt erst nach dem Stopp, nicht schon beim ersten Schritt.
- *Test 5, Neustart bei brennender Lampe* (ausgelöst durch einen Teil-Download der Applikation; ein Neustart über ETS-Geräteinfo war bei diesem Gerät nicht möglich): Die Lampe blieb unverändert (kein Aufblitzen, kein Ausgehen). Das Gateway las den Pegel beim Start (`SetDimmState 28.5/208`) und sendete *Schalten Status* Ein und *Dimmen Status* 28,6 %; kein Farbtelegramm, kein Aus. `currentState` kommt damit aus dem Treiberlevel (der Blitz nach dem v7-Flash ist weg). Es wird nach dem Start keine *Farbe RGBW Status* gesendet, weil die Farbe unbekannt ist.
- *Fund (Upstream-Pfad, nicht RGBW-spezifisch):* Nach dem Stopp eines relativen Dimmens meldet `updateCurrentDimmValue()` nur den **zuletzt abgefragten** Pegel. Test 4 endete mit Status 21,6 % (Pegel 198), der Treiber stand bei 28,6 % (Pegel 208, der Neustart hat es gezeigt), weil der letzte UP-Befehl nach der letzten Abfrage noch lief (≈ 9 Pegelstufen pro 200 ms). Vorschlag für die nächste Version: beim Stopp einmal `QUERY_ACTUAL_LEVEL` senden und den Status mit der Antwort aktualisieren.

#### v10 auf Hardware (15.11.2026): Farbe beim Start vom Treiber übernommen

Alle drei Tests bestanden, DA4-M im Modus RGBW, Kanal A0:

| Test | Aktion | Ergebnis |
|---|---|---|
| 1 | Neustart (Teil-Download) bei **ausgeschalteter** Lampe, zuletzt Blau gesendet | Log `RGBW: colour of the driver 0 0 255 0`; auf *Farbe RGBW Status* (20/0/3) genau ein Telegramm `00 00 FF 00 00 0F` (`#0000FF`), Schalten Status Aus, Dimmen Status 0 % |
| 2 | Nur Rot senden (`FF 00 00 00 00 08`, nur R gültig) auf die dunkle Lampe | Log `Got RGBW: 255 0 255 0 (valid 8)`: Rot aus dem Telegramm, G/B/W aus der vom Treiber gelesenen Farbe. Lampe schaltet in Violett (R + B) ein, 20/0/3 meldet `FF 00 FF 00 00 0F`. Ohne v10 wäre die Lampe rot geworden (fehlende Kanäle als 0) |
| 3 | Lampe rot-weiß 20 % (`FF 00 00 FF 00 0F` plus 20 %), danach Neustart (Teil-Download) bei brennender Lampe | Lampe bleibt über den ganzen Start unverändert (kein Flackern, kein Farbwechsel). Log `SetDimmState 20.0/195` und `RGBW: colour of the driver 255 0 0 255`. Auf dem Bus: Schalten Status Ein, Dimmen Status 20 %, 20/0/3 `FF 00 00 FF 00 0F` (`#FF0000 W:100%`), alles etwa 0,6 s nach dem Neustart |

Zum Einschalten in Test 2: Die Sequenz war die bekannte, Farbe zuerst, Pegel nach dem Ende des Farb-Fades (`colour fade over (status 0x00), 1350 ms after the colour telegram`, `final on value 254`).

Zu Test 3 (Einrichten der Ausgangslage über Home Assistant): `Got RGBW: 255 0 0 255 (valid F)`, `Dimming absolute to 20%`, `RGBW: level 195 after the colour fade`, `colour fade over (status 0x06), 1476 ms after the colour telegram`, `final on value 195`.

Damit ist der Start eines RGBW-Kanals vollständig: Ein/Aus-Zustand und Helligkeit (v8) und jetzt auch die Farbe (v10) kommen vom Treiber. Offen bleiben das Verhalten der Report-Werte **während** eines laufenden Fades und ein Treiber, dem noch nie eine Farbe gesendet wurde.

#### Relatives Dimmen nach einem Farbtelegramm (v11)

- **Fund (v10, Hardware, 15.11.2026):** Lampe rot-weiß 20 %. Home-Assistant-Script: Farbe Blau (`00 00 FF 00 00 0F`), 100 ms später *Dimmen relativ* heller (`09`), nach 1 s Stopp (`00`). Die Lampe blieb auf einer Mischfarbe (helles Weiß) hängen, obwohl *Farbe RGBW Status* Blau meldete. Log: `Send RGBW: 0 0 254 0`, `Dimm Up Start 195/195`, Pegel 197, 202, 212, 221, nach dem Stopp 231 (53,3 %, Dimmstatus stimmt).
- **Ursache:** Der Treiber hat eine Fade-Engine. Der erste UP direkt hinter dem ACTIVATE der Farbsequenz bricht den Farb-Fade nach wenigen Prozent ab; jeder weitere UP im 200-ms-Takt würde es wiederholen. Dasselbe passiert in der Gegenrichtung, wenn ein Farbtelegramm **während** eines laufenden Dimmens kommt.
- **Korrektur (v11):** `loopDimming` sendet bei RGBW (nicht Treppenhaus, nur Helligkeit) keinen UP/DOWN, solange der Farb-Fade laufen kann (`colourFadeMayRun()`) oder ein Einschalten noch nicht abgeschlossen ist (`_onArcPending`, `_colorPending`). Das Dimmen beginnt bzw. geht danach weiter (Log `RGBW: dimming waits for the end of the colour fade`). Kommt der Stopp während der Pause, wird nichts gesendet; die Stopp-Abfrage liefert trotzdem den echten Pegel. Die Zeit bis zum Dimmstart beträgt etwa Fade-Zeit + 400 ms (ca. 1,7 s bei 1 s Fade).
- **Hinweis für Anwender:** Es gibt kein KNX-Objekt, das Farbe und Helligkeit eines RGBW-Kanals in einem Telegramm überträgt (DPT 251.600 enthält keine Helligkeit). Wer Farbe und Dimmen unmittelbar nacheinander auslöst, sollte mit einer Anlaufzeit des Dimmens von rund 1,5 s rechnen.
- **Auf Hardware bestätigt (v11, 15.11.2026, Home-Assistant-Scripts mit `knx.send`, Start jeweils rot-weiß 20 %):**

| Test | Ablauf | Ergebnis |
|---|---|---|
| 1 | Farbe Blau, +100 ms Dimmen heller, +1,1 s Stopp | Lampe reines Blau, 20 % unverändert. Log: `RGBW: dimming waits for the end of the colour fade`, kein UP auf dem Bus, nach dem Stopp `20%-195`. Mit v10 dasselbe Script: Mischfarbe (helles Weiß), 53 % |
| 2 | Farbe Rot-Weiß, +100 ms Dimmen heller, +3,1 s Stopp | Farbe sauber, das Dimmen beginnt nach rund 1,8 s, sieben UPs à rund 9 Stufen (Status 24,7 → 31,8 → 40,8 → 51,8 → 66,3 → 84,7 %, 202 ms Abstand), Endwert 100 % (Maximum 254) |
| 3 | Farbe Blau, +100 ms Dimmen heller, +2,5 s Stopp | Lampe sauber Blau, vier UPs, Stopp-Abfrage 231 = 195 + 4 × 9 (53,4 %) |
| 4 | Gegenrichtung, Start Blau 53 % (Pegel 231): *Dimmen dunkler*, +500 ms Farbe Rot-Weiß, +2,6 s Stopp | Drei DOWNs vor dem Farbtelegramm (231 → 223 → 214 → 204), dann `RGBW: dimming waits for the end of the colour fade`, nach rund 1,8 s noch zwei DOWNs (196, 186), Stopp-Abfrage 186 = 231 − 5 × 9 (15,6 %). Lampe sauber Rot-Weiß ohne Mischfarbe, kein UP/DOWN während des Farb-Fades. Der beim Farbtelegramm gerade laufende DOWN wird vollständig ausgeführt (die Farbsequenz braucht ≈ 300 ms Bus-Zeit bis zum ACTIVATE) |

Nicht abgedeckt: Gruppen (ein Gruppenkanal kennt den Farb-Fade eines einzelnen Mitglieds nicht, ein UP/DOWN auf die Gruppenadresse kann ihn abbrechen) und Broadcast, siehe „Noch offen“.

Rechenregel für die Auswertung: ein UP (alle 200 ms) hebt den Pegel um rund 9 Stufen; die Abfrage 300 ms nach dem Stopp liefert den Wert inklusive des noch laufenden letzten UP. Der Dimmstart liegt rund 1,8 s nach dem Farbtelegramm (Fade-Zeit + Reserve + 300 ms).

#### Farbverlauf Rot → Grün → Blau → Rot (v11, 15.11.2026)

Home-Assistant-Script, 25 Farbtelegramme im Abstand von 0,5 s (je 4 s und 8 Schritte pro Übergang, lineare Mischung, Weiß 0, Pegel unverändert 40 %). Gruppenmonitor: je Telegramm auf 20/0/2 antwortet 20/0/3 mit demselben Wert nach etwa 100 ms. Seriallog: pro Telegramm `Got RGBW` / `Send RGBW`, keine Level-Befehle, keine Warnungen. Lampe: deutlich mehr als die sechs Eck- und Mischfarben, der Verlauf ist an den Stufen erkennbar, aber weich genug (der Treiber fadet jeden Schritt nach), Endzustand Rot bei 40 %. **Bestanden.**

#### Reihenfolge von Helligkeit und Farbe auf einer brennenden Lampe (v9)

**Ursache (Eigenschaft des Treibers, nicht des Gateways):** Das DA4-M hat nur **eine Fade-Engine**. Ein Pegelbefehl (*Dimmen absolut*) bricht einen laufenden Farbfade ab, und das `ACTIVATE` einer Farbsequenz bricht einen laufenden Pegelfade ab. Die Farbsequenz braucht rund 310 ms Buszeit, der Farbfade danach noch etwa 1,1 bis 1,6 s, der Pegelfade etwa 1 s.

| Reihenfolge der KNX-Telegramme | Verhalten | Sichtbar an der Lampe |
|---|---|---|
| **Erst Farbe, dann Helligkeit** (Home Assistant: `rgbw_color` mit `brightness_address`, Abstand ≈ 20 ms) | Das Gateway wartet mit dem Pegelbefehl, bis der Farbfade zu Ende ist (`QUERY_STATUS`, v7). | Erst der Farbwechsel, danach das Heller/Dunkler. Das Ergebnis stimmt (v7 auf Hardware bestätigt). |
| **Erst Helligkeit, dann Farbe** (andere Visualisierungen, Logik, Szenen-Bausteine) | Der Pegelbefehl geht sofort an den Treiber. Die Farbsequenz bricht den Pegelfade ab. Das Gateway sendet den Zielpegel deshalb nach dem Ende des Farbfades noch einmal (v9, `RGBW: level … again after the colour fade`). | Die Lampe wird zuerst ein Stück heller oder dunkler (nur etwa 0,3 s lang, bis die Farbsequenz kommt), wechselt dann die Farbe und erreicht zum Schluss den Zielpegel. |

*Gemessen mit v8 (ohne den Fix):* Rot-Weiß 20 % (Pegel 195), dann 60 % (Pegel 235) und 20 ms später Blau: Der Farbwechsel kam, der Pegel blieb bei 208 (28,5 %) stehen, *Dimmen Status* meldete aber 59,6 %. Der Pegel von 195 auf 208 passt zu etwa 0,3 s Fade (≈ 40 Stufen/s) bis zum `ACTIVATE`. In v9 wird der Zielpegel 235 nach dem Farbfade nachgesendet. **Auf Hardware bestätigt (15.11.2026, v9):** Rot-Weiß 20 %, dann 60 % und 20 ms später Blau: Log `level 235 again after the colour fade`, `colour fade over … 1572 ms`, `final on value 235`; `getLvl 00` meldet 235 = 59,53 %, *Dimmen Status* 59,6 %. Die Lampe wird zuerst kurz etwas heller, wechselt dann nach Blau und erreicht 60 %.

**Wenn die kleine Stufe am Anfang stört (Hinweis für Home Assistant und andere Systeme):**

- Die Farbe **vor** der Helligkeit senden. Home Assistant macht das bei einer Leuchte mit Helligkeitsadresse von selbst. Dann gibt es diese Stufe nicht.
- Bei anderen Systemen (Logikmodul, Szene, Visualisierung): in der Reihenfolge *Farbe*, dann *Helligkeit* senden, oder die Helligkeit etwa 2 s nach der Farbe. Nach etwa 2 s ist der Farbfade sicher zu Ende.
- Ändert sich nur eines von beiden, genügt das eine Telegramm. *Dimmen absolut* allein oder ein RGBW-Telegramm allein machen keine Probleme.
- Mehrere Telegramme für denselben Kanal möglichst nicht dichter als etwa 2 s hintereinander senden, wenn jedes davon einen Fade auslöst.

**Weitere Änderungen in v9:**

- **Status nach dem Stopp eines relativen Dimmens:** Rund 300 ms nach *Stopp* wird der Pegel einmal abgefragt, damit *Dimmen Status* dem Treiber entspricht (bisher fehlte der letzte UP-Schritt, gemessen 21,6 % statt 28,6 %). Nicht bei einem Kanal, der für das Gateway aus ist. **Auf Hardware bestätigt (v9):** *Dunkler* aus 60 % mit *Stopp* nach 2 s: letzter Schritt während des Dimmens 6,7 % (Pegel 155), ≈ 0,4 s nach dem Stopp kommt ein weiteres *Dimmen Status* mit 5,1 % (Pegel 145), identisch mit `getLvl 00` (145 = 5,10 %). Log: `Query actual level after the dimming stop`.
- **Diagnosebefehl `getCol yy [sel]`** (Konsole): fragt Farbtyp-Merkmale, Farbstatus und `QUERY_COLOUR_VALUE` des Treibers ab und gibt die rohen Antworten aus. Er ändert nichts. Die Standard-Selektoren in v9 (64–67 und 0, 1, 2) waren falsch gewählt (64–67 sind Koordinaten der Primärfarben, 0–2 X/Y/Tc, beides hat ein RGBW-Treiber nicht) und lieferten nur `0xFF` oder keine Antwort. **Ab v10** fragt `getCol yy` ohne Selektor die Report-Werte (233–236), die aktuellen Werte (9–12) und die temporären Werte (201–204) jeweils für R, G, B, W ab; mit `getCol yy sel` geht jeder einzelne Selektor.
- **Ergebnis der Farbabfrage am DA4-M (15.11.2026, Lampe Blau, gesendet `0 0 254 0`):** Farbtyp-Merkmale `0x80` (nach meiner Lesart vier Farbkanäle, nicht gegen die Norm geprüft), Farbstatus `0x80`. `getCol 00 233/234/235/236` (Report Rot/Grün/Blau/Weiß, Selektoren nach der [Beckhoff-Aufstellung](https://infosys.beckhoff.com/content/1033/tcplclib_tc3_dali/8414324235.html)) antwortet 0 / 0 / **254** / 0, also die zuletzt gesendete Farbe. Das DA4-M meldet die Farbe somit über die Report-Selektoren 233–236. **Auch bei ausgeschalteter Lampe** (Pegel 0, `getLvl 00` = 0 %, `RGBW: off (plain level off)`) meldet der Treiber dieselbe Farbe 0 / 0 / 254 / 0, er hält die Farbe also gespeichert. Offen: Verhalten während eines Fades und bei einem Treiber, dem noch nie eine Farbe gesendet wurde (Antwort 255 oder 0 ist nicht geprüft).
- **Farbe beim Start übernehmen (v10, auf Hardware bestätigt, siehe „v10 auf Hardware“):** Nach der Fade-Zeit-Abfrage liest das Gateway für jeden RGBW-Kanal (nicht für Gruppen) die vier Report-Werte (233–236) und rechnet 254 auf 255 zurück. Die Farbe wird nur übernommen, wenn kein Wert 255 (MASK) ist, nicht alle vier 0 sind (dann gilt sie als unbekannt) und in der Zwischenzeit kein Farbtelegramm kam. Dann stehen `currentColor` und *Farbe RGBW Status* wieder richtig (Log `RGBW: colour of the driver r g b w`). Teilweise gültige Telegramme (nur ein Kanal) verrechnen sich so mit der echten Farbe statt mit Nullen, und *Schalten Ein* nach einem Neustart bringt die Farbe des Treibers.

**Neustart-Hinweis:** Bis v7 kannte das Gateway nach einem Neustart weder die Farbe noch den Ein/Aus-Zustand der Lampe, während der Treiber weiterleuchtet. Ein Farbtelegramm lief dann über den Einschaltweg (beobachtet am 14.11.2026 nach dem Flashen mit noch brennender blauer Lampe: kurzes Aufblitzen, Farbwechsel noch beim alten Pegel, danach der neue Pegel). Seit v8 wird der Ein/Aus-Zustand aus dem gelesenen Pegel übernommen (Test 5); die **Farbe** liest das Gateway seit v10 ebenfalls vom Treiber (siehe „Farbe beim Start übernehmen“, auf Hardware bestätigt); bis dahin (v9 und älter) blieb sie unbekannt, und ein erstes *Dimmen absolut*/Farbtelegramm synchronisiert das Gateway wieder.

**Noch offen – Wechselwirkung mit anderen Objekten/Kanälen:**

| Bereich | Zu prüfen |
|---------|-----------|
| *Dimmen absolut* aus Aus | schaltet mit der gespeicherten Farbe ein (nicht Schwarz/Weiß) |
| *Dimmen relativ* (heller/dunkler) | bestätigt (siehe oben, auch *Dunkler* bis Minimum und *Heller* aus Aus mit v8); direkt nach einem Farbtelegramm: mit v10 Fehler, in v11 behoben und auf Hardware bestätigt (auch die Gegenrichtung: Farbe während des Dimmens) |
| Weißkanal in Mischungen | bestätigt: Rot + Weiß (v7), nur W (v6); offen: weitere Mischungen mit W bei brennender Lampe |
| Sperre, Tag/Nacht-Einschaltwerte | Verhalten bei gesperrtem Kanal; Einschaltwert statt 254 |
| HCL | wirkt bei RGBW nur auf die Helligkeit; Zusammenspiel mit Farbe prüfen |
| Szenen | KNX-Szene mit Farbe; im Treiber gespeicherte Szenen (Farbe gilt danach als unbekannt) |
| Gruppen und Broadcast | Gruppenadresse mit RGBW-Kanälen, Broadcast Ein/Aus, verzögertes Broadcast-Aus |
| Mehrere RGBW-Kanäle gleichzeitig | Auslastung der DALI-Warteschlange (Farbsequenz ≈ 300 ms pro Kanal), Reihenfolge |
| Treppenhauslicht, Funktionstasten, Identify, ETS-Funktionen | bewusst nicht angefasst, Verhalten dokumentieren |
| Regression | Test 14: RGB, HSV, TW, xyY wie vorher |
| Home Assistant | `brightness_address` auf *Dimmen absolut*, reiner RGBW-Slider ohne Helligkeitsadresse (siehe README „Farbe = Mischung“) |

**To-do / Wunschliste (nach den Tests, spätestens für die Endversion):**

- **Treiberzustand beim Start vollständig übernehmen.** *(v8: der Ein/Aus-Merker `currentState` wird aus dem gelesenen Pegel gesetzt, bei Start, Gruppen- und Broadcast-Änderungen, nur RGBW. Der Start ist auf Hardware bestätigt (Test 5), Gruppen und Broadcast sind noch ungetestet. Die Farbe wird seit v10 beim Start vom Treiber gelesen, auf Hardware bestätigt.)* Beim Init liest das Gateway den DALI-Pegel (`QUERY_ACTUAL_LEVEL`, `DaliModule::loopInitData` → `setGroupState` → `setDimmState`): Status-Objekte und „letzter Wert" folgen dem Treiber (14.11.2026 beobachtet: nach Neustart mit Leuchte Grün 100 % lautet der Einschaltwert 254). Seit v10 wird auch die **Farbe** (`currentColor`) beim Start über die DT8-Report-Werte 233–236 gelesen (auf Hardware bestätigt, siehe „v10 auf Hardware“). Das hilft auch nach einem Stromausfall des Gateways. Offen: Verhalten bei sehr vielen RGBW-Kanälen (die Abfrage stellt vier DALI-Abfragen pro Kanal in die Warteschlange) und bei einem Treiber ohne gesetzte Farbe.

- **Kosmetisch: Standardwert „letzter Wert" nach Neustart.** *(v8: Startwerte jetzt 100 % und 10 % als Pegel umgerechnet; der Standardwert 254 ist bestätigt (Test 1).)* `_lastDayValue = 100` / `_lastNightValue = 10` (schon im Upstream) werden als DALI-Pegel gelesen und entsprechen nur ≈ 1,5 % bzw. ≈ 0,1 %. Betrifft das erste Einschalten nach einem Neustart mit *Einschaltwert = 0*, wenn der Treiber beim Start aus war und vorher nicht gedimmt wurde (ist der Treiber beim Start an, übernimmt das Gateway dessen Pegel). Die Startwerte sollten Prozentwerte sein (100 % = Pegel 254, 10 % = Pegel 170).

- **Status nach dem Stopp eines relativen Dimmens.** *(v9, ungetestet: einmal `QUERY_ACTUAL_LEVEL` etwa 300 ms nach dem Stopp, gilt für alle Kanaltypen.)* Fund in „v8 auf Hardware“; Upstream-Pfad.

**Vor einem Pull Request:** `RGBW_DIAG`-Diagnosecode entfernen oder deaktiviert lassen, offene Fragen an den Maintainer (Abschnitt 6 der [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md)) klären; PR erst nach ausdrücklichem Go.

Rohdaten-Beispiele (Byte 0–5, z. B. für Home Assistant `knx.send` mit `payload: [0, 0, 0, 255, 0, 15]`): rein Weiß `00 00 00 FF 00 0F`, nur Weiß ändern (50 %) `00 00 00 80 00 01`, nur Rot ändern `FF 00 00 00 00 08`, alles aus `00 00 00 00 00 0F`.

Die Debug-Ausgabe (OpenKNX-Logging, Level *Debug*, serielle Konsole 115200 Baud) zeigt pro Telegramm `Got RGBW: …`, `Send RGBW: …`, `RGBW: switching on with …` bzw. `RGBW: all channels 0 -> switching off`. Falls Weiß nicht leuchtet: Treiber-Datenblatt prüfen (RGBW-Mapping, Anzahl Farbkanäle) und mit dem Log vergleichen.

Optional, ohne Hardware: Host-Test der Logik

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

## Pull Request

Nach erfolgreichem Test kann aus `v1dev` dieses Forks ein Pull Request gegen `OpenKNX/OFM-Dali` (Basis-Branch **`v1dev`**) erstellt werden. Vorschlag für die Beschreibung: Zusammenfassung dieser README + [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md) + Testergebnis (getesteter Treiber, Auffälligkeiten).
