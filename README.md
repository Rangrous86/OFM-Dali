# OFM-Dali – RGBW-Erweiterung (Branch `v1dev` dieses Forks, Stand `v13aa`)

Dieser Fork ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse pro Leuchte (RGBW-Modus). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (abgezweigt bei Commit `6e7aca5`; am 17.11.2026 der Upstream-Stand `a120578` mit Fix #7 `eed5a33` „export group settings“ hereingemergt, er korrigiert vier vertauschte HCL-Parameternamen im Gruppen-Template, Logik und Speicherlayout unverändert). Das Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY bleibt unverändert.

> **Stand (17.11.2026):** Branch `v1dev` = Firmware-Stand **`v13aa`** (Build `develop_REG1_USB_V1`, Projekt `GW-REG1-Dali`), ETS-Applikation **0.14**. Neu gegenüber `v12` ist die **automatische Aktivierung beim Einschalten** (Farbe nur temporär setzen, der Pegelbefehl übernimmt sie, ein Fade statt zwei, siehe [5.1](#51-einschalten-mit-farbe-abfrage-oder-automatische-aktivierung)). **`v13aa` ist noch nicht kompiliert und nicht auf Hardware getestet**; als Nächstes steht der Testplan T1–T8 an.
>
> Der Vorgänger **`v12`** (Verfahren „Farbe, auf das Ende des Farb-Fades warten, dann Pegel“) ist mit einem echten Treiber (DA4-M, 4-Kanal-Konstantspannungs-Dimmer im Modus RGBW) am `GW-REG1-Dali` ausgiebig getestet. Er liegt archiviert im Branch **`v1dev-v12`** und wird nicht mehr verändert. Das v12-Einschaltverfahren steckt weiterhin in `v13aa` und ist zur Laufzeit per Konsole `onmode 1` erreichbar. Alle Testergebnisse in dieser README mit Version v6–v12 stammen von diesem Vorgänger.
>
> Offen sind außerdem die Wechselwirkungen mit Gruppen, Broadcast, Szenen, Sperre, mehreren Kanälen und den anderen Farbtypen (siehe [Abschnitt 5](#5-offene-punkte-und-entscheidungen)).

**So ist diese README aufgebaut:** Funktion ([1](#1-funktionsüberblick)), Befunde zum Treiber mit Status ([2](#2-befunde-zum-treiber-da4-m-mit-status)), zusammengefasste Tests ([3](#3-testübersicht)), Einschränkungen ([4](#4-bekannte-einschränkungen)), Offenes und Pläne ([5](#5-offene-punkte-und-entscheidungen)), Testanleitung ([6](#6-testanleitung)). Alle Messprotokolle, Logzeilen und Zwischenstände stehen **ungekürzt** in der [Rohfassung](doc/README-Rohfassung-2026-11-16.md). Details zu den Änderungen am Code: [`doc/RGBW-Gegenueberstellung.md`](doc/RGBW-Gegenueberstellung.md).

**Statuskennzeichen in dieser README:**

| Kennzeichen | Bedeutung |
|---|---|
| **bestätigt** | auf Hardware gemessen, Test und Version genannt |
| **widerlegt** | frühere Annahme, durch spätere Messung falsch (mit Verweis auf die Lösung) |
| **überholt** | war zum damaligen Stand richtig, wurde durch eine spätere Änderung ersetzt |
| **behoben** | Fehler gefunden und in der genannten Version behoben (auf Hardware bestätigt, sofern nicht anders vermerkt) |
| **offen** | noch nicht getestet oder noch nicht entschieden |
| **zurückgestellt** | bewusst nicht Teil des ersten Pull Requests |

---

## 1. Funktionsüberblick

### 1.1 Objekte und Parameter

- Neuer Eintrag **„RGBW“** in der ETS beim Parameter *„Farbe ansteuern per“* (Einzeladressen **und** Gruppenadressen).
- Bei RGBW erscheinen **zwei Kommunikationsobjekte** (die gleichen Objektnummern wie bei den anderen Farbtypen, z. B. 306/307 für Kanal A0): **Farbe RGBW** (Eingang, DPT 251.600, 6 Byte) und **Farbe RGBW Status** (Ausgang, DPT 251.600, 6 Byte).
- Das Gateway wertet die **Gültigkeitsbits** aus: Es werden nur die Kanäle geändert, die als gültig markiert sind (z. B. nur Weiß ändern, ohne RGB anzufassen).
- Ein Farbtelegramm **schaltet einen ausgeschalteten Kanal ein**, und **R = G = B = W = 0 schaltet ihn aus** (siehe [1.5](#15-ein--und-ausschalten)).
- Ansteuerung des Treibers über DALI DT8: **RGB-Pegel + Weiß-Pegel + `ACTIVATE`**. Die Einstellung *„Farbe übertragen per“* (RGB/xy) spielt bei RGBW keine Rolle.
- ETS-Parameter **Helligkeit bei RGBW** (Einzelkanal und Gruppe, nur bei *Farbe ansteuern per = RGBW*), siehe [1.4](#14-helligkeit-zwei-modi).

### 1.2 Telegramm DPT 251.600

| Byte | Inhalt |
|------|--------|
| 0 | Rot (0–255, in der ETS 0–100 %) |
| 1 | Grün |
| 2 | Blau |
| 3 | Weiß |
| 4 | reserviert (0x00) |
| 5 | Bit 3 = R gültig, Bit 2 = G gültig, Bit 1 = B gültig, Bit 0 = W gültig |

In der ETS zeigt *„Wert schreiben“* dafür je Kanal ein Häkchen (= Gültigkeitsbit) und ein Prozentfeld.

> **Hinweis zu Gruppen:** Für einen **Gruppenkanal** behandelt das Gateway jedes Telegramm als **vollständige** Farbe: Kanäle ohne Gültigkeitsbit werden **0**, sie behalten nicht den vorherigen Wert. Das ist kein Fehler, das Ergebnis kann aber anders aussehen als bei „nur diesen Kanal ändern“. Grund: Ein EVG kann in mehreren Gruppen liegen oder einzeln angesteuert worden sein, die in der Gruppe gespeicherte Farbe ist dann nicht die tatsächliche Farbe aller Mitglieder (das Gateway fragt nur ein Referenz-EVG ab, siehe [1.6](#16-gruppen)). Beispiel: Gruppe 1 und 2 haben je vier EVGs, zwei davon liegen in beiden. Rot an Gruppe 1 und Blau an Gruppe 2 mit vollständigen Werten (`FF 00 00 00` bzw. `00 00 FF 00`) hat ein eindeutiges Ergebnis; mit „nur Blau gültig“ hinge es vom Stand der einzelnen EVGs ab. Bei **Einzelkanälen** darf weiter einzeln editiert werden (nicht gültige Kanäle behalten ihren Wert).

### 1.3 DALI-Sequenz (je Farbänderung)

1. `DTR0=R, DTR1=G, DTR2=B` → `SET TEMPORARY RGB DIMLEVEL` (DT8, Befehl 235)
2. `DTR0=W, DTR1=255, DTR2=255` → `SET TEMPORARY WAF DIMLEVEL` (DT8, Befehl 236; 255 = MASK = Amber/Freecolour unverändert)
3. `ACTIVATE` (Befehl 226) – beide temporären Werte werden gemeinsam übernommen

Der Wert 255 wird – wie schon bei RGB – auf 254 begrenzt, weil 255 bei DALI „MASK“ (nicht ändern) bedeutet. Die Farbsequenz braucht rund 310 ms Buszeit. Wie das Einschalten mit Farbe und die Reihenfolge von Farbe und Pegel gelöst sind (Grund: der Treiber hat nur **eine** Fade-Engine), steht in [1.5](#15-ein--und-ausschalten) und [Abschnitt 2](#2-befunde-zum-treiber-da4-m-mit-status). Beim Einschalten aus „Aus“ entfällt in `v13aa` das `ACTIVATE` (automatische Aktivierung, [5.1](#51-einschalten-mit-farbe-abfrage-oder-automatische-aktivierung)); Farbänderungen an einer brennenden Lampe laufen weiter über die Sequenz oben.

### 1.4 Helligkeit: zwei Modi

| Einstellung *Helligkeit bei RGBW* | Verhalten |
|-------------|-----------|
| **Über Objekt „Dimmen absolut“** (Standard) | Farbe = nur Mischung, Helligkeit nur über *Dimmen absolut* / *Schalten* / *Dimmen relativ*. Ein Farbtelegramm mit kleineren Beträgen (z. B. 50 % statt 100 % Rot) dimmt eine brennende Lampe nicht (am DA4-M gemessen, Befund B4). Visualisierungen, die RGBW-Werte „helligkeitsskaliert“ senden, brauchen hier zusätzlich eine Adresse für die Helligkeit (z. B. `brightness_address` in Home Assistant) auf *Dimmen absolut*. |
| **Aus dem RGBW-Wert (größter Kanal = Helligkeit)** („Slider-Modus“, v12) | Der größte der vier Kanäle ist die Helligkeit (255 = 100 %), das Verhältnis der Kanäle ist die Farbmischung. Für Clients mit nur einem Objekt (z. B. ein Home-Assistant-KNX-Licht ohne `brightness_address`). |

Im Slider-Modus gilt:

- **Farbtelegramm** `R G B W`: Helligkeit = größter Kanal ÷ 255 (linear in Prozent, dann wie *Dimmen absolut* auf die DALI-Kurve umgerechnet). Der Treiber bekommt die auf 255 normierte Mischung und die Helligkeit als eigenen Pegelbefehl (nach dem Ende des Farb-Fades). Beispiel: `80 00 00 80` = Rot-Weiß mit etwa 50 %.
- **Aus → Farbtelegramm:** schaltet mit dem Pegel aus dem RGBW-Wert ein (nicht mit dem Einschaltwert Tag/Nacht). **Alle Kanäle 0:** aus.
- **Status Farbe (RGBW)** zeigt Mischung × aktueller Pegel und folgt auch *Dimmen absolut*, *Schalten* und *Dimmen relativ*. Ein eben empfangener Farbwert wird unverändert zurückgemeldet.
- *Dimmen absolut* / *Schalten* / *Dimmen relativ* funktionieren weiter und stellen den Pegel direkt. Ein Client mit Helligkeitsadresse sollte den Modus *Über Objekt* benutzen.
- Szenen im Treiber, HCL und die anderen Farbtypen sind nicht betroffen.

Home Assistant sendet bei einer Leuchte **mit** Helligkeitsadresse zuerst die Farbe und ≈ 20 ms später die Helligkeit (*Dimmen absolut*), bei einer Leuchte **ohne** Helligkeitsadresse nur die Farbe (Helligkeit in den Farbwert eingerechnet).

### 1.5 Ein- und Ausschalten

| Ereignis | Verhalten bei RGBW-Kanälen |
|----------|----------------------------|
| **Ausschalten** | Normales `DAPC 0`, die Leuchte fadet mit der Fade-Zeit des Treibers aus. Ein Umweg über die Farbe 0/0/0/0 wird **nicht** gemacht, weil der DA4-M dafür Weiß statt Schwarz zeigt (Befund B3; nur per Konsole `blackoff 1` zuschaltbar, Standard aus). Die zuletzt gesetzte Farbe bleibt im Gateway gespeichert. Ausgeschaltet heißt bei uns immer nur Pegel 0, einen separaten Aus-Befehl gibt es nicht. |
| **Einschalten** (*Schalten* Ein, *Dimmen absolut* > 0, Farbtelegramm an einen ausgeschalteten Kanal) | **Standard (`onmode 2`, `v13aa`):** automatische Aktivierung. Nach dem Sammelfenster (`aawin`, 80 ms) gehen die Farbe als temporärer Wert (ohne `ACTIVATE`) und direkt danach der Einschaltwert (`DAPC`) raus; Farbe und Pegel faden gemeinsam (siehe 5.1, **ungetestet**). Einschaltwert: Tag/Nacht-Einschaltwert, bei 0 der letzte Wert; im Slider-Modus der Pegel aus dem Farbwert. **`onmode 1` (Konsole, Verfahren von `v12`, getestet):** zuerst die Farbe bei Pegel 0, der Treiber blendet sie unsichtbar über; sobald `QUERY_STATUS` (Bit 4 „Fade läuft“) das Ende des Farb-Fades meldet, folgt der Einschaltwert. Rückfall, wenn der Treiber nicht antwortet: Übertragungszeit (310 ms) + Fade-Zeit (per `QUERY_FADE_SPEEDS`, Standard 1000 ms) + Reserve (`margin`, 100 ms) + 300 ms. **`onmode 0` (nur Konsole):** Pegel zuerst, wenn Schwarz im Treiber gespeichert ist, sonst die alte Sequenz „Pegel, Farbe, Pause, Pegel“ (`settle`, 1500 ms). In allen Modi verwirft ein weiteres Telegramm (Schalten, Dimmen, Sperre, Szene) ausstehende Schritte. Grund für das Ganze: Befund B1. |
| Farbtelegramm an einen **eingeschalteten** Kanal | Nur die Farbe ändert sich, die Helligkeit bleibt. |
| *Schalten* / *Dimmen absolut* | wie bisher. Die Farbe bleibt dabei erhalten (Rot, dann 25 % = Rot mit 25 %). |

Zusätzlich wurde `koHandleDimmAbs` korrigiert: Der interne Ein/Aus-Merker (`currentState`) folgt jetzt dem Wert von *Dimmen absolut* (0 % = aus). Vorher blieb er nach „Dimmen auf 0“ auf *Ein* stehen, und ein folgendes Farbtelegramm wurde ignoriert (Treppenhauslicht-Kanäle ausgenommen).

Hinweis zum Ausgangscode: Bei den anderen Farbtypen (HSV, RGB, TW, xyY) schickt ein Farbtelegramm an einen ausgeschalteten Kanal die Farbe sofort an den Treiber und aktualisiert nur die KNX-Statusobjekte (`// TODO get real`), es sendet keinen DALI-Einschaltbefehl. Das wurde hier bewusst **nicht** angefasst (siehe [5.2](#52-schalter-farbtelegramm-bei-ausgeschaltetem-kanal)).

### 1.6 Gruppen

*(Stand v12, Hardware-Tests für Gruppen stehen noch aus.)*

- **Farbe beim Start:** Wie ein Einzelkanal liest auch ein RGBW-Gruppenkanal beim Start die Farbe aus dem Treiber, aber nur von **einem** EVG: dem Dimm-Referenz-EVG der Gruppe (dasselbe wie für Pegel und Fade-Zeit). Alle Mitglieder abzufragen würde vier Abfragen je EVG kosten, und bei abweichenden Mitgliedern gäbe es keinen eindeutigen Wert. Ohne Referenz-EVG bleibt die Gruppenfarbe bis zum ersten Farbtelegramm unbekannt.
- **Immer die komplette Farbe** (siehe Hinweis in 1.2). Der Treiber bekommt immer alle vier Kanäle in einer Sequenz; ein Telegramm, dessen gültige Kanäle alle 0 sind, schaltet die Gruppe aus.
- **Referenz-EVG:** Fade-Zeit und das Ende des Farb-Fades (`QUERY_STATUS`) kommen vom Referenz-EVG; antwortet es nicht oder gibt es keins, gilt der Standardwert (feste Wartezeit). Der Pegel ist unkritisch: ein Ziel-Pegel ist für jedes Mitglied gleich, nur die Fade-Dauer unterscheidet sich.
- **Keine Garantie:** Bei Gruppen, deren Mitglieder auch einzeln oder über andere Gruppen angesteuert werden, kann ein Mitglied mit anderem Ausgangszustand von den Zeiten des Referenz-EVGs abweichen (anderer Farb-Fade, kurz eine Mischfarbe beim Einschalten). Wird eine Gruppe immer als Gruppe und ein EVG immer gleich angesteuert, sollte das nicht auftreten.
- **Nicht abgedeckt (bekannte Lücke, nicht getestet):** Die Gruppe weiß nichts vom laufenden Farb-Fade ihrer Mitglieder, wenn dasselbe EVG über eine andere Gruppe oder einzeln angesteuert wird.

### 1.7 Serielle Konsole (Feintuning und Diagnose)

`onmode m [nnnn]` (`2` = automatische Aktivierung, **Standard**; `1` = Farbe zuerst, Verfahren von `v12`; `0` = Pegel zuerst, nur wenn Schwarz im Treiber gespeichert ist; nnnn = feste Pause vor dem Pegel bei Modus 1, 0 = automatisch per `QUERY_STATUS`), `aawin nnn` (Sammelfenster für Modus 2 in ms, 0–1000, Standard 80, 0 = sofort senden), `settle nnnn` (Pause im Fallback von Modus 0, 0–5000 ms), `margin nnn` (Reserve auf die Fade-Zeit, 0–2000 ms), `blackoff 0|1` (Farbe vor dem Ausschalten auf 0/0/0/0, Standard 0), `getCol yy [sel]` (fragt Farbtyp-Merkmale, Farbstatus und die Farbwerte des Treibers ab, ändert nichts; ohne Selektor: Report 233–236, aktuell 9–12, temporär 201–204 je R, G, B, W). Mit `#define RGBW_DIAG` in `DaliChannel.h` (Standard aus, nur Test) zusätzlich Log der DALI-Frames und der Abfragen `QUERY_ACTUAL_LEVEL`/`QUERY_STATUS`; diese Diagnose entfällt vor einem Pull Request.

---

## 2. Befunde zum Treiber DA4-M (mit Status)

Diese Tabelle fasst zusammen, was wir über das Verhalten des DA4-M (und des Gateways) gelernt haben, einschließlich der Annahmen, die später widerlegt wurden. Die Nummern **B1 bis B17** werden an anderen Stellen der README als Verweis benutzt. Messwerte und Logs stehen in der [Rohfassung](doc/README-Rohfassung-2026-11-16.md).

| Nr | Befund | Status | Belegt durch | Folge im Code |
|---|---|---|---|---|
| B1 | **Der Treiber hat nur eine Fade-Engine.** Ein Pegelbefehl bricht einen laufenden Farb-Fade ab (Mischfarbe bleibt stehen), das `ACTIVATE` einer Farbsequenz bricht einen laufenden Pegel-Fade ab. | **bestätigt** | v6-Fund: Home Assistant sendet Farbe + 20 ms später Helligkeit auf brennende Lampe, falsche Farbe (Blau + Weiß statt Rot + Weiß) bei 2 von 2 Versuchen. v8: Pegel stand bei 208 statt 235. v10-Fund: ein UP direkt nach der Farbe bricht den Farb-Fade ab. v11-Tests 1–4 bestanden. | Pegelbefehl wartet auf das Ende des Farb-Fades (`QUERY_STATUS`) in `rgbwOn` und `koHandleDimmAbs` (v7), Zielpegel wird nach dem Farb-Fade noch einmal gesendet (v9), `loopDimming` sendet keinen UP/DOWN während des Farb-Fades (v11) |
| B2 | „Der DA4-M **ignoriert eine Farbänderung bei Pegel 0**, die Lampe leuchtet mit der alten Farbe.“ | **widerlegt** | Frühe Beobachtung vom 13.11. (Commit `ea2cc95`: nach „alle 0“ leuchtete ein rotes Telegramm mit der alten Farbe Weiß, erst das zweite Telegramm wirkte). **Gegenbelege:** Farbe bei Pegel 0 senden, auf das Ende des Farb-Fades warten (1350 ms), dann Pegel: neue Farbe sofort (v10 Test 2, v12 H2, A5, B2). Vermutliche Ursache des frühen Befunds (nicht einzeln nachgeprüft): ein sofort folgender Pegelbefehl hat den Farb-Fade abgebrochen (B1) oder die Farbe 0/0/0/0 wurde als Weiß dargestellt (B3). | Die damalige Lösung „erst einschalten, dann Farbe“ (`ea2cc95`) ist durch „erst Farbe, dann Pegel“ ersetzt. Der veraltete Code-Kommentar dazu (in `handleColorRGBW`, „ignore a colour change while the channel is off“) ist am 17.11. korrigiert, ebenso der Kommentar in `rgbwOff` |
| B3 | Der DA4-M zeigt für die Farbe 0/0/0/0 **Weiß** statt Schwarz. | **bestätigt** | Hardware-Tests (Stand v6 bis v12), dokumentiert in der Rohfassung unter „Ein-/Ausschaltverhalten“ | Ausschalten per `DAPC 0`, `blackoff` Standard aus. Die frühere Sequenz „Schwarz zuerst, dann Pegel 0; Pegel zuerst aus Schwarz“ (Commit `f568871`, 13.11.) ist **überholt**: nur noch per Konsole (`blackoff 1`, `onmode 0`). Der Kommentarblock „RGBW switch on/off“ in `DaliChannel.cpp` ist am 17.11. auf den heutigen Ablauf korrigiert |
| B4 | Der Treiber **normiert** die Farbe, die Amplitude des Telegramms geht verloren (z. B. `80 00 00 80` an brennende Lampe: Lampe unverändert, Status meldet `80 00 00 80`). | **bestätigt** | v11 Test A1 | Standard: „Farbe = Mischung, Helligkeit = Dimmen absolut“. Slider-Modus (v12) rechnet die Helligkeit selbst aus dem Farbwert |
| B5 | Der Treiber meldet seine Farbe über die **Report-Selektoren 233–236** (`QUERY_COLOUR_VALUE`), auch bei Pegel 0, er hält die Farbe also gespeichert. | **bestätigt** | v9 `getCol` (Lampe Blau, auch bei Pegel 0 dieselbe Antwort 0/0/254/0); v10 Test 1 (Neustart bei ausgeschalteter Lampe, zuletzt Blau: Gateway liest `00 00 FF 00`) | Farbe beim Start vom Treiber übernehmen (v10) |
| B5a | Die Standard-Selektoren des Diagnosebefehls `getCol` in v9 (64–67, 0–2) waren falsch (Primärfarb-Koordinaten bzw. X/Y/Tc, hat ein RGBW-Treiber nicht). | **überholt** | in v10 durch 233–236, 9–12 und 201–204 ersetzt | `getCol` |
| B5b | Haltedauer einer Farbe, die **bei Pegel 0 gesendet** wurde: Die vor dem Ausschalten gesetzte Farbe bleibt nachweislich im Treiber (B5). Eine Farbe, die bei Pegel 0 gesendet wurde, haben wir nur mit höchstens etwa 1,5 s bis zum Pegel getestet. | **offen** | – | Test mit längerer Pause (z. B. 1 min) vor dem Einbau des Schalters aus 5.2 |
| B6 | `QUERY_STATUS` Bit 4 („Fade läuft“) meldet auch einen **Farb-Fade bei Pegel 0**. | **bestätigt** | Logs `colour fade over (status 0x00), 1350 ms` u. a. | Ende-Erkennung in `loopFadePoll` |
| B7 | Fade-Dauern am DA4-M (Fade-Zeit 1 s per `QUERY_FADE_SPEEDS`): Farb-Fade bei **Farbwechsel ≈ 1,35–1,58 s**, bei **unveränderter bzw. gespeicherter Farbe ≈ 0,3 s**. Der Pegel-Fade dauert etwa 1 s. | **bestätigt** | v6, v7 (1569–1571 ms), v9 (1572 ms), v10 (1350, 1476 ms), v12 (335/336 ms) | feste Zeiten nur als Rückfall |
| B8 | Das Weiß wird am DA4-M (Modus RGBW) mit **Befehl 236** (`SET TEMPORARY WAF DIMLEVEL`) gesetzt. | **bestätigt** (nur DA4-M) | alle Weiß-Tests | Befehl 237 (`RGBWAF`) bei anderen Treibern: **offen**, andere Anwender sollen es mit ihren Treibern prüfen |
| B9 | Der DA4-M-Takt für relatives Dimmen: ≈ 9 Pegelstufen alle 200 ms. Im unteren Bereich der logarithmischen Kurve wirkt das langsam (Pegel 1 bis 72 in ≈ 2 s = 0,7 %). | **bestätigt** (nicht verändert) | v7 | – |
| B10 | Die Dimmschleife (`loopDimming`) beendet das Dimmen am Minimum nicht, sondern wiederholt *Dunkler* + *Aus* bis zum Stopp (Telegrammflut, falscher Status). Struktur aus dem Ausgangscode (`git blame`); bei anderen Farbtypen ist dasselbe zu erwarten, nicht geprüft. | **behoben** (nur RGBW) v8, Test 3 | v7-Fund, v8 Test 3 | Der Schritt, der das Minimum erreicht, schaltet aus und beendet das Dimmen; *Dunkler* auf ausgeschalteter Lampe wird ignoriert |
| B11 | Nach dem Stopp eines relativen Dimmens meldete das Gateway nur den **zuletzt abgefragten** Pegel (21,6 % statt 28,6 %). Betrifft den Ausgangscode, nicht nur RGBW. | **behoben** v9 (einmal `QUERY_ACTUAL_LEVEL` ≈ 300 ms nach dem Stopp, alle Kanaltypen), auf Hardware bestätigt | v8 Test 4 (Fund), v9-Test (145 = 5,10 %, `getLvl 00` identisch). Der To-do-Eintrag „ungetestet“ in der alten README war **überholt** | – |
| B12 | Nach einem Neustart kannte das Gateway weder Farbe noch Ein/Aus-Zustand der brennenden Lampe (bis v7 Aufblitzen beim ersten Farbtelegramm). | **behoben**: Ein/Aus und Pegel seit v8 (Test 5), Farbe seit v10 (Tests 1–3) | | Beim Start `QUERY_ACTUAL_LEVEL` und Report 233–236 lesen |
| B13 | Die Farbsequenz bricht auch einen laufenden **Pegel-Fade** ab, wenn Helligkeit **vor** der Farbe kommt (andere Visualisierungen, Logik, Szenen-Bausteine). | **behoben** v9, auf Hardware bestätigt (60 % und 20 ms später Blau: Pegel 235) | v8 (Pegel blieb bei 208), v9 | Zielpegel wird nach dem Ende des Farb-Fades noch einmal gesendet (kurze kleine Stufe am Anfang ist sichtbar) |
| B14 | *Schalten Status* Ein wurde bei jedem *Dimmen absolut* auf eine brennende Lampe zusätzlich gesendet. | **behoben** v8 (nur RGBW, Schaltstatus nur bei Änderung), Test 2 | | – |
| B15 | Die Statusobjekte werden bei jedem relativen Schritt gesendet, obwohl „Dimmstatus Interval bei relativ“ auf *Deaktiviert (nur am Ende)* steht. Vermutung: Pegelabfrage `Got new actual level` (Ausgangscode, Mai 2026). | **offen** (nicht geprüft, nicht verändert) | v7 | – |
| B16 | Die ETS-Applikation sei **nicht in-place updatebar**, das Gerät müsse neu angelegt werden (Aussage der alten README, `ReplacesVersions` enthielt die neue Version nicht). | **überholt** | Ab 0.14 ersetzt die Applikation 0.0, 0.10, 0.12, 0.13; in der ETS *Aktualisieren* (ein deaktivierter Einzel-EVG verliert dabei seine Gruppenadressen). Für Versionen außerhalb dieser Liste gilt die alte Aussage weiter | – |
| B17 | KNX-Speicher: Build-Skript schätzt **DATA\* 36801 von 36864 Byte** (nur 63 Byte Reserve). *colorType* von 2 auf 3 Bit verbreitert und auf Byte 21 Bit 5–7 (Einzelkanal) bzw. Byte 17 Bit 2–4 (Gruppe) verschoben, *Helligkeit bei RGBW* liegt in Byte 7 Bit 5 (Einzelkanal) bzw. Byte 17 Bit 5 (Gruppe), die Parameterblöcke werden nicht größer (ein eigenes Byte hätte die Schätzung um 1 Byte über die Partitionsgröße gehoben). | **bestätigt** (Stand v12) | Build v12 | Neue Parameter nur in freie Bits legen. Ist das nicht möglich, `KNX_FLASH` über die `platformio.ini` anpassen (**offen**, noch nicht geprüft) |

---

## 3. Testübersicht

Alle Tests: DA4-M im Modus RGBW, Kanal A0, Auslösung über ETS-Gruppenmonitor oder Home Assistant (`knx.send`, `light.turn_on`), Auswertung per ETS-Bus und serieller Log. Die Testfälle 1–14 stehen in [6.2](#62-testfälle). Spaltenangabe „Version“ ist der Firmware-Tag des Teststands.

| Thema | Version | Ergebnis |
|---|---|---|
| **Grundfunktion** (Testfälle 1–13): Schalten, Dimmen absolut, Einzelfarben R/G/B/W und Mischfarben, Gültigkeitsbits, alle 0 = aus, Einschalten mit gespeicherter Farbe und mit neuer Farbe, Merker nach Dimmen auf 0, Statusobjekte | v6 | **bestanden**. Einschalten aus Aus: sofort die neue Farbe, keine alte Farbe, kein Weißblitz, weiches Hochfaden. Farbtelegramm an eine ausgeschaltete Lampe schaltet mit der **neuen** Farbe ein (Rot statt gespeichertem Magenta, ohne zweites Senden). Gültigkeitsbits: nur W bzw. nur R ändern bei brennender Lampe nur diesen Kanal, ein Telegramm ohne gültigen Kanal wird ignoriert. Test 13 (Rot, dann 25 %) ist durch die Dimm-Nachtests abgedeckt (50 %, 100 %, 10 %) |
| **Farbverlauf** Rot → Grün → Blau → Rot: 25 Telegramme im Abstand von 0,5 s | v11 | **bestanden**: je Telegramm antwortet 20/0/3 nach etwa 100 ms, keine Level-Befehle, keine Warnungen, Verlauf weich (der Treiber fadet jeden Schritt nach), Endzustand Rot bei 40 % |
| **Dimmen relativ**: dunkler/heller auf brennender Lampe, heller aus Aus (`RECALL_MIN`), dunkler bis zum Minimum | v6–v8 | **bestanden**. Farbe bleibt unverändert, keine Farbe wird neu gesendet. *Heller* hält am Anschlag selbst an. *Heller* aus Aus: Lampe geht in der **gespeicherten Farbe** an. Fund *Dunkler* bis zum Minimum (B10) in v8 **behoben** |
| **Status nach dem Stopp** eines relativen Dimmens | v8 (Fund), v9 | **behoben**, bestätigt (B11) |
| **Relatives Dimmen unmittelbar nach / während eines Farbtelegramms** (4 Tests, Start rot-weiß 20 %) | v10 (Fehler), v11 | **behoben**, bestätigt: Farbe Blau + 100 ms *Heller* → reines Blau, 20 % unverändert; Farbe Rot-Weiß + *Heller* → Dimmen beginnt nach ≈ 1,8 s, Endwert 100 %; Gegenrichtung (*Dunkler*, dann Farbe) → keine Mischfarbe. Nicht abgedeckt: Gruppen und Broadcast. Hinweis für Anwender: Farbe und Dimmen unmittelbar nacheinander haben eine Anlaufzeit des Dimmens von rund 1,5 s (B1) |
| **Farbe + Helligkeit gemeinsam auf brennender Lampe** (Home Assistant: erst Farbe, 20 ms später Helligkeit) | v6 (Fund), v7 | **behoben**, bestätigt: Rot-Weiß 20 % → Blau 50 % und Blau 50 % → Rot-Weiß 50 %, Farbsequenz ≈ 1,57 s, Pegel folgt danach |
| **Gegenrichtung** (erst Helligkeit, dann Farbe) | v8 (Fund), v9 | **behoben**, bestätigt (B13). Wenn die kleine Stufe am Anfang stört: Farbe **vor** der Helligkeit senden (Home Assistant macht das von selbst), oder die Helligkeit etwa 2 s nach der Farbe; mehrere fadeauslösende Telegramme für denselben Kanal nicht dichter als etwa 2 s |
| **Schaltstatus** nur bei Änderung | v8 | **bestanden** (B14) |
| **Neustart**: Pegel/Ein-Aus aus dem Treiber (bei brennender Lampe: kein Aufblitzen, kein Ausgehen, kein Farbtelegramm) | v8 Test 5 | **bestanden** (B12) |
| **Neustart**: Farbe aus dem Treiber (3 Tests: ausgeschaltete Lampe, Teilfarbe, brennende Lampe rot-weiß 20 %) | v10 | **bestanden**. Nur-Rot-Telegramm verrechnet sich mit der echten Treiberfarbe (Violett statt Rot), Lampe bleibt über den ganzen Start unverändert, Status ≈ 0,6 s nach dem Neustart |
| **Farbabfrage** am DA4-M (`getCol`) | v9/v10 | **bestätigt** (B5) |
| **Slider-Modus** „Aus dem RGBW-Wert“: A1 (Befund B4), A2–A5 (Telegramme direkt), B1–B3 (Home Assistant ohne Helligkeitsadresse) | v11/v12 | **bestanden**. Pegel entsprechen der DALI-Kurve (2 % → 110, 30 % → 210, 40 % → 220, 80 % → 246). Aus → Farbe schaltet direkt mit dem Pegel aus dem Farbwert ein (kein Aufblitzen mit dem Tag-Wert). Alle 0 schaltet aus, 20/0/3 bleibt unverändert, die Farbe bleibt gespeichert. Farb-Fade endete jeweils nach 335 ms vor dem Pegelbefehl |
| **HCL mit RGBW** (Kurve „Sonnen Auf-/Untergang“, nur Helligkeit, Min 20 %, KO 4 mit gültiger Uhrzeit): H1–H4 | v12 | **bestanden**: H1 *Aus* im Modus Auto; H2 *Ein* → rot-weiß mit **20 %** (Farbstatus 20/0/3 folgt der HCL-Helligkeit); H3 Farbtelegramm bei brennender Lampe → grün 50 %, `HCL Mode: Manu`, danach `Ignored because mode is manu`; H4 *Aus* im Modus Manu → `HCL Mode: Auto`. Lücken siehe [5.3](#53-hcl-mit-rgbw) |

---

## 4. Bekannte Einschränkungen

- **Szenen/DCA-Konfiguration:** Die Szenen-Farbe (Konfigurationstransfer) kennt nur RGB. Bei RGBW-Kanälen wird dort das Weiß nicht mitgeschrieben. Szenen per KNX-Telegramm (Szenenobjekt) funktionieren über die im Treiber gespeicherten Szenen; danach gilt die Farbe im Gateway als unbekannt.
- **Getrennte Objekte** (Rot/Grün/Blau/Weiß einzeln, 8 Bit) gibt es bei RGBW *nicht*, nur das kombinierte 6-Byte-Objekt. Das kann später nachgerüstet werden.
- **Andere Treiber:** Manche erwarten für Weiß statt Befehl 236 den Befehl `SET TEMPORARY RGBWAF CONTROL`/`RGBWAF DIMLEVEL` (237) bzw. ein anderes Kanal-Mapping. Beim DA4-M (Modus RGBW) geht es mit 236 (B8). Andere Treiber sind nicht geprüft.
- **HCL** wirkt bei RGBW wie bisher nur auf die Helligkeit; die HCL-Farbtemperatur ist nicht für RGBW gedacht (siehe 5.3).
- **Dimmkurve:** Das Gateway rechnet Prozent fest mit der logarithmischen DALI-Kurve (siehe 5.4).
- **ETS-Applikation:** Der Parameter *colorType* wurde verschoben (B17), die Applikation ist nur gegenüber den in `ReplacesVersions` genannten Versionen updatebar (B16).
- **Bewusst nicht angefasst** (bekannte Lücken dieser Version): relatives Dimmen von aus (`RECALL_MIN`) bei gespeicherter Farbe Schwarz, Treppenhauslicht-Kanäle, Funktionstasten/Identify/ETS-Funktionen des Moduls, Szenen, die im Treiber selbst ausgeführt werden.

---

## 5. Offene Punkte und Entscheidungen

### 5.1 Einschalten mit Farbe: Abfrage oder automatische Aktivierung

**Status: als `v13aa` umgesetzt, seit 17.11.2026 Stand von Branch `v1dev`; noch nicht kompiliert und nicht auf Hardware getestet.** Es gibt zwei Wege, ein Farbtelegramm an eine ausgeschaltete Lampe auf den Treiber zu bringen:

| | **A: heutiges Verfahren** | **B: automatische Aktivierung (zu testen)** |
|---|---|---|
| Ablauf | Farbe temporär setzen + `ACTIVATE` bei Pegel 0, warten bis `QUERY_STATUS` das Ende des Farb-Fades meldet, dann `DAPC` | Farbe nur temporär setzen (`SET TEMPORARY RGB/WAF DIMLEVEL`), **ohne** `ACTIVATE`, direkt danach `DAPC` mit dem Einschaltwert; der Treiber übernimmt die Farbe mit dem Pegelbefehl |
| Stand | **bestätigt** am DA4-M (B1, B2, B6, B7) | **ungetestet**. Der Standard IEC 62386-209 kennt Testabläufe „AutoActivate“ (u. a. mit `DAPC`); ob der DA4-M es kann und ob es ab Werk aktiv ist, ist nicht belegt |
| Vorteile | spezifikationskonform, auch bei Treibern ohne Farb-Fade, Rückfall auf feste Zeiten, wenn der Treiber nicht antwortet; löst auch das Zusammenspiel Farbe + Helligkeit auf brennender Lampe | ein Fade statt zwei (kürzer), keine Abfragen und keine feste Zeit, weniger Buslast |
| Grenzen | zwei Fades (Farb-Fade ≈ 1,4–1,6 s bei dunkler Lampe, dann Pegel-Fade), etwa alle 45 ms ein `QUERY_STATUS` während des Farb-Fades | Treiberunterstützung unbekannt (ohne sie bleibt die Farbe unwirksam); **brennende Lampe, nur Farbe** braucht weiterhin `ACTIVATE`; **zwei Telegramme** (Farbe, 20 ms später Helligkeit) können sich beim zweiten `DAPC` abbrechen (B1); bei **Helligkeit vor Farbe** bleibt die Farbe unwirksam, bis der nächste Pegelbefehl oder `ACTIVATE` kommt; vermutlich sichtbarer Übergang von alter zu neuer Farbe beim Hochfaden (Annahme, nicht geprüft) |

**Umsetzung in `v13aa`:** Beim Einschalten eines RGBW-Kanals (Schalten Ein, Helligkeit, Farbtelegramm bei ausgeschaltetem Kanal) sendet die Firmware die Farbe als temporären Wert (`SET TEMPORARY RGB/WAF`) **ohne** `ACTIVATE` und danach direkt den Pegel (`DAPC`); die Farbe soll der Treiber dann mit dem Pegel zusammen übernehmen (ein Fade, keine Abfrage, keine feste Wartezeit). Ein kurzes Sammelfenster (`aawin`, Standard 80 ms) fasst ein Farb- und ein Helligkeitstelegramm, die kurz nacheinander eintreffen (Home Assistant), zu einer Sequenz zusammen. Unverändert wie in `v12` bleiben: Farbänderung und Helligkeit bei brennender Lampe, relatives Dimmen, Ausschalten, Slider-Modus. Kommt ein zweites Telegramm erst nach dem Fenster, greifen die bekannten Schutzpfade aus `v12` (Abfrage des Fade-Endes). Rückfall zur Laufzeit: `onmode 1` (siehe 1.7).

**Was sich damit nicht ändert:** Ein reines Farbtelegramm an eine *brennende* Lampe braucht weiterhin `ACTIVATE`. Ein reines Farbtelegramm an einen *ausgeschalteten* Kanal schaltet die Leuchte weiterhin ein, weil das Gateway den Kanal als „ein“ führt und den gespeicherten Einschaltwert sendet; das Verhalten „nur Farbe merken“ bringt erst der Schalter in 5.2.

**Testplan `v13aa` (Kanal A0, Konsole mit `onmode`/`aawin`-Ausgaben mitlesen):**

| # | Aktion | Erwartung |
|---|--------|-----------|
| T1 | Leuchte aus, Farbe RGBW senden, die sich von der zuletzt gesetzten unterscheidet | ein Fade (ca. 1 s) auf die neue Farbe; Log `RGBW: automatic activation: temporary colour, level N`, kein `colour fade over`. Zeigt die Leuchte kurz oder dauerhaft die *alte* Farbe, übernimmt der Treiber die Farbe ohne `ACTIVATE` nicht → mit `onmode 1` zurück auf `v12` |
| T2 | wie T1, Verlauf beobachten | festhalten, ob die Farbe beim Hochfaden von alt nach neu überblendet (Annahme: ja) |
| T3 | Home Assistant `light.turn_on` mit `rgb_color`/`rgbw_color` und Helligkeit aus „aus“ | ein Fade, Farbe und Pegel richtig; Log zeigt eine Sequenz |
| T4 | Helligkeit, 20 ms später Farbe (`knx.send`) aus „aus“ | Farbe und Pegel richtig, kein hängengebliebener Farbwert |
| T5 | *Schalten* Ein mit gespeicherter Farbe | Leuchte geht in der gespeicherten Farbe an |
| T6 | Slider-Modus (A5/B2), Wert aus „aus“ | richtige Farbe und Helligkeit |
| T7 | Regression Lampe an: Farbe, Farbe + Helligkeit (HA-Paar), relatives Dimmen nach Farbe | wie `v12` |
| T8 | `aawin 0` und HA-Paar aus „aus“ | prüfen, ob der zweite `DAPC` den Fade abbricht (Begründung für das Fenster) |

**Entscheidungsregel:** Besteht `v13aa` T1–T7 und läuft ruhiger als `v12` (ein Fade statt Farb-Fade plus Pegel-Fade), bleibt sie der Stand von `v1dev` und wir testen mit ihr weiter. Bei Abweichungen entscheiden wir zwischen Rückfall per `onmode 1` als Standard und dem Archivstand `v1dev-v12`; die Treiberfähigkeit wird dokumentiert. Beide Stände laufen mit der ETS-Applikation 0.14; nach dem Upstream-Fix #7 (Gruppen-Template) muss die Applikation aber neu erzeugt werden (siehe 6.1).

### 5.2 Schalter „Farbtelegramm bei ausgeschaltetem Kanal“

**Status: geplant für v13, ungetestet, noch nicht gebaut.** Heute schaltet bei RGBW ein Farbtelegramm an einen ausgeschalteten Kanal ein; bei HSV, RGB, TW und xyY wird die Farbe sofort an den Treiber geschickt und nur der Status auf „Ein“ gesetzt. Gateways anderer Hersteller bieten teils einen Parameter dafür; ein „übliches“ Verhalten ist nicht belegt. Hintergrund: Der Treiber übernimmt bei Pegel 0 die Farbe (B2 widerlegt die gegenteilige Annahme), deshalb ist „nur Farbe setzen“ technisch möglich; offen ist die Haltedauer (B5b).

**Geplante Umsetzung:**

- Ein ETS-Parameter für alle Farbtypen mit zwei Werten: **„Einschalten“** (Standard: RGBW wie heute, die anderen Typen unverändert) und **„Nur Wert merken“** (das Gateway speichert die Farbe, schaltet nicht ein, der Status bleibt „Aus“).
- Bei „Nur Wert merken“ geht die Farbe **nicht** im Aus-Zustand an den Treiber, sondern erst beim nächsten Einschalten (erst Farbe, dann Pegel). So hängt es nicht davon ab, ob ein Treiber das im Aus-Zustand kann. Bei RGBW läuft das über den vorhandenen Einschaltpfad (`rgbwOn`); im Block `if(!currentState)` in `koHandleColor` entfallen dann `rgbwOn`, `setDimmState` und `currentState = true`. Im Slider-Modus wird die Helligkeit aus dem Farbwert als Einschaltwert gemerkt.
- Für HSV, RGB, TW und xyY muss das Wiedersenden der Farbe beim Einschalten neu gebaut werden (wahrscheinlich über das gemeinsame `sendColor()`), und sie dürfen die Farbe bei „Nur Wert merken“ im Aus-Zustand nicht mehr sofort senden.
- Neue ETS-Version **0.15** (`ReplacesVersions` erweitern). Der Parameter gehört in ein freies Bit (Reserve nur 63 Byte, B17); sonst `KNX_FLASH` über die `platformio.ini` anpassen.
- Tests: RGBW (beide Optionen, Status, Treppenhauslicht, Gruppe, Slider-Modus, Pause von einer Minute zwischen Farbe und Einschalten) auf dem DA4-M; bei den anderen Typen nur Regression des Standards, die Option „Nur Wert merken“ dort von der Community testen lassen. Da das Verhalten auch Typen betrifft, die nicht von uns sind, wird es nach Absprache umgesetzt und erst dann in den PR gelegt.

### 5.3 HCL mit RGBW

**Status: HCL-Korrekturen zurückgestellt (nach Absprache zunächst nicht Teil des ersten Pull Requests).** Getestet und bestanden sind H1–H4 (Abschnitt 3). Aufbau des Tests: Einzelkanal A0 (RGBW, Slider-Modus), HCL-Kurve „Sonnen Auf-/Untergang“, nur *Helligkeit* aktiv (Min 20 %, Max 100 %), *Farbtemperatur nicht aktiv*, KO *HCL Kurve* 20/0/7 verknüpft. Die Kurve braucht eine gültige Uhrzeit (KO 4, DPT 19.001, hier GA 0/0/4) und den Standort, sonst meldet das Log `Ungueltige Sonnenstandsdaten`; mit Uhrzeit läuft sie einmal pro Minute, vor Sonnenaufgang und nach Sonnenuntergang gilt der Min-Wert.

**Bekannte Lücken (offen, zurückgestellt):**

- Eine HCL-**Farbtemperatur** auf einem RGBW-Kanal ist nicht sicher: `setTemperature` sendet den DT8-Befehl `SET_TEMP_COLOUR_TEMPERATURE` und schreibt einen DPT-7-Wert in das 6-Byte-Objekt *Farbe Status*. Bei RGBW soll die Farbtemperatur übersprungen werden; in der ETS bleibt „Farbtemperatur aktivieren“ bis dahin ausgeschaltet.
- Die HCL-Helligkeit **auf einer brennenden Lampe** (`setBrightness`, nur mit *HCL anwenden* = „Auch wenn eingeschaltet ist“) läuft nicht über `rgbwSetLevel`/`setDimmState`; *Farbe Status* und der letzte Pegel werden dort nicht nachgezogen. Beim Einschalten (H2) ist das nicht der Fall.
- Die Kurve „Zeittabelle“ ist nicht getestet.

### 5.4 Einstellbare Dimmkurve (Idee, nicht RGBW-spezifisch)

**Status: Idee, zurückgestellt.** Das Gateway rechnet *Prozent* heute fest mit der logarithmischen DALI-Standardkurve (`DaliHelper::percentToArc`/`arcToPercent`: `Level = 253/3 · (log10(Prozent) + 1) + 1`; 1 % → 85, 2 % → 111, 20 % → 195, 50 % → 229, 80 % → 246, 100 % → 254). Die Prozentzahl ist damit die Lichtmenge, und das Auge sieht 20 % schon wie etwa die Hälfte. Vorschlag: ein ETS-Parameter (allgemein, nicht je Kanal) mit z. B. „DALI logarithmisch (heute, Standard)“ und „wahrnehmungsgleich“ (z. B. nach CIE L*: 1 % → 5, 10 % → 90, 20 % → 125, 50 % → 192, 80 % → 233). Die vier Funktionen in `DaliHelper.cpp` bekommen dafür einen Modus; alle Stellen (Min/Max, Einschaltwerte, Status, HCL, RGBW-Slider) laufen schon über diese Funktionen. Zu beachten: Der Standard muss die heutige Kurve bleiben, der Parameter braucht freie Bits und eine neue ETS-Version, und die Kurve im Treiber (DT6 *Dimmkurve auswählen*) darf nicht gleichzeitig umgestellt werden. Gehört nicht in den ersten Pull Request.

### 5.5 Erledigte Punkte der früheren Wunschliste

- **Treiberzustand beim Start übernehmen:** erledigt für Einzelkanäle (Pegel und Ein/Aus seit v8, Farbe seit v10, B12). Offen: Gruppen und Broadcast, sehr viele RGBW-Kanäle (vier DALI-Abfragen pro Kanal beim Start), ein Treiber ohne je gesetzte Farbe (Antwort 255 oder 0 ungeprüft), das Verhalten der Report-Werte **während** eines Fades.
- **Kosmetik: Startwerte „letzter Wert“:** `_lastDayValue = 100` / `_lastNightValue = 10` (schon im Ausgangscode) werden als DALI-Pegel gelesen (≈ 1,5 % bzw. ≈ 0,1 %); in v8 für RGBW auf 100 % und 10 % als Pegel umgerechnet (Standardwert 254 bestätigt, Test 1). Betrifft nur das erste Einschalten nach einem Neustart mit *Einschaltwert = 0*, wenn der Treiber beim Start aus war.
- **Status nach dem Stopp eines relativen Dimmens:** erledigt (v9, B11).

### 5.6 Noch zu testen (Reihenfolge)

1. Treppenhauslicht mit RGBW
2. zweiter RGBW-Kanal und mehrere Kanäle zugleich (Auslastung der DALI-Warteschlange, Farbsequenz ≈ 300 ms pro Kanal, Reihenfolge)
3. Gruppen, jeweils eine Gruppe nach der anderen
4. Broadcast Ein/Aus, verzögertes Broadcast-Aus, Sperre, Tag/Nacht-Einschaltwerte, KNX-Szenen mit Farbe, im Treiber gespeicherte Szenen (Farbe gilt danach als unbekannt)
5. Regression der anderen Farbtypen (RGB, HSV, TW, xyY) und anderer Kanaltypen (Testfall 14)
6. *Dimmen absolut* aus Aus bei gespeicherter Farbe (nicht Schwarz/Weiß); weitere Mischungen mit Weiß bei brennender Lampe
7. Test durch andere Anwender (insbesondere Befehl 237 bei anderen Treibern, B8)
8. Spätere Punkte aus 5.1 und 5.2

### 5.7 Vor dem Pull Request

`RGBW_DIAG`-Diagnosecode entfernen oder deaktiviert lassen; offene Fragen an den Maintainer klären (Abschnitt 6 der [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md), u. a. das Log `Dali: unhandled KO: 1` beim Heartbeat, die Warnung „loop took longer than usual“, die knappe KNX-Reserve von 63 Byte, die Idee der einstellbaren Dimmkurve); (erledigt am 17.11.: veraltete Kommentare zu B2/B3 korrigiert); die Gegenüberstellung aktualisieren (u. a. Abschnitt 5 zur Updatefähigkeit, B16, und Abschnitt 6). **PR erst nach ausdrücklichem Go.**

---

## 6. Testanleitung

### 6.1 Aufbau

**Voraussetzungen:** Windows mit Git, VS Code + PlatformIO, ETS (für die Signatur des Producers) und **OpenKNXproducer ≥ 3.11.6** (für dreiteilige `ApplicationVersion`, z. B. `0.14.0`). DALI-Gateway-Hardware `GW-REG1-Dali` (RP2040), DALI-Netzteil und ein DT8-RGBW-Treiber (hier getestet: DA4-M, Kippschalter auf *RGBW*).

**1. Firmware-Projekt mit diesem Fork einrichten**

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

**2. ETS-Applikation und Header erzeugen**

In `GW-REG1-Dali/src/DaliGateway.xml` die `ApplicationVersion` erhöhen (aktuell `0.14.0`; Teil `x.y` = ETS-Version, `z` = Firmware-Revision; `ReplacesVersions` nennt die ersetzten Versionen), dann:

```powershell
cd ..\GW-REG1-Dali
OpenKNXproducer create --Debug -h include/knxprod.h src/DaliGateway
```

Dabei entsteht `include/knxprod.h` mit `PT_colorType_RGBW` (= 4) und den neu berechneten Parameter-Offsets sowie `src/DaliGateway.knxprod` für die ETS. `include/knxprod.h` nicht von Hand ändern, sie wird immer vom Producer erzeugt.

**3. ETS importieren**

`.knxprod` in die ETS importieren. Ein vorhandenes Gerät mit einer Version aus `ReplacesVersions` per *Aktualisieren* umstellen (B16), sonst das Gerät **neu anlegen**. Danach parametrieren: Kanal auf *Gerätetyp DT8 – Farbe*, *Farbe ansteuern per: RGBW*.

**4. Firmware flashen**

Environment `develop_REG1_USB_V1` bauen (VS Code / PlatformIO: *Build*). Ist kein automatischer Upload möglich (picotool braucht unter Windows den WinUSB-Treiber via Zadig), das Gerät mit gedrückter BOOTSEL-Taste anstecken und `.pio/build/develop_REG1_USB_V1/firmware.uf2` auf das Laufwerk `RPI-RP2` kopieren. Danach das Gerät per ETS programmieren (Vollständiger Download).

**Archivstand `v12`:** Im Ordner `OFM-Dali` `git fetch rgbw`, dann `git checkout -B v1dev-v12 rgbw/v1dev-v12`; gleiche Umgebung bauen und flashen. Zurück auf den aktuellen Stand: `git checkout -B v1dev rgbw/v1dev`. Die Version steht im Log bei jedem Farbtelegramm (`Color absolute [v13aa]` bzw. `[v12]`). Hinweis: `v1dev-v12` enthält den Upstream-Fix #7 nicht.

**Praktische Hinweise:** Testkanal A0, Gruppenadressen 20/0/0 bis 20/0/7 (Schalten, Dimmen absolut, Farbe RGBW, Farbe RGBW Status, Schalten Status, Dimmen Status, Dimmen relativ, HCL Kurve). Nach Änderungen am Speicherlayout die ETS-Applikationsversion erhöhen und in der ETS *Aktualisieren* wählen. *Nur Testaufbau, nichts am Gateway selbst:* Die HCL braucht eine gültige Uhrzeit aus KO 4 (GA 0/0/4, DPT 19.001), normalerweise von einem KNX-Zeitgeber. Fällt der Zeitgeber aus, meldet die HCL `Ungueltige Sonnenstandsdaten`. Zum Testen die Uhrzeit von Hand per `knx.send` (Home Assistant) an 0/0/4 schicken (8 Byte: Jahr−1900, Monat, Tag, Wochentag·32+Stunde, Minute, Sekunde, `0x20`, `0x80`; als Dezimalbytes, z. B. `[126, 11, 15, 244, 21, 30, 32, 128]`).

### 6.2 Testfälle

1. **DALI-Adressierung** (ETS: *Dali-GW → Adressierung*): Treiber scannen, lange Adresse eintragen, Kurzadresse `0` setzen (Kurzadresse n gehört zu Kanal An).
2. Gruppenadressen auf *Schalten*, *Dimmen absolut*, *Farbe RGBW* (DPT 251.600) und die zugehörigen Status-Objekte legen, Applikation programmieren.
3. In der ETS im Gruppenmonitor per *Wert schreiben* auf *Farbe RGBW* senden. Die Häkchen vor R/G/B/W sind die Gültigkeitsbits.

| # | Senden (Kanal ist zu Beginn **aus**, sofern nicht anders angegeben) | Erwartung | Stand |
|---|----------------------------------------------------|-----------|---|
| 1 | *Schalten* Ein, *Dimmen absolut* 50 % / 100 %, *Schalten* Aus | Leuchte reagiert; Status-Objekte folgen | **bestanden** (v6, v8) |
| 2 | Alle vier Häkchen, R 100 %, G/B/W 0 % | Leuchte **schaltet ein**, rein Rot; Status `FF 00 00 00 00 0F`, Schalten Status *Ein* | **bestanden** (v6) |
| 3 | Alle vier Häkchen, G 100 %, übrige 0 % | rein Grün | **bestanden** (v6) |
| 4 | Alle vier Häkchen, B 100 %, übrige 0 % | rein Blau | **bestanden** (v6) |
| 5 | Alle vier Häkchen, W 100 %, übrige 0 % (der wichtigste Test!) | rein Weiß; Status `00 00 00 FF 00 0F` | **bestanden** (v6) |
| 6 | Nur Häkchen **W**, 30 % (Leuchte an) | nur Weiß ändert sich, RGB bleibt wie zuvor | **bestanden** (v6) |
| 7 | Nur Häkchen **R**, 100 % | nur Rot ändert sich, Weiß/Grün/Blau bleiben | **bestanden** (v6) |
| 8 | Kein Häkchen / `… 00` in Byte 5 | **keine** gültigen Kanäle: Licht darf sich nicht ändern | **bestanden** (v6) |
| 9 | Alle vier Häkchen, alle 0 % | Leuchte **schaltet aus**; Schalten Status *Aus*, Dimmen Status 0 % | **bestanden** (v6) |
| 10 | Nach 9: *Schalten* Ein | Leuchte geht mit der **zuletzt gesetzten Farbe** an (nicht schwarz) | **bestanden** (v6) |
| 11 | Nach 9 (zuletzt war z. B. Weiß gesetzt): Farbtelegramm R 100 % | Leuchte schaltet mit **Rot** ein, nicht mit der alten Farbe (Weiß); ein zweites Senden ist nicht nötig | **bestanden** (v6) |
| 12 | *Dimmen absolut* 0 %, danach ein Farbtelegramm | Leuchte schaltet wieder ein (Merker bleibt nicht auf *Ein* hängen) | **bestanden** (v6) |
| 13 | Rot gesetzt, dann *Dimmen absolut* 25 % | Rot mit 25 % Helligkeit | **bestanden** (durch Nachtests mit 50 %, 100 %, 10 % abgedeckt) |
| 14 | RGB/HSV/TW/xyY-Kanäle nachtesten | verhalten sich wie vorher | **offen** |

### 6.3 Weitere Hinweise

Rohdaten-Beispiele (Byte 0–5, z. B. für Home Assistant `knx.send` mit `payload: [0, 0, 0, 255, 0, 15]`): rein Weiß `00 00 00 FF 00 0F`, nur Weiß ändern (50 %) `00 00 00 80 00 01`, nur Rot ändern `FF 00 00 00 00 08`, alles aus `00 00 00 00 00 0F`.

Die Debug-Ausgabe (OpenKNX-Logging, Level *Debug*, serielle Konsole 115200 Baud) zeigt pro Telegramm `Got RGBW: …`, `Send RGBW: …`, `RGBW: switching on with …` bzw. `RGBW: all channels 0 -> switching off`. Falls Weiß nicht leuchtet: Treiber-Datenblatt prüfen (RGBW-Mapping, Anzahl Farbkanäle) und mit dem Log vergleichen.

Optional, ohne Hardware: Host-Test der Logik

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

---

## 7. Pull Request

Nach erfolgreichem Test kann aus `v1dev` dieses Forks ein Pull Request gegen `OpenKNX/OFM-Dali` (Basis-Branch **`v1dev`**) erstellt werden. Vorschlag für die Beschreibung: Zusammenfassung dieser README + [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md) + Testergebnis (getesteter Treiber, Auffälligkeiten). **PR erst nach ausdrücklichem Go** (siehe 5.7).
