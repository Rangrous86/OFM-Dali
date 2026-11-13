# RGBW-Gegenüberstellung: Ausgangscode ↔ Änderung

Vergleich des **Ausgangscodes** (`OpenKNX/OFM-Dali`, Branch `v1dev`, Commit `6e7aca5`) mit dem Stand in `feature/rgbw`.
Der Ausgangscode kennt die Farbtypen HSV, RGB, TW und xyY – **kein RGBW**.

> Stand: gebaut, geflasht und am 13.11.2026 mit einem DA4-M (RGBW-Modus) am `GW-REG1-Dali` erprobt (siehe README). Die Spalte „Risiko“ benennt, was am ehesten schiefgehen kann.

## 1. Überblick

| Datei | Art | Änderung | Risiko |
|-------|-----|----------|--------|
| `src/RgbwHelper.h` | **neu** | Reine Hilfsfunktionen für DPT 251.600: `decode` (Gültigkeitsbits), `isOff` (alle vier Kanäle 0), `encode` (Status), `daliLevel` (255→254). Ohne Arduino-Abhängigkeit, per Host-Test geprüft (`test/test_rgbw_helper.cpp`). | gering |
| `src/DaliChannel.cpp` | geändert | `koHandleColor`: neuer Fall `PT_colorType_RGBW`; bei RGBW schaltet ein Farbtelegramm einen ausgeschalteten Kanal wirklich ein (`sendArc`). Neu: `handleColorRGBW` (inkl. „alles 0 → aus“), `sendRgbwState`, `sendColorAsRGBW`; `sendColor` und `updateCurrentDimmValue` mit RGBW-Zweig. `koHandleDimmAbs`: `currentState` folgt jetzt dem Dimmwert (Fehlerkorrektur, wirkt für alle Kanaltypen außer Treppenhauslicht). | mittel (Ein-/Ausschaltverhalten nach Fix noch einmal auf Hardware zu bestätigen) |
| `src/DaliChannel.h` | geändert | Deklarationen der neuen Methoden; `currentColor[4]` wird mit 0 initialisiert (vorher uninitialisiert). | gering |
| `src/DaliModule.share.xml` | geändert | `colorType`: 2 → 3 Bit, neuer Eintrag `RGBW = 4` (erzeugt `PT_colorType_RGBW`). | gering |
| `src/DaliModule.single.templ.xml` | geändert | `colorType` im Speicher von Byte 7/Bit 5 nach **Byte 21/Bit 5** (3 Bit). Zwei neue KO-Referenzen (DPST-251-600, 6 Byte). Neuer `<when test="4">` in der Farb-Dynamik. Szenen-Spalte „Farbe“ gilt auch für Typ 4 (`0 1 3` → `0 1 3 4`, 17×). | mittel (Producer nicht gelaufen) |
| `src/DaliModule.group.templ.xml` | geändert | `colorType` von Byte 4/Bit 2 nach **Byte 17/Bit 2** (3 Bit). Zwei neue KO-Referenzen, neuer `<when test="=4">`. | mittel |
| `README.md`, `doc/RGBW-Gegenueberstellung.md`, `test/test_rgbw_helper.cpp` | neu | Dokumentation und Host-Test | – |

Nicht geändert: `DaliModule.cpp`, `DaliModule.script.js` (Szenen-Konfigurationstransfer; RGBW wird dort wie RGB behandelt), HCL- und Szenen-Vorlagen, Bibliothek `dali`.

## 2. Warum wandert `colorType` im Speicher?

Der Parameter hatte 2 Bit (Werte 0–3, alle vergeben). Für den fünften Wert (RGBW = 4) braucht er 3 Bit. Die Bits direkt hinter ihm sind in beiden Vorlagen bereits belegt, deshalb wird er in freie Bits am Ende des Parameterblocks verschoben. Die Blockgröße ändert sich dadurch **nicht** (die Zielbytes werden schon benutzt). Alle anderen Parameter behalten ihre Position.

| Vorlage | vorher | nachher |
|---------|--------|---------|
| Einzeladresse | Byte 7, Bit 5–6 (2 Bit) | Byte 21, Bit 5–7 (3 Bit) |
| Gruppe | Byte 4, Bit 2–3 (2 Bit) | Byte 17, Bit 2–4 (3 Bit) |

Folge: Die Applikation ist nicht abwärtskompatibel zum alten Parameterlayout → ETS-Applikationsversion erhöhen (siehe README). Das Skript-Ergebnis (keine überlappenden Bits) wurde mit einem Prüfskript bestätigt, der endgültige Beweis ist der Producer-Lauf.

## 3. Vorher/Nachher: Logik in `DaliChannel.cpp`

### 3.1 Dispatcher `koHandleColor`

Vorher:

```cpp
switch(colorType)
{
    case PT_colorType_HSV: handleColorHSV(ko); break;
    case PT_colorType_RGB: handleColorRGB(ko); break;
    case PT_colorType_TW:  handleColorTW(ko);  break;
    case PT_colorType_XYY: handleColorXYY(ko); break;
}
```

Nachher – zusätzlich:

```cpp
case PT_colorType_RGBW:
    if (!handleColorRGBW(ko))
        return; // Telegramm ändert nichts -> Licht nicht anfassen
    break;
```

### 3.2 Senden an den Treiber `sendColor`

Vorher: Immer nur 3 Kanäle (RGB, ggf. aus HSV gewandelt), dann je nach *Farbe übertragen per* als RGB-Level oder als xy-Koordinaten.

Nachher: RGBW verzweigt vorab und sendet **RGB + Weiß**:

| Schritt | DALI | Daten |
|---------|------|-------|
| 1 | `SET_DTR`, `SET_DTR1`, `SET_DTR2` + `SET TEMPORARY RGB DIMLEVEL` (235, DT8) | R, G, B |
| 2 | `SET_DTR`, `SET_DTR1`, `SET_DTR2` + `SET TEMPORARY WAF DIMLEVEL` (236, DT8) | W, 255, 255 (255 = MASK = unverändert) |
| 3 | `ACTIVATE` (226, DT8) | übernimmt beide Werte gemeinsam |

### 3.3 Statusobjekt

Vorher: 3-Byte-Status (DPT 232.600) bzw. einzelne Rot/Grün/Blau-Status per `sendKoStateOnChange`.
Nachher (nur RGBW): 6-Byte-Status `[R,G,B,W,0x00,0x0F]` direkt in den Objektspeicher geschrieben (nur bei Änderung, `memcmp`), weil `sendKoStateOnChange` keinen 6-Byte-DPT unterstützt. Auch die Aktualisierung am Ende des Dimmvorgangs (`updateCurrentDimmValue`) verwendet bei RGBW diesen Weg, statt das 3-Byte-Format in ein 6-Byte-Objekt zu schreiben.

### 3.4 Gültigkeitsbits

Ein DPT-251.600-Telegramm kann einzelne Kanäle als „ungültig“ markieren. `RgbwHelper::decode` übernimmt nur markierte Kanäle; ein Telegramm ohne gültigen Kanal ändert nichts und löst keinen DALI-Befehl aus. Das ist ein Unterschied zu den anderen Farbtypen, die keine Gültigkeitsbits kennen.

### 3.5 Ein-/Ausschalten (Fund aus dem Hardware-Test)

Vorher (alle Farbtypen): `koHandleColor` rief bei ausgeschaltetem Kanal nur `setDimmState(_onDay, true, true)` auf. Das aktualisiert ausschließlich die KNX-Statusobjekte (*Schalten Status* = Ein, *Dimmen Status*), sendet aber **keinen DALI-Befehl** – die Leuchte blieb dunkel, obwohl das Gateway „Ein“ meldete (`// TODO get real`). Außerdem setzte `koHandleDimmAbs` den Merker `currentState` nie; nach „Schalten Ein“ und „Dimmen auf 0“ blieb er auf *Ein*, und ein folgendes Farbtelegramm wurde komplett übergangen.

Nachher:

