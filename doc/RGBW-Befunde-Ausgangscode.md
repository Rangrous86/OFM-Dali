# Befunde im Ausgangscode und in der DALI-Bibliothek

**Für den Maintainer, getrennt von der RGBW-Erweiterung.** Beim Entwickeln und Testen der RGBW-Erweiterung (Fork `Rangrous86/OFM-Dali`, Branch `v1dev`) sind Stellen im bestehenden Code von `OpenKNX/OFM-Dali` (Basis `v1dev`, Commit `a120578`) und in der Bibliothek `OpenKNX/dali` aufgefallen, die **nicht** aus der RGBW-Erweiterung stammen. Sie sind hier gesammelt, damit sie unabhängig vom RGBW-Pull-Request bewertet werden können.

- Diese Stellen werden im RGBW-PR **nicht** verändert, außer wo „bei RGBW behoben“ steht: Dort ist der Fehler nur im neuen RGBW-Zweig korrigiert, für die anderen Kanaltypen ist er unverändert.
- Gefunden durch Code-Durchsicht (18.11.2026, G1–G15) und durch Hardware-Tests mit zwei DA4-M (RGBW) am `GW-REG1-Dali` (19.–23.11.2026). Andere Treibertypen waren nicht vorhanden; Befunde zu anderen Kanaltypen sind deshalb aus dem Code abgeleitet, nicht am Gerät bestätigt.
- Die Nummern G1–G24 und F1–F4 sind dieselben wie im [Testprotokoll](RGBW-Testprotokoll.md); dort stehen auch die Testdetails. Begriffe und Kürzel: [README, Abschnitt 12](../README.md#12-begriffe-und-kürzel).
- Die Abstimmung zur RGBW-Erweiterung selbst (Speicherplatz, Grundverfahren, kleine Änderungen für alle Kanaltypen) steht in Abschnitt 7 der [Gegenüberstellung](RGBW-Gegenueberstellung.md).
- Zeilenangaben beziehen sich auf den Ausgangscode (`a120578`).

## Übersicht

| Gruppe | Nummern |
|---|---|
| Bibliothek `OpenKNX/dali` | G1, G2 |
| Fehler im Ausgangscode, bei RGBW behoben | G4, G16, G24 |
| Fehler im Ausgangscode, offen | G3, G5, G6, G7, G8, G9, G10, G11, G12, G13, G14, G21, G22, G23 |
| Nur Log, Warnungen, Hinweise | G15, G17, G18, G19 |
| Ideen und Vorschläge | G20, F1, F2, F3, F4 |

Kurzliste:

| # | Thema | betrifft |
|---|---|---|
| G1 | `getResponse` gibt nach dem Löschen die falsche Antwort zurück | alle Treiberabfragen |
| G2 | `process` löscht Einträge während der Schleife | alle Treiberabfragen |
| G3 | Zustand/Status der Einzelkanäle nach Gruppenbefehl nicht nachgeführt | alle Kanaltypen |
| G4 | Sperre: vier Fehler | alle außer RGBW |
| G5 | Broadcast *Dimmen absolut* rechnet falsch | alle |
| G6 | Fehlerobjekt wird nie gesetzt | alle Einzel-EVGs |
| G7 | Treppenhaus: falscher *Dimmen Status* beim Einschalten | Treppenhaus |
| G8 | Gruppe: R/G/B-Wert liest für „HCL auf Manu“ den Haken des Einzel-EVGs | Gruppen mit HCL, Objekte R/G/B absolut |
| G9 | Relatives Farbdimmen ändert die Farbe nicht | Farbtypen RGB/HSV |
| G10 | HSV schreibt V in *Dimmen Status* | Farbtyp HSV |
| G11 | Start: toter Code, Gruppen ohne Zustand | alle |
| G12 | `RECALL MIN` mit Antworterwartung | relatives Dimmen |
| G13 | Func-Taste: Zustand/Status nicht nachgeführt | alle, nur wenn Func-Aktion *Alle …* belegt |
| G14 | Randwerte der Prozent-Umrechnung | alle (derzeit nicht erreichbar) |
| G16 | *Schalten Status* erst beim Dimmstopp | alle außer RGBW |
| G21 | Zeiger `currentDimmValue` anfangs nicht gesetzt | alle |
| G22 | Broadcast *Schalten* setzt *Dimmen Status* nicht | alle |
| G23 | HCL-Werte veralten im Modus *Manu* | alle mit HCL |
| G24 | HCL beachtet keine Sperre | alle außer RGBW |

## 1. Bibliothek `OpenKNX/dali`

Diese Punkte gehören an das Repository der Bibliothek. Sie betreffen **jede** Abfrage an einen Treiber (Pegel, Status, Fehler, Gruppen, Adressierung, Farbe), also alle Kanaltypen und Funktionen.

### G1 – `Master::getResponse` gibt nach dem Löschen die falsche Antwort zurück

- **Wo:** `src/Dali/Master.cpp`, `Master::getResponse(uint32_t ref)`.
- **Was passiert:** Eine Abfrage an einen Treiber bekommt eine Nummer, die Antwort landet in einer Liste (`_responses`). `getResponse` sucht den Eintrag mit `for(auto &r : _responses)`. `r` ist dabei ein **Verweis** auf die Stelle in der Liste, keine Kopie. Ist die Antwort fertig, wird der Eintrag mit `removeResponse(ref)` gelöscht und danach `return r;` ausgeführt. Nach dem Löschen rücken alle folgenden Einträge eine Stelle nach vorn; `r` zeigt dann auf die **nächste** Antwort (oder hinter das Listenende).
- **Folge:** Mit nur einer offenen Abfrage fällt es meist nicht auf (der Speicher enthält noch die alten Werte). Sind **zwei Abfragen gleichzeitig offen**, bekommt die erste die Antwort der zweiten, z. B. übernimmt Kanal A den Pegel von Kanal B, und die Antwort für B ist verloren.
- **Betrifft:** alle Kanaltypen, sobald zwei Abfragen überlappen (z. B. mehrere Kanäle mit zyklischer Pegelabfrage, Fehlerabfrage parallel zur Pegelabfrage).
- **Stand:** offen. Bei RGBW seit `v13` umgangen, weil RGBW seine Abfragen nacheinander stellt.
- **Vorschlag:** erst kopieren, dann löschen: `Response copy = r; removeResponse(ref); unlock(); return copy;`

### G2 – `Master::process` löscht Einträge während der Schleife

- **Wo:** `src/Dali/Master.cpp`, `Master::process()`.
- **Was passiert:** Die Funktion läuft mit `for(auto &r : _responses)` über die Liste der offenen Antworten und ruft **innerhalb** der Schleife `removeResponse()` auf (Antwort älter als 100 ms und nicht abgeholt, oder Telegramm nach 10 s nicht gesendet). Das Löschen aus einem `std::vector` während einer Schleife über denselben Vektor ist in C++ nicht erlaubt (undefiniertes Verhalten): Der folgende Eintrag wird übersprungen, am Listenende kann über das Ende hinaus gelesen werden.
- **Folge:** Einzelne Einträge werden in diesem Durchlauf nicht geprüft; im ungünstigen Fall falscher Speicherzugriff. Außerdem: Eine Antwort, die nicht innerhalb von 100 ms nach dem Senden abgeholt wird, wird verworfen. Wer seltener als alle 100 ms nachfragt (z. B. weil die Schleife gerade länger dauert, G18), verliert die Antwort.
- **Betrifft:** alle Treiberabfragen.
- **Stand:** offen.
- **Vorschlag:** Schleife mit Iterator und `it = _responses.erase(it)` oder Löschen nach der Schleife (`erase`/`remove_if`); die 100 ms ggf. großzügiger.

## 2. Fehler im Ausgangscode, bei RGBW behoben

Hier ist der Fehler im neuen RGBW-Zweig korrigiert; für die anderen Kanaltypen besteht er weiter.

### G4 – Sperre: vier Fehler in `koHandleLock`

- **Wo:** `DaliChannel::koHandleLock`.
- **Was passiert:**
  1. „Anschalten“ und „Ausschalten“ bei *Verhalten bei Sperren/Entsperren* sind vertauscht: „Anschalten“ schaltet aus und umgekehrt.
  2. „Fester Wert in %“ wird direkt als DALI-Pegel gesendet, ohne Umrechnung: 50 % werden als Pegel 50 gesendet, das sind etwa 0,1 %.
  3. Gruppen lesen die Sperr-Parameter des **Kanals** mit derselben Nummer (`ParamDGW_…` statt `ParamDGWG_…`), nicht ihre eigenen.
  4. Der interne Ein/Aus-Zustand (`currentState`) wird beim Sperren und Entsperren nicht nachgeführt; folgende Befehle (z. B. Dimmen relativ, *Zurück*) gehen von einem falschen Zustand aus.
- **Betrifft:** alle Kanaltypen außer RGBW.
- **Stand:** bei RGBW behoben (eigener Zweig `rgbwLock`/`rgbwUnlock`/`rgbwLockArc`), getestet. Andere Kanaltypen offen.

### G16 – *Schalten Status* erst beim Dimmstopp

- **Wo:** `DaliChannel::koHandleDimmRel`.
- **Was passiert:** Wird eine ausgeschaltete Lampe mit *Dimmen relativ* „heller“ eingeschaltet, geht sie sofort an (`RECALL MIN`), *Schalten Status* meldet „Ein“ aber erst beim Stopp-Telegramm.
- **Folge:** Während des Hochdimmens zeigen Visualisierungen „Aus“, obwohl die Lampe brennt; kommt kein Stopp (z. B. Taster mit fester Dimmdauer), bleibt der Status bis zum nächsten Befehl falsch.
- **Betrifft:** alle Kanaltypen außer RGBW.
- **Stand:** bei RGBW behoben („Ein“ sofort beim Einschalten), getestet. Andere Kanaltypen offen.

### G24 – HCL beachtet keine Sperre

- **Wo:** `DaliChannel::setHcl` / `setBrightness` / `setTemperature`.
- **Was passiert:** Steht *HCL anwenden* auf „Auch wenn eingeschaltet ist“ und gilt der Kanal als eingeschaltet, sendet die HCL jede Minute einen Pegel (und ggf. eine Farbtemperatur) an die Lampe, **auch während einer Sperre**.
- **Folge:** Eine gesperrte Lampe ändert trotzdem ihre Helligkeit. Bei RGBW mit „Bei Entsperren vorherigen Zustand wiederherstellen“ bleibt der gespeicherte Zustand beim Sperren „an“; ohne Korrektur wäre die Lampe eine Minute nach einem Sperren mit *Ausschalten* wieder angegangen.
- **Betrifft:** alle Kanaltypen mit HCL außer RGBW.
- **Stand:** bei RGBW behoben (`v15b`, getestet H6b): während der Sperre nichts an die Lampe, mit „wiederherstellen“ folgt der gespeicherte Pegel der Kurve. Andere Kanaltypen offen.

## 3. Fehler im Ausgangscode, offen

### G3 – Zustand und Status der Einzelkanäle werden nach einem Gruppenbefehl nicht nachgeführt

- **Wo:** `DaliModule::loopGroupState()` und `DaliModule::processGroupKo()`.
- **Was passiert:** Es geht nur um den Zustand im Gateway und die Statusobjekte; der Befehl selbst erreicht die Treiber richtig, und es werden keine Befehle an Einzeladressen gesendet. `loopGroupState()` soll nach einem Gruppenbefehl den Zustand der Einzelkanäle in dieser Gruppe nachführen, wird aber **nirgends aufgerufen**. Zusätzlich wären die Werte falsch kodiert: Der Ein/Aus-Wert (0/1) wird mit `percentToArc` umgerechnet und danach mit `== 1` verglichen (ergibt nie „Ein“), der Dimmwert wird als 1-Bit-Wert (`DPT_Switch`) gelesen statt als Prozent.
- **Folge:** Nach einem Befehl an eine Gruppe kennen die Einzelkanäle den neuen Zustand nicht: *Schalten Status*, *Dimmen Status* (und bei RGBW *Farbe Status*) der Einzel-EVGs bleiben alt, der interne Zustand ebenso. Ein folgender Einzelbefehl geht von falschen Werten aus. Im Kurztest am 23.11. (KT2) sichtbar: Nach „G0 Ein“ meldet A0 beim Farbbefehl „Kanal ist aus“, obwohl die Lampe brennt.
- **Betrifft:** alle Kanaltypen.
- **Stand:** offen, als bekanntes Verhalten akzeptiert. Wirkt auf RGBW: Deshalb schickt ein Einzel-EVG bei einer reinen Farbänderung keinen Pegel mit, und *Broadcast – Zurück* nimmt bei Kanälen in einer Gruppe den Zustand der Gruppe.

### G5 – Broadcast *Dimmen absolut* rechnet falsch

- **Wo:** `DaliModule::koHandleDimm`.
- **Was passiert:** Statt `DaliHelper::percentToArc` steht eine eigene Formel: `((253 / 3) * (log10(value) + 1)) + 1`, danach `value++`. `253 / 3` ist eine Ganzzahldivision (84 statt 84,33), das zusätzliche `++` erhöht jeden Wert um eins, und bei 0 % wird `log10(0)` (minus unendlich) gerechnet und in `uint8_t` gewandelt (undefiniert).
- **Folge:** Broadcast-Pegel weichen von den Pegeln der Kanäle ab; *Broadcast Dimmen 0 %* schaltet nicht zuverlässig aus.
- **Betrifft:** alle Kanaltypen.
- **Stand:** offen.
- **Vorschlag:** `DaliHelper::percentToArc(value)` verwenden (liefert bei 0 % korrekt 0).

### G6 – Fehlerobjekt wird nie gesetzt

- **Wo:** `DaliChannel::loopError`.
- **Was passiert:** Der Kanal fragt einmal pro Minute den Treiberstatus ab und setzt `_errorState` (keine Antwort = Fehler). Direkt danach wird `_errorState` mit dem **aktuellen Wert des Fehlerobjekts** überschrieben (`_errorState = val != 0;`), und das Objekt wird nur gesendet, wenn sich dieser Wert von sich selbst unterscheidet, also nie. Die Antwort auf `QUERY STATUS` (Lampenfehler usw.) wird gar nicht ausgewertet.
- **Folge:** *Fehlerstatus auslesen* = Ja hat keine Wirkung: Das Fehlerobjekt bleibt immer auf seinem Anfangswert, auch wenn ein Treiber nicht antwortet.
- **Betrifft:** alle Einzel-EVGs mit *Fehlerstatus auslesen*.
- **Stand:** offen.

### G7 – Treppenhaus: falscher *Dimmen Status* beim Einschalten

- **Wo:** `DaliChannel::handleSwitchStaircase`.
- **Was passiert:** Der Einschaltwert (`_onDay`/`_onNight`, bzw. HCL) ist schon ein DALI-Pegel. Für den Status wird er trotzdem noch einmal mit `percentToArc` umgerechnet: `setDimmState(DaliHelper::percentToArc(onValue))`.
- **Folge:** *Dimmen Status* zeigt nach dem Einschalten eines Treppenhauskanals einen falschen Wert (z. B. bei 100 %: Pegel 254 wird wie „254 %“ umgerechnet). Die Lampe selbst bekommt den richtigen Pegel.
- **Betrifft:** Kanäle und Gruppen mit *Betriebsart* = Treppenhauslicht.
- **Stand:** offen. Hinweis: Die Treppenhaus-Funktion prüft bei HCL außerdem nicht, ob in der Kurve *Helligkeit* aktiv ist (der normale Einschaltzweig tut das).

### G8 – Gruppe: Umschalten von HCL auf *Manu* bei einzelnen R/G/B-Werten liest den Parameter des falschen Kanals

- **Hintergrund:** Läuft HCL im Modus *Auto*, stellt die Kurve Helligkeit und Farbtemperatur selbst nach. Schickt man von Hand eine Farbe, soll HCL auf *Manu* wechseln, damit die Kurve die Farbe nicht gleich wieder überschreibt. Ob das passiert, legt man je Einzel-EVG und je Gruppe in der ETS fest: HCL Einstellungen → *Der automatische Modus wird verlassen, wenn ...* → *Farbe/Temperatur geändert wird* (ab Werk angehakt).
- **Wo:** `DaliChannel::koHandleColorAbs`. Diese Funktion verarbeitet die drei einzelnen Objekte *Rot/Grün/Blau absolut*.
- **Was passiert:** Die Abfrage lautet `if(_isGroup ? ParamDGW_hcl_manu_col : ParamDGW_hcl_manu_col)`. Beide Zweige lesen `ParamDGW_`, also den Parameter eines **Einzel-EVGs**. Bei einer Gruppe müsste `ParamDGWG_hcl_manu_col` stehen. So liest Gruppe 3 den Haken von Einzel-EVG A3.
- **Folge:** Ob ein R/G/B-Wert an eine Gruppe deren HCL auf *Manu* schaltet, hängt nicht vom Haken der Gruppe ab, sondern vom Haken des Einzel-EVGs mit derselben Nummer. Ab Werk sind beide angehakt, daher fällt es nur auf, wenn man die beiden unterschiedlich einstellt.
- **Betrifft:** nur Gruppen mit HCL, und nur die einzelnen Objekte *Rot/Grün/Blau absolut*. Das gemeinsame Farbobjekt (`koHandleColor`) und das relative Farbdimmen lesen den Parameter richtig.
- **Stand:** offen.
- **Vorschlag:** `ParamDGW_hcl_manu_col` im ersten Zweig durch `ParamDGWG_hcl_manu_col` ersetzen. Nebenbei: In `koHandleColor` sind in der zweiten Logzeile `AutoConf` die beiden Parameter vertauscht (betrifft nur die Ausgabe im Log).

### G9 – Relatives Farbdimmen ändert die Farbe nicht

- **Wo:** `DaliChannel::koHandleColorRel` (relatives Dimmen eines Farbkanals) mit `loopDimming`.
- **Was passiert:** Beim Start setzt die Funktion `currentDimmValue = &currentStep`, also auf den **Helligkeitswert** des Kanals, nicht auf den gewählten Farbkanal (`currentColor[…]`). `loopDimming` zählt dann diesen Wert hoch oder runter und sendet über `sendColor()` die **unveränderte** Farbe.
- **Folge:** Relatives Farbdimmen hat keine sichtbare Wirkung auf die Farbe; nebenbei wird der gespeicherte Helligkeitswert verstellt.
- **Betrifft:** Farbtypen mit relativen Farbobjekten (RGB/HSV).
- **Stand:** offen (aus dem Code abgeleitet, nicht am Gerät geprüft).

### G10 – HSV schreibt V in *Dimmen Status*

- **Wo:** `DaliChannel::handleColorHSV`.
- **Was passiert:** Beim Status wird der dritte Wert (V) nicht in ein Farbstatus-Objekt, sondern in `DGW_Kodimm_state` geschrieben: `sendKoStateOnChange(DGW_Kodimm_state, currentColor[2], …)`. Die Status-Objekte heißen außerdem „rot/grün“ statt H/S.
- **Folge:** Nach einer HSV-Farbe zeigt *Dimmen Status* den V-Wert der Farbe statt der tatsächlichen Helligkeit.
- **Betrifft:** Farbtyp HSV.
- **Stand:** offen.

### G11 – Start: toter Code, Gruppen ohne Zustand

- **Wo:** `DaliModule::loopInitData`.
- **Was passiert:** `_initDataIndex` wird **vor** der Abfrage `if (_initDataIndex == 0)` erhöht; der Broadcast auf 10 % beim Start wird deshalb nie gesendet (toter Code). Außerdem liest die Funktion beim Start nur die Einzelkanäle aus (Gruppenzugehörigkeit, Pegel), Gruppen bekommen keinen Anfangszustand.
- **Folge:** Nach einem Neustart kennen Gruppen weder Ein/Aus noch Pegel, bis der erste Befehl kommt; ihre Statusobjekte stimmen bis dahin nicht.
- **Betrifft:** alle Kanaltypen (Gruppen).
- **Stand:** offen. Ob der 10-%-Broadcast gewollt ist, sollte der Maintainer entscheiden (eher nicht, sonst würden alle Lampen beim Start angehen).

### G12 – `RECALL MIN` mit Antworterwartung

- **Wo:** `DaliChannel::loopDimming` (relatives Dimmen „heller“ aus „aus“).
- **Was passiert:** `daliMaster.sendCommand(…, RECALL_MIN, _isGroup, true)`: Das letzte `true` bedeutet „Antwort erwarten“. `RECALL MIN` ist ein Befehl ohne Antwort.
- **Folge:** Die Bibliothek wartet vergeblich und legt einen Antwort-Eintrag an, der nie abgeholt wird (zusätzliche Last, erhöht das Risiko von G1/G2).
- **Betrifft:** relatives Dimmen bei allen Kanaltypen.
- **Stand:** offen.
- **Vorschlag:** letzten Parameter auf `false`.

### G13 – Func-Taste am Gateway: Zustand und Status werden nicht nachgeführt

- **Hintergrund:** Der Func-Taste am Gateway kann man in der ETS drei Aktionen zuweisen: *Func Aktion Klick*, *Doppelklick* und *Langer Klick*. Zur Auswahl stehen *Deaktiviert*, *Alle Einschalten*, *Alle Ausschalten*, *Alle Umschalten*, *Sperren*, *Entsperren* und *Sperre Umschalten*. Ab Werk stehen alle drei auf *Deaktiviert*.
- **Wo:** `DaliModule::handleFunc`. Die Zuordnung zur Taste steht in `DaliModule::setup`.
- **Was passiert:**
  - Bei *Alle Einschalten/Ausschalten/Umschalten* sendet das Gateway einen DALI-Broadcast, `RECALL MAX` oder `OFF`. Die Lampen schalten richtig. Anders als beim KNX-Objekt *Broadcast Schalten* (`koHandleSwitch`) wird danach aber `broadcastGroupState()` nicht aufgerufen. Der interne Zustand der Einzel-EVGs und Gruppen bleibt deshalb alt, und deren *Schalten Status* und *Dimmen Status* werden nicht gesendet.
  - *Alle Umschalten* merkt sich die Richtung in einer eigenen Variable (`_currentToggleState`), die nur die Taste selbst ändert. Wurde vorher über KNX geschaltet, kann der erste Druck in die falsche Richtung gehen, zum Beispiel „Ein“, obwohl schon alles an ist. Dann braucht es einen zweiten Druck.
  - *Alle Einschalten* nutzt immer `RECALL MAX`, also den Maximalwert des Treibers. Die Einschaltwerte Tag/Nacht und HCL werden dabei nicht beachtet.
- **Folge:** Nach Bedienung am Gerät zeigen Visualisierung und Statusobjekte den alten Zustand. Der nächste KNX-Befehl an einen Kanal geht von falschen Werten aus, genauso wie bei G3. Ein Beispiel: Nach *Alle Ausschalten* an der Taste hält sich A0 noch für eingeschaltet.
- **Betrifft:** alle Kanaltypen, aber nur, wenn eine Func-Aktion *Alle …* eingestellt ist. Ab Werk ist das nicht der Fall.
- **Stand:** offen.
- **Vorschlag:**
  - Nach dem Broadcast `broadcastGroupState(true/false)` aufrufen, wie bei `koHandleSwitch`.
  - Bei *Alle Einschalten* Status 100 % setzen, passend zu `RECALL MAX`.
  - Für *Umschalten* die Richtung aus dem aktuellen Zustand ableiten, statt sie in einer eigenen Variable zu führen.
- **Hinweis zu *Sperren* (kein Fehler, zur Einordnung; aus dem Code gelesen, nicht am Gerät getestet):**
  - Ist das Gerät gesperrt, verwirft `processInputKo` **alle** eingehenden Telegramme: Einzel-EVGs, Gruppen, HCL-Werte, Broadcast und Tag/Nacht. Sie werden auch nach dem Entsperren nicht nachgeholt. Wird zum Beispiel während der Sperre auf Nacht umgeschaltet, bleibt das Gateway danach auf Tag.
  - Es gibt kein Objekt, das die Sperre meldet. Die LED-Anzeige ist im Code auskommentiert (`info1Led.errorCode()`).
  - Die Sperre wird nicht gespeichert: Nach einem Neustart ist das Gerät wieder entsperrt.
  - Die Func-Taste selbst ist von der Sperre nicht betroffen. *Alle Ein/Aus* an der Taste funktioniert auch im gesperrten Zustand.
  - Diese Sperre des ganzen Geräts ist eine andere als die Sperre je Einzel-EVG oder Gruppe (G4).

### G14 – Randwerte der Prozent-Umrechnung

- **Wo:** `DaliHelper::percentToArc` / `arcToPercent`.
- **Was passiert:** `percentToArc` für Werte unter etwa 0,1 % ergibt rechnerisch einen negativen Pegel, der in `uint8_t` überläuft. `arcToPercent(255)` (255 = MASK/„unbekannt“) ergibt 103 %.
- **Folge:** keine, solange diese Werte nicht vorkommen.
- **Betrifft:** alle.
- **Stand:** offen, derzeit nicht erreichbar.

### G21 – Zeiger `currentDimmValue` anfangs nicht gesetzt

- **Wo:** `DaliChannel`, Zeiger `currentDimmValue`, verwendet in `loopQueryLevel`.
- **Was passiert:** Der Zeiger wird erst beim ersten relativen Dimmen gesetzt. Eine Pegelabfrage davor (Parameter *Dimmwert abfragen*, oder die Abfrage nach dem Dimmen) schreibt mit `*currentDimmValue = data` über einen nicht gesetzten Zeiger.
- **Folge:** Schreiben an eine zufällige Speicheradresse; auf dem RP2040 bisher ohne sichtbare Folge, kann aber jederzeit andere Daten verändern.
- **Betrifft:** alle Kanaltypen.
- **Stand:** offen.
- **Vorschlag:** Zeiger im Konstruktor auf `&currentStep` setzen.

### G22 – Broadcast *Schalten* setzt *Dimmen Status* nicht

- **Wo:** `DaliModule::broadcastGroupState(bool)`.
- **Was passiert:** Nach *Broadcast Schalten* wird bei allen Kanälen und Gruppen nur *Schalten Status* gesetzt, **nicht** *Dimmen Status*; auch der gespeicherte Pegel bleibt stehen.
- **Folge:** Nach *Broadcast Ein* (100 %) bzw. *Aus* zeigt *Dimmen Status* weiter den alten Wert. Bei RGBW bringt *Broadcast – Zurück* danach alle RGBW-Gruppen in ihrem gespeicherten Pegel zurück, aber eingeschaltet bzw. ausgeschaltet je nach Broadcast.
- **Betrifft:** alle Kanaltypen.
- **Stand:** offen (gefunden 21.11. im Test BC1). Genauere Erklärung folgt unter F2 und F3 (Abschnitt 5).

### G23 – HCL-Werte veralten im Modus *Manu*

- **Wo:** `DaliChannel::setHcl`.
- **Was passiert:** Im Modus *Manu* bricht `setHcl` ab, **bevor** der neue Kurvenwert gespeichert wird (`_hclCurrentTemp`, `_hclCurrentBri`). Solange der Kanal auf *Manu* steht, bleiben die gespeicherten Werte auf dem Stand des Wechsels.
- **Folge:** Nach dem Zurückschalten auf *Auto* (z. B. durch *Aus*) nimmt das nächste *Ein* den veralteten Wert, bis zur nächsten vollen Minute. Im Test (22.11., H5): *Ein* um 15:51 mit 3313 K / 41 % vom Stand 15:48, die Kurve stand bei 3273 K / 39 %.
- **Betrifft:** alle Kanaltypen mit HCL.
- **Stand:** offen.
- **Vorschlag:** den Kurvenwert vor der Abfrage auf *Manu* speichern.

## 4. Nur Log, Warnungen, Hinweise

### G15 – Compiler-Warnungen

Vergleich von `size_t`/`uint32_t` mit `-1` (z. B. `getResponseIndex`, Rückgabewert `-1` in einem vorzeichenlosen Typ), unpassende Formatbreiten in `logDebugP`. Harmlos, aber verdecken neue Warnungen.

### G17 – „unhandled KO: 1“ bei jedem Heartbeat

`DaliModule::processInputKo` meldet für jedes Objekt, das es nicht selbst behandelt, `Dali: unhandled KO: n` im Debug-Log, also auch beim *In Betrieb*-Telegramm (Objekt 1) alle 5 Minuten und bei der Uhrzeit (Objekt 4). Nur Log, kein Fehler; macht das Log aber unübersichtlich. Vorschlag: Objekte des OpenKNX-Kerns nicht melden.

### G18 – „loop took longer than usual“

Warnung des OpenKNX-Kerns, z. B. 171 ms beim Start. Nur Log; zusammen mit G2 (Antworten nach 100 ms verworfen) aber ein Hinweis, dass lange Schleifen Antworten kosten können.

### G19 – KNX-Speicher fast voll

Im Parameterspeicher (`KNX_FLASH`) sind nur noch 63 Byte frei. Vor neuen ETS-Parametern (z. B. G20, F3, F4) prüfen; ggf. Speicherbereich vergrößern.

## 5. Ideen und Vorschläge

### G20 – Idee: einstellbare Dimmkurve

Das Gateway rechnet *Prozent* fest mit der logarithmischen DALI-Standardkurve (`DaliHelper::percentToArc`/`arcToPercent`; 1 % → 85, 20 % → 195, 50 % → 229, 100 % → 254). Vorschlag: ein allgemeiner ETS-Parameter „DALI logarithmisch (Standard)“ / „wahrnehmungsgleich“ (z. B. nach CIE L*: 10 % → 90, 50 % → 192). Die vier Funktionen in `DaliHelper.cpp` bekämen einen Modus; alle Stellen laufen schon über diese Funktionen (außer G5). Der Standard muss die heutige Kurve bleiben; braucht freie Bits (G19) und eine neue ETS-Version. Nicht RGBW-spezifisch.

### F1 – Vorschlag: *Broadcast – Zurück* auch für die anderen Kanaltypen

*Broadcast – Farbe RGBW* und *Broadcast – Zurück* gelten heute nur für RGBW, weil andere Treibertypen hier nicht testbar waren. Die Erweiterung auf die anderen Kanaltypen wäre klein (`rgbwRestore`/`rgbwSendState` als Vorlage). **Vorschlag:** *Broadcast – Farbe RGBW* und *Broadcast – Zurück* auf alle Kanaltypen erweitern.

### F2 – Vorschlag: normaler Broadcast lässt den gespeicherten Zustand stehen

Heute überschreiben *Broadcast Schalten* und *Dimmen* den gespeicherten Zustand aller Kanäle; danach bringt *Zurück* den Broadcast-Zustand, nicht den von vorher. Würde der Broadcast den gespeicherten Zustand stehen lassen, wirkte *Zurück* auch nach „Alarm alles an“. Das ändert das Verhalten aller Typen (Status würde beim Broadcast nicht mehr nachgeführt). **Vorschlag:** *Broadcast Schalten* und *Dimmen* als vorübergehenden Zustand behandeln, wie *Broadcast – Farbe RGBW*.

### F3 – Vorschlag: Broadcast insgesamt überarbeiten

Heute verhalten sich die Broadcast-Objekte unterschiedlich:
- *Schalten* setzt Ein/Aus aller Kanäle und Gruppen, aber nicht *Dimmen Status* (G22).
- *Dimmen* setzt Pegel und Status aller Kanäle und Gruppen, mit eigener, fehlerhafter Umrechnung (G5).
- *Farbe RGBW* setzt **nichts** und sendet keinen Status (vorübergehender Zustand).
- *Zurück* gilt nur für RGBW (F1).

Bei gemischten Anlagen (RGBW mit normalen Dimm- oder Schalttreibern) wirkt der Pegel von *Broadcast – Farbe RGBW* auf **alle** Treiber, *Zurück* stellt aber nur RGBW wieder her; die anderen Leuchten bleiben auf dem Broadcast-Pegel, ihr Status zeigt den alten Wert.

**Anwendungsfall:** Alarm- oder Blinklicht mit Rückkehr in den vorherigen Zustand, heute nur mit *Broadcast – Farbe RGBW* (z. B. Rot 100 % und „alles 0“ im Wechsel, danach *Zurück*). Mit *Broadcast Dimmen* wäre derselbe Alarm auch für normale Dimmtreiber möglich, wenn es wie *Farbe RGBW* arbeitet.

**Umsetzungsidee (wie bei RGBW):** Broadcast-Befehle als vorübergehenden Zustand behandeln, also kein Status und kein Überschreiben des gespeicherten Zustands; *Zurück* sendet für alle Kanaltypen den gespeicherten Zustand der Gruppen und der Kanäle ohne Gruppe erneut. Reine Schalttreiber können nur Ein/Aus. Nicht getestet (keine anderen Treibertypen vorhanden); Grundlage sind die RGBW-Tests. Zusammen mit F1, F2, G5 und G22 entscheiden. Bis dahin bleibt RGBW getrennt behandelt (Entscheidung 21.11.).

### F4 – Vorschlag: Szenen insgesamt überarbeiten

Heute werden DALI-Szenen 1:1 durchgereicht:
- Die Werte stehen nur im Treiber (in der ETS über Schaltflächen hineingeschrieben, keine Projektparameter; bei einem Treibertausch verloren).
- Die Zuordnung KNX-Szene → DALI-Szene → Adresse/Gruppe/Broadcast steht in einer getrennten Tabelle.
- Die Farbe geht nur als RGB, ohne Weiß.
- Vor allem: **Nach einer Szene ist das Gateway „blind“.** Es kennt die angefahrenen Werte nicht; *Schalten*, *Dimmen* und *Farbe Status* bleiben bei Einzel-EVGs, Gruppen und Broadcast auf dem alten Wert, und *Ein*, *Zurück*, Sperre und HCL arbeiten mit veralteten Werten weiter. Damit verlieren die Statusobjekte nach der ersten Szene ihren Sinn.

**Vorschlag:** Szenen wie bei KNX-Aktoren pro Kanal und Gruppe als Parameter (Ein/Aus, Helligkeit, Farbe inklusive Weiß), ausgeführt über die normale Ansteuerung, also mit Status und bei RGBW über das Sammelfenster; Lernen per KNX speichert den aktuellen Zustand im Gateway. DALI-Szenen könnten als Option für den gleichzeitigen Abruf vieler Treiber bleiben (ein Telegramm, synchron, auch von anderen DALI-Geräten abrufbar), dann mit Statusabfrage danach. Betrifft alle Kanaltypen, deshalb nicht Teil des RGBW-PR (22./23.11.).

### Hinweis: Farbtelegramm bei ausgeschaltetem Kanal (andere Farbtypen)

Bei HSV, RGB, TW und xyY schickt ein Farbtelegramm an einen ausgeschalteten Kanal die Farbe an den Treiber und setzt die Statusobjekte auf *Ein* mit dem Tag-Einschaltwert (`// TODO get real` in `koHandleColor`), obwohl die Lampe aus bleibt. RGBW lässt den Status auf *Aus*. Die anderen Farbtypen haben wir bewusst nicht geändert. **Vorschlag:** HSV, RGB, TW und xyY ebenso anpassen, damit sich alle Farbtypen gleich verhalten und der Status überall den tatsächlichen Zustand der Lampe zeigt.
