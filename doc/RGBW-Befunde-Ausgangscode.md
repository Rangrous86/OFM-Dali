# Befunde im Ausgangscode und in der DALI-Bibliothek

**Für den Maintainer, getrennt von der RGBW-Erweiterung.** Beim Entwickeln und Testen der RGBW-Erweiterung (Fork `Rangrous86/OFM-Dali`, Branch `v1dev`, Stand `v14b`) sind Stellen im bestehenden Code von `OpenKNX/OFM-Dali` (Basis `v1dev`, Commit `a120578`) und in der Bibliothek `OpenKNX/dali` aufgefallen, die **nicht** aus der RGBW-Erweiterung stammen. Sie sind hier gesammelt, damit sie unabhängig vom RGBW-Pull-Request bewertet werden können.

- Diese Stellen werden im RGBW-PR **nicht** verändert, außer wo „bei RGBW behoben“ steht: Dort ist der Fehler nur im neuen RGBW-Zweig korrigiert, für die anderen Kanaltypen ist er unverändert.
- Gefunden durch Code-Durchsicht (Prüfbericht vom 18.11.2026, G1–G15) und durch Hardware-Tests mit zwei DA4-M (RGBW) am `GW-REG1-Dali` (19.–21.11.2026). Andere Treibertypen waren nicht vorhanden.
- Die Nummern G1–G24 und F1–F4 sind dieselben wie im [Testprotokoll](RGBW-Testprotokoll.md); dort stehen auch die Testdetails (B…-Nummern).
- Fragen zur RGBW-Erweiterung selbst (Speicherlayout, WAF-Befehl, Szenen mit Weiß usw.) stehen in Abschnitt 6 der [Gegenüberstellung](RGBW-Gegenueberstellung.md).

## Übersicht

| Gruppe | Nummern |
|---|---|
| Bibliothek `OpenKNX/dali` | G1, G2 |
| Fehler im Ausgangscode, bei RGBW behoben | G4, G16, G24 |
| Fehler im Ausgangscode, offen | G3, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G21, G22, G23 |
| Nur Log, Warnungen, Hinweise | G15, G17, G18, G19 |
| Ideen und Fragen | G20, F1, F2, F3, F4 |

## 1. Bibliothek `OpenKNX/dali`

Diese Punkte gehören an das Repository der Bibliothek.

| # | Stelle | Befund | Stand |
|---|---|---|---|
| G1 | Bibliothek OpenKNX/dali, `Master::getResponse` | Rückgabe einer Referenz nach dem Löschen: bei zwei offenen Abfragen geht eine Antwort verloren oder Kanal A übernimmt den Pegel von Kanal B | offen; in `v13` umgangen (Versionsabfrage erst nach der Fade-Abfrage), entfällt mit Modus 0 fest |
| G2 | Bibliothek, `Master::process` | löscht Einträge während der Schleife über denselben Vektor (Einträge werden übersprungen); Antworten, die nicht binnen 100 ms abgeholt werden, gehen verloren | offen |

## 2. Fehler im Ausgangscode, bei RGBW behoben

| # | Stelle | Befund | Stand |
|---|---|---|---|
| G4 | Sperre `koHandleLock` | **S1** „Anschalten“/„Ausschalten“ vertauscht; **S2** „Fester Wert in %“ als DALI-Pegel gesendet (50 % → ≈ 0,1 %); **S3** Gruppen lesen die Kanal-Parameter (`ParamDGW_` statt `ParamDGWG_`); **S4** `currentState` wird nicht nachgeführt | **bei RGBW behoben** (RGBW-Zweig `rgbwLock`/`rgbwUnlock`); andere Kanaltypen offen |
| G16 | `koHandleDimmRel` | *Schalten Status* Ein bei *Heller* aus Aus erst beim Stopp, nicht beim Einschalten  | **bei RGBW behoben** (RGBW: `koHandleDimmRel`); andere Kanaltypen offen |
| G24 | `setHcl` / `setBrightness` (HCL) | Die HCL prüft keine Sperre: Steht *HCL anwenden* auf „Auch wenn eingeschaltet ist“ und gilt der Kanal als eingeschaltet, sendet die Kurve auch während einer Sperre jede Minute einen Pegel an die Lampe. Bei RGBW mit „Bei Entsperren vorherigen Zustand wiederherstellen“ bleibt der gespeicherte Zustand beim Sperren „an“, die Lampe wäre also eine Minute nach dem Sperren mit *Ausschalten* wieder angegangen | **bei RGBW behoben** (`v15b`, getestet H6b; `setBrightness`: während der Sperre nichts an die Lampe, mit „wiederherstellen“ folgt der gespeicherte Pegel der Kurve); andere Kanaltypen offen (gefunden 22.11. beim Vorbereiten von Test H6) |

## 3. Fehler im Ausgangscode, offen