| Stelle | Änderung |
|--------|----------|
| `rgbwOff()` / `rgbwOn()` (nur RGBW, neu) | Aus: `DAPC 0`. Ein: Farbe zuerst (Leuchte noch aus, Farb-Fade unsichtbar), der Einschaltwert folgt, sobald `QUERY_STATUS` Bit 4 (Fade läuft) zurückfällt (`loopFadePoll()`, Rückfallzeit Übertragung + Fade-Zeit aus `QUERY_FADE_SPEEDS` + Reserve + 300 ms). Grund aus dem Hardware-Test (DA4-M): der Treiber hat **eine** Fade-Engine, jedes neue Fade-Kommando bricht das laufende ab; die Farbe 0/0/0/0 zeigt der DA4-M als Weiß (Schwarz-Umweg per Konsole `blackoff 1` zuschaltbar, Standard aus); Farbe wird nur bei Pegel 0 unsichtbar übernommen. Unbekannte Treiberfarbe (`markColourUnknown`): alter Ablauf mit Pause `rgbwSettleMs`. |
| `koHandleColor` (nur RGBW) | Ist `currentState` false: `rgbwOn(onValue)`, `setDimmState(onValue,…)`, `currentState = true`. `onValue` (`_onDay`/`_onNight`/`_lastXValue`) ist bereits ein DALI-Wert. |
| `handleSwitchNormal`, `loopDimming`, `koHandleDimmAbs`, `koHandleScene`, `koHandleLock` | RGBW: Aus → `rgbwOff()`, Ein → `rgbwOn()` (ein bereits laufendes Einschalten wird nicht wiederholt); Szene/Sperre verwerfen ausstehende Schritte. `DaliModule`: `broadcastOn()`/`broadcastOff()` (Broadcast-Aus nach Ausblenden aller RGBW-Kanäle), Szenen markieren die Farben als unbekannt. |
| `handleColorRGBW` | Telegramm wird zuerst in eine Kopie dekodiert. Sind danach R, G, B und W alle 0 (`RgbwHelper::isOff`), wird `rgbwOff()` aufgerufen (Farbe → Schwarz → Aus), `setDimmState(0,…)` setzt *Schalten Status* = Aus und *Dimmen Status* = 0 %, `currentState = false`. Die gespeicherte Farbe bleibt erhalten. Bei eingeschaltetem Kanal wird die Farbe direkt gesendet. |
| `koHandleDimmAbs` | `currentState = (arc > 0)` (außer Treppenhauslicht). Wirkt für alle Kanaltypen. |

## 4. Vorher/Nachher: ETS-Applikation

| Thema | Vorher | Nachher |
|-------|--------|---------|
| Farbtyp-Auswahl | HSV, RGB, TW, xyY | + **RGBW** |
| Objekte bei RGBW | – | *Farbe RGBW* (KO 6) und *Farbe RGBW Status* (KO 7), je DPST-251-600, 6 Byte |
| „Farbe übertragen per“ | sichtbar bei HSV/RGB/xyY | bei RGBW ausgeblendet (nicht relevant) |
| Szenen-Farbspalte | für Typ 0, 1, 3 | für Typ 0, 1, 3, **4** (nur RGB-Anteil) |

## 5. Nicht gelöst / offen

- Szenenfarbe per Konfigurationstransfer enthält kein Weiß.
- Keine getrennten Einzelobjekte (R/G/B/W 8 Bit) bei RGBW.
- Das DALI-Mapping des Weißkanals (WAF-Level, Kanalzuordnung) hängt vom Treiber ab. Getestet nur mit dem DA4-M (RGBW-Modus).
- Bei HSV/RGB/TW/xyY meldet ein Farbtelegramm an einen ausgeschalteten Kanal weiterhin nur „Ein“ in den Statusobjekten, ohne DALI-Einschaltbefehl (Ausgangscode, `TODO get real`). Nur für RGBW korrigiert.
- Die ETS-Applikation ist nicht in-place zu Geräten der Vorgängerversion updatebar (verschobener Parameter, `ReplacesVersions`).

## 6. Fragen an den Maintainer (thewhobox)

1. Ist die Verschiebung von `colorType` im Parameterspeicher (Einzel: Byte 21, Gruppe: Byte 17) in Ordnung, oder soll das Layout anders erweitert werden?
2. Soll das Weiß über `SET TEMPORARY WAF DIMLEVEL` (236) laufen, oder bevorzugst du `RGBWAF` (237)/einen anderen Weg?
3. Sollen zusätzlich getrennte 8-Bit-Objekte für R/G/B/W angeboten werden (wie bei MDT)?
4. Wie sollen Szenen/DCA-Konfiguration mit Weiß umgehen?
5. Soll das Einschalten durch ein Farbtelegramm (echter `sendArc`) auch für HSV/RGB/TW/xyY gelten? Und ist die Korrektur von `currentState` in `koHandleDimmAbs` so in Ordnung?

## 7. Neue Datei `src/RgbwHelper.h` (vollständig)

```cpp
#pragma once
#include <stdint.h>

/*
 * Hilfsfunktionen für KNX DPT 251.600 (DPT_Colour_RGBW) und die Übersetzung auf DALI DT8.
 *
 * Telegramm (6 Byte). Die Feldreihenfolge entspricht der Definition in knx_master.xml (MSB zuerst):
 *   Byte 0: Rot    (0..255, die ETS zeigt 0..100 %)
 *   Byte 1: Grün
 *   Byte 2: Blau
 *   Byte 3: Weiß
 *   Byte 4: reserviert (0x00)
 *   Byte 5: Bit 7..4 reserviert, Bit 3 = mR, Bit 2 = mG, Bit 1 = mB, Bit 0 = mW  (1 = Wert gültig)
 *
 * Bewusst ohne Abhängigkeit zu Arduino/KNX-Stack, damit die Logik auf dem Rechner testbar ist.
 */
namespace RgbwHelper
{
    constexpr uint8_t DataLength = 6;

    constexpr uint8_t ValidRed = 0x08;
    constexpr uint8_t ValidGreen = 0x04;
    constexpr uint8_t ValidBlue = 0x02;
    constexpr uint8_t ValidWhite = 0x01;
    constexpr uint8_t ValidAll = 0x0F;

    /*
     * Übernimmt nur die im Telegramm als gültig markierten Kanäle nach color[0..3] (R, G, B, W).
     * Nicht markierte Kanäle behalten ihren bisherigen Wert.
     * Rückgabe: Gültigkeitsmaske (untere 4 Bit). 0 = das Telegramm ändert nichts.
     */
    inline uint8_t decode(const uint8_t *data, uint8_t *color)
    {
        const uint8_t valid = data[5] & ValidAll;
        if (valid & ValidRed)
            color[0] = data[0];
        if (valid & ValidGreen)
            color[1] = data[1];
        if (valid & ValidBlue)
            color[2] = data[2];
        if (valid & ValidWhite)
            color[3] = data[3];
        return valid;
    }

    /*
     * true, wenn alle vier Kanäle (R, G, B, W) 0 sind. Das gilt als "Aus".
     */
    inline bool isOff(const uint8_t *color)
    {
        return color[0] == 0 && color[1] == 0 && color[2] == 0 && color[3] == 0;
    }

    /*
     * Schreibt color[0..3] als 6-Byte-Telegramm (z.B. für das Statusobjekt). Alle Kanäle gelten als gültig.
     */
    inline void encode(const uint8_t *color, uint8_t *data)
    {
        data[0] = color[0];
        data[1] = color[1];
        data[2] = color[2];
        data[3] = color[3];
        data[4] = 0x00;
        data[5] = ValidAll;
    }

    /*
     * DALI-Pegel: 255 bedeutet bei DT8 "MASK" (= Kanal nicht ändern), 254 ist der Maximalwert.
     */
    inline uint8_t daliLevel(uint8_t value)
    {
        return value == 255 ? 254 : value;
    }
}
```

## 8. Vollständiges Diff gegenüber `v1dev`

Ohne `src/RgbwHelper.h` (siehe Abschnitt 7). Enthält Diagnose-Code (`RGBW_DIAG`, Standard aus), der vor einem Pull Request entfällt.

