# OFM-Dali – RGBW-Erweiterung (Branch `v1dev` dieses Forks, Stand `v13`)

Dieser Fork ergänzt das OpenKNX-DALI-Gateway um **RGBW (4. Kanal „Weiß“)** über **ein 6-Byte-Objekt nach KNX DPT 251.600** (`DPT_Colour_RGBW`) – so wie es z. B. das MDT DALI-Control-Gateway anbietet. Angesteuert werden DALI-DT8-Treiber mit **einer** Kurzadresse pro Leuchte (RGBW-Modus). Das DT6-Prinzip „vier Adressen für vier Kanäle“ wird nicht unterstützt.

Basis: `OpenKNX/OFM-Dali`, Branch **`v1dev`** (abgezweigt bei Commit `6e7aca5`; am 17.11.2026 der Upstream-Stand `a120578` mit Fix #7 `eed5a33` „export group settings“ hereingemergt, er korrigiert vier vertauschte HCL-Parameternamen im Gruppen-Template, Logik und Speicherlayout unverändert). Das Verhalten der bisherigen Farbtypen HSV, RGB, TW und xyY bleibt unverändert.

> **Stand (20.11.2026):** Branch `v1dev` = Firmware-Stand **`v13`** (Build `develop_REG1_USB_V1`, Projekt `GW-REG1-Dali`), ETS-Applikation 0.15: Gegenüber `v12` ist nur das Gruppen-Template durch den Upstream-Fix #7 geändert (vier HCL-Parameternamen); die RGBW-Ansteuerung von `v13` ändert an der ETS nichts (keine neuen Parameter oder Objekte, gleiche Speicherbelegung). Vor dem Update die Applikationsversion prüfen (siehe 6.1). `v13` steuert RGBW **wie den bestehenden RGB-Code** an (jedes Telegramm = ein DALI-Befehl = ein Fade, keine Busabfrage im Betrieb, kein Warten auf das Ende eines Fades) und weicht nur dort ab, wo der Treiber mit seiner einzigen Fade-Engine es verlangt: Sammelfenster, temporäre Farbe + `DAPC` in einem Fade (automatische Aktivierung, am DA4-M **bestätigt**, B18/B19) und für Farbe allein bzw. Helligkeit allein die Modi A/B/C, siehe [1.5](#15-ein--und-ausschalten) und [5.1](#51-ansteuerung-in-v13-konzept-und-testablauf). **`v13` ist kompiliert und seit dem 19.11. auf Hardware im Test** (DA4-M, Kanal A0): Phase 0 bis 3.11 der ersten Runde sind durch (3.11 **nicht bestanden**, B25), Ergebnisse in [5.1](#ergebnisse-der-ersten-runde-stand-20112026) und als Befunde B18–B25 in [Abschnitt 2](#2-befunde-zum-treiber-da4-m-mit-status); weiter geht es mit Phase 4. **Plan (20.11.):** vor Phase 5/6 entscheiden, ob Modus 0 (C) fest wird und A/B samt Zeitmarken entfallen, siehe [5.1, Plan](#plan-nach-phase-3-stand-20112026). Für den Test ist die Diagnose `RGBW_DIAG` vorübergehend eingeschaltet (Build-Tag `v13-diag`, siehe 1.7); sie wird in der finalen Version entfernt.
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
   - ein **Pegelbefehl** direkt dahinter, **ohne** `ACTIVATE`: `DAPC` mit dem Zielpegel, in Modus A bei einer Farbe allein `GO TO LAST ACTIVE LEVEL` (Befehl 10, DALI-2). Der Pegelbefehl übernimmt die temporäre Farbe mit, wenn im Treiber die automatische Aktivierung eingeschaltet ist (IEC 62386-209 sieht dafür eine eigene, ein- und ausschaltbare Funktion vor). Dass Farbe und Pegel dabei gemeinsam in **einem** Fade laufen, ist am DA4-M mit `DAPC` **bestätigt** (5.1, Phase 2.2/2.3, B19); ob die Übernahme auch bei `GO TO LAST ACTIVE LEVEL` greift, ist noch offen (Phase 6.2).

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

**Grundregel (`v13`):** wie RGB, jedes Telegramm wird zu einem DALI-Befehl mit einem Fade, ohne Busabfrage und ohne Warten auf das Ende eines Fades. Der Treiber hat nur eine Fade-Engine (B1); damit ein Befehl einen laufenden Fade nicht halb stehen lässt, trägt er wo nötig **beide Ziele** (temporäre Farbe + `DAPC`), dann fadet der Treiber von seinem aktuellen Stand zu beiden Zielen (am DA4-M bestätigt, 5.1 Phase 2, B19). „Aktueller Stand“ heißt auch: aus dem Aus fadet die **Farbe** von der im Treiber gespeicherten Farbe zur neuen (B20).

**Sammelfenster** (`aawin`, Standard 80 ms): Nach dem ersten RGBW-Telegramm wartet das Gateway 80 ms, ob ein zweites folgt (Home Assistant schickt Farbe und Helligkeit mit etwa 20 ms Abstand). Danach geht **ein** Befehl mit den neuesten Werten raus. Das ist die einzige feste Wartezeit. **Ausnahme:** Eine Farbe an einen **ausgeschalteten** Kanal öffnet kein Fenster, sie geht sofort mit `ACTIVATE` raus (siehe Tabelle). Schaltet Home Assistant aus „aus“ mit Farbe und Helligkeit ein (erst Farbe, dann Helligkeit), gehen deshalb zwei Farbsequenzen raus: erst Farbe + `ACTIVATE` bei Pegel 0, dann temporäre Farbe + `DAPC`. Sichtbar ist ein Fade, der Bus ist aber ≈ 310 ms länger belegt. Aus „aus“ wird nur die Reihenfolge Helligkeit vor Farbe zu einem Befehl zusammengeführt.

| Ereignis | Verhalten bei RGBW-Kanälen (`v13`) |
|----------|----------------------------|
| **Ausschalten** (*Schalten* Aus, *Dimmen absolut* 0 %, Farbe mit allen Kanälen 0, *Dunkler* bis zum Minimum) | Sofort `DAPC 0`, die Leuchte fadet mit der Fade-Zeit des Treibers aus; ein laufendes Sammelfenster wird verworfen. Ein Umweg über die Farbe 0/0/0/0 wird **nicht** gemacht, weil der DA4-M dafür Weiß statt Schwarz zeigt (B3). Die zuletzt gesetzte Farbe bleibt im Gateway und im Treiber gespeichert. |
| **Einschalten** (*Schalten* Ein, *Dimmen absolut* > 0) | Nach dem Sammelfenster: gespeicherte Farbe als temporärer Wert + `DAPC` mit dem Einschaltwert, **ein** Fade. Einschaltwert: Tag/Nacht-Einschaltwert, bei 0 der letzte Wert (bzw. der Wert von *Dimmen absolut*). Normalerweise hat der Treiber diese Farbe schon aktiv (zuletzt gesetzt oder bei Pegel 0 mit `ACTIVATE` übernommen); die mitgeschickte Farbe ändert nur dann etwas, wenn die Farbe im Treiber abweicht (Szene, Gruppenbefehl, anderer DALI-Master, Stromausfall nur des Treibers; dass der DA4-M eine bei Pegel 0 gesetzte Farbe vergisst, ist widerlegt, B5b). Weicht sie ab, fadet die Farbe beim Einschalten von der Treiberfarbe zur neuen; bei niedrigem Pegel ist dabei kurz eine Mischfarbe zu sehen (B20, normgerecht). Wer aus dem Aus in einer **neuen** Farbe einschalten will, ohne Mischfarbe, schickt die Farbe zuerst bei ausgeschalteter Lampe (nächste Zeile) und schaltet dann ein (5.1, Phase 3.2/3.3). Kennt das Gateway keine Farbe (z. B. nach einem Neustart vor dem ersten Farbtelegramm), geht nur der `DAPC` raus, und der Treiber leuchtet in seiner eigenen, zuletzt aktiven Farbe. |
| **Farbtelegramm an einen ausgeschalteten Kanal** | Wie RGB: Farbe + `ACTIVATE` sofort, der Treiber übernimmt sie bei Pegel 0 unsichtbar (B2) und hält sie auch über eine Minute (B5b, `v13` Phase 3.2/3.9 bestätigt), die Lampe **bleibt aus**, *Schalten Status* bleibt *Aus*, nur *Farbe RGBW Status* zeigt die neue Farbe. Das nächste Einschalten schickt die Farbe noch einmal mit dem `DAPC` mit, ein abgebrochener Farb-Fade schadet also nicht. **Ausnahme Slider-Modus** (1.4): Dort steckt die Helligkeit im Farbwert, ein Farbwert > 0 schaltet ein (temporäre Farbe + `DAPC`). |
| **Farbtelegramm an einen eingeschalteten Kanal** | Nach dem Sammelfenster: Kam im Fenster auch eine Helligkeit, gehen temporäre Farbe + `DAPC` zusammen raus (ein Fade). Sonst je nach Modus für **Farbe allein** (siehe unten): **A** temporäre Farbe + `GO TO LAST ACTIVE LEVEL` (der Treiber nimmt seinen eigenen Zielpegel, Einzelkanäle und Gruppen); **B** bei einem Einzelkanal, dessen eigener Pegel-Fade noch laufen kann, temporäre Farbe + `DAPC` mit dem Zielpegel, den das Gateway selbst zuletzt gesendet hat, sonst Farbe + `ACTIVATE`; **C** Farbe + `ACTIVATE`. Die Helligkeit bleibt unverändert. |
| **Helligkeit an einen eingeschalteten Kanal** (*Dimmen absolut*, *Schalten* Ein) | Nach dem Sammelfenster je nach Modus für **Helligkeit allein**: **A** `COPY REPORT TO TEMPORARY` + `DAPC` (gedacht: der Treiber nimmt seine eigene Farbe mit, ein laufender Farb-Fade läuft zu seinem Ziel weiter; **Risiko:** am DA4-M ist die Report-Farbe nachweislich oft veraltet, B22, dann käme eine alte Farbe zurück; Prüfung in Phase 6.8); **B** `DAPC`, und kann der eigene Farb-Fade noch laufen, die Farbe des Gateways als temporären Wert davor; **C** `DAPC`. Die Farbe bleibt erhalten (Rot, dann 25 % = Rot mit 25 %). |
| **Dimmen relativ** | Wie RGB, ohne Warten: `UP`/`DOWN` alle 200 ms, *Heller* aus Aus mit `RECALL MIN` ohne Farbe (die Leuchte kommt in der Farbe, die im Treiber steht, B5b; `v13` Phase 3.9 bestätigt). Ein offenes Sammelfenster wird vorher gesendet. Ein `UP`/`DOWN` direkt nach einer Farbänderung bricht den Farb-Fade ab: In Phase 3.11 blieb die Lampe fast in der alten Farbe, der Farbstatus meldete die neue (B25). ~~„bewusst hingenommen“~~ – **nicht hinnehmbar**, Lösung geplant (5.1, Plan). *Dunkler* bis zum Minimum schaltet aus und beendet das Dimmen (B10; `v13` Phase 3.10 bestätigt). *Schalten Status* Ein kommt bei *Heller* aus Aus erst beim Stopp (Ausgangscode, B23). |
| **Sperre, Szene** | verwerfen ein offenes Sammelfenster und den gemerkten Zielpegel; die Sperre sendet ihren Wert wie bisher als `DAPC`. |

**Farbe allein / Helligkeit allein bei brennender Lampe: Modi A, B, C** (Konsole `p3mode`, Standard `auto`): Ein einzelner Befehl würde einen laufenden Fade des Treibers halb stehen lassen (B1). Deshalb trägt er das andere Ziel mit:

| Modus | Farbe allein | Helligkeit allein | gilt für | Voraussetzung |
|---|---|---|---|---|
| **A** (Standard bei DALI-2-Treibern) | temporäre Farbe + `GO TO LAST ACTIVE LEVEL` | `COPY REPORT TO TEMPORARY` + `DAPC` | Einzelkanäle und Gruppen, immer, ohne Zeitprüfung | DALI-2; der Treiber übernimmt die temporäre Farbe auch bei diesen Befehlen; „Report“ entspricht der aktuellen bzw. Zielfarbe (5.1, Phase 6). **Am DA4-M für „Helligkeit allein“ fraglich:** die Report-Farbe war in 3.2 und 3.9 veraltet (B22) |
| **B** (Standard bei DALI-1-Treibern) | temporäre Farbe + `DAPC` mit dem eigenen Zielpegel, solange der eigene Pegel-Fade laufen kann, sonst `ACTIVATE` | temporäre Farbe (des Gateways) + `DAPC`, solange der eigene Farb-Fade laufen kann, sonst `DAPC` | „Farbe allein“ nur Einzelkanäle (Gruppen: `ACTIVATE`) | jeder DT8-Treiber mit automatischer Aktivierung |
| **C** (wie RGB) | `ACTIVATE` | `DAPC` | alle | keine; ein laufender Fade kann halb stehen bleiben |

In Modus A kommen Pegel bzw. Farbe vom **Treiber selbst**, das Gateway muss sie nicht kennen; das stimmt auch nach Szenen, relativem Dimmen, Befehlen anderer DALI-Master und bei Gruppenmitgliedern mit unterschiedlichen Farben.

**Zeitprüfung in Modus B:** Das Gateway merkt sich bei jedem eigenen `DAPC` an einen RGBW-Kanal Zeitpunkt und Zielpegel und bei jeder eigenen Farbe den Zeitpunkt, nur im RAM (9 Byte pro Kanal, bei 64 Kanälen und 16 Gruppen unter 1 kB). Ein Fade „kann noch laufen“, wenn der Befehl weniger als 310 ms (Buszeit einer Farbsequenz) + Fade-Zeit + 300 ms Reserve her ist (gemessen endete ein Farb-Fade bei 1 s Fade-Zeit bis ≈ 1,57 s nach dem Farbtelegramm, B7). Es gibt dafür keine Abfrage auf dem Bus. Der Pegel wird nie geschätzt: Gesendet wird nur ein Zielpegel, den das Gateway selbst kurz vorher gesendet hat. Verworfen wird der gemerkte Zielpegel bei allem, was den Pegel ohne festes Ziel ändert (Start von relativem Dimmen, Ausschalten, Szene, Sperre, Gruppen- und Broadcast-Befehle, HCL-Helligkeit) und sobald die Prüfung ihn als abgelaufen erkennt (so bleibt auch der Überlauf von `millis()` nach ≈ 49,7 Tagen ohne Folgen).

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
- **Farbe allein / Helligkeit allein** an eine brennende Gruppe: In **Modus A** wie bei Einzelkanälen (`GO TO LAST ACTIVE LEVEL` bzw. `COPY REPORT TO TEMPORARY`), jedes Mitglied nimmt seinen eigenen Pegel bzw. seine eigene Farbe. Offen: Ein einzeln ausgeschaltetes Mitglied einer „eingeschalteten“ Gruppe würde durch `GO TO LAST ACTIVE LEVEL` eingeschaltet (zweite Testrunde, 5.6). In **Modus B** geht eine Farbe allein an eine Gruppe mit `ACTIVATE` (wie RGB), weil Mitglieder auf unterschiedlichen Pegeln stehen können und ein `DAPC` mit dem Gruppenpegel sie alle auf diesen Pegel ziehen würde; Restrisiko wie bei RGB.
- **Farbe beim Start:** Das Gateway liest beim Start keine Farbe mehr (auch nicht für Gruppen); die Gruppenfarbe ist bis zum ersten Farbtelegramm unbekannt.
- **Referenz-EVG:** Pegelabfragen (relatives Dimmen, Status) und die einmaligen Abfragen von Fade-Zeit und DALI-Version (falls die Gruppe der erste RGBW-Kanal ist) gehen an das Dimm-Referenz-EVG der Gruppe.
- Ein Gruppenbefehl verwirft bei den Mitgliedern ein offenes Sammelfenster und den gemerkten Zielpegel.
- **Mitglied nach einer Gruppenfarbe:** Die Farbe eines Gruppenbefehls wird nicht in die Einzelkanäle übernommen. Schaltet man ein Mitglied danach einzeln aus und wieder ein, schickt es seine eigene, zuletzt einzeln gesetzte Farbe mit und überschreibt die Gruppenfarbe (wie nach einer Szene, Abschnitt 4).

### 1.7 Serielle Konsole (Feintuning und Diagnose)

`p3mode auto|0|1|2` (Modus für Farbe allein / Helligkeit allein bei brennender Lampe, siehe 1.5: `auto` = A bei DALI-2-Treibern, sonst B; `1` = A; `2` = B; `0` = C wie RGB; Standard `auto`), `aawin nnn` (Sammelfenster in ms, 0–1000, Standard 80, 0 = jedes Telegramm sofort senden); beide gelten bis zum Neustart. Nur im Diagnose-Build: `diagq 0|1` schaltet die Pegel- und Statusabfragen der Diagnose nach jedem Befehl aus bzw. ein (Standard ein, das Frame-Log bleibt an; gilt bis zum Neustart). `aastat` (liest bei allen konfigurierten RGBW-Kanälen die DT8-Einstellung *automatische Aktivierung* aus, Gear Features/Status Bit 0), `aaon [yy]` (soll die automatische Aktivierung **dauerhaft** im Treiber einschalten, ohne `yy` bei allen konfigurierten RGBW-Kanälen; einmal ausführen genügt. Nach DALI muss ein Konfigurationsbefehl zweimal kurz hintereinander ankommen, das erledigt `aaon` selbst, fragt danach nach und versucht bei Bedarf automatisch eine zweite Sendeform. Der Treiber behält die Einstellung auch stromlos (am DA4-M bestätigt, B18). Am DA4-M war `aaon` nicht nötig, weil die Einstellung ab Werk an ist; ob sich die Einstellung per `aaon` setzen lässt, ist deshalb weiter **ungetestet**). Beide nur von Hand, das Gateway fragt oder schreibt diese Einstellung nie selbst. `getCol yy [sel]` (fragt Farbtyp-Merkmale, Farbstatus und die Farbwerte des Treibers ab, ändert nichts; ohne Selektor: Report 233–236, aktuell 9–12, temporär 201–204 je R, G, B, W). Die Befehle `onmode`, `settle`, `margin` und `blackoff` aus `v12` gibt es in `v13` nicht mehr.

Mit `#define RGBW_DIAG` in `DaliChannel.h` zusätzlich Log jedes DALI-Frames und Abfragen `QUERY_ACTUAL_LEVEL`/`QUERY_STATUS` zu festen Zeiten nach jedem RGBW-Befehl aus dem Sammelfenster. **Für den Test von `v13` ist die Diagnose vorübergehend eingeschaltet** (Build-Tag `v13-diag`), um die automatische Aktivierung auf dem Bus zu prüfen (siehe 5.1). Nebenwirkungen: zusätzliche Frames; die Pegelabfragen setzen *Dimmen Status*, den RGBW-Status (Slider-Modus) und den „letzten Wert“ auf Zwischenwerte des laufenden Fades. Die Abfragen eines Befehls (bis 5 s danach) laufen weiter, wenn schon der nächste Befehl kommt; *Dimmen Status* zeigt dann kurz einen Wert des vorigen Fades (B24). Für Tests dieser Punkte `diagq 0` oder das `#define` auskommentieren. **Die Diagnose wird in der finalen Version entfernt.**

### 1.8 Broadcast

| Ereignis | Verhalten mit RGBW-Kanälen (`v13`) |
|----------|----------------------------|
| **Broadcast Ein** (Objekt *Broadcast Schalten*) | `DAPC FE` (100 %) an alle, wie im Ausgangscode; Tag/Nacht-Einschaltwerte gelten dabei nicht. Danach bekommt jeder RGBW-Kanal und jede RGBW-Gruppe, die aus war und eine bekannte Farbe hat, sofort die Farbe als temporären Wert + `DAPC FE`. Der zweite `DAPC` trägt beide Ziele, der Treiber fadet von seinem Stand dorthin. |
| **Broadcast Aus** | Ein einzelnes `DAPC 0` an alle, wie im Ausgangscode. RGBW-Kanäle verwerfen nur ihr offenes Sammelfenster und ihren Zielpegel. |
| *Broadcast Dimmen absolut*, Taster am Gerät (`RECALL MAX`/`OFF`) | wie im Ausgangscode, **ohne** RGBW-Behandlung. |

Bekannte Lücken: Weicht die Farbe im Treiber von der des Gateways ab (Szene, Gruppenbefehl), ist bei Broadcast Ein kurz die Treiberfarbe zu sehen, bevor der zweite `DAPC` greift; normalerweise sind beide gleich, weil eine Farbe bei ausgeschaltetem Kanal per `ACTIVATE` übernommen wird; sind eine RGBW-Gruppe und ihre Mitglieder konfiguriert, gehen zwei Farbsequenzen raus. *Broadcast Dimmen absolut* und der Taster setzen weder die Farbe noch den Status der RGBW-Kanäle. Nichts davon ist auf Hardware getestet (5.1, 5.6).

---

## 2. Befunde zum Treiber DA4-M (mit Status)

Diese Tabelle fasst zusammen, was wir über das Verhalten des DA4-M (und des Gateways) gelernt haben, einschließlich der Annahmen, die später widerlegt wurden. Die Nummern **B1 bis B25** werden an anderen Stellen der README als Verweis benutzt (B18–B25 aus dem `v13`-Test ab dem 19.11.2026). Messwerte und Logs stehen in der [Rohfassung](doc/README-Rohfassung-2026-11-16.md).

| Nr | Befund | Status | Belegt durch | Folge im Code |
|---|---|---|---|---|
| B1 | **Der Treiber hat nur eine Fade-Engine.** Ein Pegelbefehl bricht einen laufenden Farb-Fade ab (Mischfarbe bleibt stehen), das `ACTIVATE` einer Farbsequenz bricht einen laufenden Pegel-Fade ab. | **bestätigt** | v6-Fund: Home Assistant sendet Farbe + 20 ms später Helligkeit auf brennende Lampe, falsche Farbe (Blau + Weiß statt Rot + Weiß) bei 2 von 2 Versuchen. v8: Pegel stand bei 208 statt 235. v10-Fund: ein UP direkt nach der Farbe bricht den Farb-Fade ab. v11-Tests 1–4 bestanden. | `v7`–`v12`: Pegelbefehl wartet auf das Ende des Farb-Fades (`QUERY_STATUS`), Zielpegel wird nach dem Farb-Fade noch einmal gesendet (v9), kein UP/DOWN während des Farb-Fades (v11). **`v13`:** stattdessen Befehle mit beiden Zielen (temporäre Farbe + `DAPC`) und Sammelfenster, ohne Abfrage und ohne Warten (1.5); relatives Dimmen wartet nicht mehr |
| B2 | „Der DA4-M **ignoriert eine Farbänderung bei Pegel 0**, die Lampe leuchtet mit der alten Farbe.“ | **widerlegt** | Frühe Beobachtung vom 13.11. (Commit `ea2cc95`: nach „alle 0“ leuchtete ein rotes Telegramm mit der alten Farbe Weiß, erst das zweite Telegramm wirkte). **Gegenbelege:** Farbe bei Pegel 0 senden, auf das Ende des Farb-Fades warten (1350 ms), dann Pegel: neue Farbe sofort (v10 Test 2, v12 H2, A5, B2). Vermutliche Ursache des frühen Befunds (nicht einzeln nachgeprüft): ein sofort folgender Pegelbefehl hat den Farb-Fade abgebrochen (B1) oder die Farbe 0/0/0/0 wurde als Weiß dargestellt (B3). | Die damalige Lösung „erst einschalten, dann Farbe“ (`ea2cc95`) ist überholt. **`v13`:** Farbe an eine ausgeschaltete Lampe geht wie bei RGB mit `ACTIVATE` an den Treiber, die Lampe bleibt aus (1.5) |
| B3 | Der DA4-M zeigt für die Farbe 0/0/0/0 **Weiß** statt Schwarz. | **bestätigt** | Hardware-Tests (Stand v6 bis v12), dokumentiert in der Rohfassung unter „Ein-/Ausschaltverhalten“ | Ausschalten per `DAPC 0`. Die frühere Sequenz „Schwarz zuerst, dann Pegel 0; Pegel zuerst aus Schwarz“ (Commit `f568871`, 13.11.) ist **überholt**; die Konsolenbefehle dafür (`blackoff`, `onmode 0`) sind in `v13` entfernt |
| B4 | Der Treiber **normiert** die Farbe, die Amplitude des Telegramms geht verloren (z. B. `80 00 00 80` an brennende Lampe: Lampe unverändert, Status meldet `80 00 00 80`). | **bestätigt** | v11 Test A1 | Standard: „Farbe = Mischung, Helligkeit = Dimmen absolut“. Slider-Modus (v12) rechnet die Helligkeit selbst aus dem Farbwert |
| B5 | Der Treiber meldet seine Farbe über die **Report-Selektoren 233–236** (`QUERY_COLOUR_VALUE`), auch bei Pegel 0, er hält die Farbe also gespeichert. | **bestätigt** | v9 `getCol` (Lampe Blau, auch bei Pegel 0 dieselbe Antwort 0/0/254/0); v10 Test 1 (Neustart bei ausgeschalteter Lampe, zuletzt Blau: Gateway liest `00 00 FF 00`) | `v10`–`v12`: Farbe beim Start vom Treiber übernehmen. **`v13`:** entfernt (bei 64 RGBW-Kanälen ≈ 11 s Buszeit beim Start); die Farbe ist bis zum ersten Farbtelegramm unbekannt. `getCol` bleibt als Diagnose |
| B5a | Die Standard-Selektoren des Diagnosebefehls `getCol` in v9 (64–67, 0–2) waren falsch (Primärfarb-Koordinaten bzw. X/Y/Tc, hat ein RGBW-Treiber nicht). | **überholt** | in v10 durch 233–236, 9–12 und 201–204 ersetzt | `getCol` |
| B5b | Haltedauer einer Farbe, die **bei Pegel 0 gesendet** wurde: Der DA4-M hält sie mindestens etwa 1,5 min (länger nicht geprüft). ~~„Haben wir nur mit höchstens etwa 1,5 s bis zum Pegel getestet“~~ (überholt). | **bestätigt** `v13` (20.11.) | 5.1 Phase 3.9: Blau bei Pegel 0 (`ACTIVATE`), nach ≈ 1,5 min `getCol`: aktuell 0/0/254/0; danach *Heller* aus Aus (`RECALL MIN`, **ohne** Farbe vom Gateway): Lampe in reinem Blau. Ebenso 3.2 (Rot, aktuell 254/0/0/0 bei Pegel 0) | Wege ohne Farbe (*Heller* aus Aus, Broadcast Ein bei unbekannter Farbe, Einschalten durch andere Geräte) kommen in der richtigen Farbe. Das Argument „der Treiber könnte die Farbe vergessen“ für C2 ist damit für den DA4-M **widerlegt** |
| B6 | `QUERY_STATUS` Bit 4 („Fade läuft“) meldet auch einen **Farb-Fade bei Pegel 0**. | **bestätigt** | Logs `colour fade over (status 0x00), 1350 ms` u. a. | `v7`–`v12`: Ende-Erkennung in `loopFadePoll`. **`v13`:** nicht mehr genutzt (nur noch in der Diagnose) |
| B7 | Fade-Dauern am DA4-M (Fade-Zeit 1 s per `QUERY_FADE_SPEEDS`): Farb-Fade bei **Farbwechsel ≈ 1,35–1,58 s**, bei **unveränderter bzw. gespeicherter Farbe ≈ 0,3 s**. Der Pegel-Fade dauert etwa 1 s. | **bestätigt** | v6, v7 (1569–1571 ms), v9 (1572 ms), v10 (1350, 1476 ms), v12 (335/336 ms) | `v7`–`v12`: feste Zeiten nur als Rückfall. **`v13`:** die Fade-Zeit dient nur noch der Zeitprüfung in Modus B (1.5) |
| B8 | Das Weiß wird am DA4-M (Modus RGBW) mit **Befehl 236** (`SET TEMPORARY WAF DIMLEVEL`) gesetzt. | **bestätigt** (nur DA4-M) | alle Weiß-Tests | Befehl 237 (`RGBWAF`) bei anderen Treibern: **offen**, andere Anwender sollen es mit ihren Treibern prüfen |
| B9 | Der DA4-M-Takt für relatives Dimmen: ≈ 9 Pegelstufen alle 200 ms. Im unteren Bereich der logarithmischen Kurve wirkt das langsam (Pegel 1 bis 72 in ≈ 2 s = 0,7 %). | **bestätigt** (nicht verändert) | v7 | – |
| B10 | Die Dimmschleife (`loopDimming`) beendet das Dimmen am Minimum nicht, sondern wiederholt *Dunkler* + *Aus* bis zum Stopp (Telegrammflut, falscher Status). Struktur aus dem Ausgangscode (`git blame`); bei anderen Farbtypen ist dasselbe zu erwarten, nicht geprüft. | **behoben** (nur RGBW) v8, Test 3; in `v13` erneut bestätigt (5.1 Phase 3.10: am Minimum `Dimm Stop at: 1`, `0x0000`, ein Stopp 4 s später ohne Frame) | v7-Fund, v8 Test 3 | Der Schritt, der das Minimum erreicht, schaltet aus und beendet das Dimmen; *Dunkler* auf ausgeschalteter Lampe wird ignoriert |
| B11 | Nach dem Stopp eines relativen Dimmens meldete das Gateway nur den **zuletzt abgefragten** Pegel (21,6 % statt 28,6 %). Betrifft den Ausgangscode, nicht nur RGBW. | **behoben** v9 (einmal `QUERY_ACTUAL_LEVEL` ≈ 300 ms nach dem Stopp, alle Kanaltypen), auf Hardware bestätigt; in `v13` erneut bestätigt (5.1 Phase 3.10: Stopp bei 162, Abfrage 172, Status 10,6 %) | v8 Test 4 (Fund), v9-Test (145 = 5,10 %, `getLvl 00` identisch). Der To-do-Eintrag „ungetestet“ in der alten README war **überholt** | – |
| B12 | Nach einem Neustart kannte das Gateway weder Farbe noch Ein/Aus-Zustand der brennenden Lampe (bis v7 Aufblitzen beim ersten Farbtelegramm). | **behoben**: Ein/Aus und Pegel seit v8 (Test 5); Farbe v10–v12 (Tests 1–3), in `v13` bewusst wieder entfernt (Abschnitt 4) | | Beim Start `QUERY_ACTUAL_LEVEL` lesen (Ein/Aus und Pegel). Die Farbabfrage (Report 233–236) ist in `v13` entfernt |
| B13 | Die Farbsequenz bricht auch einen laufenden **Pegel-Fade** ab, wenn Helligkeit **vor** der Farbe kommt (andere Visualisierungen, Logik, Szenen-Bausteine). | **behoben** v9, auf Hardware bestätigt (60 % und 20 ms später Blau: Pegel 235) | v8 (Pegel blieb bei 208), v9 | `v9`–`v12`: Zielpegel nach dem Ende des Farb-Fades noch einmal gesendet. **`v13`:** Farbe allein bei brennender Lampe → Modus A: temporäre Farbe + `GO TO LAST ACTIVE LEVEL`; Modus B: während des eigenen Pegel-Fades temporäre Farbe + `DAPC` mit dem eigenen Zielpegel (Einzelkanäle), siehe 1.5 |
| B14 | *Schalten Status* Ein wurde bei jedem *Dimmen absolut* auf eine brennende Lampe zusätzlich gesendet. | **behoben** v8 (nur RGBW, Schaltstatus nur bei Änderung), Test 2 | | – |
| B15 | Die Statusobjekte werden bei jedem relativen Schritt gesendet, obwohl „Dimmstatus Interval bei relativ“ auf *Deaktiviert (nur am Ende)* steht. Vermutung: Pegelabfrage `Got new actual level` (Ausgangscode, Mai 2026). | **offen** (nicht geprüft, nicht verändert) | v7 | – |
| B16 | Die ETS-Applikation sei **nicht in-place updatebar**, das Gerät müsse neu angelegt werden (Aussage der alten README, `ReplacesVersions` enthielt die neue Version nicht). | **überholt** | Ab 0.14 ersetzt die Applikation 0.0, 0.10, 0.12, 0.13; in der ETS *Aktualisieren* (ein deaktivierter Einzel-EVG verliert dabei seine Gruppenadressen). Für Versionen außerhalb dieser Liste gilt die alte Aussage weiter | – |
| B17 | KNX-Speicher: Build-Skript schätzt **DATA\* 36801 von 36864 Byte** (nur 63 Byte Reserve). *colorType* von 2 auf 3 Bit verbreitert und auf Byte 21 Bit 5–7 (Einzelkanal) bzw. Byte 17 Bit 2–4 (Gruppe) verschoben, *Helligkeit bei RGBW* liegt in Byte 7 Bit 5 (Einzelkanal) bzw. Byte 17 Bit 5 (Gruppe), die Parameterblöcke werden nicht größer (ein eigenes Byte hätte die Schätzung um 1 Byte über die Partitionsgröße gehoben). | **bestätigt** (Stand v12) | Build v12 | Neue Parameter nur in freie Bits legen. Ist das nicht möglich, `KNX_FLASH` über die `platformio.ini` anpassen (**offen**, noch nicht geprüft) |
| B18 | Die DT8-Einstellung **automatische Aktivierung** (Gear Features/Status Bit 0) ist am DA4-M **ab Werk an** und bleibt es auch, wenn der Treiber stromlos war. Das passt zum Grundwert der Norm. | **bestätigt** `v13` (19.11.) | 5.1 Phase 1.1 (`aastat`: `0x01`, ON), 1.4 (nach Abschalten der Versorgung weiter ON). Farbtyp-Merkmale `0x80` (4 Kanäle RGBWAF), Farbstatus `0x80` | `aaon` war nicht nötig (ungetestet). Zu C4 siehe 5.1 |
| B19 | **Temporäre Farbe + `DAPC` = ein gemeinsamer Fade.** Der `DAPC` übernimmt die temporäre Farbe ohne `ACTIVATE`; Farbe und Pegel faden gleichzeitig und weich, auch wenn der Pegel gleich bleibt (dann nur Farbfade). | **bestätigt** `v13` (19.11.) | 5.1 Phase 2.2 (Blau 50 % → Grün 80 %: Pegel 230 → 236 → 241 → 246, Farbe weich), 2.3 (Grün 80 % → Rot 80 % bzw. Rot → Blau bei 80 %: Pegel konstant 246, Mischfarbe Rot/Blau sichtbar, ≈ 1,3 s), 2.5b, 3.1, 3.3, 3.6–3.8 | Grundverfahren von `v13` trägt; Rückfall auf `v1dev-v12` nicht nötig |
| B20 | **Einschalten aus Aus mit einer neuen Farbe in einem Befehl:** Der Treiber fadet die Farbe von seiner gespeicherten (alten) Farbe zur neuen, gleichzeitig mit dem Pegel von 0 hoch. Bei niedrigem Pegel ist dabei kurz eine **Mischfarbe** zu sehen. Nach IEC 62386-209 behandelt der Treiber eine Farbänderung wie eine Pegeländerung, ein Fade ändert beides gleichzeitig; eine Ausnahme für den ausgeschalteten Zustand ist uns nicht bekannt (Norm nur in Auszügen gelesen). Das Verhalten ist also normgerecht. | **bestätigt** `v13` (19.11.), **akzeptiert** | 5.1 Phase 2.4 (alte Farbe Blau, neue Grün: kurz Türkis), 2.4a (alte Grün, neue Rot: deutlich Gelb/Orange). Gateway korrekt: ein Befehl, temporäre Farbe + `DAPC`, kein `ACTIVATE`. Gegenprobe 3.2/3.3: Farbe zuerst bei Pegel 0, dann Ein → keine Mischfarbe | Keine Code-Änderung (Entscheidung 19.11.). Abhilfe für Anwender: neue Farbe zuerst bei ausgeschalteter Lampe senden, dann einschalten (1.5) |
| B21 | Die **temporären Farbwerte** (Selektoren 201–204) werden vom DA4-M nach der Übernahme **nicht** auf MASK (255) zurückgesetzt, weder nach einem automatisch aktivierten `DAPC` noch nach `ACTIVATE`. Nach unserer Lesart der Norm (Tabelle 5) müssten sie danach MASK sein. ~~„Am Gerät bestätigt (Phase 2.2): danach stehen die temporären Werte auf MASK“~~ (**widerlegt**: in 2.2 wurde das nicht abgefragt). | **bestätigt** `v13` (19.11.) | 5.1 Phase 2.5 (nach Einschalten aus Aus: temporär 254/0/0/0), 2.5b (nach Farbe + `DAPC` bei brennender Lampe: temporär 0/254/0/0), 3.2 (nach `ACTIVATE` bei Pegel 0: temporär 254/0/0/0), 3.9 (temporär 0/0/254/0). **Ergänzung 20.11.:** Nach *Heller* aus Aus (`RECALL MIN`), `UP`/`DOWN` und Ausschalten am Minimum (3.9 Teil B, 3.10) standen sie am nächsten Morgen auf MASK; ebenso nach 3.11 (`ACTIVATE` + `UP`). Einer dieser Befehle setzt sie also zurück, welcher, ist offen | Folge: Ein späterer Pegelbefehl **ohne** Farbe (z. B. `DAPC` in Modus C, Befehl eines anderen Masters) kann die zuletzt gesendete Farbe erneut anwenden. Im Normalfall harmlos (der Treiber hat diese Farbe ohnehin); sichtbar nur, wenn die Farbe zwischendurch anders gesetzt wurde (Szene, Gruppe, anderer Master). Zu prüfen in der zweiten Runde (5.6), Bezug zu C2. Keine Code-Änderung beschlossen |
| B22 | Die **Report-Farbe** (Selektoren 233–236) ist am DA4-M **kein Ist-Wert**: Nach einer Farbe per `ACTIVATE` bei Pegel 0 bleibt sie auf der vorigen Farbe stehen; nach einem Neustart des Treibers ist sie MASK. Sie wird offenbar nur durch bestimmte Abfragen neu gefüllt (vermutlich u. a. `QUERY ACTUAL LEVEL`, nicht einzeln bewiesen). | **bestätigt** `v13` (19.11.) | 5.1 Phase 1.4 (nach Neustart des Treibers: Report MASK, aktuell 254 × 4), 3.2 (aktuell Rot, Report noch Grün), 3.9 (aktuell Blau, Report noch Grün). Nach Abfragen des Pegels stimmten Report und aktuell überein (1.3, 2.5b); nach den Pegelabfragen beim relativen Dimmen (3.10) stand der Report am 20.11. auf Blau = aktuell, nach 3.11 ebenfalls Report = aktuell (14/0/240/0) | Risiko für Modus A „Helligkeit allein“ (`COPY REPORT TO TEMPORARY` kopiert dann eine alte Farbe), Prüfung in Phase 6.8; ggf. Standard für `p3mode` ändern (5.1, Entscheidungen) |
| B23 | Bei *Heller* aus Aus (`RECALL MIN`) setzt das Gateway den internen Zustand sofort auf *Ein*, sendet *Schalten Status* Ein aber erst beim **Stopp** (in Phase 3.9 ≈ 2 s nach dem Einschalten). Code aus dem Ausgangscode (`koHandleDimmRel`), betrifft alle Kanaltypen. | **offen** (nicht verändert) | 5.1 Phase 3.9 (Ein um 00:03:10,7, *Schalten Status* Ein um 00:03:12,8 mit dem Stopp) | Frage an den Maintainer bzw. später; nicht RGBW-spezifisch |
| B24 | Nur Diagnose-Build: Die Pegelabfragen der Diagnose zu festen Zeiten nach einem Befehl (bis 5 s) werden durch einen neuen Befehl nicht abgebrochen. *Dimmen Status* zeigt dann kurz einen Wert des vorigen Fades (3.1: bei 100 % kurz 50,6 %; beim Aus kurz 54,9 %, danach 0 %). | **bestätigt**, betrifft nur `RGBW_DIAG` | 5.1 Phase 3.1 | Keine Änderung, die Diagnose wird vor dem PR entfernt (5.7). Für saubere Statuswerte `diagq 0` |
| B25 | **`UP` bricht einen laufenden Farb-Fade ab.** Kommt *Heller*/*Dunkler* kurz nach einer Farbe, bleibt die Farbe dort stehen, wo der Fade gerade war; das Gateway meldet trotzdem die neue Farbe. Die Farbe kommt erst mit dem nächsten Farbtelegramm oder *Ein* mit Farbe. | **bestätigt** `v13` (20.11.) | 5.1 Phase 3.11: Blau, dann Rot, 200 ms später *Heller*. Log: `0x1E2` (ACTIVATE Rot) bei 50331422 ms, erstes `0x0101` (UP) bei 50331488 ms, also 66 ms später. `getCol` danach: aktuell 14/0/240/0 (≈ 5 % des Wegs Blau → Rot), Farbstatus 20/0/3 `FF 00 00`. Lampe sichtbar fast blau | Betrifft praktisch nur Taster (relatives Dimmen) innerhalb ≈ 1 s nach einem Farbwechsel. Lösung ohne neue Zeitmarke geplant (5.1, Plan, Punkt 3) |
| B26 | **Getrennte Befehle außerhalb des Sammelfensters brechen den laufenden Fade des anderen ab** (Modus 0): Helligkeit nach Farbe bricht den Farb-Fade ab (Farbe bleibt nahe der alten, Status meldet die neue), Farbe nach Helligkeit bricht den Pegel-Fade ab (Pegel bleibt auf halbem Weg, Status meldet das Ziel). | **bestätigt** `v13` (20.11.) | 5.1 Phase 4.3 (Schritt 2), 4.5, 4.6 | Für Gruppen gelöst durch Plan Punkt 4 (immer beide Ziele); für Einzel-EVG offen (5.1, Plan) |

---

## 3. Testübersicht

**Hinweis:** Alle Ergebnisse dieses Abschnitts stammen von `v6`–`v12`. `v13` ändert die Ansteuerung (1.5); die betroffenen Punkte werden mit dem Testablauf in 5.1 neu geprüft. Die bisherigen `v13`-Ergebnisse stehen in [5.1, Ergebnisse der ersten Runde](#ergebnisse-der-ersten-runde-stand-20112026). Achtung bei der Zeile „Grundfunktion“: „Einschalten aus Aus: sofort die neue Farbe“ galt für `v6` (Farbe zuerst, Pegel nach dem Farb-Fade); in `v13` blendet die Farbe beim Einschalten mit neuer Farbe in einem Befehl von der alten Farbe über (B20).

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

- **Szenen/DCA-Konfiguration:** Die Szenen-Farbe (Konfigurationstransfer) kennt nur RGB. Bei RGBW-Kanälen wird dort das Weiß nicht mitgeschrieben (später als eigener Szenen-Parameter geplant). Szenen per KNX-Telegramm (Szenenobjekt) funktionieren über die im Treiber gespeicherten Szenen. Danach kennt das Gateway die Farbe der Szene nicht: Schaltet man nach einer Szene aus und wieder ein, schickt das Gateway seine zuletzt gesetzte Farbe mit und überschreibt die Szenenfarbe. Dasselbe gilt für ein Gruppenmitglied nach einer Gruppenfarbe (1.6) und für Farben, die ein anderer DALI-Master gesetzt hat.
- **Gleicher Treibertyp, gleiche Übergangszeit:** Fade-Zeit und DALI-Version werden nur einmal beim Start gelesen und gelten für alle RGBW-Kanäle. Hat ein RGBW-Treiber eine längere Übergangszeit als der abgefragte, ist in Modus B die Erkennung „eigener Fade läuft noch“ bei ihm zu kurz; ist ein DALI-1-Treiber gemischt mit DALI-2-Treibern verbaut, bekommt er in Modus A Befehle, die er nicht kennt (Farbe allein bliebe wirkungslos). Empfehlung: nur einen RGBW-Treibertyp mit derselben Übergangszeit, sonst `p3mode 2`.
- **Stromausfall nur des Treibers:** Der Treiber startet mit seinem Einschaltwert (*Power-On-Level*, DALI-Norm: ab Werk 254) und seiner Einschaltfarbe; der DA4-M ging nach dem Wiederanschalten **voll weiß** an (5.1 Phase 1.4: aktuell 254/254/254/254). Das Gateway bekommt davon nichts mit. Einschaltwert und Fehlerwert (*System Failure Level*) lassen sich in der ETS einstellen (*Allgemein* → *ETS Version ≥ 6.1.1* = Ja → bei A0 der Reiter *DALI Einstellungen*, „Aus Gerät lesen“ / „Auf Gerät übertragen“). Ein dauerhaftes Mitschicken der Farbe beim Einschalten (C2) fängt den Fall beim nächsten Schalten ab.
- **Nach einem Neustart des Gateways** ist die Farbe bis zum ersten Farbtelegramm unbekannt: Der Treiber behält seine Farbe, die Lampe leuchtet richtig, nur *Farbe RGBW Status* wird bis dahin nicht gesendet, und im Slider-Modus gilt beim Einschalten der Tag/Nacht-Einschaltwert.
- **Getrennte Objekte** (Rot/Grün/Blau/Weiß einzeln, 8 Bit) gibt es bei RGBW *nicht*, nur das kombinierte 6-Byte-Objekt. Das kann später nachgerüstet werden.
- **Andere Treiber:** Manche erwarten für Weiß statt Befehl 236 den Befehl `SET TEMPORARY RGBWAF CONTROL`/`RGBWAF DIMLEVEL` (237) bzw. ein anderes Kanal-Mapping. Beim DA4-M (Modus RGBW) geht es mit 236 (B8). Andere Treiber sind nicht geprüft.
- **HCL** wirkt bei RGBW wie bisher nur auf die Helligkeit; die HCL-Farbtemperatur ist nicht für RGBW gedacht (siehe 5.3).
- **Dimmkurve:** Das Gateway rechnet Prozent fest mit der logarithmischen DALI-Kurve (siehe 5.4).
- **ETS-Applikation:** Der Parameter *colorType* wurde verschoben (B17), die Applikation ist nur gegenüber den in `ReplacesVersions` genannten Versionen updatebar (B16).
- **Bewusst nicht angefasst** (bekannte Lücken dieser Version): Treppenhauslicht-Kanäle (Ein/Aus wie im Ausgangscode, ohne Sammelfenster), Funktionstasten/Identify/ETS-Funktionen des Moduls.

---

## 5. Offene Punkte und Entscheidungen

### 5.1 Ansteuerung in v13: Konzept und Testablauf

**Status: umgesetzt (Branch `v1dev`, Build-Tag `v13-diag`), kompiliert, auf Hardware im Test (erste Runde Phase 0–3.10 durch, siehe [Ergebnisse](#ergebnisse-der-ersten-runde-stand-20112026)).** Die automatische Aktivierung funktioniert am DA4-M (B18, B19), der Rückfall auf `v1dev-v12` ist nicht nötig. Reihenfolge der Rückfälle: Modus A (DALI-2) → Modus B (`p3mode 2`) → Modus C wie RGB (`p3mode 0`). Kann der Treiber die automatische Aktivierung überhaupt nicht (Phase 2 zeigt die alte Farbe, auch nach `aaon`), hilft keiner der drei, dann ist `v1dev-v12` der Rückfall.

**Warum der Wechsel:** `v12` löste die Fade-Abbrüche (B1) mit `QUERY_STATUS`-Abfragen und Warten auf das Ende des Farb-Fades. Das funktioniert am DA4-M, kostet aber zwei Fades nacheinander, Buszeit und Sonderfälle. `v13` folgt deshalb dem RGB-Code und nutzt die automatische Aktivierung der DT8-Norm: Eine temporäre Farbe ohne `ACTIVATE` wird vom folgenden `DAPC` übernommen, Farbe und Pegel faden in einem Fade. Ablauf und Regeln: [1.5](#15-ein--und-ausschalten).

**Voraussetzung:** Der Treiber muss die automatische Aktivierung können und sie muss eingeschaltet sein (nach unserem Verständnis der DT8-Norm eine im Treiber gespeicherte Einstellung, Gear Features/Status Bit 0). Nach der Variablentabelle der Norm (wiedergegeben in der Beckhoff-Dokumentation) ist sie ab Werk und nach RESET eingeschaltet. Der DA4-M hält sich daran (**bestätigt**, B18), obwohl er nur für die Teile 101, 102 und 207 DALI-2-zertifiziert ist, nicht für Teil 209; ob sie sich per `aaon` einschalten lässt, ist weiter ungetestet (war nicht nötig). Andere Treiber: offen.

**Erwartete DALI-Frames** (Diagnose-Log, Zeilen `DALI … ms: 0x…`, Werte nach IEC 62386-209; das Log zeigt die Frames ohne führende Nullen, z. B. `0x1EB`, `0xE5`, `0x0`), Kanal A0 (alle unten genannten bis Phase 3.10 am Log bestätigt; `0x010A` und `0x01EE` stehen noch aus):
- temporäre Farbe: `0xA3rr`, `0xC3gg`, `0xC5bb` (DTR0–2), `0xC108` (Gerätetyp 8), `0x01EB` (temporär RGB), `0xA3ww`, `0xC3FF`, `0xC5FF`, `0xC108`, `0x01EC` (temporär WAF)
- danach entweder `0x01E2` (`ACTIVATE`) oder direkt `0x00nn` (`DAPC` mit dem Pegel) bzw. `0x010A` (`GO TO LAST ACTIVE LEVEL`), **nie** `ACTIVATE` zusammen mit einem Pegelbefehl. Einzige Folge zweier Sequenzen: Farbe an einen ausgeschalteten Kanal (mit `0x01E2`) und danach das Einschalten (zweite Sequenz ohne `0x01E2`, dann `0x00nn`), z. B. Home Assistant aus „aus“ (Phase 4.2)
- Modus A, Helligkeit allein: `0xC108`, `0x01EE` (`COPY REPORT TO TEMPORARY`), dann `0x00nn`
- Ausschalten: `0x0000`; Pegel: 50 % = `0xE5` (229), 80 % = `0xF6` (246), 100 % = `0xFE` (254). Zurückgemeldet wird der Prozentwert des Pegels auf der DALI-Kurve, deshalb meldet *Dimmen Status* nach 80 % den Wert 80,4 % (246) und nach 50 % den Wert 50,6 % (229); das ist richtig
- relatives Dimmen: `RECALL MIN` = `0x0106`, `UP` = `0x0101`, `DOWN` = `0x0102`, `QUERY ACTUAL LEVEL` = `0x01A0`
- beim Start einmal `0x0197` (`QUERY VERSION NUMBER`, Befehl 151), Log `RGBW: DALI version 0x.. = DALI-2/DALI-1, point 3 mode …`

#### Testablauf `v13` (erste Runde: nur ein Einzel-EVG)

Die erste Runde prüft nur die Änderung von `v12` auf `v13` an **einem** Einzel-EVG (Kanal A0): automatische Aktivierung, Sammelfenster, Zeitprüfung im RAM, Modi A/B/C. Gruppen, Szenen, Broadcast, Sperre, HCL und mehrere Kanäle folgen erst in der zweiten Runde (5.6). Jede Phase beginnt erst, wenn die vorige bestanden ist; ein Fehler wird zuerst geklärt.

**Aufbau:**
- Gesendet wird aus **Home Assistant**: einzelne Telegramme per `knx.send`, echte Home-Assistant-Fälle per `light.turn_on` an die Testleuchten (mit und ohne Helligkeitsadresse). Für feste Abstände (20 ms, 300 ms, 3 s) gibt es für den Test das Skript `script.dali_rgbw_testfolge` (Liste von Telegrammen mit Wartezeiten dazwischen); nach dem Test wird es wieder entfernt.
- **Ablauf je Test:** Jeder Test wird vorher beschrieben (was gesendet wird, worauf zu achten ist, was „bestanden“ heißt) und erst nach einem „Go“ gesendet.
- Beobachtet werden die Lampe, der ETS-Gruppenmonitor und die serielle Konsole (Build `v13-diag`, Log-Level *Debug*, jede Farbe meldet `Color absolute [v13-diag]`). Die Statusobjekte werden zusätzlich in Home Assistant gelesen.
- Konsolenbefehle (`aastat`, `aaon`, `getCol`, `p3mode`, `aawin`) werden von Hand eingegeben.
- Testfarben: Rot `FF 00 00 00 00 0F`, Grün `00 FF 00 00 00 0F`, Blau `00 00 FF 00 00 0F`, Weiß `00 00 00 FF 00 0F`.
- Die Diagnose fragt nach jedem Befehl einige Sekunden lang den Pegel ab, *Dimmen Status* zeigt dabei Zwischenwerte. Statusobjekte deshalb etwa 5 s nach dem Befehl bewerten.

**Phase 0 – Start-Log** (keine Aktion)

| # | Prüfen | Erwartung |
|---|---|---|
| 0.1 | Log nach dem Neustart | `RGBW: fade time x=… = … ms` und danach `RGBW: DALI version 0x.. = DALI-2/DALI-1, point 3 mode A/B` (die Versionsabfrage startet erst nach der Fade-Abfrage, weil die DALI-Bibliothek bei zwei offenen Abfragen eine Antwort verliert) |
| 0.2 | `p3mode` und `aawin` ohne Wert | `auto` und 80 |

**Phase 1 – Konsole: automatische Aktivierung im EVG** (Voraussetzung für alles Weitere)

| # | Aktion | Erwartung / festhalten |
|---|---|---|
| 1.1 | `aastat` | `EVG 0: gear features/status 0x.., automatic activation ON/OFF`; Zustand ab Werk als Befund festhalten |
| 1.2 | nur bei `OFF`: `aaon 00`, danach `aastat` | `ON`; im Log festhalten, welche Sendeform gewirkt hat |
| 1.3 | `getCol 00` | Ausgangswerte festhalten: Report 233–236, aktuell 9–12, temporär 201–204 |
| 1.4 | EVG kurz stromlos machen, danach `aastat` | weiterhin `ON` (dauerhaft gespeichert) |

Bleibt es nach `aaon` bei `OFF`, wird Phase 2 trotzdem gemacht: Entscheidend ist das Verhalten der Lampe, die Deutung des Bits ist nicht bestätigt.

**Phase 2 – Kerntest automatische Aktivierung** (`p3mode 0`, `aawin 80`)

`p3mode 0` schaltet die Modi A und B ab, damit nur das Grundverfahren geprüft wird. Jeder Test ist so gebaut, dass der Treiber die neue Farbe **noch nicht** aktiv hat; nur dann belegt er die automatische Aktivierung. (Eine Farbe an einen ausgeschalteten Kanal übernimmt der Treiber schon mit `ACTIVATE`; ein Einschalten danach zeigt die neue Farbe auch ohne automatische Aktivierung, siehe Phase 3.2/3.3.)

| # | Ausgang | Aktion | Erwartung |
|---|---|---|---|
| 2.1 | Lampe an, Rot 50 % | nur Blau | Gegenprobe des bekannten Wegs: Log `colour (ACTIVATE)`, Frames mit `0x01E2`, Lampe fadet zu Blau, Helligkeit bleibt |
| 2.2 | Lampe an, Blau 50 % | Grün, 20 ms später *Dimmen absolut* 80 % | Grün 80 % in **einem** Fade. Log **ein** `level 246 with temporary colour (one fade)`; Frames `0x01EB`, `0x01EC`, **kein** `0x01E2`, dann `0x00F6`. Blau mit 80 % = keine automatische Aktivierung. Zusätzlich beobachten: fadet die Farbe weich mit oder springt sie? |
| 2.3 | Lampe an, Grün 80 % | Rot, 20 ms später *Dimmen absolut* 80 % (gleicher Pegel) | Rot: Aktivierung auch ohne Pegeländerung. Beobachten: fadet die Farbe (mit der Fade-Zeit) oder springt sie? |
| 2.4 | *Schalten* Aus (der Treiber hat die alte Farbe) | *Dimmen absolut* 50 %, 20 ms später eine andere Farbe | Log ein Befehl, kein `0x01E2`; Lampe kommt in **einem** Fade bei ≈ 50 % in der neuen Farbe an. Die Farbe blendet dabei von der alten Farbe über, bei niedrigem Pegel ist kurz eine Mischfarbe zu sehen (normgerecht, B20). ~~„Lampe kommt direkt in Blau, kein Rot zu sehen“~~ (**widerlegt**, 2.4/2.4a) |
| 2.5 | nach 2.4 | `getCol 00` | aktuell 9–12 = neue Farbe. Temporär 201–204 nur notieren: ~~„nach der Norm 255 (MASK, nach der Aktivierung zurückgesetzt)“~~ – der DA4-M setzt sie nicht zurück (B21). Report 233–236 nur notieren (kein Ist-Wert, B22) |

Zeigen 2.2–2.4 die alte Farbe, trägt `v13` am DA4-M nicht: Rückfall `v1dev-v12`, Ergebnis als Befund festhalten.

**Phase 3 – Grundverhalten** (`p3mode 0`, `aawin 80`)

| # | Aktion | Erwartung |
|---|---|---|
| 3.1 | *Schalten* Ein; *Dimmen absolut* 50 % und 100 %; *Schalten* Aus | Lampe folgt, Status richtig. Ein mit bekannter Farbe: Log `level N with temporary colour (one fade)`. Aus: sofort `0x0000`, Log `RGBW: off` |
| 3.2 | Lampe aus, Rot | Lampe **bleibt aus**, *Schalten Status* bleibt *Aus*, Farbe Status `FF 00 00 00 00 0F`; Log `colour for a switched-off channel, lamp stays off`, Frames mit `0x01E2` |
| 3.3 | danach *Schalten* Ein | Rot in einem Fade (prüft Ablauf und Status, nicht die automatische Aktivierung) |
| 3.4 | Lampe an (Rot 100 %): nur Gültigkeitsbit W mit 30 %, dann nur R mit **50 %** (nicht 100 %: Rot stand schon auf 100 %, die Änderung wäre nicht zu sehen), dann ohne Gültigkeitsbit | nur W bzw. nur R ändert sich; ohne Gültigkeitsbit Log `no valid channel in telegram, ignored`, nichts passiert |
| 3.5 | alle vier Kanäle 0 % | Lampe aus (`0x0000`), Status *Aus* / 0 %, Farbe Status unverändert; Log `all channels 0 -> switching off` |
| 3.6 | danach *Schalten* Ein | zuletzt gesetzte Farbe |
| 3.7 | *Dimmen absolut* 0 %, dann Grün, dann *Schalten* Ein | Grün lässt die Lampe aus, *Ein* bringt Grün (der Merker hängt nicht auf *Ein*) |
| 3.8 | Lampe aus, *Dimmen absolut* 50 % | gespeicherte Farbe mit 50 %, ein Fade |
| 3.9 | **Haltedauer (B5b):** Lampe aus, eine andere Farbe als zuletzt, 1 min warten, `getCol 00`, danach *Dimmen relativ: Heller* aus Aus (**nicht** vorher einschalten) | aktuell 9–12 = neue Farbe (Report nur notieren), `RECALL MIN` kommt in der neuen Farbe. Grundlage für die Entscheidung über C2 (unten) |
| 3.10 | *Heller* bei brennender Lampe + Stopp; *Dunkler* bis zum Minimum | Status nach dem Stopp = echter Pegel (B11); am Minimum aus, Dimmen endet, keine Telegrammflut (B10) |
| 3.11 | Vorbereitung: *Schalten* Ein, *Dimmen absolut* 50 % (bei 100 % tut *Heller* nichts). Dann eine andere Farbe, 200 ms später *Heller*, nach 2 s Stopp, danach `getCol 00` | ~~„Mischfarbe möglich (hingenommen), nur beobachten“~~. Nach der geplanten Änderung (Plan, Punkt 3): nach dem Ende des Dimmens blendet die Lampe zur neuen Farbe, `getCol` aktuell = neue Farbe |
| 3.12 | Farbverlauf: etwa 20 Farben im Abstand von ≈ 0,5 s | je Farbe `colour (ACTIVATE)`, keine Warnungen, Verlauf weich |

**Phase 4 – Sammelfenster** (`p3mode 0`)

| # | Aktion | Erwartung |
|---|---|---|
| 4.1 | Lampe an: Farbe, 20 ms später Helligkeit; danach umgekehrt | jeweils **ein** Befehl `level N with temporary colour (one fade)`, Farbe und Pegel richtig |
| 4.2 | Lampe aus: `light.turn_on` mit Farbe und Helligkeit (Home Assistant sendet die Farbe zuerst) | Lampe richtig, ein sichtbarer Fade. Log **zwei** Sequenzen: `colour for a switched-off channel` (mit `0x01E2`), dann `level N with temporary colour (one fade)` (bekannt, siehe C1) |
| 4.3 | `aawin 0`, dann 4.1 wiederholen | zwei Befehle; in Modus C sind Mischfarbe oder falscher Pegel möglich (zeigt, wofür das Fenster da ist) |
| 4.4 | `aawin 80` | zurückgestellt |
| 4.5 | **neu (20.11.):** Lampe an: Farbe, **300 ms** später Helligkeit (außerhalb des Sammelfensters, noch im Farb-Fade) | Modus 0: zwei Befehle; der `DAPC` bricht den Farb-Fade ab, die Farbe bleibt auf halbem Weg. Nur beobachten: wie stark? `getCol 00` |
| 4.6 | **neu (20.11.):** Lampe an: Helligkeit, **300 ms** später Farbe | Modus 0: zwei Befehle; `ACTIVATE` bricht den Pegel-Fade ab, die Helligkeit bleibt auf halbem Weg. Nur beobachten: wie stark? |

**Phase 5 – Modus B, Zeitprüfung im RAM** (`p3mode 2`; Zeitfenster = 310 ms + Fade-Zeit + 300 ms, bei 1 s Fade-Zeit ≈ 1,6 s)

| # | Aktion (Lampe an) | Erwartung |
|---|---|---|
| 5.1 | mindestens 3 s nichts senden, dann nur Farbe | `colour (ACTIVATE)` |
| 5.2 | *Dimmen absolut* 50 %, 300 ms später Farbe | `colour with the running level target 229 (one fade)`; neue Farbe **und** 50 % erreicht |
| 5.3 | Farbe, 300 ms später *Dimmen absolut* 80 % | `level 246 with temporary colour (own colour fade may still run)`; neue Farbe **und** 80 %, keine Mischfarbe |
| 5.4 | wie 5.2 bzw. 5.3, aber 3 s Abstand | abgelaufen: `colour (ACTIVATE)` bzw. nur `level N` |
| 5.5 | *Dimmen absolut* 50 %, 300 ms später *Heller*, 300 ms später Stopp, 300 ms später Farbe | `colour (ACTIVATE)`: das relative Dimmen hat den gemerkten Zielpegel verworfen, die Lampe springt nicht auf 50 % zurück |

**Phase 6 – Modus A, Ziele vom Treiber** (`p3mode 1`; bei DALI-2 entspricht das `auto`)

| # | Aktion | Erwartung |
|---|---|---|
| 6.1 | **Report während eines Fades:** Fade-Zeit des EVG vorübergehend lang stellen (z. B. 8 s, EVG-Einstellungen in der ETS), Lampe an, Farbe ändern, sofort `getCol 00` (Report und aktuell direkt nacheinander); danach Fade-Zeit zurückstellen | Report 233–236 = Zielfarbe, aktuell 9–12 = Zwischenwerte. Nach IEC 62386-209:2011 ist die Report-Farbe allerdings ein Abfragepuffer (gefüllt u. a. von STORE ACTUAL LEVEL IN DTR und den Abfragen POWER ON / SYSTEM FAILURE / SCENE LEVEL), die Zielfarbe ist dort also nicht zu erwarten. Zeigt Report nicht die Zielfarbe, taugt Modus A für „Helligkeit allein“ nicht → Modus B |
| 6.2 | Lampe an (mindestens 3 s ruhig), nur Farbe | `colour with the driver's own level (go to last active level)`, Frames mit `0x010A`, ohne `0x01E2`; Helligkeit bleibt |
| 6.3 | Lampe an, nur *Dimmen absolut*: einmal ruhig, einmal 300 ms nach einer Farbe | `level N with the driver's own colour (copy report to temporary)`, Frames `0x01EE`, `0x00nn`; Farbe bleibt bzw. läuft zur Zielfarbe weiter |
| 6.4 | *Dimmen absolut*, 300 ms später Farbe | neuer Pegel **und** neue Farbe, kein Rücksprung auf den alten Pegel |
| 6.5 | *Heller* + Stopp, danach Farbe | die gedimmte Helligkeit bleibt |
| 6.6 | Lampe aus, Farbe | Lampe bleibt aus, Frames mit `0x01E2`, **kein** `0x010A` |
| 6.7 | Farbverlauf wie 3.12 | je Farbe `0x010A`, Verlauf weich, Helligkeit konstant |
| 6.8 | **Alter Report:** `diagq 0`, Lampe an in Rot, *Heller* + Stopp (das Gateway fragt danach den Pegel ab), 3 s warten, Blau, 3 s warten, *Dimmen absolut* 50 % | Farbe bleibt Blau. Springt sie auf Rot, kopiert `COPY REPORT TO TEMPORARY` eine alte Farbe → Modus A für „Helligkeit allein“ nicht verwenden. Danach `diagq 1`. (Dass der Report veralten kann, zeigen schon 3.2 und 3.9, B22; 6.8 prüft, ob das bei brennender Lampe im Betrieb zu einer falschen Farbe führt) |

Phase 6.2–6.5 zusätzlich einmal mit `diagq 0` wiederholen: Die Pegelabfragen der Diagnose können den Report-Puffer füllen und so ein anderes Ergebnis erzeugen als im Betrieb ohne Diagnose.

**Phase 7 – Slider-Modus** (ETS: *Helligkeit bei RGBW = Aus dem RGBW-Wert*, Applikation programmieren; danach zurückstellen)

| # | Aktion | Erwartung |
|---|---|---|
| 7.1 | Lampe aus: `80 00 00 80 00 0F` | schaltet mit ≈ 50 % Rot-Weiß ein, **ein** Befehl, kein `0x01E2` (belegt die automatische Aktivierung auch hier) |
| 7.2 | Lampe an: `00 00 FF 00 00 0F`, danach `light.turn_on` an die Testleuchte ohne Helligkeitsadresse | Farbe und Pegel jeweils in einem Fade |
| 7.3 | *Dimmen absolut* 20 % | Farbe Status = Mischung × Pegel |
| 7.4 | alle vier Kanäle 0 % | aus, Farbe bleibt gespeichert |
| 7.5 | *Schalten* Ein | Tag-Einschaltwert in der gespeicherten Mischung |

**Phase 8 – Neustart des Gateways**

| # | Aktion | Erwartung |
|---|---|---|
| 8.1 | Lampe an, Gateway neu starten | kein Aufblitzen, Status *Ein* mit Pegel; Farbe Status wird nicht gesendet |
| 8.2 | danach *Dimmen absolut* | Modus A: `copy report to temporary`; Modus B/C: nur `level N`. Die Farbe des Treibers bleibt |
| 8.3 | Lampe aus, Gateway neu starten, *Schalten* Ein | nur `level N` (Farbe unbekannt), Lampe in der Farbe des Treibers |
| 8.4 | erste Farbe danach | normales Verhalten, Farbe Status wird gesendet |

#### Ergebnisse der ersten Runde (Stand 20.11.2026)

Aufbau: GW-REG1-Dali mit `v13-diag`, ETS-Applikation 0.15, DA4-M (RGBW) an A0, Fade-Zeit 1 s, Tag-Einschaltwert 100 %. Gesendet aus Home Assistant (`script.dali_rgbw_testfolge`), beobachtet an Lampe, ETS-Gruppenmonitor und serieller Konsole. Phase 2 und 3 mit `p3mode 0`, `aawin 80`.

| # | Ergebnis | Beobachtung / Log |
|---|---|---|
| 0.1 | **bestanden** | `RGBW: fade time x=2 = 1000 ms` (raw `0x27`), danach `DALI version 0x08 = DALI-2`, Modus A. Die Versionsabfrage nach der Fade-Abfrage umgeht den Fehler der DALI-Bibliothek. Bekannte Warnungen `loop took longer` und `unhandled KO: 1` |
| 0.2 | **bestanden** | `auto`, 80 |
| 1.1 | **bestanden** | `aastat`: `0x01`, ON ab Werk (B18); 1.2 entfällt |
| 1.3 | notiert | Farbtyp-Merkmale `0x80`, Farbstatus `0x80`; Report = aktuell 0/254/0/0 (Grün), temporär 255. Einmal ein Fehlerframe `0x0` mit `flags 0x81` während `getCol`, die Antwort kam trotzdem richtig (beobachten) |
| 1.4 | **bestanden** | nach Abschalten der Versorgung weiter ON. Der DA4-M ging dabei voll weiß an (Einschaltwert, Abschnitt 4); aktuell 254 × 4, Report MASK (B22) |
| 2.1 | **bestanden** | nur Blau: `colour (ACTIVATE)`, `0x1E2`, Pegel bleibt 229, Fade ≈ 1,5 s |
| 2.2 | **bestanden** | Blau 50 % → Grün + 80 %: ein Befehl `level 246 with temporary colour (one fade)`, `0x1EB`, `0x1EC`, `0xF6`, **kein** `0x1E2`; Pegel 230 → 236 → 241 → 246, Farbe fadet weich mit (B19) |
| 2.3 | **bestanden** | Farbe + 80 % bei gleichem Pegel: Pegel konstant 246, `fadeRunning` ≈ 1,3 s; Lampe blendet mit Mischfarbe (Rot/Blau) weich über, Helligkeit bleibt (B19) |
| 2.4 / 2.4a | Gateway **bestanden**, Lichteindruck **akzeptiert** | Aus → 50 % + neue Farbe: ein Befehl, kein `0x1E2`; Lampe blendet von der alten Farbe über (Blau → Grün: kurz Türkis; Grün → Rot: Gelb/Orange), normgerecht (B20). Entscheidung 19.11.: so lassen |
| 2.5 / 2.5b | aktuell **bestanden**, temporär **abweichend** | aktuell = neue Farbe; temporär nicht MASK, sondern die zuletzt gesendete Farbe, auch nach Farbe + `DAPC` bei brennender Lampe (2.5b) (B21) |
| 3.1 | **bestanden** | Aus → Ein (`Turning on day value`, `level 254 with temporary colour`) → 50 % (nur `0xE5`) → 100 % (nur `0xFE`) → Aus (`0x0`). Zwischenwerte im Dimmstatus nur durch die Diagnose (B24) |
| 3.2 | **bestanden** | Lampe aus, Rot: bleibt aus, nur Farbstatus; `colour for a switched-off channel, lamp stays off`, `0x1E2`. `getCol`: aktuell Rot, temporär Rot, **Report noch Grün** (B21, B22) |
| 3.3 | **bestanden** | danach Ein: direkt in Rot, weich auf 100 %, **keine** Mischfarbe; kein `0x1E2` |
| 3.4 | **bestanden** | nur W 30 %: `Got RGBW: 255 0 0 77 (valid 1)`; nur R 50 %: `128 0 0 77 (valid 8)`; je `colour (ACTIVATE)`, kein `DAPC`. Ohne gültigen Kanal: `no valid channel in telegram, ignored`, kein Frame, kein Status |
| 3.5 | **bestanden** | alle 0: `all channels 0 -> switching off`, nur `0x0`, kein Weißblitz; Schalt-/Dimmstatus Aus/0 %, **kein** neuer Farbstatus |
| 3.6 | **bestanden** | Ein: gespeicherte Farbe 128/0/0/77 als temporärer Wert + `0xFE`, weich |
| 3.7 | **bestanden** | 0 % (`RGBW: off`) → Grün (bleibt aus, `0x1E2`) → Ein (Grün, weich, kein `0x1E2`); der Merker steht nach „Dimmen auf 0“ richtig auf *Aus* |
| 3.8 | **bestanden** | Aus → nur *Dimmen absolut* 50 %: Grün mit 50 %, ein Fade (16 → 102 → 191 → 229), kein `0x1E2` |
| 3.9 | **bestanden** | Blau bei Pegel 0, nach ≈ 1,5 min `getCol`: aktuell Blau, Report noch Grün; *Heller* aus Aus (`starting with min 1`, `0x106`, dann `0x101`, keine Farbframes): Lampe in reinem Blau (B5b). *Schalten Status* Ein erst beim Stopp (B23) |
| 3.10 | **bestanden** | *Heller* 82 → 162, Stopp, Abfrage 172 → Status 10,6 % (B11). *Dunkler* 172 → … → 2, `Dimm Stop at: 1`, `Turn off device`, `0x0`; Stopp 4 s später ohne Frame (B10) |
| 3.11 | **nicht bestanden** | Rot, 200 ms später *Heller*: Lampe bleibt fast blau, `getCol` aktuell 14/0/240/0, Farbstatus meldet Rot (B25). *Heller* lief bis `Dimm Stop at max 254`, der Stopp kam danach |
| 4.1 | **bestanden** | Rot allein: `colour (ACTIVATE)`. Grün, 20 ms später 50 %: **ein** Befehl `level 229 with temporary colour (one fade)`, kein `0x1E2`. 80 %, 20 ms später Blau (Helligkeit zuerst): **ein** Befehl `level 246 with temporary colour (one fade)`, kein `0x1E2` |
| 4.2 | **bestanden** | Aus, dann `light.turn_on` (`light.dali_rgbw_test`, Rot, 50 %): Home Assistant sendet Farbe und Helligkeit 20 ms nacheinander; zwei Sequenzen (C1), der `DAPC` kam ≈ 280 ms nach dem `ACTIVATE`; Ende Rot 50,6 %. Beim Einschalten kurz Lila (B20). Die Lampe wirkt in den ersten ≈ 0,7 s dunkel (logarithmische DALI-Kurve: 0,1 % nach 535 ms, 0,4 % nach 738 ms); mit C1 käme sie ≈ 0,28 s früher (eine statt zwei Sequenzen) |
| 4.3 | beobachtet | `aawin 0`. Grün, 20 ms später 80 %: zwei Befehle (`0x1E2`, 26 ms später `0xF6`), Farbe trotzdem Grün; vermutlich wendet der `DAPC` die stehengebliebene temporäre Farbe neu an (B21, nicht normgerecht). 50 %, 20 ms später Blau: zwei Befehle, `ACTIVATE` 327 ms nach dem `DAPC` bricht den Pegel-Fade ab: **Pegel bleibt bei 240 (68 %) statt 229 (50 %)**, Farbe Blau. Danach `aawin 80` |
| 4.5 | beobachtet, **Fehler wie erwartet** | Rot, 300 ms später 80 %: auf dem DALI-Bus nur 65 ms zwischen `ACTIVATE` und `DAPC` (der `DAPC` wartet hinter den Farbframes). Der `DAPC` bricht den Farb-Fade ab: `getCol` aktuell 15/0/239/0 (≈ 6 % des Wegs Blau → Rot), Farbstatus meldet `FF 00 00`. Pegel 80 % richtig. Die Vermutung aus 4.3 („der DA4-M repariert das über B21“) ist damit **widerlegt** |
| 4.6 | beobachtet, **Fehler wie erwartet** | 50 %, 300 ms später Grün: `ACTIVATE` 627 ms nach dem `DAPC` bricht den Pegel-Fade ab: Pegel bleibt bei 235 (≈ 60 %) statt 229 (50 %); Farbe Grün richtig. *Dimmen Status* zeigt 59,6 % nur dank der Diagnose-Abfragen, ohne Diagnose bliebe er bei 50,6 % |
| 5.1–8.4 | offen | vor Phase 5/6: offene Punkte des Plans entscheiden, umsetzen, dann Schnelldurchlauf Einzel-EVG und Gruppe |

#### Plan nach Phase 3 (Stand 20.11.2026)

Ziel: Der DT8-Code soll möglichst **ohne Merker und Zeitmarken** auskommen. Vorgehen: Phase 4 (mit `p3mode 0`) einschließlich 4.5/4.6 testen; **vor Phase 5/6** entscheiden, ob die Punkte 1–3 umgesetzt werden. Bis dahin ist nichts davon im Code.

1. **Modus 0 (C, wie RGB) fest, A/B entfallen** (**entschieden** 20.11.; A/B bleiben hier als verworfen dokumentiert). Begründung: A braucht DALI-2 (`GO TO LAST ACTIVE LEVEL`); viele RGBW-Treiber sind DALI-1 (RGBWAF ist erst seit Oktober 2021 Teil der DALI-2-Zertifizierung, davor nur DALI version-1; der DA4-M meldet DALI-2, ist für Teil 209 aber nicht zertifiziert). Modus 0 nutzt nur temporäre Farbe, `ACTIVATE` und `DAPC` aus Teil 209:2011 und hängt nicht von der DALI-Version ab; Voraussetzung bleibt die automatische Aktivierung (C4). Außerdem ist A für „Helligkeit allein“ wegen B22 fraglich. Entfallen würden: `p3mode`, `_levelSentAt`, `_levelTarget`, `_colourSentAt`, `levelFadeMayRun()`, `colourFadeMayRun()`, `rgbwFadeWindowMs()`, die Fade-Zeit-Abfrage beim Start (`rgbwFadeMs`, `loopFadeQuery`) und die DALI-Versionsabfrage (`rgbwDali2`, `loopVersionQuery`, damit auch der Umweg um den Bibliotheksfehler); Phase 5 und 6 entfallen dann. **Nachteil von Modus 0:** Helligkeit allein außerhalb des Sammelfensters, aber noch im Farb-Fade, bricht den Farb-Fade ab, und umgekehrt (dafür waren A/B gedacht). Phase 4.5/4.6 haben es bestätigt (B26); für Gruppen löst es Punkt 4, für Einzel-EVGs ist es gewolltes Verhalten (Punkt 4).
2. **Sammelfenster (80 ms) bleibt** (**entschieden** 20.11.; Phase 4.3 zeigt, was ohne Fenster passiert). Es ist eine kurze Zeitmarke, aber ohne es würden getrennt, kurz nacheinander gesendete Farbe und Helligkeit (z. B. Home Assistant) zu zwei Befehlen, und der zweite bräche den Fade des ersten ab (wie B25, nur bei *Dimmen absolut*).
3. **Lösung für B25 (3.11) ohne neue Zeitmarke** (**entschieden** 20.11.): über die vorhandene Pegelabfrage 300 ms nach dem Ende des relativen Dimmens (`_stopQueryAt`, B11, Ausgangscode). Bei RGBW schickt das Gateway dann zusätzlich **immer** seine gespeicherte Farbe mit `ACTIVATE` (`sendColor(true)`, kein neuer Farbstatus); ist die Farbe schon richtig, ist nichts zu sehen. Gilt für das Ende per Stopp-Telegramm (Fall 1, `koHandleDimmRel`) und für das Ende am Maximum (Fall 2, `loopDimming`: dort wird `_stopQueryAt` zusätzlich gesetzt, das korrigiert nebenbei auch den Dimmstatus). Ende am Minimum mit Ausschalten (Fall 3): nichts, das nächste *Ein* bringt die Farbe mit; nur *Heller* aus Aus käme in der stehengebliebenen Mischfarbe (dokumentieren). Kosten: ≈ 11 Frames (≈ 0,3 s Bus) nach jedem relativen Dimmen eines RGBW-Kanals. Verworfen: ein eigener Merker „Farb-Fade läuft“ mit Zeitprüfung (widerspricht dem Ziel ohne Merker).

4. **Gruppe = Standard, Einzel-EVG = Ausnahme** (**entschieden** 20.11.). In typischen Anlagen werden Leuchten über Gruppen angesteuert, die Einzelansteuerung ist der Eingriff „diese eine Leuchte anders“. Bei brennender Lampe:

   | | Farbe allein | Helligkeit allein | Farbe + Helligkeit innerhalb 80 ms |
   |---|---|---|---|
   | **Gruppe** | temporäre Farbe + `DAPC` mit dem Gruppenpegel des Gateways | temporäre Farbe (Gruppenfarbe) + `DAPC` | ein Befehl (wie 4.1) |
   | **Einzel-EVG** | `ACTIVATE`, der Pegel bleibt | `DAPC` allein, die Farbe bleibt | ein Befehl (wie 4.1) |

   Ein Gruppenbefehl gibt damit immer den ganzen Zustand vor (beide Ziele, ein Fade); ein laufender Fade wird nicht abgebrochen (löst den Fall aus 4.3, Schritt 2, für Gruppen), ohne Merker. Ist die Gruppe aus, bleibt es bei `ACTIVATE` (Lampe bleibt aus). Zu beachten: (a) Ein einzeln gedimmtes Mitglied wird bei einem Farbwechsel auf die Gruppe auf den Gruppenpegel zurückgesetzt (gewollt: die Gruppe gibt den Zustand vor). (b) Das Gateway kennt nur den Pegel und die Farbe, die es zuletzt selbst an die Gruppe gesendet hat; Änderungen anderer DALI-Master kennt es nicht. Einzel-EVG bewusst **ohne** mitgeschickten Pegel: das Gateway führt die Zustände der Mitglieder nach Gruppenbefehlen nicht nach (Ausgangscode, `loopGroupState` wird nie aufgerufen, siehe Prüfbericht), ein mitgeschickter Pegel könnte deshalb veraltet sein. Wer bei einem Einzel-EVG Farbe und Helligkeit ändern will, sendet beides innerhalb des Sammelfensters. Werden sie getrennt gesendet, kann der zweite Befehl den Fade des ersten abbrechen (B26); das ist beim Einzel-EVG **gewolltes, dokumentiertes Verhalten**, kein Fehler.

5. **C1 umsetzen** (**entschieden** 20.11.): Auch eine Farbe an einen ausgeschalteten Kanal läuft durch das Sammelfenster. Kommt in den 80 ms ein *Ein* oder eine Helligkeit, geht beides als **ein** Befehl raus (temporäre Farbe + `DAPC`; Home Assistant aus „aus“ ≈ 0,28 s schneller, 4.2); sonst nach 80 ms wie bisher Farbe + `ACTIVATE`, Lampe bleibt aus. Damit gilt: jeder Befehl an einen RGBW-Kanal läuft durch das Sammelfenster. Die Mischfarbe beim Einschalten (B20) bleibt.
6. **Broadcast** (**entschieden** 20.11.): DALI kennt keinen eigenen Broadcast-Befehl, Broadcast ist neben Einzel- und Gruppenadresse die dritte Adressart (Adressbyte `1111111S`, alle Treiber); jeder Befehl wirkt dort wie bei einer Gruppe. *Broadcast Schalten Ein* = nur `DAPC 254` an alle, **keine Farbe**; das bisherige einzelne Nachsenden der Gateway-Farbe an jeden ausgeschalteten RGBW-Kanal und jede RGBW-Gruppe (`rgbwBroadcastOn()`, in `v13` hinzugekommen, ≈ 0,3 s je Kanal, bei 20 Kanälen ≈ 6 s) **entfällt**. *Broadcast Schalten Aus* (`DAPC 0`) und *Broadcast Dimmen* (`DAPC` mit Pegel) bleiben wie im Ausgangscode. Jede RGBW-Leuchte kommt in ihrer Treiberfarbe (B5b); abweichen kann sie nach Stromausfall nur des Treibers (weiß), nach B26 oder nach einer Szene. Für bestimmte Farben Gruppen oder Szenen verwenden. Später (nächster ETS-Schritt): Objekt „Broadcast RGBW“ (Farbe + Pegel in einem Befehl an alle; Farbe wirkt nur auf RGBW-Treiber, der Pegel auf alle Treiber).
7. **Szenen** (**entschieden** 20.11.): unverändert. `GO TO SCENE` an Adresse, Gruppe oder Broadcast; jeder Treiber fährt seine gespeicherten Werte (Pegel und Farbe) in einem Fade an. Szenen sollten mit Pegel **und** Farbe gespeichert werden (Farbe MASK = Treiber behält seine Farbe). Das Gateway führt Status und Farbe nach einer Szene nicht nach (Ausgangscode); der nächste Gruppenbefehl bringt wieder die Gruppenfarbe (Gruppe = Standard).
8. **Sperre bei RGBW** (**entschieden** 20.11.): Die Sperre ist eine gemeinsame Funktion aller Kanaltypen (`koHandleLock`, Ausgangscode) und sendet nur einen `DAPC`, keine Farbe. Gefundene Fehler: **S1** „Anschalten“ (ETS-Wert 1) und „Ausschalten“ (Wert 2) vertauscht; **S2** „Fester Wert in %“ wird als DALI-Pegel gesendet (50 % → Pegel 50 ≈ 0,1 %), der Status dagegen als Prozent; **S3** Gruppen lesen „Verhalten bei Sperren/Entsperren“ und die Werte aus den Kanal-Parametern (`ParamDGW_…` statt `ParamDGWG_…`); **S4** der Ein/Aus-Merker (`currentState`) wird nicht gesetzt (für die RGBW-Logik nötig). Entscheidung: **für RGBW-Kanäle und -Gruppen S1–S4 korrigieren** (eigener Zweig für RGBW), alle anderen Kanaltypen bleiben wie im Ausgangscode; S1–S3 für die anderen Kanaltypen für den Maintainer dokumentieren (Abschnitt 5.7). RGBW bei Sperren/Entsperren: nur Pegel, keine Farbe (Leuchte in der Treiberfarbe).
9. **RGBW-Entsperren → vorheriger bzw. nachgeführter Zustand** (**entschieden** 20.11.: Variante (b), neue Auswahl in der ETS, **nächster ETS-Schritt** zusammen mit „Broadcast RGBW“; bis dahin Entsperren wie Punkt 8) (während der Sperre empfangene Befehle werden gespeichert, beim Entsperren ein Befehl: an → temporäre Farbe + `DAPC` mit gespeichertem Pegel; aus → `DAPC 0` und Farbe per `ACTIVATE`). Einstellung entweder (a) ohne ETS-Änderung: „Keine Änderung“ bedeutet bei RGBW „zurück zum Zustand“, oder (b) mit ETS-Änderung: neue Auswahl (nächster ETS-Schritt).

Nach der Umsetzung: Build, Update, dann die Testprozedur im Schnelldurchlauf **für das Einzel-EVG und für eine Gruppe** (A0 als Mitglied einer DALI-Gruppe mit eigenen Gruppenadressen, vorher in der ETS anlegen), dazu Broadcast, Szene und Sperre; darin 3.11 wiederholen.

**Entscheidungen nach der ersten Runde:**
- ~~Phase 2 nicht bestanden → Rückfall `v1dev-v12`.~~ Phase 2 ist bestanden (B19), kein Rückfall.
- Fehler in Phase 3, 4, 7 oder 8 → Fehler in `v13`, zuerst beheben. Phase 3.1–3.10: keine Fehler in `v13`.
- Standard für `p3mode`: Phase 6 bestanden → `auto` (A bei DALI-2); sonst Phase 5 bestanden → Modus B; sonst Modus C. Hinweis: Wegen B22 ist Phase 6.8 hier entscheidend.
- Phase 3.11 nicht bestanden (B25): Lösung siehe Plan, Punkt 3.
- Phase 3.9 entscheidet über C2: Phase 3.9 ist bestanden, das Argument „Treiber vergisst die Farbe“ entfällt (B5b). Die Entscheidung über C2 ist noch offen (siehe C2).

**Mögliche Code-Änderungen nach der ersten Runde** (bewusst noch nicht umgesetzt):
- **C1** (**entschieden** 20.11., Plan Punkt 5)**:** Eine Farbe an einen ausgeschalteten Kanal läuft nicht über das Sammelfenster, deshalb gehen bei Home Assistant aus „aus“ zwei Farbsequenzen raus (Phase 4.2). Möglich: die Farbe auch dann ins Fenster legen und `ACTIVATE` nur senden, wenn keine Helligkeit folgt.
- **C2:** Das Einschalten schickt immer die Farbe des Gateways mit. Abwägung: Es **hilft**, wenn der Treiber eine andere Farbe hat als zuletzt gesendet, z. B. nach einem Stromausfall nur des Treibers (er startet dann mit seinen Einschaltwerten, am DA4-M voll weiß, Abschnitt 4). ~~„oder falls er eine bei ausgeschalteter Lampe gesetzte Farbe nicht länger hält (B5b, Phase 3.9)“~~ – **widerlegt**, der DA4-M hält sie (B5b). Weicht die Farbe ab, blendet sie beim Einschalten von der Treiberfarbe über (B20). Wegen B21 kann ein späterer Pegelbefehl ohne Farbe die zuletzt gesendete Farbe ohnehin erneut anwenden. Es **schadet**, weil es Szenen- und Gruppenfarben und Farben eines anderen DALI-Masters überschreibt (Abschnitt 4), und es kostet ≈ 310 ms Buszeit pro Kanal; im Normalfall hat der Treiber die Farbe schon. Alternative: Das Einschalten sendet wie bei RGB nur den `DAPC`. Entscheidung nach den Tests.
- **C3** (**entfällt**, Modus B verworfen)**:** `rgbwCancel` setzt den Zeitpunkt der letzten Farbe nicht zurück. In Modus B kann eine Helligkeit, die innerhalb von ≈ 1,6 s nach Farbe und Szene kommt, die Szenenfarbe überschreiben.
- **C4:** Das Gateway liest die Einstellung *automatische Aktivierung* bisher nie selbst. Ist sie aus, bleibt in Modus A eine Farbe allein ganz ohne Wirkung, und bei Farbe + Helligkeit bleibt die alte Farbe. Möglich: beim Start einmal am ersten RGBW-Kanal den Gear-Status abfragen (wie die DALI-Version), bei *aus* eine Warnung ins Log schreiben und auf Modus C schalten. Ergebnis Phase 1: am DA4-M ab Werk an (B18); für andere Treiber weiter offen. Entscheidung steht noch aus.
- **C5 (neu, nur Hinweis):** *Schalten Status* Ein bei *Heller* aus Aus erst beim Stopp (B23, Ausgangscode, alle Kanaltypen). Nicht RGBW-spezifisch; Frage an den Maintainer, keine Änderung geplant.

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

### 5.6 Zweite Testrunde (nach 5.1, Reihenfolge)

1. Zweiter RGBW-Kanal und mehrere Kanäle zugleich (Auslastung der DALI-Warteschlange, Farbsequenz ≈ 310 ms pro Kanal, Reihenfolge)
2. Gruppen, jeweils eine Gruppe nach der anderen; dabei in Modus A ein einzeln ausgeschaltetes Mitglied einer eingeschalteten Gruppe (bleibt es bei einer Farbe an die Gruppe aus?) und ein Mitglied, das nach einer Gruppenfarbe einzeln geschaltet wird (1.6)
3. Broadcast Ein/Aus (1.8), Sperre, Tag/Nacht-Einschaltwerte, KNX-Szenen mit Farbe, im Treiber gespeicherte Szenen (Abschnitt 4)
4. Treppenhauslicht mit RGBW, HCL mit RGBW (5.3)
5. Regression der anderen Farbtypen (RGB, HSV, TW, xyY) und anderer Kanaltypen (Testfall 14, Diagnose dafür auskommentieren)
6. Weitere Mischungen mit Weiß bei brennender Lampe
   Dazu (aus der ersten Runde): Wirkung von B21 (temporäre Werte bleiben stehen) nach einer Szene bzw. Gruppenfarbe und anschließendem Pegelbefehl ohne Farbe
7. Test durch andere Anwender (insbesondere Befehl 237 bei anderen Treibern, B8)
8. Treiber mit unterschiedlicher Übergangszeit (Abschnitt 4)

### 5.7 Vor dem Pull Request

`RGBW_DIAG`-Diagnosecode entfernen (für den Test von `v13` vorübergehend eingeschaltet); ~~nach den Tests den Standard für `p3mode` festlegen~~ (entfällt: Modus 0 fest, 5.1 Plan Punkt 1); offene Fragen an den Maintainer klären (Abschnitt 6 der [Gegenüberstellung](doc/RGBW-Gegenueberstellung.md), u. a. die Sperrfehler S1–S3 bei allen nicht-RGBW-Kanaltypen (5.1 Plan Punkt 8; bei RGBW behoben), das Log `Dali: unhandled KO: 1` beim Heartbeat, *Schalten Status* bei *Heller* aus Aus erst beim Stopp (B23), die Warnung „loop took longer than usual“, die knappe KNX-Reserve von 63 Byte, die Idee der einstellbaren Dimmkurve); (erledigt am 17.11.: veraltete Kommentare zu B2/B3 korrigiert); die Gegenüberstellung neu erzeugen (Stand v6, siehe Hinweis dort; Diff automatisch aus Git). Das Home-Assistant-Testskript `script.dali_rgbw_testfolge` nach den Tests löschen. **PR erst nach ausdrücklichem Go.**

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

Die Tabelle ist der Grundtest aus `v6` mit den Erwartungen für `v13`. Für `v13` gilt der Testablauf in [5.1](#51-ansteuerung-in-v13-konzept-und-testablauf); er enthält diese Fälle (Phase 3).

| # | Senden (die Tests bauen aufeinander auf; zu Beginn von Test 1 ist der Kanal **aus**, Test 3–8 laufen bei brennender Leuchte) | Erwartung | Stand |
|---|----------------------------------------------------|-----------|---|
| 1 | *Schalten* Ein, *Dimmen absolut* 50 % / 100 %, *Schalten* Aus | Leuchte reagiert; Status-Objekte folgen | **bestanden** (v6, v8) |
| 2 | Alle vier Häkchen, R 100 %, G/B/W 0 % | `v13`: Leuchte **bleibt aus**, Farbstatus `FF 00 00 00 00 0F`, Schalten Status *Aus*; danach *Schalten* Ein → rein Rot (bis `v12`: Farbtelegramm schaltete ein) | **bestanden** (v6, altes Verhalten); `v13` **offen** (5.1 Phase 3) |
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
