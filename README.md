# OFM-Dali – RGBW-Erweiterung (Branch `v1dev` dieses Forks, Stand `v13`)

Dieser Fork ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse pro Leuchte (RGBW-Modus). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (abgezweigt bei Commit `6e7aca5`; am 17.11.2026 der Upstream-Stand `a120578` mit Fix #7 `eed5a33` „export group settings“ hereingemergt, er korrigiert vier vertauschte HCL-Parameternamen im Gruppen-Template, Logik und Speicherlayout unverändert). Das Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY bleibt unverändert.

> **Stand (17.11.2026):** Branch `v1dev` = Firmware-Stand **`v13`** (Build `develop_REG1_USB_V1`, Projekt `GW-REG1-Dali`), ETS-Applikation: Gegenüber `v12` ist nur das Gruppen-Template durch den Upstream-Fix #7 geändert (vier HCL-Parameternamen); die RGBW-Ansteuerung von `v13` ändert an der ETS nichts (keine neuen Parameter oder Objekte, gleiche Speicherbelegung). Die Applikation muss neu erzeugt werden; vor dem Update die Applikationsversion prüfen (siehe 6.1). `v13` steuert RGBW **wie den bestehenden RGB-Code** an (jedes Telegramm = ein DALI-Befehl = ein Fade, keine Busabfrage im Betrieb, kein Warten auf das Ende eines Fades) und weicht nur dort ab, wo der Treiber mit seiner einzigen Fade-Engine es verlangt: Sammelfenster, temporäre Farbe + `DAPC` in einem Fade (automatische Aktivierung) und für Farbe allein bzw. Helligkeit allein die Modi A/B/C, siehe [1.5](#15-ein--und-ausschalten) und [5.1](#51-ansteuerung-in-v13-konzept-und-testplan). **`v13` ist noch nicht kompiliert und nicht auf Hardware getestet**; als Nächstes steht der Testplan in 5.1 an. Für diesen Test ist die Diagnose `RGBW_DIAG` vorübergehend eingeschaltet (Build-Tag `v13-diag`, siehe 1.7); sie wird in der finalen Version entfernt.
>
> Der Vorgänger **`v12`** (Verfahren „Farbe bei Pegel 0, per `QUERY_STATUS` auf das Ende des Farb-Fades warten, dann Pegel“) ist mit einem echten Treiber (DA4-M, 4-Kanal-Konstantspannungs-Dimmer im Modus RGBW) am `GW-REG1-Dali` ausgiebig getestet. Er liegt archiviert im Branch **`v1dev-v12`** und wird nicht mehr verändert; er ist der Rückfall, falls `v13` am Treiber nicht funktioniert. Alle Testergebnisse in dieser README mit Version v6–v12 stammen von diesem Vorgänger. Ein Zwischenentwurf `v13aa` (automatische Aktivierung nur beim Einschalten) wurde nie gebaut oder getestet und ist durch `v13` ersetzt.
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
- Ein Farbtelegramm an einen **ausgeschalteten** Kanal schaltet ihn **nicht** ein (wie bei RGB): Die Farbe geht an den Treiber, die Lampe bleibt aus, der Status bleibt *Aus*. Ausnahme: Helligkeit aus dem RGBW-Wert (Slider-Modus). **R = G = B = W = 0 schaltet aus** (siehe [1.5](#15-ein--und-ausschalten)).
- Ansteuerung des Treibers über DALI DT8: **RGB-Pegel + Weiß-Pegel**, danach `ACTIVATE` oder ein Pegelbefehl, der die Farbe mit übernimmt (siehe [1.3](#13-dali-sequenz-je-farbänderung)). Die Einstellung *„Farbe übertragen per“* (RGB/xy) spielt bei RGBW keine Rolle.
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

> **Hinweis zu Gruppen:** Für einen **Gruppenkanal** behandelt das Gateway jedes Telegramm als **vollständige** Farbe: Kanäle ohne Gültigkeitsbit werden **0**, sie behalten nicht den vorherigen Wert. Das ist kein Fehler, das Ergebnis kann aber anders aussehen als bei „nur diesen Kanal ändern“. Grund: Ein EVG kann in mehreren Gruppen liegen oder einzeln angesteuert worden sein, die im Gateway gespeicherte Gruppenfarbe (die zuletzt an die Gruppe gesendete) ist dann nicht die tatsächliche Farbe aller Mitglieder (siehe [1.6](#16-gruppen)). Beispiel: Gruppe 1 und 2 haben je vier EVGs, zwei davon liegen in beiden. Rot an Gruppe 1 und Blau an Gruppe 2 mit vollständigen Werten (`FF 00 00 00` bzw. `00 00 FF 00`) hat ein eindeutiges Ergebnis; mit „nur Blau gültig“ hinge es vom Stand der einzelnen EVGs ab. Bei **Einzelkanälen** darf weiter einzeln editiert werden (nicht gültige Kanäle behalten ihren Wert).

### 1.3 DALI-Sequenz (je Farbänderung)

1. `DTR0=R, DTR1=G, DTR2=B` → `SET TEMPORARY RGB DIMLEVEL` (DT8, Befehl 235)
2. `DTR0=W, DTR1=255, DTR2=255` → `SET TEMPORARY WAF DIMLEVEL` (DT8, Befehl 236; 255 = MASK = Amber/Freecolour unverändert)
3. Übernahme der beiden temporären Werte, je nach Fall:
   - **`ACTIVATE`** (Befehl 226), wenn nur die Farbe geändert wird und kein Pegelbefehl mitgehen soll (Farbe an eine ausgeschaltete Lampe; Farbe allein an eine brennende Lampe in Modus C und in Modus B außerhalb des Zeitfensters bzw. bei Gruppen), oder
   - ein **Pegelbefehl** direkt dahinter, **ohne** `ACTIVATE`: `DAPC` mit dem Zielpegel, in Modus A bei einer Farbe allein `GO TO LAST ACTIVE LEVEL` (Befehl 10, DALI-2). Der Pegelbefehl übernimmt die temporäre Farbe mit (automatische Aktivierung nach IEC 62386-209), Farbe und Pegel faden gemeinsam in **einem** Fade.

In Modus A geht bei einer Helligkeit allein statt der Schritte 1–2 `COPY REPORT TO TEMPORARY` (DT8, Befehl 238) vor den `DAPC`: Der Treiber kopiert seine eigene Farbe in die temporären Werte.

Der Wert 255 wird – wie schon bei RGB – auf 254 begrenzt, weil 255 bei DALI „MASK“ (nicht ändern) bedeutet. Die Farbsequenz braucht rund 310 ms Buszeit. Wann welcher Fall gilt, steht in [1.5](#15-ein--und-ausschalten); der Grund ist die **eine** Fade-Engine des Treibers ([Abschnitt 2](#2-befunde-zum-treiber-da4-m-mit-status), B1).

### 1.4 Helligkeit: zwei Modi

| Einstellung *Helligkeit bei RGBW* | Verhalten |
|-------------|-----------|
| **Über Objekt „Dimmen absolut“** (Standard) | Farbe = nur Mischung, Helligkeit nur über *Dimmen absolut* / *Schalten* / *Dimmen relativ*. Ein Farbtelegramm mit kleineren Beträgen (z. B. 50 % statt 100 % Rot) dimmt eine brennende Lampe nicht (am DA4-M gemessen, Befund B4). Visualisierungen, die RGBW-Werte „helligkeitsskaliert“ senden, brauchen hier zusätzlich eine Adresse für die Helligkeit (z. B. `brightness_address` in Home Assistant) auf *Dimmen absolut*. |
| **Aus dem RGBW-Wert (größter Kanal = Helligkeit)** („Slider-Modus“, v12) | Der größte der vier Kanäle ist die Helligkeit (255 = 100 %), das Verhältnis der Kanäle ist die Farbmischung. Für Clients mit nur einem Objekt (z. B. ein Home-Assistant-KNX-Licht ohne `brightness_address`). |

Im Slider-Modus gilt:

- **Farbtelegramm** `R G B W`: Helligkeit = größter Kanal ÷ 255 (linear in Prozent, dann wie *Dimmen absolut* auf die DALI-Kurve umgerechnet). Der Treiber bekommt die auf 255 normierte Mischung als temporäre Farbe und die Helligkeit als `DAPC` direkt dahinter, beides in einem Fade (ab `v13`; in `v12` kam der Pegel erst nach dem Ende des Farb-Fades). Beispiel: `80 00 00 80` = Rot-Weiß mit etwa 50 %.
- **Aus → Farbtelegramm:** schaltet mit dem Pegel aus dem RGBW-Wert ein (nicht mit dem Einschaltwert Tag/Nacht). **Alle Kanäle 0:** aus.
- **Status Farbe (RGBW)** zeigt Mischung × aktueller Pegel und folgt auch *Dimmen absolut*, *Schalten* und *Dimmen relativ*. Ein eben empfangener Farbwert wird unverändert zurückgemeldet.
- *Dimmen absolut* / *Schalten* / *Dimmen relativ* funktionieren weiter und stellen den Pegel direkt. Ein Client mit Helligkeitsadresse sollte den Modus *Über Objekt* benutzen.
- Szenen im Treiber, HCL und die anderen Farbtypen sind nicht betroffen.

Home Assistant sendet bei einer Leuchte **mit** Helligkeitsadresse zuerst die Farbe und ≈ 20 ms später die Helligkeit (*Dimmen absolut*), bei einer Leuchte **ohne** Helligkeitsadresse nur die Farbe (Helligkeit in den Farbwert eingerechnet).

### 1.5 Ein- und Ausschalten

**Grundregel (`v13`):** wie RGB, jedes Telegramm wird zu einem DALI-Befehl mit einem Fade, ohne Busabfrage und ohne Warten auf das Ende eines Fades. Der Treiber hat nur eine Fade-Engine (B1); damit ein Befehl einen laufenden Fade nicht halb stehen lässt, trägt er wo nötig **beide Ziele** (temporäre Farbe + `DAPC`), dann fadet der Treiber von seinem aktuellen Stand zu beiden Zielen.

**Sammelfenster** (`aawin`, Standard 80 ms): Nach dem ersten RGBW-Telegramm wartet das Gateway 80 ms, ob ein zweites folgt (Home Assistant schickt Farbe und Helligkeit mit etwa 20 ms Abstand, in beliebiger Reihenfolge). Danach geht **ein** Befehl mit den neuesten Werten raus. Das ist die einzige feste Wartezeit.

| Ereignis | Verhalten bei RGBW-Kanälen (`v13`) |
|----------|----------------------------|
| **Ausschalten** (*Schalten* Aus, *Dimmen absolut* 0 %, Farbe mit allen Kanälen 0, *Dunkler* bis zum Minimum) | Sofort `DAPC 0`, die Leuchte fadet mit der Fade-Zeit des Treibers aus; ein laufendes Sammelfenster wird verworfen. Ein Umweg über die Farbe 0/0/0/0 wird **nicht** gemacht, weil der DA4-M dafür Weiß statt Schwarz zeigt (B3). Die zuletzt gesetzte Farbe bleibt im Gateway und im Treiber gespeichert. |
| **Einschalten** (*Schalten* Ein, *Dimmen absolut* > 0) | Nach dem Sammelfenster: gespeicherte Farbe als temporärer Wert + `DAPC` mit dem Einschaltwert, **ein** Fade. Einschaltwert: Tag/Nacht-Einschaltwert, bei 0 der letzte Wert (bzw. der Wert von *Dimmen absolut*). Kennt das Gateway keine Farbe (z. B. nach einem Neustart vor dem ersten Farbtelegramm), geht nur der `DAPC` raus, und der Treiber leuchtet in seiner eigenen, zuletzt aktiven Farbe. |
| **Farbtelegramm an einen ausgeschalteten Kanal** | Wie RGB: Farbe + `ACTIVATE` sofort, der Treiber übernimmt sie bei Pegel 0 unsichtbar (B2), die Lampe **bleibt aus**, *Schalten Status* bleibt *Aus*, nur *Farbe RGBW Status* zeigt die neue Farbe. Das nächste Einschalten schickt die Farbe noch einmal mit dem `DAPC` mit, ein abgebrochener Farb-Fade schadet also nicht. **Ausnahme Slider-Modus** (1.4): Dort steckt die Helligkeit im Farbwert, ein Farbwert > 0 schaltet ein (temporäre Farbe + `DAPC`). |
| **Farbtelegramm an einen eingeschalteten Kanal** | Nach dem Sammelfenster: Kam im Fenster auch eine Helligkeit, gehen temporäre Farbe + `DAPC` zusammen raus (ein Fade). Sonst je nach Modus für **Farbe allein** (siehe unten): **A** temporäre Farbe + `GO TO LAST ACTIVE LEVEL` (der Treiber nimmt seinen eigenen Zielpegel, Einzelkanäle und Gruppen); **B** bei einem Einzelkanal, dessen eigener Pegel-Fade noch laufen kann, temporäre Farbe + `DAPC` mit dem Zielpegel, den das Gateway selbst zuletzt gesendet hat, sonst Farbe + `ACTIVATE`; **C** Farbe + `ACTIVATE`. Die Helligkeit bleibt unverändert. |
| **Helligkeit an einen eingeschalteten Kanal** (*Dimmen absolut*, *Schalten* Ein) | Nach dem Sammelfenster je nach Modus für **Helligkeit allein**: **A** `COPY REPORT TO TEMPORARY` + `DAPC` (der Treiber nimmt seine eigene Farbe mit, ein laufender Farb-Fade läuft zu seinem Ziel weiter); **B** `DAPC`, und kann der eigene Farb-Fade noch laufen, die Farbe des Gateways als temporären Wert davor; **C** `DAPC`. Die Farbe bleibt erhalten (Rot, dann 25 % = Rot mit 25 %). |
| **Dimmen relativ** | Wie RGB, ohne Warten: `UP`/`DOWN` alle 200 ms, *Heller* aus Aus mit `RECALL MIN` (die Leuchte kommt in der im Treiber gespeicherten Farbe). Ein offenes Sammelfenster wird vorher gesendet. Ein `UP`/`DOWN` direkt nach einer Farbänderung kann den Farb-Fade abbrechen (bewusst hingenommen). *Dunkler* bis zum Minimum schaltet aus und beendet das Dimmen (B10). |
| **Sperre, Szene** | verwerfen ein offenes Sammelfenster und den gemerkten Zielpegel; die Sperre sendet ihren Wert wie bisher als `DAPC`. |

**Farbe allein / Helligkeit allein bei brennender Lampe: Modi A, B, C** (Konsole `p3mode`, Standard `auto`): Ein einzelner Befehl würde einen laufenden Fade des Treibers halb stehen lassen (B1). Deshalb trägt er das andere Ziel mit:

| Modus | Farbe allein | Helligkeit allein | gilt für | Voraussetzung |
|---|---|---|---|---|
| **A** (Standard bei DALI-2-Treibern) | temporäre Farbe + `GO TO LAST ACTIVE LEVEL` | `COPY REPORT TO TEMPORARY` + `DAPC` | Einzelkanäle und Gruppen, immer, ohne Zeitprüfung | DALI-2; der Treiber übernimmt die temporäre Farbe auch bei diesen Befehlen; „Report“ ist während eines Farb-Fades die Zielfarbe (5.1, T15–T19) |
| **B** (Standard bei DALI-1-Treibern) | temporäre Farbe + `DAPC` mit dem eigenen Zielpegel, solange der eigene Pegel-Fade laufen kann, sonst `ACTIVATE` | temporäre Farbe (des Gateways) + `DAPC`, solange der eigene Farb-Fade laufen kann, sonst `DAPC` | „Farbe allein“ nur Einzelkanäle (Gruppen: `ACTIVATE`) | jeder DT8-Treiber mit automatischer Aktivierung |
| **C** (wie RGB) | `ACTIVATE` | `DAPC` | alle | keine; ein laufender Fade kann halb stehen bleiben |

In Modus A kommen Pegel bzw. Farbe vom **Treiber selbst**, das Gateway muss sie nicht kennen; das stimmt auch nach Szenen, relativem Dimmen, Befehlen anderer DALI-Master und bei Gruppenmitgliedern mit unterschiedlichen Farben.

**Zeitprüfung in Modus B:** Das Gateway merkt sich bei jedem eigenen `DAPC` an einen RGBW-Kanal Zeitpunkt und Zielpegel und bei jeder eigenen Farbe den Zeitpunkt, nur im RAM (9 Byte pro Kanal, bei 64 Kanälen und 16 Gruppen unter 1 kB). Ein Fade „kann noch laufen“, wenn der Befehl weniger als 310 ms (Buszeit einer Farbsequenz) + Fade-Zeit + 100 ms her ist. Es gibt dafür keine Abfrage auf dem Bus. Der Pegel wird nie geschätzt: Gesendet wird nur ein Zielpegel, den das Gateway selbst kurz vorher gesendet hat. Verworfen wird der gemerkte Zielpegel bei allem, was den Pegel ohne festes Ziel ändert (Start von relativem Dimmen, Ausschalten, Szene, Sperre, Gruppen- und Broadcast-Befehle, HCL-Helligkeit) und sobald die Prüfung ihn als abgelaufen erkennt (so bleibt auch der Überlauf von `millis()` nach ≈ 49,7 Tagen ohne Folgen).

**Abfragen beim Start:** Das Gateway fragt **einmal** die Fade-Zeit (`QUERY_FADE_SPEEDS`) und die DALI-Version (`QUERY VERSION NUMBER`, ab `0x08` = DALI-2) ab, jeweils am ersten RGBW-Kanal bzw. am Referenz-EVG einer RGBW-Gruppe, und nutzt die Werte für **alle** RGBW-Kanäle. Alle RGBW-Treiber einer Anlage sollten deshalb vom selben Typ sein und dieselbe Übergangszeit haben (siehe Abschnitt 4). Ohne Antwort gelten 1000 ms bzw. DALI-1 (Modus B).

**Änderungen, die für alle Kanal- und Farbtypen gelten** (nicht nur RGBW, daher Regressionstest 14):
- `koHandleDimmAbs`: Der interne Ein/Aus-Merker (`currentState`) folgt jetzt dem Wert von *Dimmen absolut* (0 % = aus). Vorher blieb er nach „Dimmen auf 0“ auf *Ein* stehen, und ein folgendes Farbtelegramm wurde ignoriert (Treppenhauslicht-Kanäle ausgenommen).
- Nach dem Stopp eines relativen Dimmens wird der Pegel einmal (≈ 300 ms danach) per `QUERY_ACTUAL_LEVEL` abgefragt und gemeldet (B11).
- Die Startwerte für „Einschalten mit letztem Wert“ sind als DALI-Pegel 100 % bzw. 10 % gesetzt statt der Zahlen 100 und 10 (siehe 5.5).

Hinweis zum Ausgangscode: Bei den anderen Farbtypen (HSV, RGB, TW, xyY) schickt ein Farbtelegramm an einen ausgeschalteten Kanal die Farbe sofort an den Treiber und setzt die KNX-Statusobjekte auf *Ein* mit dem Tag-Einschaltwert (`// TODO get real`), obwohl die Lampe aus bleibt. RGBW verhält sich beim Treiber genauso, lässt den Status aber auf *Aus*. Die anderen Farbtypen wurden bewusst **nicht** angefasst (siehe [5.2](#52-schalter-farbtelegramm-bei-ausgeschaltetem-kanal)).

### 1.6 Gruppen

*(Hardware-Tests für Gruppen stehen noch aus.)*

- **Immer die komplette Farbe** (siehe Hinweis in 1.2). Der Treiber bekommt immer alle vier Kanäle in einer Sequenz; ein Telegramm, dessen gültige Kanäle alle 0 sind, schaltet die Gruppe aus.
- **Einschalten und das zusammengeführte Paar** (Farbe + Helligkeit im Sammelfenster) laufen über temporäre Farbe + `DAPC`. Das braucht den alten Zustand der Mitglieder nicht und ist damit auch bei EVGs in mehreren Gruppen eindeutig.
- **Farbe allein / Helligkeit allein** an eine brennende Gruppe: In **Modus A** wie bei Einzelkanälen (`GO TO LAST ACTIVE LEVEL` bzw. `COPY REPORT TO TEMPORARY`), jedes Mitglied nimmt seinen eigenen Pegel bzw. seine eigene Farbe. Offen: Ein einzeln ausgeschaltetes Mitglied einer „eingeschalteten“ Gruppe würde durch `GO TO LAST ACTIVE LEVEL` eingeschaltet (T18). In **Modus B** geht eine Farbe allein an eine Gruppe mit `ACTIVATE` (wie RGB), weil Mitglieder auf unterschiedlichen Pegeln stehen können und ein `DAPC` mit dem Gruppenpegel sie alle auf diesen Pegel ziehen würde; Restrisiko wie bei RGB.
- **Farbe beim Start:** Das Gateway liest beim Start keine Farbe mehr (auch nicht für Gruppen); die Gruppenfarbe ist bis zum ersten Farbtelegramm unbekannt.
- **Referenz-EVG:** Pegelabfragen (relatives Dimmen, Status) und die einmaligen Abfragen von Fade-Zeit und DALI-Version (falls die Gruppe der erste RGBW-Kanal ist) gehen an das Dimm-Referenz-EVG der Gruppe.
- Ein Gruppenbefehl verwirft bei den Mitgliedern ein offenes Sammelfenster und den gemerkten Zielpegel.

### 1.7 Serielle Konsole (Feintuning und Diagnose)

`p3mode auto|0|1|2` (Modus für Farbe allein / Helligkeit allein bei brennender Lampe, siehe 1.5: `auto` = A bei DALI-2-Treibern, sonst B; `1` = A; `2` = B; `0` = C wie RGB; Standard `auto`), `aawin nnn` (Sammelfenster in ms, 0–1000, Standard 80, 0 = jedes Telegramm sofort senden); beide gelten bis zum Neustart. `aastat` (liest bei allen konfigurierten RGBW-Kanälen die DT8-Einstellung *automatische Aktivierung* aus, Gear Features/Status Bit 0), `aaon [yy]` (soll die automatische Aktivierung **dauerhaft** im Treiber einschalten, ohne `yy` bei allen konfigurierten RGBW-Kanälen; einmal ausführen genügt. Nach DALI muss ein Konfigurationsbefehl zweimal kurz hintereinander ankommen, das erledigt `aaon` selbst, fragt danach nach und versucht bei Bedarf automatisch eine zweite Sendeform. Nach unserem Verständnis der Norm behält der Treiber die Einstellung auch stromlos; ob sich die Einstellung am DA4-M so setzen lässt, ist **ungetestet**, T0). Beide nur von Hand, das Gateway fragt oder schreibt diese Einstellung nie selbst. `getCol yy [sel]` (fragt Farbtyp-Merkmale, Farbstatus und die Farbwerte des Treibers ab, ändert nichts; ohne Selektor: Report 233–236, aktuell 9–12, temporär 201–204 je R, G, B, W). Die Befehle `onmode`, `settle`, `margin` und `blackoff` aus `v12` gibt es in `v13` nicht mehr.

Mit `#define RGBW_DIAG` in `DaliChannel.h` zusätzlich Log jedes DALI-Frames und Abfragen `QUERY_ACTUAL_LEVEL`/`QUERY_STATUS` zu festen Zeiten nach jedem RGBW-Befehl aus dem Sammelfenster. **Für den Test von `v13` ist die Diagnose vorübergehend eingeschaltet** (Build-Tag `v13-diag`), um die automatische Aktivierung auf dem Bus zu prüfen (siehe 5.1). Nebenwirkungen: zusätzliche Frames; die Pegelabfragen setzen *Dimmen Status*, den RGBW-Status (Slider-Modus) und den „letzten Wert“ auf Zwischenwerte des laufenden Fades. Für Tests dieser Punkte das `#define` auskommentieren. **Die Diagnose wird in der finalen Version entfernt.**

### 1.8 Broadcast

| Ereignis | Verhalten mit RGBW-Kanälen (`v13`) |
|----------|----------------------------|
| **Broadcast Ein** (Objekt *Broadcast Schalten*) | `DAPC FE` (100 %) an alle, wie im Ausgangscode; Tag/Nacht-Einschaltwerte gelten dabei nicht. Danach bekommt jeder RGBW-Kanal und jede RGBW-Gruppe, die aus war und eine bekannte Farbe hat, sofort die Farbe als temporären Wert + `DAPC FE`. Der zweite `DAPC` trägt beide Ziele, der Treiber fadet von seinem Stand dorthin. |
| **Broadcast Aus** | Ein einzelnes `DAPC 0` an alle, wie im Ausgangscode. RGBW-Kanäle verwerfen nur ihr offenes Sammelfenster und ihren Zielpegel. |
| *Broadcast Dimmen absolut*, Taster am Gerät (`RECALL MAX`/`OFF`) | wie im Ausgangscode, **ohne** RGBW-Behandlung. |

Bekannte Lücken: Bei Broadcast Ein ist kurz die alte Farbe möglich, bevor der zweite `DAPC` greift; sind eine RGBW-Gruppe und ihre Mitglieder konfiguriert, gehen zwei Farbsequenzen raus. *Broadcast Dimmen absolut* und der Taster setzen weder die Farbe noch den Status der RGBW-Kanäle. Nichts davon ist auf Hardware getestet (5.1, 5.6).

---

## 2. Befunde zum Treiber DA4-M (mit Status)

Diese Tabelle fasst zusammen, was wir über das Verhalten des DA4-M (und des Gateways) gelernt haben, einschließlich der Annahmen, die später widerlegt wurden. Die Nummern **B1 bis B17** werden an anderen Stellen der README als Verweis benutzt. Messwerte und Logs stehen in der [Rohfassung](doc/README-Rohfassung-2026-11-16.md).

| Nr | Befund | Status | Belegt durch | Folge im Code |
|---|---|---|---|---|
| B1 | **Der Treiber hat nur eine Fade-Engine.** Ein Pegelbefehl bricht einen laufenden Farb-Fade ab (Mischfarbe bleibt stehen), das `ACTIVATE` einer Farbsequenz bricht einen laufenden Pegel-Fade ab. | **bestätigt** | v6-Fund: Home Assistant sendet Farbe + 20 ms später Helligkeit auf brennende Lampe, falsche Farbe (Blau + Weiß statt Rot + Weiß) bei 2 von 2 Versuchen. v8: Pegel stand bei 208 statt 235. v10-Fund: ein UP direkt nach der Farbe bricht den Farb-Fade ab. v11-Tests 1–4 bestanden. | `v7`–`v12`: Pegelbefehl wartet auf das Ende des Farb-Fades (`QUERY_STATUS`), Zielpegel wird nach dem Farb-Fade noch einmal gesendet (v9), kein UP/DOWN während des Farb-Fades (v11). **`v13`:** stattdessen Befehle mit beiden Zielen (temporäre Farbe + `DAPC`) und Sammelfenster, ohne Abfrage und ohne Warten (1.5); relatives Dimmen wartet nicht mehr |
| B2 | „Der DA4-M **ignoriert eine Farbänderung bei Pegel 0**, die Lampe leuchtet mit der alten Farbe.“ | **widerlegt** | Frühe Beobachtung vom 13.11. (Commit `ea2cc95`: nach „alle 0“ leuchtete ein rotes Telegramm mit der alten Farbe Weiß, erst das zweite Telegramm wirkte). **Gegenbelege:** Farbe bei Pegel 0 senden, auf das Ende des Farb-Fades warten (1350 ms), dann Pegel: neue Farbe sofort (v10 Test 2, v12 H2, A5, B2). Vermutliche Ursache des frühen Befunds (nicht einzeln nachgeprüft): ein sofort folgender Pegelbefehl hat den Farb-Fade abgebrochen (B1) oder die Farbe 0/0/0/0 wurde als Weiß dargestellt (B3). | Die damalige Lösung „erst einschalten, dann Farbe“ (`ea2cc95`) ist überholt. **`v13`:** Farbe an eine ausgeschaltete Lampe geht wie bei RGB mit `ACTIVATE` an den Treiber, die Lampe bleibt aus (1.5) |
| B3 | Der DA4-M zeigt für die Farbe 0/0/0/0 **Weiß** statt Schwarz. | **bestätigt** | Hardware-Tests (Stand v6 bis v12), dokumentiert in der Rohfassung unter „Ein-/Ausschaltverhalten“ | Ausschalten per `DAPC 0`. Die frühere Sequenz „Schwarz zuerst, dann Pegel 0; Pegel zuerst aus Schwarz“ (Commit `f568871`, 13.11.) ist **überholt**; die Konsolenbefehle dafür (`blackoff`, `onmode 0`) sind in `v13` entfernt |
| B4 | Der Treiber **normiert** die Farbe, die Amplitude des Telegramms geht verloren (z. B. `80 00 00 80` an brennende Lampe: Lampe unverändert, Status meldet `80 00 00 80`). | **bestätigt** | v11 Test A1 | Standard: „Farbe = Mischung, Helligkeit = Dimmen absolut“. Slider-Modus (v12) rechnet die Helligkeit selbst aus dem Farbwert |
| B5 | Der Treiber meldet seine Farbe über die **Report-Selektoren 233–236** (`QUERY_COLOUR_VALUE`), auch bei Pegel 0, er hält die Farbe also gespeichert. | **bestätigt** | v9 `getCol` (Lampe Blau, auch bei Pegel 0 dieselbe Antwort 0/0/254/0); v10 Test 1 (Neustart bei ausgeschalteter Lampe, zuletzt Blau: Gateway liest `00 00 FF 00`) | `v10`–`v12`: Farbe beim Start vom Treiber übernehmen. **`v13`:** entfernt (bei 64 RGBW-Kanälen ≈ 11 s Buszeit beim Start); die Farbe ist bis zum ersten Farbtelegramm unbekannt. `getCol` bleibt als Diagnose |
| B5a | Die Standard-Selektoren des Diagnosebefehls `getCol` in v9 (64–67, 0–2) waren falsch (Primärfarb-Koordinaten bzw. X/Y/Tc, hat ein RGBW-Treiber nicht). | **überholt** | in v10 durch 233–236, 9–12 und 201–204 ersetzt | `getCol` |
| B5b | Haltedauer einer Farbe, die **bei Pegel 0 gesendet** wurde: Die vor dem Ausschalten gesetzte Farbe bleibt nachweislich im Treiber (B5). Eine Farbe, die bei Pegel 0 gesendet wurde, haben wir nur mit höchstens etwa 1,5 s bis zum Pegel getestet. | **offen** | – | Test in `v13` (5.1, T3): Farbe an ausgeschaltete Lampe, eine Minute warten, dann einschalten. Weil das Einschalten die Farbe ohnehin mitschickt, ist die Lampe in jedem Fall richtig; der Test zeigt nur, ob der Treiber die Farbe hält |
| B6 | `QUERY_STATUS` Bit 4 („Fade läuft“) meldet auch einen **Farb-Fade bei Pegel 0**. | **bestätigt** | Logs `colour fade over (status 0x00), 1350 ms` u. a. | `v7`–`v12`: Ende-Erkennung in `loopFadePoll`. **`v13`:** nicht mehr genutzt (nur noch in der Diagnose) |
| B7 | Fade-Dauern am DA4-M (Fade-Zeit 1 s per `QUERY_FADE_SPEEDS`): Farb-Fade bei **Farbwechsel ≈ 1,35–1,58 s**, bei **unveränderter bzw. gespeicherter Farbe ≈ 0,3 s**. Der Pegel-Fade dauert etwa 1 s. | **bestätigt** | v6, v7 (1569–1571 ms), v9 (1572 ms), v10 (1350, 1476 ms), v12 (335/336 ms) | `v7`–`v12`: feste Zeiten nur als Rückfall. **`v13`:** die Fade-Zeit dient nur noch der Zeitprüfung in Modus B (1.5) |
| B8 | Das Weiß wird am DA4-M (Modus RGBW) mit **Befehl 236** (`SET TEMPORARY WAF DIMLEVEL`) gesetzt. | **bestätigt** (nur DA4-M) | alle Weiß-Tests | Befehl 237 (`RGBWAF`) bei anderen Treibern: **offen**, andere Anwender sollen es mit ihren Treibern prüfen |
| B9 | Der DA4-M-Takt für relatives Dimmen: ≈ 9 Pegelstufen alle 200 ms. Im unteren Bereich der logarithmischen Kurve wirkt das langsam (Pegel 1 bis 72 in ≈ 2 s = 0,7 %). | **bestätigt** (nicht verändert) | v7 | – |
| B10 | Die Dimmschleife (`loopDimming`) beendet das Dimmen am Minimum nicht, sondern wiederholt *Dunkler* + *Aus* bis zum Stopp (Telegrammflut, falscher Status). Struktur aus dem Ausgangscode (`git blame`); bei anderen Farbtypen ist dasselbe zu erwarten, nicht geprüft. | **behoben** (nur RGBW) v8, Test 3 | v7-Fund, v8 Test 3 | Der Schritt, der das Minimum erreicht, schaltet aus und beendet das Dimmen; *Dunkler* auf ausgeschalteter Lampe wird ignoriert |
| B11 | Nach dem Stopp eines relativen Dimmens meldete das Gateway nur den **zuletzt abgefragten** Pegel (21,6 % statt 28,6 %). Betrifft den Ausgangscode, nicht nur RGBW. | **behoben** v9 (einmal `QUERY_ACTUAL_LEVEL` ≈ 300 ms nach dem Stopp, alle Kanaltypen), auf Hardware bestätigt | v8 Test 4 (Fund), v9-Test (145 = 5,10 %, `getLvl 00` identisch). Der To-do-Eintrag „ungetestet“ in der alten README war **überholt** | – |
| B12 | Nach einem Neustart kannte das Gateway weder Farbe noch Ein/Aus-Zustand der brennenden Lampe (bis v7 Aufblitzen beim ersten Farbtelegramm). | **behoben**: Ein/Aus und Pegel seit v8 (Test 5); Farbe v10–v12 (Tests 1–3), in `v13` bewusst wieder entfernt (Abschnitt 4) | | Beim Start `QUERY_ACTUAL_LEVEL` lesen (Ein/Aus und Pegel). Die Farbabfrage (Report 233–236) ist in `v13` entfernt |
| B13 | Die Farbsequenz bricht auch einen laufenden **Pegel-Fade** ab, wenn Helligkeit **vor** der Farbe kommt (andere Visualisierungen, Logik, Szenen-Bausteine). | **behoben** v9, auf Hardware bestätigt (60 % und 20 ms später Blau: Pegel 235) | v8 (Pegel blieb bei 208), v9 | `v9`–`v12`: Zielpegel nach dem Ende des Farb-Fades noch einmal gesendet. **`v13`:** Farbe allein bei brennender Lampe → Modus A: temporäre Farbe + `GO TO LAST ACTIVE LEVEL`; Modus B: während des eigenen Pegel-Fades temporäre Farbe + `DAPC` mit dem eigenen Zielpegel (Einzelkanäle), siehe 1.5 |
| B14 | *Schalten Status* Ein wurde bei jedem *Dimmen absolut* auf eine brennende Lampe zusätzlich gesendet. | **behoben** v8 (nur RGBW, Schaltstatus nur bei Änderung), Test 2 | | – |
| B15 | Die Statusobjekte werden bei jedem relativen Schritt gesendet, obwohl „Dimmstatus Interval bei relativ“ auf *Deaktiviert (nur am Ende)* steht. Vermutung: Pegelabfrage `Got new actual level` (Ausgangscode, Mai 2026). | **offen** (nicht geprüft, nicht verändert) | v7 | – |
| B16 | Die ETS-Applikation sei **nicht in-place updatebar**, das Gerät müsse neu angelegt werden (Aussage der alten README, `ReplacesVersions` enthielt die neue Version nicht). | **überholt** | Ab 0.14 ersetzt die Applikation 0.0, 0.10, 0.12, 0.13; in der ETS *Aktualisieren* (ein deaktivierter Einzel-EVG verliert dabei seine Gruppenadressen). Für Versionen außerhalb dieser Liste gilt die alte Aussage weiter | – |
| B17 | KNX-Speicher: Build-Skript schätzt **DATA\* 36801 von 36864 Byte** (nur 63 Byte Reserve). *colorType* von 2 auf 3 Bit verbreitert und auf Byte 21 Bit 5–7 (Einzelkanal) bzw. Byte 17 Bit 2–4 (Gruppe) verschoben, *Helligkeit bei RGBW* liegt in Byte 7 Bit 5 (Einzelkanal) bzw. Byte 17 Bit 5 (Gruppe), die Parameterblöcke werden nicht größer (ein eigenes Byte hätte die Schätzung um 1 Byte über die Partitionsgröße gehoben). | **bestätigt** (Stand v12) | Build v12 | Neue Parameter nur in freie Bits legen. Ist das nicht möglich, `KNX_FLASH` über die `platformio.ini` anpassen (**offen**, noch nicht geprüft) |

---

## 3. Testübersicht

**Hinweis:** Alle Ergebnisse dieses Abschnitts stammen von `v6`–`v12`. `v13` ändert die Ansteuerung (1.5); die betroffenen Punkte werden mit dem Testplan in 5.1 neu geprüft.

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

- **Szenen/DCA-Konfiguration:** Die Szenen-Farbe (Konfigurationstransfer) kennt nur RGB. Bei RGBW-Kanälen wird dort das Weiß nicht mitgeschrieben (später als eigener Szenen-Parameter geplant). Szenen per KNX-Telegramm (Szenenobjekt) funktionieren über die im Treiber gespeicherten Szenen. Danach kennt das Gateway die Farbe der Szene nicht: Schaltet man nach einer Szene aus und wieder ein, schickt das Gateway seine zuletzt gesetzte Farbe mit und überschreibt die Szenenfarbe.
- **Gleicher Treibertyp, gleiche Übergangszeit:** Fade-Zeit und DALI-Version werden nur einmal beim Start gelesen und gelten für alle RGBW-Kanäle. Hat ein RGBW-Treiber eine längere Übergangszeit als der abgefragte, ist in Modus B die Erkennung „eigener Fade läuft noch“ bei ihm zu kurz; ist ein DALI-1-Treiber gemischt mit DALI-2-Treibern verbaut, bekommt er in Modus A Befehle, die er nicht kennt (Farbe allein bliebe wirkungslos). Empfehlung: nur einen RGBW-Treibertyp mit derselben Übergangszeit, sonst `p3mode 2`.
- **Nach einem Neustart des Gateways** ist die Farbe bis zum ersten Farbtelegramm unbekannt: Der Treiber behält seine Farbe, die Lampe leuchtet richtig, nur *Farbe RGBW Status* wird bis dahin nicht gesendet, und im Slider-Modus gilt beim Einschalten der Tag/Nacht-Einschaltwert.
- **Getrennte Objekte** (Rot/Grün/Blau/Weiß einzeln, 8 Bit) gibt es bei RGBW *nicht*, nur das kombinierte 6-Byte-Objekt. Das kann später nachgerüstet werden.
- **Andere Treiber:** Manche erwarten für Weiß statt Befehl 236 den Befehl `SET TEMPORARY RGBWAF CONTROL`/`RGBWAF DIMLEVEL` (237) bzw. ein anderes Kanal-Mapping. Beim DA4-M (Modus RGBW) geht es mit 236 (B8). Andere Treiber sind nicht geprüft.
- **HCL** wirkt bei RGBW wie bisher nur auf die Helligkeit; die HCL-Farbtemperatur ist nicht für RGBW gedacht (siehe 5.3).
- **Dimmkurve:** Das Gateway rechnet Prozent fest mit der logarithmischen DALI-Kurve (siehe 5.4).
- **ETS-Applikation:** Der Parameter *colorType* wurde verschoben (B17), die Applikation ist nur gegenüber den in `ReplacesVersions` genannten Versionen updatebar (B16).
- **Bewusst nicht angefasst** (bekannte Lücken dieser Version): Treppenhauslicht-Kanäle (Ein/Aus wie im Ausgangscode, ohne Sammelfenster), Funktionstasten/Identify/ETS-Funktionen des Moduls.

---

## 5. Offene Punkte und Entscheidungen

### 5.1 Ansteuerung in v13: Konzept und Testplan

**Status: umgesetzt (Branch `v1dev`, Build-Tag `v13-diag`), noch nicht kompiliert und nicht auf Hardware getestet.** Reihenfolge der Rückfälle: Modus A (DALI-2) → Modus B (`p3mode 2`) → Modus C wie RGB (`p3mode 0`). Kann der Treiber die automatische Aktivierung überhaupt nicht (T0/T2, auch nicht nach `aaon`), hilft keiner der drei, dann ist `v1dev-v12` der Rückfall.

**Warum der Wechsel:** `v12` löste die Fade-Abbrüche (B1) mit `QUERY_STATUS`-Abfragen und Warten auf das Ende des Farb-Fades. Das funktioniert am DA4-M, kostet aber zwei Fades nacheinander, Buszeit und Sonderfälle. `v13` folgt deshalb dem RGB-Code und nutzt die automatische Aktivierung der DT8-Norm: Eine temporäre Farbe ohne `ACTIVATE` wird vom folgenden `DAPC` übernommen, Farbe und Pegel faden in einem Fade. Ablauf und Regeln: [1.5](#15-ein--und-ausschalten).

**Voraussetzung:** Der Treiber muss die automatische Aktivierung können und sie muss eingeschaltet sein (nach unserem Verständnis der DT8-Norm eine im Treiber gespeicherte Einstellung, Gear Features/Status Bit 0). Ob sie ab Werk an ist und ob sie sich per `aaon` einschalten lässt, ist am DA4-M **ungetestet**; T0 prüft das vor allen anderen Tests, T2, T5 und T9 prüfen die Wirkung.

**Erwartete DALI-Frames** (Diagnose-Log, Zeilen `DALI … ms: 0x…`, Werte nach IEC 62386-209, am Log zu bestätigen), Kanal A0:
- temporäre Farbe: `0xA3rr`, `0xC3gg`, `0xC5bb` (DTR0–2), `0xC108` (Gerätetyp 8), `0x01EB` (temporär RGB), `0xA3ww`, `0xC3FF`, `0xC5FF`, `0xC108`, `0x01EC` (temporär WAF)
- danach entweder `0x01E2` (`ACTIVATE`) oder direkt `0x00nn` (`DAPC` mit dem Pegel) bzw. `0x010A` (`GO TO LAST ACTIVE LEVEL`), **nie** `ACTIVATE` zusammen mit einem Pegelbefehl
- Modus A, Helligkeit allein: `0xC108`, `0x01EE` (`COPY REPORT TO TEMPORARY`), dann `0x00nn`
- beim Start einmal `0x0197` (`QUERY VERSION NUMBER`, Befehl 151), Log `RGBW: DALI version 0x.. = DALI-2/DALI-1, point 3 mode …`

**Testplan `v13`** (Kanal A0, Konsole mitlesen; die Log-Zeilen beginnen mit `RGBW:`):

| # | Aktion | Erwartung |
|---|--------|-----------|
| T0 | **Vor allen Tests:** `aastat` | Log `EVG 0: gear features/status 0x.., automatic activation ON`. Steht dort `OFF`, einmal `aaon 00` und erneut `aastat`; bleibt es `OFF`, kann der Treiber keine automatische Aktivierung → Rückfall `v1dev-v12`. Ergebnis (ab Werk an oder aus) als Befund festhalten |
| T1 | Leuchte aus, Farbe senden (Modus *Über Objekt Dimmen absolut*) | Lampe **bleibt aus**, *Schalten Status* bleibt *Aus*, *Farbe RGBW Status* zeigt die neue Farbe. Log `colour for a switched-off channel, lamp stays off`, Frames mit `0x01E2` |
| T2 | danach *Schalten* Ein | **ein** Fade in der neuen Farbe. Log `level N with temporary colour (one fade)`, Frames ohne `0x01E2`, direkt danach `0x00nn`. **Zeigt die Lampe die alte Farbe, kann der Treiber keine automatische Aktivierung → Rückfall `v1dev-v12`** |
| T3 | Leuchte aus, Farbe senden, **1 Minute** warten, *Schalten* Ein | neue Farbe (B5b) |
| T4 | Home Assistant `light.turn_on` mit Farbe und Helligkeit aus „aus“ (beide Reihenfolgen, auch `knx.send` mit 20 ms Abstand) | ein Fade, Farbe und Pegel richtig; Log zeigt **einen** Befehl |
| T5 | Lampe an: Farbe + Helligkeit (HA-Paar) | ein Fade, keine Mischfarbe, Pegel richtig |
| T6 | `p3mode 2`: Lampe an, nur Farbe | Farbe fadet über (`colour (ACTIVATE)`), Helligkeit bleibt |
| T7 | `p3mode 2`: Lampe an, *Dimmen absolut* (anderer Wert), **300 ms** später nur Farbe (`knx.send`) | Lampe erreicht Zielpegel **und** neue Farbe; Log `colour with the running level target` |
| T8 | `p3mode 2`: Lampe an, nur Farbe, **300 ms** später *Dimmen absolut* | Lampe erreicht neue Farbe **und** Zielpegel, keine Mischfarbe; Log `level N with temporary colour (own colour fade may still run)` |
| T9 | Lampe an: Farbe + **unveränderte** Helligkeit (HA-Paar mit gleicher Helligkeit) | Farbe ändert sich (automatische Aktivierung auch ohne Pegeländerung) |
| T10 | Slider-Modus: Farbwert aus „aus“, Farbwert bei „an“, Farbwert 0 | schaltet mit dem Pegel aus dem Farbwert ein, ändert Farbe und Pegel in einem Fade, schaltet aus |
| T11 | Relatives Dimmen: *Heller* aus Aus, *Dunkler* bis zum Minimum, *Heller* direkt nach einer Farbe | `RECALL MIN` in der gespeicherten Farbe; am Minimum aus; direkt nach einer Farbe ist eine Mischfarbe möglich (hingenommen, beobachten) |
| T12 | `aawin 0`, HA-Paar bei Lampe an | beobachten: zweiter Befehl startet den Fade neu, Ergebnis trotzdem richtig? (Begründung für das Fenster) |
| T13 (beobachten) | Broadcast Ein / Aus mit A0 als RGBW-Kanal | Aus: sofort ein `DAPC 0`. Ein: `DAPC FE`, danach temporäre Farbe + `DAPC FE` an A0 (1.8) |
| T14 | Regression der anderen Farbtypen und Kanaltypen (Testfall 14, Diagnose dafür auskommentieren) | unverändert |
| T15 | Start-Log prüfen, dann **Report während eines Fades**: Lampe an, Farbe ändern und sofort `getCol 00 233` und `getCol 00 9` | Log zeigt DALI-2 und Modus A. **Wichtigster Test für Modus A:** Zeigt 233 (Report) schon die Zielfarbe, während 9 (aktuell) noch einen Zwischenwert zeigt, ist Report das Ziel. Zeigen beide den Zwischenwert, taugt `COPY REPORT TO TEMPORARY` nicht → `p3mode 2` |
| T16 | Modus A: Lampe an, nur Farbe (ruhende Lampe) | Farbe fadet über, Helligkeit bleibt; Log `colour with the driver's own level (go to last active level)`, Frames mit `0x010A`, ohne `0x01E2` |
| T17 | Modus A: wie T7 und T8 (Farbe 300 ms nach Helligkeit und umgekehrt) | beide Ziele werden erreicht, keine Mischfarbe, kein Rückfall auf den alten Pegel (prüft, ob „last active level“ der neue Zielpegel ist) |
| T18 | Modus A: *Dimmen relativ* (heller/dunkler), danach nur Farbe; Gruppe „an“ mit einem einzeln ausgeschalteten Mitglied, danach Farbe an die Gruppe | nach dem Dimmen bleibt die gedimmte Helligkeit (springt nicht zurück); das ausgeschaltete Mitglied bleibt aus (sonst Lücke dokumentieren) |
| T19 | Modus A: Lampe an, nur Helligkeit, auch kurz nach einer Farbe | Helligkeit ändert sich, Farbe bleibt bzw. läuft zur Zielfarbe weiter; Log `with the driver's own colour (copy report to temporary)` |

**Entscheidungsregel:** Bestehen T1–T5 und T9–T10, trägt die automatische Aktivierung, und `v13` bleibt der Stand von `v1dev`. Bestehen außerdem T15–T19, bleibt Modus A der Standard für DALI-2; sonst wird Modus B (T6–T8) Standard, und wenn auch der nicht trägt, Modus C. Kann der DA4-M keine automatische Aktivierung (T0 bleibt `OFF` oder T2, T5, T9 zeigen trotz `ON` die alte Farbe), ist `v1dev-v12` der Rückfall; das Ergebnis wird als Befund dokumentiert.

### 5.2 Schalter „Farbtelegramm bei ausgeschaltetem Kanal“

**Status: für RGBW durch `v13` erledigt, ein ETS-Schalter ist vorerst nicht geplant.** Als Standard gilt bei RGBW wie bei RGB: Farbe setzen, ohne einzuschalten, damit bestehende Installationen gleich bleiben. `v13` macht das bei RGBW so (wie RGB, Lampe bleibt aus, Status bleibt *Aus*); nur der Slider-Modus schaltet ein, weil dort die Helligkeit im Farbwert steckt. Die anderen Farbtypen (HSV, RGB, TW, xyY) bleiben unverändert, einschließlich des falschen Status *Ein* (`TODO get real`); ob das korrigiert werden soll, ist eine Frage an den Maintainer.

### 5.3 HCL mit RGBW

**Status: HCL-Korrekturen zurückgestellt (nach Absprache zunächst nicht Teil des ersten Pull Requests).** Getestet und bestanden sind H1–H4 (Abschnitt 3). Aufbau des Tests: Einzelkanal A0 (RGBW, Slider-Modus), HCL-Kurve „Sonnen Auf-/Untergang“, nur *Helligkeit* aktiv (Min 20 %, Max 100 %), *Farbtemperatur nicht aktiv*, KO *HCL Kurve* 20/0/7 verknüpft. Die Kurve braucht eine gültige Uhrzeit (KO 4, DPT 19.001, hier GA 0/0/4) und den Standort, sonst meldet das Log `Ungueltige Sonnenstandsdaten`; mit Uhrzeit läuft sie einmal pro Minute, vor Sonnenaufgang und nach Sonnenuntergang gilt der Min-Wert.

**Bekannte Lücken (offen, zurückgestellt):**

- Eine HCL-**Farbtemperatur** auf einem RGBW-Kanal ist nicht sicher: `setTemperature` sendet den DT8-Befehl `SET_TEMP_COLOUR_TEMPERATURE` und schreibt einen DPT-7-Wert in das 6-Byte-Objekt *Farbe Status*. Bei RGBW soll die Farbtemperatur übersprungen werden; in der ETS bleibt „Farbtemperatur aktivieren“ bis dahin ausgeschaltet.
- Die HCL-Helligkeit **auf einer brennenden Lampe** (`setBrightness`, nur mit *HCL anwenden* = „Auch wenn eingeschaltet ist“) läuft nicht über das Sammelfenster und `setDimmState`; *Farbe Status* und der letzte Pegel werden dort nicht nachgezogen. Seit `v13` verwirft sie nur den gemerkten Zielpegel (1.5). Beim Einschalten (H2) ist das nicht der Fall.
- Die Kurve „Zeittabelle“ ist nicht getestet.

### 5.4 Einstellbare Dimmkurve (Idee, nicht RGBW-spezifisch)

**Status: Idee, zurückgestellt.** Das Gateway rechnet *Prozent* heute fest mit der logarithmischen DALI-Standardkurve (`DaliHelper::percentToArc`/`arcToPercent`: `Level = 253/3 · (log10(Prozent) + 1) + 1`; 1 % → 85, 2 % → 111, 20 % → 195, 50 % → 229, 80 % → 246, 100 % → 254). Die Prozentzahl ist damit die Lichtmenge, und das Auge sieht 20 % schon wie etwa die Hälfte. Vorschlag: ein ETS-Parameter (allgemein, nicht je Kanal) mit z. B. „DALI logarithmisch (heute, Standard)“ und „wahrnehmungsgleich“ (z. B. nach CIE L*: 1 % → 5, 10 % → 90, 20 % → 125, 50 % → 192, 80 % → 233). Die vier Funktionen in `DaliHelper.cpp` bekommen dafür einen Modus; alle Stellen (Min/Max, Einschaltwerte, Status, HCL, RGBW-Slider) laufen schon über diese Funktionen. Zu beachten: Der Standard muss die heutige Kurve bleiben, der Parameter braucht freie Bits und eine neue ETS-Version, und die Kurve im Treiber (DT6 *Dimmkurve auswählen*) darf nicht gleichzeitig umgestellt werden. Gehört nicht in den ersten Pull Request.

### 5.5 Erledigte Punkte der früheren Wunschliste

- **Treiberzustand beim Start übernehmen:** Pegel und Ein/Aus seit v8 (B12). Die Farbübernahme (v10–v12) ist in `v13` entfernt, weil sie bei vielen RGBW-Kanälen den Start zu lange blockiert (vier Abfragen pro Kanal, bei 64 Kanälen ≈ 11 s); siehe Abschnitt 4.
- **Kosmetik: Startwerte „letzter Wert“:** `_lastDayValue = 100` / `_lastNightValue = 10` (schon im Ausgangscode) werden als DALI-Pegel gelesen (≈ 1,5 % bzw. ≈ 0,1 %); in v8 auf 100 % und 10 % als Pegel umgerechnet (Standardwert 254 bestätigt, Test 1). Die Änderung steckt im Startwert der Variablen und gilt damit für **alle Kanaltypen**, nicht nur für RGBW. Betrifft nur das erste Einschalten nach einem Neustart mit *Einschaltwert = 0*, wenn der Treiber beim Start aus war.
- **Status nach dem Stopp eines relativen Dimmens:** erledigt (v9, B11).

### 5.6 Noch zu testen (Reihenfolge)

1. Treppenhauslicht mit RGBW
2. zweiter RGBW-Kanal und mehrere Kanäle zugleich (Auslastung der DALI-Warteschlange, Farbsequenz ≈ 300 ms pro Kanal, Reihenfolge)
3. Gruppen, jeweils eine Gruppe nach der anderen
4. Broadcast Ein/Aus, Sperre, Tag/Nacht-Einschaltwerte, KNX-Szenen mit Farbe, im Treiber gespeicherte Szenen (danach überschreibt das nächste Einschalten die Szenenfarbe, Abschnitt 4)
5. Regression der anderen Farbtypen (RGB, HSV, TW, xyY) und anderer Kanaltypen (Testfall 14)
6. *Dimmen absolut* aus Aus bei gespeicherter Farbe (nicht Schwarz/Weiß); weitere Mischungen mit Weiß bei brennender Lampe
7. Test durch andere Anwender (insbesondere Befehl 237 bei anderen Treibern, B8)
8. Treiber mit unterschiedlicher Übergangszeit (Abschnitt 4)

### 5.7 Vor dem Pull Request

`RGBW_DIAG`-Diagnosecode entfernen (für den Test von `v13` vorübergehend eingeschaltet); nach den Tests den Standard für `p3mode` festlegen (5.1, Entscheidungsregel); offene Fragen an den Maintainer klären (Abschnitt 6 der [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md), u. a. das Log `Dali: unhandled KO: 1` beim Heartbeat, die Warnung „loop took longer than usual“, die knappe KNX-Reserve von 63 Byte, die Idee der einstellbaren Dimmkurve); (erledigt am 17.11.: veraltete Kommentare zu B2/B3 korrigiert); die Gegenüberstellung neu erzeugen (Stand v6, siehe Hinweis dort; Diff automatisch aus Git). **PR erst nach ausdrücklichem Go.**

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

In `GW-REG1-Dali/src/DaliGateway.xml` die `ApplicationVersion` erhöhen (Teil `x.y` = ETS-Version, `z` = Firmware-Revision; `ReplacesVersions` nennt die ersetzten Versionen), dann:

```powershell
cd ..\GW-REG1-Dali
OpenKNXproducer create --Debug -h include/knxprod.h src/DaliGateway
```

Dabei entsteht `include/knxprod.h` mit `PT_colorType_RGBW` (= 4) und den neu berechneten Parameter-Offsets sowie `src/DaliGateway.knxprod` für die ETS. `include/knxprod.h` nicht von Hand ändern, sie wird immer vom Producer erzeugt.

> **Applikationsversion vor jedem Update prüfen:** Die neue `ApplicationVersion` muss höher sein als die bereits in der ETS importierte, und die im Gerät verwendete Version muss in `ReplacesVersions` stehen, sonst lässt sich das Gerät nicht per *Aktualisieren* umstellen (B16). Eine schon einmal vergebene Version nicht wiederverwenden: Die ETS kennt sie bereits, und ob sie einen geänderten Inhalt unter derselben Nummer übernimmt, ist nicht sicher. Die Versionsnummern in dieser README gelten nur für unseren Testaufbau; für ein Release wird die Version beim Pull Request festgelegt.

**3. ETS importieren**

`.knxprod` in die ETS importieren. Ein vorhandenes Gerät mit einer Version aus `ReplacesVersions` per *Aktualisieren* umstellen (B16), sonst das Gerät **neu anlegen**. Danach parametrieren: Kanal auf *Gerätetyp DT8 – Farbe*, *Farbe ansteuern per: RGBW*.

**4. Firmware flashen**

Environment `develop_REG1_USB_V1` bauen (VS Code / PlatformIO: *Build*). Ist kein automatischer Upload möglich (picotool braucht unter Windows den WinUSB-Treiber via Zadig), das Gerät mit gedrückter BOOTSEL-Taste anstecken und `.pio/build/develop_REG1_USB_V1/firmware.uf2` auf das Laufwerk `RPI-RP2` kopieren. Danach das Gerät per ETS programmieren (Vollständiger Download).

**Archivstand `v12`:** Im Ordner `OFM-Dali` `git fetch rgbw`, dann `git checkout -B v1dev-v12 rgbw/v1dev-v12`; gleiche Umgebung bauen und flashen. Zurück auf den aktuellen Stand: `git checkout -B v1dev rgbw/v1dev`. Die Version steht im Log bei jedem Farbtelegramm (`Color absolute [v13-diag]`, ohne Diagnose `[v13]`, bzw. `[v12]`). Hinweis: `v1dev-v12` enthält den Upstream-Fix #7 nicht.

**Praktische Hinweise:** Testkanal A0, Gruppenadressen 20/0/0 bis 20/0/7 (Schalten, Dimmen absolut, Farbe RGBW, Farbe RGBW Status, Schalten Status, Dimmen Status, Dimmen relativ, HCL Kurve). *Nur Testaufbau, nichts am Gateway selbst:* Die HCL braucht eine gültige Uhrzeit aus KO 4 (GA 0/0/4, DPT 19.001), normalerweise von einem KNX-Zeitgeber. Fällt der Zeitgeber aus, meldet die HCL `Ungueltige Sonnenstandsdaten`. Zum Testen die Uhrzeit von Hand per `knx.send` (Home Assistant) an 0/0/4 schicken (8 Byte: Jahr−1900, Monat, Tag, Wochentag·32+Stunde, Minute, Sekunde, `0x20`, `0x80`; als Dezimalbytes, z. B. `[126, 11, 15, 244, 21, 30, 32, 128]`).

### 6.2 Testfälle

1. **DALI-Adressierung** (ETS: *Dali-GW → Adressierung*): Treiber scannen, lange Adresse eintragen, Kurzadresse `0` setzen (Kurzadresse n gehört zu Kanal An).
2. Gruppenadressen auf *Schalten*, *Dimmen absolut*, *Farbe RGBW* (DPT 251.600) und die zugehörigen Status-Objekte legen, Applikation programmieren.
3. In der ETS im Gruppenmonitor per *Wert schreiben* auf *Farbe RGBW* senden. Die Häkchen vor R/G/B/W sind die Gültigkeitsbits.

| # | Senden (Kanal ist zu Beginn **aus**, sofern nicht anders angegeben) | Erwartung | Stand |
|---|----------------------------------------------------|-----------|---|
| 1 | *Schalten* Ein, *Dimmen absolut* 50 % / 100 %, *Schalten* Aus | Leuchte reagiert; Status-Objekte folgen | **bestanden** (v6, v8) |
| 2 | Alle vier Häkchen, R 100 %, G/B/W 0 % | `v13`: Leuchte **bleibt aus**, Farbstatus `FF 00 00 00 00 0F`, Schalten Status *Aus*; danach *Schalten* Ein → rein Rot (bis `v12`: Farbtelegramm schaltete ein) | **bestanden** (v6, altes Verhalten); `v13` **offen** (5.1 T1/T2) |
| 3 | Alle vier Häkchen, G 100 %, übrige 0 % | rein Grün | **bestanden** (v6) |
| 4 | Alle vier Häkchen, B 100 %, übrige 0 % | rein Blau | **bestanden** (v6) |
| 5 | Alle vier Häkchen, W 100 %, übrige 0 % (der wichtigste Test!) | rein Weiß; Status `00 00 00 FF 00 0F` | **bestanden** (v6) |
| 6 | Nur Häkchen **W**, 30 % (Leuchte an) | nur Weiß ändert sich, RGB bleibt wie zuvor | **bestanden** (v6) |
| 7 | Nur Häkchen **R**, 100 % | nur Rot ändert sich, Weiß/Grün/Blau bleiben | **bestanden** (v6) |
| 8 | Kein Häkchen / `… 00` in Byte 5 | **keine** gültigen Kanäle: Licht darf sich nicht ändern | **bestanden** (v6) |
| 9 | Alle vier Häkchen, alle 0 % | Leuchte **schaltet aus**; Schalten Status *Aus*, Dimmen Status 0 % | **bestanden** (v6) |
| 10 | Nach 9: *Schalten* Ein | Leuchte geht mit der **zuletzt gesetzten Farbe** an (nicht schwarz) | **bestanden** (v6) |
| 11 | Nach 9 (zuletzt war z. B. Weiß gesetzt): Farbtelegramm R 100 %, dann *Schalten* Ein | `v13`: Lampe bleibt beim Farbtelegramm aus und geht bei *Ein* mit **Rot** an, nicht mit Weiß (bis `v12`: das Farbtelegramm selbst schaltete mit Rot ein) | **bestanden** (v6, altes Verhalten); `v13` **offen** |
| 12 | *Dimmen absolut* 0 %, danach ein Farbtelegramm, dann *Schalten* Ein | Merker bleibt nicht auf *Ein* hängen: das Farbtelegramm wird als „aus“ behandelt (`v13`: Lampe bleibt aus), *Ein* schaltet mit der neuen Farbe ein | **bestanden** (v6, altes Verhalten); `v13` **offen** |
| 13 | Rot gesetzt, dann *Dimmen absolut* 25 % | Rot mit 25 % Helligkeit | **bestanden** (durch Nachtests mit 50 %, 100 %, 10 % abgedeckt) |
| 14 | RGB/HSV/TW/xyY-Kanäle nachtesten | verhalten sich wie vorher | **offen** |

### 6.3 Weitere Hinweise

Rohdaten-Beispiele (Byte 0–5, z. B. für Home Assistant `knx.send` mit `payload: [0, 0, 0, 255, 0, 15]`): rein Weiß `00 00 00 FF 00 0F`, nur Weiß ändern (50 %) `00 00 00 80 00 01`, nur Rot ändern `FF 00 00 00 00 08`, alles aus `00 00 00 00 00 0F`.

Die Debug-Ausgabe (OpenKNX-Logging, Level *Debug*, serielle Konsole 115200 Baud) zeigt pro Telegramm `Got RGBW: …`, `Send RGBW: …` (mit dem Zusatz `temporary, activated by the following DAPC`, wenn ohne `ACTIVATE`), dazu je nach Fall `RGBW: level N`, `RGBW: level N with temporary colour (one fade)`, `RGBW: level N with the driver's own colour (copy report to temporary)` (Modus A), `RGBW: level N with temporary colour (own colour fade may still run)` (Modus B), `RGBW: colour (ACTIVATE)`, `RGBW: colour with the driver's own level (go to last active level)` (Modus A), `RGBW: colour with the running level target N` (Modus B), `RGBW: colour for a switched-off channel, lamp stays off`, `RGBW: off` bzw. `RGBW: all channels 0 -> switching off`. Beim Start einmal `RGBW: fade time …` und `RGBW: DALI version …, point 3 mode …`. Falls Weiß nicht leuchtet: Treiber-Datenblatt prüfen (RGBW-Mapping, Anzahl Farbkanäle) und mit dem Log vergleichen.

Optional, ohne Hardware: Host-Test der Logik

```bash
g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
```

---

## 7. Pull Request

Nach erfolgreichem Test kann aus `v1dev` dieses Forks ein Pull Request gegen `OpenKNX/OFM-Dali` (Basis-Branch **`v1dev`**) erstellt werden. Vorschlag für die Beschreibung: Zusammenfassung dieser README + [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md) + Testergebnis (getesteter Treiber, Auffälligkeiten). **PR erst nach ausdrücklichem Go** (siehe 5.7).