```diff
diff --git a/src/DaliChannel.cpp b/src/DaliChannel.cpp
index dffeaaf..2151a1f 100644
--- a/src/DaliChannel.cpp
+++ b/src/DaliChannel.cpp
@@ -1,4 +1,5 @@
 #include "DaliChannel.h"
+#include "RgbwHelper.h"
 #include "OpenKNX.h"
 
 DaliChannel::DaliChannel(Dali::Master &_daliMaster) : daliMaster(_daliMaster) {}
@@ -109,6 +110,337 @@ void DaliChannel::loop1()
     loopDimming();
     loopError();
     loopQueryLevel();
+    loopPendingOn();
+    loopRgbwSeq();
+    loopFadeQuery();
+#ifdef RGBW_DIAG
+    loopDiag();
+#endif
+}
+
+#ifdef RGBW_DIAG
+// Asks the driver for its real level at fixed times after an RGBW colour switch-on and logs the answers
+// (the answer is logged in loopQueryLevel and also updates the "Dimmen Status" object).
+void DaliChannel::loopDiag()
+{
+    static const uint16_t times[] = {300, 700, 1100, 1500, 2000, 3000, 5000};
+    static const uint16_t statusTimes[] = {150, 500, 900, 1300, 1700, 2300, 3300};
+    const uint8_t count = sizeof(times) / sizeof(times[0]);
+
+    // answer of a QUERY_STATUS probe: bit 4 = "fade running"
+    if (_diagStatusId != 0)
+    {
+        Dali::Response response = daliMaster.getResponse(_diagStatusId);
+        if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER)
+        {
+            logDebugP("DIAG status: no answer");
+            _diagStatusId = 0;
+        }
+        else if (response.state != Dali::ResponseState::SENT && response.state != Dali::ResponseState::WAITING)
+        {
+            uint8_t st = response.frame.data & 0xFF;
+            logDebugP("DIAG status 0x%02X fadeRunning=%i (t+%lu ms)", st, (st >> 4) & 1, millis() - _diagStart);
+            _diagStatusId = 0;
+        }
+    }
+
+    if (!_diagActive)
+        return;
+    if (_diagStep >= count && _diagStatusStep >= count)
+    {
+        _diagActive = false;
+        return;
+    }
+
+    unsigned long t = millis() - _diagStart;
+    if (_diagStatusStep < count && _diagStatusId == 0 && _queryId == 0 && t >= statusTimes[_diagStatusStep])
+    {
+        _diagStatusStep++;
+        if (!_isGroup)
+            _diagStatusId = daliMaster.sendCommand(_channelIndex, Dali::Command::QUERY_STATUS, false, true);
+        return;
+    }
+    if (_diagStep < count && _queryId == 0 && _diagStatusId == 0 && t >= times[_diagStep])
+    {
+        logDebugP("DIAG t+%lu ms: query actual level", t);
+        queryActualLevel();
+        _diagStep++;
+    }
+}
+
+void DaliChannel::diagStart()
+{
+    _diagStart = millis();
+    _diagStep = 0;
+    _diagStatusStep = 0;
+    _diagActive = true;
+}
+#endif
+
+uint16_t DaliChannel::rgbwSettleMs = RgbwSettleMs;
+uint16_t DaliChannel::rgbwMarginMs = RgbwMarginMs;
+uint8_t DaliChannel::rgbwOnMode = 1;
+bool DaliChannel::rgbwBlackOff = false;
+uint16_t DaliChannel::rgbwOnDelayMs = 0; // 0 = automatic: wait until the driver reports that the colour fade is over
+
+// ---------------------------------------------------------------------------------------------------------
+// RGBW switch on/off (DA4-M findings, all from hardware tests):
+// - The driver fades level AND colour changes (fade time, ~1 s) and has ONE fade at a time: every new fade
+//   command (DAPC, ACTIVATE) aborts the running one and the driver stays where it is.
+// - A colour is only stored/shown correctly when it is sent while the lamp is on.
+// So "off" is done in two steps: the colour fades to black (stored colour = black), then the level goes to 0.
+// The next switch-on starts with a black colour: level first (dark), when its fade is over the colour fades
+// in. If the stored colour is not known to be black (first use, scenes, other DALI masters) the old sequence
+// "level, colour, pause, level" is used (rgbwSettleMs).
+// ---------------------------------------------------------------------------------------------------------
+bool DaliChannel::isRgbw()
+{
+    return (_isGroup ? ParamDGWG_colorType : ParamDGW_colorType) == PT_colorType_RGBW;
+}
+
+// Time (ms, from queueing the colour sequence) until the colour change is complete: bus time of the
+// sequence up to its ACTIVATE + fade time of the driver + reserve
+uint16_t DaliChannel::rgbwOffWaitMs()
+{
+    return RgbwTxMs + _fadeMs + rgbwMarginMs;
+}
+
+void DaliChannel::markColourUnknown()
+{
+    _driverBlack = false;
+}
+
+void DaliChannel::rgbwOff()
+{
+    // A channel that is off for us gets the plain level off: a colour sent at level 0 may not be taken over
+    // by the driver, so "stored colour is black" could not be guaranteed.
+    bool wasOn = currentState;
+    currentState = false;
+    _onArcPending = false;
+    _colorPending = false;
+    if (_offPending)
+        return; // already on its way (e.g. repeated off commands while dimming down)
+
+    if (!rgbwBlackOff || _driverBlack || !wasOn)
+    {
+        logDebugP("RGBW: off (%s)", !rgbwBlackOff ? "plain level off" : (_driverBlack ? "stored colour is black" : "channel was off"));
+        daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
+        return;
+    }
+
+    uint16_t wait = rgbwOffWaitMs();
+    logDebugP("RGBW: off: colour to black first, level off in %i ms", wait);
+    sendColorAsRGBW(0, 0, 0, 0); // sets _driverBlack
+    _offPending = true;
+    _offAt = millis() + wait;
+#ifdef RGBW_DIAG
+    diagStart();
+#endif
+}
+
+// The channel is off from the gateway's point of view and has to be switched on with the stored colour.
+// onValue is a DALI arc value. Callers set currentState and the status objects.
+void DaliChannel::rgbwOn(uint8_t onValue)
+{
+    bool colourValid = !RgbwHelper::isOff(currentColor);
+
+    if (_offPending)
+    {
+        // The lamp still shines and its colour fades to black: bring the colour back, then the level.
+        _offPending = false;
+        logDebugP("RGBW: on while fading out");
+        if (colourValid)
+            sendColor();
+        _onArcValue = onValue;
+        _onArcAt = millis() + rgbwOffWaitMs();
+        _onArcPending = true;
+        return;
+    }
+
+    if (!colourValid)
+    {
+        // no colour ever received: nothing to restore, plain switch on
+        daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+        return;
+    }
+
+    if (_driverBlack && rgbwOnMode == 0)
+    {
+        // Level first. The lamp stays dark (black). The colour is sent so that its ACTIVATE (at the end of the
+        // sequence) comes after the level fade, because ACTIVATE would abort it.
+        uint16_t wait = _fadeMs + rgbwMarginMs;
+        _colorAt = millis() + (wait > RgbwTxMs ? wait - RgbwTxMs : 0);
+        _colorPending = true;
+        logDebugP("RGBW: on from black: level %i, colour in %lu ms", onValue, (unsigned long)(_colorAt - millis()));
+        daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+#ifdef RGBW_DIAG
+        diagStart();
+#endif
+        return;
+    }
+
+    if (_driverBlack || rgbwOnMode == 1)
+    {
+        // Colour first (fades unseen at level 0, if the driver takes it), then the level
+        sendColor();
+        _onArcValue = onValue;
+        _onArcPending = true;
+        if (rgbwOnDelayMs > 0)
+        {
+            logDebugP("RGBW: on, colour first, level %i in %i ms (fixed)", onValue, rgbwOnDelayMs);
+            _onArcAt = millis() + rgbwOnDelayMs;
+        }
+        else
+        {
+            // The level follows as soon as QUERY_STATUS reports that the colour fade is over (bit 4 "fade running").
+            // Fallback when no answer comes: transmission + fade time + reserve + a little extra.
+            _onArcAt = millis() + rgbwOffWaitMs() + 300;
+            logDebugP("RGBW: on, colour first, level %i when the colour fade is over (latest in %i ms)", onValue, rgbwOffWaitMs() + 300);
+            _fadePolling = true;
+            _fadePollAt = millis(); // the queue is FIFO: the first poll goes out right behind the ACTIVATE frame
+            _fadePollT0 = millis();
+        }
+#ifdef RGBW_DIAG
+        diagStart();
+#endif
+        return;
+    }
+
+    // Stored colour unknown: level, colour, pause, level (the old colour is visible for a moment)
+    logDebugP("RGBW: on, stored colour unknown: level, colour, level in %i ms", rgbwSettleMs);
+    daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+    sendColor();
+    _onArcValue = onValue;
+    _onArcAt = millis() + rgbwSettleMs;
+    _onArcPending = true;
+#ifdef RGBW_DIAG
+    diagStart();
+#endif
+}
+
+// Broadcast on: the level command went out to everybody already, the colour of an RGBW channel is restored here
+void DaliChannel::rgbwBroadcastOn()
+{
+    if (currentState)
+        return;
+    rgbwOn(0xFE);
+    currentState = true;
+}
+
+void DaliChannel::loopRgbwSeq()
+{
+    if (_offPending && (long)(millis() - _offAt) >= 0)
+    {
+        _offPending = false;
+        logDebugP("RGBW: level off");
+        daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
+    }
+    if (_colorPending && (long)(millis() - _colorAt) >= 0)
+    {
+        _colorPending = false;
+        logDebugP("RGBW: colour after level");
+        sendColor();
+    }
+}
+
+// Reads the fade time of the driver once (QUERY_FADE_SPEEDS: high nibble = fade time, low = fade rate)
+void DaliChannel::loopFadeQuery()
+{
+    static const uint32_t fadeTimes[16] = {0, 700, 1000, 1400, 2000, 2800, 4000, 5700, 8000, 11300, 16000, 22600, 32000, 45300, 64000, 90500};
+
+    if (!_fadeQueried)
+    {
+        _fadeQueried = true;
+        if (!isRgbw())
+            return;
+        if (_isGroup && _dimmReferenceAddress == NoDimmReference)
+        {
+            logDebugP("RGBW: no dimm reference address for the group, fade time stays %i ms", _fadeMs);
+            return;
+        }
+        _fadeQueryId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_FADE_SPEEDS, false, true);
+        return;
+    }
+    if (_fadeQueryId == 0)
+        return;
+
+    Dali::Response response = daliMaster.getResponse(_fadeQueryId);
+    if (response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
+        return;
+    _fadeQueryId = 0;
+    if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER ||
+        (response.frame.flags & DALI_FRAME_ERROR))
+    {
+        logDebugP("RGBW: fade time query failed, using %i ms", _fadeMs);
+        return;
+    }
+    uint8_t raw = response.frame.data & 0xFF;
+    uint32_t fade = fadeTimes[raw >> 4];
+    _fadeMs = fade > 65000 ? 65000 : fade; // member is 16 bit
+    logInfoP("RGBW: fade time x=%i = %i ms (raw 0x%02X)", raw >> 4, _fadeMs, raw);
+}
+
+// Sends the delayed final "on value" of an RGBW colour switch-on (see rgbwOn).
+
+// Asks the driver (QUERY_STATUS, bit 4 = fade running) whether the colour fade of an RGBW switch-on is over.
+// The answer was verified to cover colour fades at level 0.
+void DaliChannel::loopFadePoll()
+{
+    if (_fadePollId != 0)
+    {
+        Dali::Response response = daliMaster.getResponse(_fadePollId);
+        if (response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
+            return;
+        _fadePollId = 0;
+        if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER ||
+            (response.frame.flags & DALI_FRAME_ERROR))
+        {
+            _fadePolling = false; // no answer: the fallback time of _onArcAt applies
+            return;
+        }
+        uint8_t st = response.frame.data & 0xFF;
+        if (_onArcPending && _fadePolling)
+        {
+            if (!((st >> 4) & 1))
+            {
+                logDebugP("RGBW: colour fade over (status 0x%02X), %lu ms after the colour telegram", st, millis() - _fadePollT0);
+                _fadePolling = false;
+                _onArcAt = millis();
+            }
+            else
+                _fadePollAt = millis(); // next poll at once, a poll takes ~45 ms on the bus
+        }
+        return;
+    }
+    if (!_fadePolling)
+        return;
+    if (!_onArcPending)
+    {
+        _fadePolling = false;
+        return;
+    }
+    if ((long)(millis() - _fadePollAt) < 0)
+        return;
+    if (_isGroup && _dimmReferenceAddress == NoDimmReference)
+    {
+        _fadePolling = false;
+        return;
+    }
+    _fadePollId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_STATUS, false, true);
+    if (_fadePollId == 0)
+        _fadePolling = false;
+}
+
+void DaliChannel::loopPendingOn()
+{
+    loopFadePoll();
+    if (!_onArcPending)
+        return;
+    if ((long)(millis() - _onArcAt) < 0)
+        return;
+    _onArcPending = false;
+    logDebugP("RGBW: final on value %i", _onArcValue);
+    daliMaster.sendArc(_channelIndex, _onArcValue, _isGroup);
 }
 
 void DaliChannel::loopStaircase()
@@ -169,7 +501,10 @@ void DaliChannel::loopDimming()
                         _dimmDirection = DimmDirection::None;
                     } else {
                         logDebugP("Turn off device");
-                        daliMaster.sendCommand(_channelIndex, Dali::Command::OFF, _isGroup);
+                        if (isRgbw())
+                            rgbwOff();
+                        else
+                            daliMaster.sendCommand(_channelIndex, Dali::Command::OFF, _isGroup);
                     }
                 }
             }
@@ -271,7 +606,11 @@ void DaliChannel::loopQueryLevel()
                 return;
             } else {
                 uint8_t data = response.frame.data & 0xFF;
+#ifdef RGBW_DIAG
+                logDebugP("Got new actual level %i%%-%i (t+%lu ms)", DaliHelper::arcToPercent(data), data, millis() - _diagStart);
+#else
                 logDebugP("Got new actual level %i%%-%i", DaliHelper::arcToPercent(data), data);
+#endif
                 setDimmState(data, false, true);
 
                 if(currentDimmType == DimmType::Brigthness)
@@ -303,8 +642,8 @@ void DaliChannel::processInputKo(GroupObject &ko)
         //logDebugP("Got SHORT KO %i", chanIndex);
     }
 
-    // Case numbers not listed below (1, 4, 7, 10, 13, 16, 19) are read-only
-    // *_state KOs and are never received as input here.
+    // Case numbers not listed below (1, 4, 7, 10, 13, 16, 19) are the *_state KOs; they are handed to us
+    // when we write them ourselves and are ignored here.
     switch (chanIndex)
     {
     case DGW_Koswitch:
@@ -382,6 +721,10 @@ void DaliChannel::koHandleScene(GroupObject &ko)
         return;
     }
 
+    // the driver executes the scene on its own: the colour it stores afterwards is not known to us
+    _driverBlack = false;
+    _onArcPending = false;
+    _colorPending = false;
     daliMaster.sendCommand(_channelIndex, Dali::Command::GO_TO_SCENE | number, _isGroup);
 }
 
@@ -487,7 +830,16 @@ void DaliChannel::handleSwitchNormal(GroupObject &ko)
         if(!hclBriActivated) {
             logDebugP(isNight ? "Turning on night value" : "Turning on day value");
         }
-        daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+        if (isRgbw())
+        {
+            // see rgbwOn(): a switch-on already running (e.g. the colour telegram of the same command) is not repeated
+            if (!currentState)
+                rgbwOn(onValue);
+            else if (!_onArcPending && !_colorPending)
+                daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+        }
+        else
+            daliMaster.sendArc(_channelIndex, onValue, _isGroup);
 
         bool hclTempActivated = false;
         if(_hclCurve != HclDisabled && _hclIsAutoMode)
@@ -505,7 +857,10 @@ void DaliChannel::handleSwitchNormal(GroupObject &ko)
     else
     {
         logDebugP("Turning off");
-        daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
+        if (isRgbw())
+            rgbwOff();
+        else
+            daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
         setSwitchState(false);
     }
     currentState = value;
@@ -633,8 +988,30 @@ void DaliChannel::koHandleDimmAbs(GroupObject &ko)
     uint8_t value = ko.value(Dpt(5, 1));
     logDebugP("Dimming absolute to %i%%", value);
     uint8_t arc = DaliHelper::percentToArc(value);
-    daliMaster.sendArc(_channelIndex, arc, _isGroup);
+    if (isRgbw() && !_isStaircase)
+    {
+        if (arc == 0)
+            rgbwOff();
+        else if (!currentState)
+            rgbwOn(arc);
+        else if (_colorPending)
+        {
+            // level first, colour follows: apply the new level after the colour fade
+            _onArcValue = arc;
+            _onArcAt = _colorAt + rgbwOffWaitMs();
+            _onArcPending = true;
+        }
+        else if (_onArcPending)
+            _onArcValue = arc; // a switch-on is running: just change its final level
+        else
+            daliMaster.sendArc(_channelIndex, arc, _isGroup);
+    }
+    else
+        daliMaster.sendArc(_channelIndex, arc, _isGroup);
     setDimmState(arc, true, true);
+    // keep the on/off flag in sync, otherwise e.g. a colour telegram after "dimm to 0" is ignored
+    if (!_isStaircase)
+        currentState = arc > 0;
 }
 
 void DaliChannel::koHandleLock(GroupObject &ko)
@@ -701,13 +1078,15 @@ void DaliChannel::koHandleLock(GroupObject &ko)
     }
     }
     logDebugP("%i - %i", behavevalue, DaliHelper::percentToArc(behavevalue));
+    _onArcPending = false;
+    _colorPending = false;
     daliMaster.sendArc(_channelIndex, behavevalue, _isGroup);
     setDimmState(DaliHelper::percentToArc(behavevalue));
 }
 
 void DaliChannel::koHandleColor(GroupObject &ko)
 {
-    logDebugP("Color absolute");
+    logDebugP("Color absolute [%s]", RGBW_BUILD_TAG);
     if (currentIsLocked)
     {
         logErrorP("is locked");
@@ -738,11 +1117,33 @@ void DaliChannel::koHandleColor(GroupObject &ko)
         case PT_colorType_XYY:
             handleColorXYY(ko);
             break;
+
+        case PT_colorType_RGBW:
+            if (!handleColorRGBW(ko))
+                return; // Telegram changes nothing (no valid channel) -> do not touch the light
+            break;
     }
 
     if(!currentState)
-        setDimmState(_onDay, true, true); // TODO get real
-    
+    {
+        if (colorType == PT_colorType_RGBW)
+        {
+            // A colour telegram on a switched-off channel really switches it on (same on value as
+            // "Schalten = Ein"). setDimmState() alone only updates the KNX status objects and would
+            // report "On" while the luminaire stays dark.
+            uint8_t onValue = isNight ? _onNight : _onDay;
+            if (onValue == 0)
+                onValue = isNight ? _lastNightValue : _lastDayValue;
+            logDebugP("RGBW: switching on with %i", onValue);
+            // _onDay/_onNight/_lastXValue are DALI arc values already (no percent conversion).
+            rgbwOn(onValue);
+            setDimmState(onValue, true, true);
+            currentState = true;
+        }
+        else
+            setDimmState(_onDay, true, true); // TODO get real
+    }
+
     logDebugP("AutoConf %i %i %i", _isGroup, _isGroup ? ParamDGW_hcl_manu_col : ParamDGWG_hcl_manu_col, _hclIsAutoMode);
 }
 
@@ -780,6 +1181,81 @@ void DaliChannel::handleColorRGB(GroupObject &ko)
     sendColor();
 }
 
+bool DaliChannel::handleColorRGBW(GroupObject &ko)
+{
+    // DPT 251.600 has 6 bytes: R, G, B, W, reserved, validity bits (mR, mG, mB, mW)
+    if (ko.valueSize() != RgbwHelper::DataLength)
+    {
+        logErrorP("RGBW: unexpected object size %i", (int)ko.valueSize());
+        return false;
+    }
+
+    // Decode into a copy first: all channels 0 means "off" and must not overwrite the stored colour
+    uint8_t newColor[4] = {currentColor[0], currentColor[1], currentColor[2], currentColor[3]};
+    const uint8_t valid = RgbwHelper::decode(ko.valueRef(), newColor);
+    if (valid == 0)
+    {
+        logDebugP("RGBW: no valid channel in telegram, ignored");
+        return false;
+    }
+
+    logDebugP("Got RGBW: %i %i %i %i (valid %X)", newColor[0], newColor[1], newColor[2], newColor[3], valid);
+
+    if (RgbwHelper::isOff(newColor))
+    {
+        // R = G = B = W = 0 -> switch the channel off (status objects report "Off", 0 %).
+        // The last colour stays stored in gateway and driver, so "Schalten = Ein" or the next
+        // colour telegram brings the light back with that colour instead of staying dark.
+        logDebugP("RGBW: all channels 0 -> switching off");
+        rgbwOff();
+        setDimmState(0, true, true);
+        currentState = false;
+        return false; // already handled, do not switch on again
+    }
+
+    memcpy(currentColor, newColor, sizeof(currentColor));
+    sendRgbwState();
+
+    // Some drivers (seen with a DA4-M) ignore a colour change while the channel is off (arc level 0)
+    // and light up with the old colour. For a switched-off channel the colour is therefore sent in
+    // koHandleColor() while the channel is switched on; here only for channels that are already on.
+    if (currentState)
+    {
+        if (_colorPending)
+        {
+            // level first, colour follows: loopRgbwSeq sends the newest colour at its time
+        }
+        else
+        {
+            sendColor();
+            if (_onArcPending)
+            {
+                _onArcAt = millis() + rgbwOffWaitMs(); // another colour change is running: wait for it as well
+                _fadePolling = false;                  // fixed time again, a poll answer could be about the old colour
+            }
+        }
+    }
+    return true;
+}
+
+void DaliChannel::sendRgbwState()
+{
+    GroupObject &ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_rgb_state));
+    if (ko.valueSize() != RgbwHelper::DataLength)
+        return;
+
+    uint8_t data[RgbwHelper::DataLength];
+    RgbwHelper::encode(currentColor, data);
+
+    // sendKoStateOnChange() has no DPT for 6 byte colour objects -> compare and write raw
+    uint8_t *state = ko.valueRef();
+    if (memcmp(state, data, RgbwHelper::DataLength) != 0)
+    {
+        memcpy(state, data, RgbwHelper::DataLength);
+        ko.objectWritten();
+    }
+}
+
 void DaliChannel::handleColorTW(GroupObject &ko)
 {
     uint16_t kelvin = ko.value(Dpt(7, 600));
@@ -872,6 +1348,13 @@ void DaliChannel::sendColor()
     // TODO only send every 100ms?
     uint8_t r, g, b;
     uint8_t colorType = _isGroup ? ParamDGWG_colorType : ParamDGW_colorType;
+    if (colorType == PT_colorType_RGBW)
+    {
+        // RGBW is always transmitted as RGB level + white level, "colorSpace" does not apply
+        sendColorAsRGBW(currentColor[0], currentColor[1], currentColor[2], currentColor[3]);
+        return;
+    }
+
     if (colorType == PT_colorType_HSV)
     {
         ColorHelper::hsvToRGB(currentColor[0], currentColor[1], currentColor[2], r, g, b);
@@ -905,6 +1388,31 @@ void DaliChannel::sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b)
     daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
 }
 
+void DaliChannel::sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
+{
+    r = RgbwHelper::daliLevel(r);
+    g = RgbwHelper::daliLevel(g);
+    b = RgbwHelper::daliLevel(b);
+    w = RgbwHelper::daliLevel(w);
+    logDebugP("Send RGBW: %i %i %i %i", r, g, b, w);
+    _driverBlack = (r | g | b | w) == 0;
+
+    // 1. RGB level (DTR0 = R, DTR1 = G, DTR2 = B)
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, r);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, g);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, b);
+    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_RGB_LEVEL, _isGroup);
+
+    // 2. WAF level (DTR0 = W, DTR1 = Amber, DTR2 = Freecolour); 255 = MASK = leave unchanged
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, w);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, 255);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, 255);
+    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_WAF_LEVEL, _isGroup);
+
+    // 3. Apply both temporary values at once
+    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
+}
+
 void DaliChannel::sendColorAsXY(uint8_t r, uint8_t g, uint8_t b)
 {
     uint16_t x;
@@ -1002,6 +1510,12 @@ void DaliChannel::updateCurrentDimmValue()
 
         case DimmType::Color:
         {
+            if ((_isGroup ? ParamDGWG_colorType : ParamDGW_colorType) == PT_colorType_RGBW)
+            {
+                sendRgbwState();
+                break;
+            }
+
             uint32_t value = currentColor[2];
             value |= currentColor[1] << 8;
             value |= currentColor[0] << 16;
diff --git a/src/DaliChannel.h b/src/DaliChannel.h
index 19b49c9..7f965c9 100644
--- a/src/DaliChannel.h
+++ b/src/DaliChannel.h
@@ -8,11 +8,28 @@
 #include "DaliConstants.h"
 
 #define DimmInterval 200
+// Temporary diagnosis for the RGBW colour switch-on (remove before a PR): level queries after the
+// colour sequence and a log of every DALI frame (see DaliModule::loopDiagFrames).
+// #define RGBW_DIAG
+#define RGBW_BUILD_TAG "v6"
+// RGBW: default pause (ms, counted from queueing the colour sequence) before the final on value of a colour
+// switch-on. The colour sequence itself needs ~300 ms on the bus. Changeable at runtime: console command "settle".
+#define RgbwSettleMs 1500
+// RGBW: time the colour sequence (12 DALI frames) needs on the bus before its ACTIVATE is sent
+#define RgbwTxMs 310
+// RGBW: default reserve (ms) on top of the driver's fade time
+#define RgbwMarginMs 100
 #define DimmStatusInterval 500
 
 class DaliChannel : public OpenKNX::Channel
 {
 	public:
+		// RGBW timing, changeable at runtime through the console (settle / margin / onmode)
+		static uint16_t rgbwSettleMs;   // fallback pause when the stored driver colour is unknown
+		static uint16_t rgbwMarginMs;   // reserve on top of the fade time
+		static uint8_t rgbwOnMode;      // 0 = level first, then colour; 1 = colour first, then level (default)
+		static bool rgbwBlackOff;       // true: fade the colour to all 0 before the level off (the DA4-M shows white for all 0, so default false)
+		static uint16_t rgbwOnDelayMs;  // mode 1: pause before the level
         static constexpr uint16_t AllGroups = DaliConstants::AllGroups;
 
         DaliChannel(Dali::Master &master);
@@ -20,6 +37,15 @@ class DaliChannel : public OpenKNX::Channel
 
 		void loop() override;
 		void loop1();
+
+		// RGBW (DPT 251.600) switch on/off sequences, see DaliChannel.cpp
+		bool isRgbw();
+		void rgbwOff();
+		void rgbwOn(uint8_t onValue);
+		uint16_t rgbwOffWaitMs();
+		void markColourUnknown();
+		void rgbwBroadcastOn();
+
 		void setup() override;
 		void processInputKo(GroupObject &ko) override;
 		const bool isConfigured();
@@ -106,14 +132,51 @@ class DaliChannel : public OpenKNX::Channel
 		uint8_t currentStep = 0;
 		//Current lock state
 		bool currentIsLocked = false;
-		//Current color
-		uint8_t currentColor[4];
+		//Current color (R, G, B, W). With xyY the 4 bytes are used as two 16 bit values.
+		uint8_t currentColor[4] = {0, 0, 0, 0};
 
 		//Querying current state
 		uint32_t _queryId = 0;
 		uint16_t _queryInterval = 0;
 		unsigned long _lastValueQuery = 0;
 
+		//RGBW: final "on value" after a colour switch-on from off, sent delayed so that it does not abort the
+		//colour fade of the driver (see koHandleColor)
+		bool _onArcPending = false;
+		uint8_t _onArcValue = 0;
+		unsigned long _onArcAt = 0;
+		void loopPendingOn();
+		void loopRgbwSeq();
+		void loopFadeQuery();
+		void loopFadePoll();
+		// switch-on with colour first: wait for the end of the colour fade (QUERY_STATUS) before the level
+		bool _fadePolling = false;
+		unsigned long _fadePollAt = 0;
+		uint32_t _fadePollId = 0;
+		unsigned long _fadePollT0 = 0;
+
+		// Driver colour is known to be black: we sent R=G=B=W=0 last and nothing changed it since
+		bool _driverBlack = false;
+		// off sequence: colour fades to black, then the level is switched off (_offAt)
+		bool _offPending = false;
+		unsigned long _offAt = 0;
+		// on sequence from level 0: level first, the colour follows when the level fade is over (_colorAt)
+		bool _colorPending = false;
+		unsigned long _colorAt = 0;
+		// fade time of the driver (QUERY_FADE_SPEEDS), default 1 s
+		uint16_t _fadeMs = 1000;
+		bool _fadeQueried = false;
+		uint32_t _fadeQueryId = 0;
+#ifdef RGBW_DIAG
+		unsigned long _diagStart = 0;
+		uint8_t _diagStep = 0;
+		bool _diagActive = false;
+		uint8_t _diagStatusStep = 0;
+		uint32_t _diagStatusId = 0;
+		void loopDiag();
+		void diagStart();
+#endif
+
 		//Turn-on with last value
 		uint8_t _lastDayValue = 100;
 		uint8_t _lastNightValue = 10;
@@ -131,6 +194,8 @@ class DaliChannel : public OpenKNX::Channel
 		void updateCurrentDimmValue();
 		void sendColor();
 		void sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b);
+		void sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
+		void sendRgbwState();
 		void sendColorAsXY(uint8_t r, uint8_t g, uint8_t b);
 		void sendKoStateOnChange(uint16_t koNr, const KNXValue &value, const Dpt &type);
 		void setTemperature(uint16_t value);
@@ -148,6 +213,7 @@ class DaliChannel : public OpenKNX::Channel
 		void handleColorRGB(GroupObject &ko);
 		void handleColorTW(GroupObject &ko);
 		void handleColorXYY(GroupObject &ko);
+		bool handleColorRGBW(GroupObject &ko);
 		void koHandleColorRel(GroupObject &ko, ColorChannel channel);
 		void koHandleColorAbs(GroupObject &ko, ColorChannel channel);
 		void koHandleHclCurve(GroupObject &ko);
diff --git a/src/DaliModule.cpp b/src/DaliModule.cpp
index 47a905c..828ff46 100644
--- a/src/DaliModule.cpp
+++ b/src/DaliModule.cpp
@@ -115,8 +115,30 @@ void DaliModule::handleFunc(uint8_t setting)
 void DaliModule::setup1(bool conf) {
     pinMode(REG1_APP_PIN7, INPUT);
     daliMaster.init(REG1_APP_PIN6, REG1_APP_PIN7);
+#ifdef RGBW_DIAG
+    daliMaster.registerMonitor([this](Dali::Frame frame) {
+        uint8_t next = (_diagHead + 1) % DiagRingSize;
+        if (next == _diagTail)
+            return; // ring full: drop
+        _diagRing[_diagHead] = {millis(), frame.data, frame.size, frame.flags};
+        _diagHead = next;
+    });
+#endif
 }
 
+#ifdef RGBW_DIAG
+void DaliModule::loopDiagFrames()
+{
+    uint8_t n = 0;
+    while (_diagTail != _diagHead && n++ < 8)
+    {
+        const DiagFrame &f = _diagRing[_diagTail];
+        logDebugP("DALI %lu ms: 0x%lX (%u bit, flags 0x%02X)", f.ms, f.data, f.size, f.flags);
+        _diagTail = (_diagTail + 1) % DiagRingSize;
+    }
+}
+#endif
+
 #ifdef DALI_NO_TIMER
 bool __isr __time_critical_func(daliTimerInterruptCallback)(repeating_timer *t)
 {
@@ -127,6 +149,14 @@ bool __isr __time_critical_func(daliTimerInterruptCallback)(repeating_timer *t)
 
 void DaliModule::loop(bool configured)
 {
+#ifdef RGBW_DIAG
+    loopDiagFrames();
+#endif
+    if (_broadcastOffPending && (long)(millis() - _broadcastOffAt) >= 0)
+    {
+        _broadcastOffPending = false;
+        daliMaster.sendArc(BroadcastAddress, 0x00, true);
+    }
     OpenKNX::DateTime currentTime = openknx.time.getLocalTime();
     if (currentTime.minute != _lastTimeMinute)
     {
@@ -348,6 +378,10 @@ void DaliModule::showHelp()
     openknx.console.printHelpLine("stepUp", "stepUp xyy => send x times StepUp to evg y");
     openknx.console.printHelpLine("stepDown", "stepDown xyy => send x times StepDown to evg y");
     openknx.console.printHelpLine("getLvl", "getLvl yy => get level of evg y");
+    openknx.console.printHelpLine("settle", "settle nnnn => RGBW fallback pause in ms (colour of the driver unknown), 0-5000");
+    openknx.console.printHelpLine("margin", "margin nnn => RGBW reserve in ms on top of the fade time, 0-2000");
+    openknx.console.printHelpLine("blackoff", "blackoff 0|1 => RGBW: 1 = fade colour to 0/0/0/0 before level off (default 0)");
+    openknx.console.printHelpLine("onmode", "onmode m [nnnn] => RGBW on: 0 = level first, 1 = colour first; nnnn = fixed ms before the level (0 = automatic)");
 }
 
 bool DaliModule::processCommand(const std::string cmd, bool diagnoseKo)
@@ -396,6 +430,67 @@ bool DaliModule::processCommand(const std::string cmd, bool diagnoseKo)
         return true;
     }
 
+    if (command == "settle")
+    {
+        if (!hasArg || arg.empty() || arg.length() > 4 || arg.find_first_not_of("0123456789") != std::string::npos)
+        {
+            logErrorP("settle nnnn (ms, 0-5000), now %i", DaliChannel::rgbwSettleMs);
+            return true;
+        }
+        int v = std::stoi(arg);
+        if (v > 5000)
+            v = 5000;
+        DaliChannel::rgbwSettleMs = v;
+        logInfoP("RGBW settle time = %i ms", v);
+        return true;
+    }
+
+    if (command == "margin")
+    {
+        if (!hasArg || arg.empty() || arg.length() > 4 || arg.find_first_not_of("0123456789") != std::string::npos)
+        {
+            logErrorP("margin nnn (ms, 0-2000), now %i", DaliChannel::rgbwMarginMs);
+            return true;
+        }
+        int v = std::stoi(arg);
+        if (v > 2000)
+            v = 2000;
+        DaliChannel::rgbwMarginMs = v;
+        logInfoP("RGBW margin = %i ms", v);
+        return true;
+    }
+    if (command == "blackoff")
+    {
+        if (!hasArg || (arg != "0" && arg != "1"))
+        {
+            logErrorP("blackoff 0|1 (1 = fade colour to 0/0/0/0 before the level off), now %i", DaliChannel::rgbwBlackOff);
+            return true;
+        }
+        DaliChannel::rgbwBlackOff = arg == "1";
+        logInfoP("RGBW black before off = %i", DaliChannel::rgbwBlackOff);
+        return true;
+    }
+    if (command == "onmode")
+    {
+        // onmode m [nnnn]
+        bool ok = hasArg && arg.length() >= 1 && (arg[0] == '0' || arg[0] == '1');
+        if (ok && arg.length() > 1)
+            ok = arg[1] == ' ' && arg.length() <= 6 && arg.find_first_not_of("0123456789", 2) == std::string::npos && arg.length() > 2;
+        if (!ok)
+        {
+            logErrorP("onmode m [nnnn] (m = 0 level first, 1 colour first, nnnn = ms before the level), now %i %i", DaliChannel::rgbwOnMode, DaliChannel::rgbwOnDelayMs);
+            return true;
+        }
+        DaliChannel::rgbwOnMode = arg[0] - '0';
+        if (arg.length() > 2)
+        {
+            int v = std::stoi(arg.substr(2));
+            DaliChannel::rgbwOnDelayMs = v > 5000 ? 5000 : v;
+        }
+        logInfoP("RGBW on mode = %i, level delay = %i ms", DaliChannel::rgbwOnMode, DaliChannel::rgbwOnDelayMs);
+        return true;
+    }
+
     if (command == "getLvl")
     {
         cmdHandleGetLvl(hasArg, arg);
@@ -712,12 +807,87 @@ void DaliModule::koHandleSwitch(GroupObject &ko)
 {
     bool value = ko.value(DPT_Switch);
     logDebugP("Broadcast Switch %i", value);
-    daliMaster.sendArc(BroadcastAddress, value ? 0xFE : 0x00, true);
+    if (value)
+        broadcastOn();
+    else
+        broadcastOff();
 
     broadcastGroupState(value);
     logDebugP("Broadcast Switch set");
 }
 
+// Broadcast on: the level goes to everybody; RGBW channels additionally restore their colour (see DaliChannel::rgbwOn)
+void DaliModule::broadcastOn()
+{
+    _broadcastOffPending = false;
+    daliMaster.sendArc(BroadcastAddress, 0xFE, true);
+    for (int i = 0; i < ChannelCount; i++)
+        if (channels[i].isConfigured() && channels[i].isRgbw())
+            channels[i].rgbwBroadcastOn();
+    for (int i = 0; i < GroupCount; i++)
+        if (groups[i].isConfigured() && groups[i].isRgbw())
+            groups[i].rgbwBroadcastOn();
+}
+
+// Broadcast off. Without RGBW channels this is the single broadcast frame as before. Otherwise the RGBW
+// channels first fade their colour to black (then switch their level off themselves), all other configured
+// channels are switched off at once, and devices unknown to the gateway get a broadcast off afterwards.
+void DaliModule::broadcastOff()
+{
+    bool anyRgbw = false;
+    for (int i = 0; i < ChannelCount; i++)
+        if (channels[i].isConfigured() && channels[i].isRgbw())
+            anyRgbw = true;
+    for (int i = 0; i < GroupCount; i++)
+        if (groups[i].isConfigured() && groups[i].isRgbw())
+            anyRgbw = true;
+
+    if (!anyRgbw)
+    {
+        daliMaster.sendArc(BroadcastAddress, 0x00, true);
+        return;
+    }
+
+    uint16_t wait = 0;
+    for (int i = 0; i < ChannelCount; i++)
+    {
+        if (!channels[i].isConfigured())
+            continue;
+        if (channels[i].isRgbw())
+        {
+            channels[i].rgbwOff();
+            if (channels[i].rgbwOffWaitMs() > wait)
+                wait = channels[i].rgbwOffWaitMs();
+        }
+        else
+            daliMaster.sendArc(i, 0x00, false);
+    }
+    for (int i = 0; i < GroupCount; i++)
+    {
+        if (!groups[i].isConfigured())
+            continue;
+        if (groups[i].isRgbw())
+        {
+            groups[i].rgbwOff();
+            if (groups[i].rgbwOffWaitMs() > wait)
+                wait = groups[i].rgbwOffWaitMs();
+        }
+        else
+            daliMaster.sendArc(i, 0x00, true);
+    }
+    logDebugP("Broadcast off with RGBW channels: broadcast level off in %i ms", wait + 50);
+    _broadcastOffAt = millis() + wait + 50;
+    _broadcastOffPending = true;
+}
+
+void DaliModule::markAllColoursUnknown()
+{
+    for (int i = 0; i < ChannelCount; i++)
+        channels[i].markColourUnknown();
+    for (int i = 0; i < GroupCount; i++)
+        groups[i].markColourUnknown();
+}
+
 void DaliModule::koHandleDimm(GroupObject &ko)
 {
     uint8_t value = ko.value(Dpt(5, 1));
@@ -800,6 +970,7 @@ void DaliModule::koHandleScene(GroupObject &ko)
             continue;
 
         logDebugP("Found scene number: %u, save: %u", i, save);
+        markAllColoursUnknown(); // a scene may change the colours stored in the drivers
         if (save && !ParamDGWS_save)
         {
             logDebugP("Save not allowed. Skip!");
diff --git a/src/DaliModule.group.templ.xml b/src/DaliModule.group.templ.xml
index 881dfd9..b0840ec 100644
--- a/src/DaliModule.group.templ.xml
+++ b/src/DaliModule.group.templ.xml
@@ -44,7 +44,7 @@
                 <Memory CodeSegment="%AID%_RS-04-00000" Offset="0" BitOffset="7" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%3%CC%019" Name="%C%colorType" ParameterType="%AID%_PT-colorType" Text="Farbe ansteuern per" Value="0">
-                <Memory CodeSegment="%AID%_RS-04-00000" Offset="4" BitOffset="2" />
+                <Memory CodeSegment="%AID%_RS-04-00000" Offset="17" BitOffset="2" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%3%CC%020" Name="%C%colorSpace" ParameterType="%AID%_PT-colorSpace" Text="Farbe übertragen per" Value="1">
                 <Memory CodeSegment="%AID%_RS-04-00000" Offset="3" BitOffset="7" />
@@ -173,6 +173,8 @@
               <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00703" FunctionText="Farbtemperatur Status" DatapointType="DPST-7-600" ObjectSize="2 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%006" Id="%AID%_O-%T%3%CC%006_R-%T%3%CC%00604" FunctionText="Farbe xyY / xy" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00704" FunctionText="Farbe xyY / xy Status" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
+              <ComObjectRef RefId="%AID%_O-%T%3%CC%006" Id="%AID%_O-%T%3%CC%006_R-%T%3%CC%00605" FunctionText="Farbe RGBW" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
+              <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00705" FunctionText="Farbe RGBW Status" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%017" Id="%AID%_O-%T%3%CC%017_R-%T%3%CC%01701" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%018" Id="%AID%_O-%T%3%CC%018_R-%T%3%CC%01801" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
             </ComObjectRefs>
@@ -288,6 +290,10 @@
                           <ComObjectRefRef RefId="%AID%_O-%T%3%CC%006_R-%T%3%CC%00604" />
                           <ComObjectRefRef RefId="%AID%_O-%T%3%CC%007_R-%T%3%CC%00704" />
                         </when>
+                        <when test="=4">
+                          <ComObjectRefRef RefId="%AID%_O-%T%3%CC%006_R-%T%3%CC%00605" />
+                          <ComObjectRefRef RefId="%AID%_O-%T%3%CC%007_R-%T%3%CC%00705" />
+                        </when>
                       </choose>
                       <choose ParamRefId="%AID%_P-%T%3%CC%020_R-%T%3%CC%02001">
                         <when test="!=0">
diff --git a/src/DaliModule.h b/src/DaliModule.h
index 1206625..ff62133 100644
--- a/src/DaliModule.h
+++ b/src/DaliModule.h
@@ -63,6 +63,21 @@ class DaliModule : public OpenKNX::Module
 		uint8_t _currentIdentifyDevice = 0;
 #endif
 		bool _currentLockState = false;
+		// RGBW: broadcast off for devices the gateway does not know, sent after the RGBW fade-out
+		bool _broadcastOffPending = false;
+		unsigned long _broadcastOffAt = 0;
+		void broadcastOff();
+		void broadcastOn();
+		void markAllColoursUnknown();
+#ifdef RGBW_DIAG
+		// Frames seen on the DALI bus (incl. our own echo), filled on core 1, logged on core 0
+		struct DiagFrame { unsigned long ms; unsigned long data; uint8_t size; uint8_t flags; };
+		static constexpr uint8_t DiagRingSize = 64;
+		DiagFrame _diagRing[DiagRingSize];
+		volatile uint8_t _diagHead = 0;
+		volatile uint8_t _diagTail = 0;
+		void loopDiagFrames();
+#endif
 		int16_t getInfo(byte address, uint8_t command, uint8_t additional = 0);
 	
 		DaliAddressing _addressing{daliMaster};
diff --git a/src/DaliModule.share.xml b/src/DaliModule.share.xml
index 1219e9f..500e71b 100644
--- a/src/DaliModule.share.xml
+++ b/src/DaliModule.share.xml
@@ -114,11 +114,12 @@
                 <TypeNumber Type="unsignedInt" minInclusive="1" maxInclusive="64" SizeInBit="7" />
               </ParameterType>
               <ParameterType Id="%AID%_PT-colorType" Name="colorType" op:headerExport="define">
-                <TypeRestriction Base="Value" SizeInBit="2">
+                <TypeRestriction Base="Value" SizeInBit="3">
                   <Enumeration Text="HSV" Value="0" Id="%ENID%" />
                   <Enumeration Text="RGB" Value="1" Id="%ENID%" />
                   <Enumeration Text="TW" Value="2" Id="%ENID%" />
                   <Enumeration Text="xyY / xy" Value="3" Id="%ENID%" op:headerName="XYY" />
+                  <Enumeration Text="RGBW" Value="4" Id="%ENID%" op:headerName="RGBW" />
                 </TypeRestriction>
               </ParameterType>
               <ParameterType Id="%AID%_PT-colorSpace" Name="colorSpace" op:headerExport="define">
diff --git a/src/DaliModule.single.templ.xml b/src/DaliModule.single.templ.xml
index 78764c5..94ad8f1 100644
--- a/src/DaliModule.single.templ.xml
+++ b/src/DaliModule.single.templ.xml
@@ -105,7 +105,7 @@
               <Memory CodeSegment="%AID%_RS-04-00000" Offset="7" BitOffset="4" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%2%CC%073" Name="%C%colorType" ParameterType="%AID%_PT-colorType" Text="Farbe ansteuern per" Value="0">
-              <Memory CodeSegment="%AID%_RS-04-00000" Offset="7" BitOffset="5" />
+              <Memory CodeSegment="%AID%_RS-04-00000" Offset="21" BitOffset="5" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%2%CC%074" Name="%C%colorSpace" ParameterType="%AID%_PT-colorSpace" Text="Farbe übertragen per" Value="0">
               <Memory CodeSegment="%AID%_RS-04-00000" Offset="7" BitOffset="7" />
@@ -363,6 +363,8 @@
               <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00703" FunctionText="Farbtemperatur Status" DatapointType="DPST-7-600" ObjectSize="2 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%006" Id="%AID%_O-%T%2%CC%006_R-%T%2%CC%00604" FunctionText="Farbe xyY / xy" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00704" FunctionText="Farbe xyY / xy Status" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
+              <ComObjectRef RefId="%AID%_O-%T%2%CC%006" Id="%AID%_O-%T%2%CC%006_R-%T%2%CC%00605" FunctionText="Farbe RGBW" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
+              <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00705" FunctionText="Farbe RGBW Status" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%017" Id="%AID%_O-%T%2%CC%017_R-%T%2%CC%01701" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%018" Id="%AID%_O-%T%2%CC%018_R-%T%2%CC%01801" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
             </ComObjectRefs>
@@ -502,6 +504,10 @@
                           <ComObjectRefRef RefId="%AID%_O-%T%2%CC%006_R-%T%2%CC%00604" />
                           <ComObjectRefRef RefId="%AID%_O-%T%2%CC%007_R-%T%2%CC%00704" />
                         </when>
+                        <when test="4">
+                          <ComObjectRefRef RefId="%AID%_O-%T%2%CC%006_R-%T%2%CC%00605" />
+                          <ComObjectRefRef RefId="%AID%_O-%T%2%CC%007_R-%T%2%CC%00705" />
+                        </when>
                       </choose>
                       <choose ParamRefId="%AID%_P-%T%2%CC%074_R-%T%2%CC%07401">
                         <when test="!=0">
@@ -676,7 +682,7 @@
                           <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                             <when test="9">
                               <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                <when test="0 1 3">
+                                <when test="0 1 3 4">
                                   <ParameterSeparator Id="%AID%_PS-nnn" Text="Farbe" UIHint="Headline" Cell="1,4" />
                                 </when>
                                 <when test="2">
@@ -693,7 +699,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%090_R-%T%2%CC%09001" Cell="2,4" />
                                     </when>
                                     <when test="2">
@@ -712,7 +718,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%092_R-%T%2%CC%09201" Cell="3,4" />
                                     </when>
                                     <when test="2">
@@ -731,7 +737,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%094_R-%T%2%CC%09401" Cell="4,4" />
                                     </when>
                                     <when test="2">
@@ -750,7 +756,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%096_R-%T%2%CC%09601" Cell="5,4" />
                                     </when>
                                     <when test="2">
@@ -769,7 +775,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%098_R-%T%2%CC%09801" Cell="6,4" />
                                     </when>
                                     <when test="2">
@@ -788,7 +794,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%100_R-%T%2%CC%10001" Cell="7,4" />
                                     </when>
                                     <when test="2">
@@ -807,7 +813,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%102_R-%T%2%CC%10201" Cell="8,4" />
                                     </when>
                                     <when test="2">
@@ -826,7 +832,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%104_R-%T%2%CC%10401" Cell="9,4" />
                                     </when>
                                     <when test="2">
@@ -845,7 +851,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%106_R-%T%2%CC%10601" Cell="10,4" />
                                     </when>
                                     <when test="2">
@@ -864,7 +870,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%108_R-%T%2%CC%10801" Cell="11,4" />
                                     </when>
                                     <when test="2">
@@ -883,7 +889,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%110_R-%T%2%CC%11001" Cell="12,4" />
                                     </when>
                                     <when test="2">
@@ -902,7 +908,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%112_R-%T%2%CC%11201" Cell="13,4" />
                                     </when>
                                     <when test="2">
@@ -921,7 +927,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%114_R-%T%2%CC%11401" Cell="14,4" />
                                     </when>
                                     <when test="2">
@@ -940,7 +946,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%116_R-%T%2%CC%11601" Cell="15,4" />
                                     </when>
                                     <when test="2">
@@ -959,7 +965,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%118_R-%T%2%CC%11801" Cell="16,4" />
                                     </when>
                                     <when test="2">
@@ -978,7 +984,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%120_R-%T%2%CC%12001" Cell="17,4" />
                                     </when>
                                     <when test="2">
```