| # | Stelle | Befund | Stand |
|---|---|---|---|
| G3 | `DaliModule::loopGroupState()` | wird nie aufgerufen: Einzelkanäle übernehmen Gruppenbefehle nicht in Zustand und Status; die Funktion dekodiert die Werte außerdem falsch | offen; wirkt auf RGBW (Grund, warum bei RGBW ein Einzel-EVG keinen Pegel mitschickt) |
| G5 | Broadcast *Dimmen absolut* | eigene Umrechnung statt `percentToArc`; bei 0 % wird `log10(0)` gerechnet | offen |
| G6 | Fehlerobjekt `loopError` | der ermittelte Fehlerzustand wird überschrieben, das Fehlerobjekt wird nie gesetzt | offen |
| G7 | Treppenhaus, `setSwitchState(true)` | doppelte Umrechnung Prozent → DALI-Pegel beim Dimmstatus | offen |
| G8 | `koHandleColorAbs` | Gruppe liest den Kanal-Parameter `hcl_manu_col` | offen |
| G9 | Relatives Farbdimmen | zählt nur einen Zähler hoch, sendet nichts an den Treiber | offen |
| G10 | HSV | V-Wert wird in *Dimmen Status* geschrieben | offen |
| G11 | `loopInitData` | Broadcast auf 10 % beim Start ist toter Code; Gruppen bekommen beim Start keinen Zustand | offen |
| G12 | `RECALL MIN` | wird mit Antworterwartung gesendet, obwohl keine Antwort kommt | offen |
| G13 | Taster am Gerät | Broadcast `RECALL MAX`/`OFF` aktualisieren weder Zustand noch Status | offen |
| G14 | `percentToArc` / `arcToPercent` | Randwerte (< 0,1 % negativ; 255 → 103 %) | offen, derzeit nicht erreichbar |
| G21 | `DaliChannel`, `currentDimmValue` | Zeiger wird erst beim ersten relativen Dimmen gesetzt; eine Pegelabfrage davor (Abfrageintervall, Diagnose) schreibt in `loopQueryLevel` über einen nicht gesetzten Zeiger (`*currentDimmValue = data`). Auf dem RP2040 bisher ohne sichtbare Folge | offen (gefunden beim `v14`-Umbau, nicht geändert) |
| G22 | Broadcast *Schalten* | `broadcastGroupState(bool)` setzt bei allen Kanälen und Gruppen nur *Schalten Status*, **nicht** *Dimmen Status*: nach *Broadcast Ein* (100 %) bzw. *Aus* (0 %) zeigt *Dimmen Status* weiter den alten Wert. Pegel und (bei RGBW) Farbe im Gateway bleiben stehen; nach *Broadcast Ein* bringt *Broadcast zurück* deshalb alle RGBW-Gruppen in ihrer gespeicherten Farbe und ihrem gespeicherten Pegel **eingeschaltet** zurück, nach *Broadcast Aus* alle ausgeschaltet | offen (gefunden `v14` Test B1, 21.11., nicht geändert) |
| G23 | `setHcl` (HCL im Modus *Manu*) | Im Modus *Manu* bricht `setHcl` ab, **bevor** der neue Kurvenwert gespeichert wird (`_hclCurrentTemp`, `_hclCurrentBri`). Die gespeicherten Werte veralten, solange der Kanal auf *Manu* steht. Nach dem Zurückschalten auf *Auto* (z. B. durch *Aus*) nimmt das nächste *Ein* den Wert vom Zeitpunkt des Wechsels auf *Manu*, bis zur nächsten vollen Minute. Betrifft alle Kanaltypen | offen (gefunden `v15` Test H5, 22.11.: *Ein* um 15:51:07 mit 3313 K / 41 % vom Stand 15:48, die Kurve stand bei 3273 K / 39 %; korrigiert sich zur nächsten Minute). Vorschlag: Wert vor der Abfrage auf *Manu* speichern |

## 4. Nur Log, Warnungen, Hinweise

| # | Stelle | Befund | Stand |
|---|---|---|---|
| G15 | allgemein | Compiler-Warnungen (size_t mit -1, Formatbreiten) | harmlos |
| G17 | Heartbeat | Log `Dali: unhandled KO: 1` bei jedem Heartbeat | offen, nur Log |
| G18 | Schleife | Warnung „loop took longer than usual“ (z. B. 171 ms beim Start) | offen, nur Log |
| G19 | KNX-Speicher | nur 63 Byte Reserve ; vor neuen ETS-Parametern `KNX_FLASH` prüfen | Hinweis, betrifft den nächsten ETS-Schritt |

## 5. Ideen und Fragen

| # | Stelle | Befund | Stand |
|---|---|---|---|
| G20 | Idee | einstellbare Dimmkurve (siehe unten) | Vorschlag, nicht RGBW-spezifisch |
| F1 | Frage | *Broadcast zurück* (und *Broadcast – Farbe RGBW*) auch für die anderen Kanaltypen? In `v14` nur RGBW, weil andere Typen hier nicht testbar sind; die Erweiterung wäre klein | Frage an den Maintainer |
| F2 | Frage | Soll der normale Broadcast (*Schalten*, *Dimmen*) den gespeicherten Zustand der Kanäle stehen lassen, damit *Broadcast zurück* auch nach „Alarm alles an“ wirkt? Ändert Ausgangsverhalten aller Typen (Status würde nicht mehr nachgeführt) | Frage an den Maintainer |
| F3 | Frage | **Broadcast insgesamt überarbeiten** und an das RGBW-Verfahren angleichen? Heute verhalten sich die Broadcast-Objekte unterschiedlich: *Schalten* setzt Ein/Aus aller Kanäle und Gruppen (G22), *Dimmen* setzt Pegel und Status aller Kanäle und Gruppen (eigene Umrechnung, G5), *Farbe RGBW* setzt **nichts** und sendet keinen Status, *Zurück* gilt nur für RGBW (F1). Bei gemischten Anlagen (RGBW mit normalen Dimm- oder Schalttreibern) wirkt der Pegel von *Broadcast – Farbe RGBW* auf **alle** Treiber, *Zurück* stellt aber nur RGBW wieder her; die anderen Leuchten bleiben auf dem Broadcast-Pegel, ihr Status zeigt den alten Wert. **Anwendungsfall:** Alarm- oder Blinklicht mit Rückkehr in den vorherigen Zustand, heute nur mit *Broadcast – Farbe RGBW* (z. B. Rot 100 % und „alles 0“ im Wechsel, danach *Zurück*). Mit *Broadcast Dimmen* (hoch/runter) wäre derselbe Alarm auch für normale Dimmtreiber möglich, wenn es wie *Farbe RGBW* arbeitet. **Umsetzungsidee (wie in `v14` für RGBW):** Broadcast-Befehle als vorübergehenden Zustand behandeln, also kein Status und kein Überschreiben des gespeicherten Zustands; *Zurück* sendet für alle Kanaltypen den gespeicherten Zustand der Gruppen und der Kanäle ohne Gruppe erneut (`rgbwRestore`/`rgbwSendState` als Vorlage). Reine Schalttreiber können nur Ein/Aus. Nicht getestet (keine anderen Treibertypen vorhanden); Grundlage sind die RGBW-Tests in `v14` (B2/B3). Zusammen mit F1, F2, G5 und G22 entscheiden | Frage an den Maintainer (Entscheidung 21.11.: RGBW bleibt bis dahin getrennt behandelt) |
| F4 | Frage | **Szenen insgesamt überarbeiten?** Heute werden DALI-Szenen 1:1 durchgereicht: Die Werte stehen nur im Treiber (in der ETS über Schaltflächen hineingeschrieben, keine Projektparameter; bei einem Treibertausch verloren), die Zuordnung KNX-Szene → DALI-Szene → Adresse/Gruppe/Broadcast steht in einer getrennten Tabelle, die Farbe geht nur als RGB ohne Weiß. Vor allem: **Nach einer Szene ist das Gateway „blind“.** Es kennt die angefahrenen Werte nicht, *Schalten*, *Dimmen* und *Farbe Status* bleiben bei Einzel-EVGs, Gruppen und Broadcast auf dem alten Wert, und *Ein*, *Zurück*, Sperre und HCL arbeiten mit veralteten Werten weiter. Damit verlieren die Statusobjekte nach der ersten Szene ihren Sinn. Vorschlag: Szenen wie bei KNX-Aktoren pro Kanal und Gruppe als Parameter (Ein/Aus, Helligkeit, Farbe inklusive Weiß), ausgeführt über die normale Ansteuerung, also mit Status und bei RGBW über das Sammelfenster; Lernen per KNX speichert den aktuellen Zustand im Gateway. DALI-Szenen könnten als Option für den gleichzeitigen Abruf vieler Treiber bleiben (ein Telegramm, synchron, auch von anderen DALI-Geräten abrufbar), dann mit Statusabfrage danach. Betrifft alle Kanaltypen, deshalb nicht Teil des RGBW-PR | offen, Frage an den Maintainer (22.11./23.11.) |

**Einstellbare Dimmkurve (G20):** Das Gateway rechnet *Prozent* fest mit der logarithmischen DALI-Standardkurve (`DaliHelper::percentToArc`/`arcToPercent`; 1 % → 85, 20 % → 195, 50 % → 229, 100 % → 254). Vorschlag: ein allgemeiner ETS-Parameter „DALI logarithmisch (Standard)“ / „wahrnehmungsgleich“ (z. B. nach CIE L*: 10 % → 90, 50 % → 192). Die vier Funktionen in `DaliHelper.cpp` bekämen einen Modus; alle Stellen laufen schon über diese Funktionen. Der Standard muss die heutige Kurve bleiben; braucht freie Bits (G19) und eine neue ETS-Version.

**Hinweis zum Farbtelegramm bei ausgeschaltetem Kanal (andere Farbtypen):** Bei HSV, RGB, TW und xyY schickt ein Farbtelegramm an einen ausgeschalteten Kanal die Farbe an den Treiber und setzt die Statusobjekte auf *Ein* mit dem Tag-Einschaltwert (`// TODO get real` in `koHandleColor`), obwohl die Lampe aus bleibt. RGBW lässt den Status auf *Aus*. Soll das für die anderen Farbtypen ebenso korrigiert werden?
