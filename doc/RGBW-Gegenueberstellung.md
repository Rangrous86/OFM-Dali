# RGBW-Gegenüberstellung: Ausgangscode ↔ Änderung (Stand `v14b`)

> **Neu erzeugt am 21.11.2026** für den Stand **`v14b`** (Branch `v1dev` dieses Forks, Code-Stand Commit `b71a36f`, Testdiagnose entfernt). Die frühere Fassung (Stand v6) liegt in der Git-Historie. Verhalten, Befunde und Testergebnisse stehen in der [README](../README.md); dieses Dokument zeigt, **was am Code geändert ist**.

**Vergleichsbasis:** `OpenKNX/OFM-Dali`, Branch `v1dev`, Commit `a120578` (Stand des letzten Merges in diesen Fork, enthält Fix #7). Upstream ist seitdem nur um `3badb1e` („updated: lib version“, eine Zeile in `library.json`) weiter; das berührt die RGBW-Änderung nicht und wird beim PR mitgenommen.

Der Ausgangscode kennt die Farbtypen HSV, RGB, TW und xyY, **kein RGBW**. Die Bibliothek `OpenKNX/dali` ist **unverändert**.

## 1. Überblick

| Datei | Art | Änderung | Risiko |
|-------|-----|----------|--------|
| `src/RgbwHelper.h` | **neu** | Reine Hilfsfunktionen für DPT 251.600 ohne Arduino-Abhängigkeit: `decode` (Gültigkeitsbits), `isOff`, `encode` (Status), `daliLevel` (255 → 254), `normalise`/Mischung und Helligkeit aus dem größten Kanal (Slider-Modus, Broadcast RGBW). Per Host-Test geprüft (`test/test_rgbw_helper.cpp`). | gering |
| `src/DaliChannel.cpp` / `.h` | geändert | RGBW-Zweig in `koHandleColor` (`handleColorRGBW`), Senden RGB + Weiß als **temporäre** Farbe (`sendColor`), 6-Byte-Status (`sendRgbwState`). **Sammelfenster** (`rgbwRequest…`, `loopRgbwWindow`, `rgbwFlush`): jedes RGBW-Telegramm wird ≈ 80 ms gesammelt und als **ein** DALI-Befehl gesendet (temporäre Farbe + `DAPC` bzw. `ACTIVATE`). Gruppe = immer Farbe + Pegel, Einzel-EVG = einzeln. Relatives Dimmen: Farbe nach dem Ende noch einmal (`loopStopQuery`), späterer Stopp nach Max/Min ohne Wirkung (B27). Sperre bei RGBW: eigener Zweig (`rgbwLock`/`rgbwUnlock`, `rgbwLockArc`) mit Speichern während der Sperre. *Broadcast zurück*: `rgbwRestore`, `rgbwSendState`, `rgbwColourKnown`, `isInRgbwGroup`. *Schalten Status* sofort bei *Heller* aus Aus (RGBW). | mittel (auf Hardware getestet, 21.11.) |
| `src/DaliModule.cpp` / `.h` | geändert | Broadcast: *Schalten* ohne Farbe (`broadcastOn`/`broadcastOff`, offene Sammelfenster verwerfen), neu `koHandleBroadcastRgbw` (Farbe + Pegel an die Broadcast-Adresse, kein Status) und `koHandleBroadcastBack` (zwei Durchgänge, B28). Szenen verwerfen offene Sammelfenster. Konsole: `aawin`, `aastat`, `aaon`, `getCol`. | mittel |
| `src/DaliModule.share.xml` | geändert | `colorType` 2 → 3 Bit mit `RGBW = 4`; Parametertyp *Helligkeit bei RGBW*; zwei neue Modul-Objekte *Broadcast – Farbe RGBW* (6, DPST-251-600) und *Broadcast – Zurück* (7, DPST-1-15). | gering |
| `src/DaliModule.single.templ.xml` | geändert | `colorType` nach Byte 21 Bit 5–7; *Helligkeit bei RGBW* (Byte 7 Bit 5); *Bei Entsperren vorherigen Zustand wiederherstellen* (Byte 7 Bit 6); zwei KO-Referenzen (DPST-251-600, 6 Byte); Szenen-Farbspalte auch für Typ 4. | mittel |
| `src/DaliModule.group.templ.xml` | geändert | `colorType` nach Byte 17 Bit 2–4; *Helligkeit bei RGBW* (Byte 17 Bit 5); *wiederherstellen* (Byte 4 Bit 2, frei geworden durch das Verschieben von `colorType`); zwei KO-Referenzen. | mittel |
| `test/test_rgbw_helper.cpp` | **neu** | Host-Test für `RgbwHelper.h` (`g++ -std=c++11 -Isrc test/test_rgbw_helper.cpp`). | – |

**Nicht geändert:** `DaliModule.script.js` (Szenen-Konfigurationstransfer, RGBW wie RGB), HCL- und Szenen-Vorlagen, die Bibliothek `dali`, alle anderen Farbtypen (HSV, RGB, TW, xyY) in ihrem Verhalten.

**Änderungen, die für alle Kanaltypen gelten** (README 1.5): `koHandleDimmAbs` setzt `currentState` nach dem Dimmwert (0 % = aus); nach dem Stopp eines relativen Dimmens einmal `QUERY_ACTUAL_LEVEL` (≈ 300 ms danach); Startwerte „letzter Wert“ als DALI-Pegel 100 % / 10 %.

## 2. Warum wandert `colorType` im Speicher?

Der Parameter hatte 2 Bit (Werte 0–3, alle vergeben). Für den fünften Wert (RGBW = 4) braucht er 3 Bit. Die Bits direkt hinter ihm sind in beiden Vorlagen belegt, deshalb liegt er jetzt in freien Bits am Ende des Parameterblocks. Die Blockgröße ändert sich **nicht**. Die frei gewordenen Bits werden für die neuen 1-Bit-Parameter genutzt.

| Vorlage | `colorType` vorher | `colorType` nachher | neue Parameter |
|---------|--------|---------|---------|
| Einzeladresse | Byte 7, Bit 5–6 (2 Bit) | Byte 21, Bit 5–7 (3 Bit) | *Helligkeit bei RGBW* Byte 7 Bit 5, *wiederherstellen* Byte 7 Bit 6 |
| Gruppe | Byte 4, Bit 2–3 (2 Bit) | Byte 17, Bit 2–4 (3 Bit) | *Helligkeit bei RGBW* Byte 17 Bit 5, *wiederherstellen* Byte 4 Bit 2 |

Folge: neue ETS-Applikationsversion (im Test 1.0; die alten Versionen stehen in `ReplacesVersions`, das Gerät lässt sich per *Aktualisieren* umstellen). KNX-Speicher: Schätzung 36801 von 36864 Byte (63 Byte Reserve, README B17).

## 3. Ablauf im Code (`v14b`)

| Ereignis | Stelle | DALI |
|---|---|---|
| Farbe und/oder Helligkeit (Einzel-EVG oder Gruppe) | `handleColorRGBW` / `koHandleSwitch` / `koHandleDimmAbs` → Sammelfenster → `rgbwFlush` | temporäre Farbe (`0x…EB`, `0x…EC`) + `DAPC` in **einem** Fade; Farbe allein: Einzel-EVG `ACTIVATE`, Gruppe Farbe + Gruppenpegel; Helligkeit allein: Einzel-EVG nur `DAPC`, Gruppe Gruppenfarbe + `DAPC` |
| Farbe an einen ausgeschalteten Kanal | `rgbwFlush` | `ACTIVATE` (Lampe bleibt aus); mit *Ein*/Helligkeit im Fenster ein Befehl |
| Ausschalten | `rgbwOff` | `DAPC 0`, Fenster verworfen |
| Relatives Dimmen | `koHandleDimmRel`, `loopDimming`, `loopStopQuery` | `RECALL MIN`/`UP`/`DOWN` wie RGB; ≈ 300 ms nach dem Ende Farbe + `ACTIVATE`, danach Pegelabfrage |
| Sperre | `koHandleLock` → `rgbwLock`/`rgbwUnlock` | nur `DAPC`; mit *wiederherstellen*: beim Entsperren gespeicherter Zustand als ein Befehl |
| Szene | `koHandleScene` | unverändert `GO TO SCENE`, offene Fenster verworfen |
| Broadcast Schalten | `broadcastOn`/`broadcastOff` | nur `DAPC` an `0xFE` |
| Broadcast – Farbe RGBW | `koHandleBroadcastRgbw` | temporäre Farbe an `0xFF` + `DAPC` an `0xFE`, kein Status |
| Broadcast – Zurück | `koHandleBroadcastBack` → `rgbwRestore` → `rgbwSendState` | erst Gruppen/Kanäle ohne bekannte Farbe (nur `DAPC`), dann mit Farbe |

Der Code ist im Kopf von `DaliChannel.cpp` (Kommentar „RGBW control (v14 …)“) zusammengefasst.

## 4. Vorher/Nachher: ETS-Applikation

| Thema | Vorher | Nachher |
|-------|--------|---------|
| Farbtyp-Auswahl | HSV, RGB, TW, xyY | + **RGBW** |
| Objekte bei RGBW | – | *Farbe RGBW* und *Farbe RGBW Status* (Kanal-Objekte 6/7, z. B. 306/307 für A0), DPST-251-600, 6 Byte |
| *Helligkeit bei RGBW* | – | „Über Objekt *Dimmen absolut*“ (Standard) / „Aus dem RGBW-Wert“, nur bei RGBW |
| *Bei Entsperren vorherigen Zustand wiederherstellen* | – | Ja/Nein, nur bei RGBW und eingeblendetem Sperrobjekt, mit Hinweistext |
| Modul-Objekte Broadcast | Schalten, Dimmen absolut | + *Farbe RGBW* (6) und *Zurück* (7) |
| „Farbe übertragen per“ | sichtbar bei HSV/RGB/xyY | bei RGBW ausgeblendet |
| Szenen-Farbspalte | für Typ 0, 1, 3 | für Typ 0, 1, 3, **4** (nur RGB-Anteil) |

## 5. Nicht gelöst / offen (RGBW)

- Szenenfarbe per Konfigurationstransfer enthält kein Weiß.
- Keine getrennten 8-Bit-Objekte R/G/B/W bei RGBW.
- Weiß über `SET TEMPORARY WAF DIMLEVEL` (236); nur mit dem DA4-M (RGBW-Modus) getestet. Andere Treiber, Befehl 237: offen.
- HCL-Farbtemperatur auf RGBW-Kanälen, Treppenhauslicht mit RGBW: nicht angepasst (README 4, 5.3).
- Regression der anderen Farbtypen (README Testfall 14) und der Stand ohne Diagnose auf Hardware: offen (README 5.6, 5.7).
- Build-Tag im Log (`Color absolute [v14b]`, `RGBW_BUILD_TAG`): vor dem PR entfernen oder behalten.

## 6. Fragen an den Maintainer (zur RGBW-Erweiterung)

1. Ist die Verschiebung von `colorType` im Parameterspeicher (Einzel: Byte 21, Gruppe: Byte 17) und die Nutzung der frei gewordenen Bits in Ordnung?
2. Weiß über `SET TEMPORARY WAF DIMLEVEL` (236), oder lieber `RGBWAF` (237) bzw. ein anderer Weg?
3. Sollen zusätzlich getrennte 8-Bit-Objekte für R/G/B/W angeboten werden (wie bei MDT)?
4. Wie sollen Szenen/DCA-Konfiguration mit Weiß umgehen?
5. Passt das Verfahren „temporäre Farbe + `DAPC` mit automatischer Aktivierung, Sammelfenster 80 ms“ als Grundlage, auch für die anderen Farbtypen?
6. Sind die Änderungen, die für alle Kanaltypen gelten (`currentState` in `koHandleDimmAbs`, Pegelabfrage nach dem Stopp, Startwerte), so in Ordnung?

Befunde im Ausgangscode und Fragen zum Broadcast insgesamt (G1–G22, F1–F3) stehen getrennt in [README 5.8](../README.md#58-befunde-im-ausgangscode-und-in-der-dali-bibliothek-für-den-maintainer).

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

    /*
     * Gegenrichtung zu daliLevel(): DALI-Pegel (254 = Maximum) -> KNX-Wert (255 = 100 %).
     */
    inline uint8_t fromDaliLevel(uint8_t level)
    {
        return level == 254 ? 255 : level;
    }

    /*
     * Farbe aus den vier Report-Werten (R, G, B, W) des Treibers (QUERY_COLOUR_VALUE, Selektor 233-236).
     * Nutzbar nur, wenn kein Wert 255 (MASK = unbekannt) ist und mindestens ein Kanal leuchtet: ein Treiber,
     * der nur Nullen meldet, hat (noch) keine Farbe, und eine Farbe aus lauter Nullen wäre "aus".
     * Gibt true zurück und schreibt color[0..3], wenn die Farbe übernommen werden kann.
     */
    inline bool colourFromDriver(const uint8_t *report, uint8_t *color)
    {
        for (uint8_t i = 0; i < 4; i++)
            if (report[i] == 255)
                return false;
        if (report[0] == 0 && report[1] == 0 && report[2] == 0 && report[3] == 0)
            return false;
        for (uint8_t i = 0; i < 4; i++)
            color[i] = fromDaliLevel(report[i]);
        return true;
    }

    /*
     * Helligkeit aus dem RGBW-Wert ("Slider"-Modus): Der größte Kanal ist die Helligkeit (255 = 100 %), das Verhältnis
     * der Kanäle zueinander ist die Farbmischung. Der DA4-M wertet die Amplitude der Farbwerte nicht aus (er
     * normiert), die Helligkeit muss deshalb als eigener Pegel gesendet werden.
     */
    inline uint8_t maxChannel(const uint8_t *color)
    {
        uint8_t m = color[0];
        for (uint8_t i = 1; i < 4; i++)
            if (color[i] > m)
                m = color[i];
        return m;
    }

    /*
     * Farbmischung: color so skaliert, dass der größte Kanal 255 ist (alle 0 bleibt 0).
     */
    inline void normalise(const uint8_t *color, uint8_t *mix)
    {
        const uint8_t m = maxChannel(color);
        for (uint8_t i = 0; i < 4; i++)
            mix[i] = m == 0 ? 0 : (uint8_t)(((uint16_t)color[i] * 255 + m / 2) / m);
    }

    /*
     * Gegenrichtung: Farbmischung (größter Kanal 255) auf die Amplitude amp (= größter Kanal) skalieren.
     */
    inline void scale(const uint8_t *mix, uint8_t amp, uint8_t *color)
    {
        for (uint8_t i = 0; i < 4; i++)
            color[i] = (uint8_t)(((uint16_t)mix[i] * amp + 127) / 255);
    }

    // Amplitude (0..255) <-> Helligkeit in Prozent (0..100, linear)
    inline float percentFromAmplitude(uint8_t amp)
    {
        return amp * 100.0f / 255.0f;
    }

    inline uint8_t amplitudeFromPercent(float percent)
    {
        if (percent <= 0.0f)
            return 0;
        if (percent >= 100.0f)
            return 255;
        return (uint8_t)(percent * 2.55f + 0.5f);
    }
}
```

## 8. Vollständiges Diff gegenüber `a120578` (`src`, `test`)

Erzeugt mit `git diff a120578 -- src test library.json`.

```
 src/DaliChannel.cpp             | 628 +++++++++++++++++++++++++++++++++++++++-
 src/DaliChannel.h               |  72 ++++-
 src/DaliModule.cpp              | 284 +++++++++++++++++-
 src/DaliModule.group.templ.xml  |  32 +-
 src/DaliModule.h                |   9 +
 src/DaliModule.share.xml        |  15 +-
 src/DaliModule.single.templ.xml |  66 +++--
 src/RgbwHelper.h                | 148 ++++++++++
 test/test_rgbw_helper.cpp       | 136 +++++++++
 9 files changed, 1351 insertions(+), 39 deletions(-)
```

```diff
diff --git a/src/DaliChannel.cpp b/src/DaliChannel.cpp
index dffeaaf..35f5202 100644
--- a/src/DaliChannel.cpp
+++ b/src/DaliChannel.cpp
@@ -1,4 +1,5 @@
 #include "DaliChannel.h"
+#include "RgbwHelper.h"
 #include "OpenKNX.h"
 
 DaliChannel::DaliChannel(Dali::Master &_daliMaster) : daliMaster(_daliMaster) {}
@@ -109,6 +110,212 @@ void DaliChannel::loop1()
     loopDimming();
     loopError();
     loopQueryLevel();
+    loopRgbwWindow();
+    loopStopQuery();
+}
+
+uint16_t DaliChannel::rgbwWindowMs = RgbwWindowMs;
+
+// ---------------------------------------------------------------------------------------------------------
+// RGBW control (v14, see README sections 1.5 and 5.1, plan after phase 3):
+// The driver has ONE fade engine (DA4-M, B1): every new fade command (DAPC, ACTIVATE, UP/DOWN) aborts the running
+// fade and the driver stays where it is. RGBW follows the RGB code (every telegram = one DALI command = one fade, no
+// bus query, no waiting for the end of a fade, no time marks) with these deviations:
+// 1. Collection window (rgbwWindowMs, default 80 ms): every RGBW command goes through it. Telegrams that arrive close
+//    together (Home Assistant sends colour and brightness ~20 ms apart) become ONE command: the colour as temporary
+//    value (SET TEMPORARY RGB/WAF, no ACTIVATE) and the DAPC right behind it; the DAPC activates the colour
+//    ("automatic activation"), colour and level fade together.
+// 2. Switch-on (Schalten Ein, Dimmen absolut > 0) always sends the stored colour as temporary value with the DAPC.
+//    Without a known colour only the DAPC goes out (like RGB).
+// 3. Group = standard: a group command always carries both targets (temporary colour + DAPC), also a colour alone
+//    (with the stored group level) or a level alone (with the stored group colour). All members return to the
+//    state of the group.
+// 4. Single channel = exception: a colour alone on a lit channel goes out with ACTIVATE, a level alone as DAPC.
+//    Sent separately (outside the window), the second command may abort the fade of the first one (B26, accepted).
+// 5. A colour for a switched-off channel goes out with ACTIVATE (after the window), the lamp stays off. Exception:
+//    brightness from the RGBW value (slider mode), there a colour > 0 switches on.
+// 6. Relative dimming: 300 ms after its end (stop or maximum) the stored colour is sent again with ACTIVATE (B25).
+// 7. Broadcast: level only (module). Broadcast RGBW: own value, the channels keep their state; "Broadcast zurueck"
+//    sends the stored state of every RGBW group and of every RGBW channel that is in no RGBW group.
+// 8. Lock: level only, no colour (S1-S4 corrected for RGBW); optionally restore the stored state on unlock.
+// ---------------------------------------------------------------------------------------------------------
+bool DaliChannel::isRgbw()
+{
+    return (_isGroup ? ParamDGWG_colorType : ParamDGW_colorType) == PT_colorType_RGBW;
+}
+
+// Level request (Schalten Ein, Dimmen absolut > 0, level from the colour value in slider mode). arc > 0.
+// Call it BEFORE currentState is set by the caller: a request on a switched-off channel always takes the colour.
+void DaliChannel::rgbwRequestLevel(uint8_t arc)
+{
+    if (!currentState)
+        _winFromOff = true;
+    _winLevel = arc;
+    rgbwStartWindow();
+}
+
+// Colour request of a lit channel (the colour is already stored in currentColor / _mix)
+void DaliChannel::rgbwRequestColour()
+{
+    _winColour = true;
+    rgbwStartWindow();
+}
+
+void DaliChannel::rgbwStartWindow()
+{
+    if (rgbwWindowMs == 0)
+    {
+        rgbwFlush();
+        return;
+    }
+    if (_winPending)
+        return; // the window keeps running, the newest values are sent at its end
+    _winPending = true;
+    _winAt = millis() + rgbwWindowMs;
+}
+
+void DaliChannel::loopRgbwWindow()
+{
+    if (_winPending && (long)(millis() - _winAt) >= 0)
+        rgbwFlush();
+}
+
+// End of the collection window: send what was requested, as one command where possible
+void DaliChannel::rgbwFlush()
+{
+    _winPending = false;
+    const bool colourKnown = !RgbwHelper::isOff(currentColor);
+
+    if (_winLevel > 0)
+    {
+        if (colourKnown && (_winFromOff || _winColour || _isGroup))
+        {
+            // switch-on, colour + level in the window, or a group (always both targets): one fade
+            sendColor(false); // temporary colour, activated by the DAPC
+            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
+            logDebugP("RGBW: level %i with temporary colour (one fade)", _winLevel);
+        }
+        else
+        {
+            // single channel, level alone on a lit channel (like RGB)
+            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
+            logDebugP("RGBW: level %i", _winLevel);
+        }
+    }
+    else if (_winColour && colourKnown)
+    {
+        if (!currentState)
+        {
+            // switched-off channel or group: the colour fades in unseen at level 0, the lamp stays off
+            sendColor(true);
+            logDebugP("RGBW: colour for a switched-off channel, lamp stays off (ACTIVATE)");
+        }
+        else if (_isGroup)
+        {
+            // group: colour alone with the stored group level, one fade, all members to the group state
+            uint8_t level = rgbwStoredLevel();
+            sendColor(false);
+            daliMaster.sendArc(_channelIndex, level, _isGroup);
+            logDebugP("RGBW: group colour with the group level %i (one fade)", level);
+        }
+        else
+        {
+            // single channel, colour alone on a lit channel (like RGB)
+            sendColor(true);
+            logDebugP("RGBW: colour (ACTIVATE)");
+        }
+    }
+
+    _winLevel = 0;
+    _winColour = false;
+    _winFromOff = false;
+}
+
+// Drops a pending window (scene, lock, group or broadcast command)
+void DaliChannel::rgbwCancel()
+{
+    _winPending = false;
+    _winLevel = 0;
+    _winColour = false;
+    _winFromOff = false;
+}
+
+// Plain level off; the colour stays stored in gateway and driver
+void DaliChannel::rgbwOff()
+{
+    rgbwCancel();
+    currentState = false;
+    logDebugP("RGBW: off");
+    daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
+}
+
+// Stored level of a lit channel or group (DALI arc value): the last level, otherwise the switch-on value
+uint8_t DaliChannel::rgbwStoredLevel()
+{
+    if (currentStep > 0)
+        return currentStep;
+    uint8_t onValue = isNight ? _onNight : _onDay;
+    if (onValue == 0)
+        onValue = isNight ? _lastNightValue : _lastDayValue;
+    return onValue;
+}
+
+// Sends the stored state as one command: temporary colour + DAPC with the stored level (on) or 0 (off). From "off"
+// the colour fades in unseen while the lamp goes out. Without a known colour only the DAPC goes out.
+void DaliChannel::rgbwSendState()
+{
+    uint8_t level = currentState ? rgbwStoredLevel() : 0;
+    if (!RgbwHelper::isOff(currentColor))
+        sendColor(false);
+    daliMaster.sendArc(_channelIndex, level, _isGroup);
+    logDebugP("RGBW: stored state sent again: %s, level %i", currentState ? "on" : "off", level);
+}
+
+// "Broadcast zurueck": this channel or group gets its own stored state again (locked ones keep their lock state)
+void DaliChannel::rgbwRestore()
+{
+    if (!_isConfigured || !isRgbw() || _isStaircase || currentIsLocked)
+        return;
+    rgbwCancel();
+    rgbwSendState();
+}
+
+bool DaliChannel::rgbwColourKnown()
+{
+    return !RgbwHelper::isOff(currentColor);
+}
+
+// Single channel: member of at least one configured RGBW group (gets its state from the group on "Broadcast zurueck")
+bool DaliChannel::isInRgbwGroup(DaliChannel *groups)
+{
+    if (_isGroup)
+        return false;
+    for (int i = 0; i < GroupCount; i++)
+        if ((_groups >> i) & 1 && groups[i].isConfigured() && groups[i].isRgbw())
+            return true;
+    return false;
+}
+
+// One level query a short time after the stop of a relative dimming, so that the status objects show the level
+// the driver really reached (not the one before the last UP/DOWN step).
+void DaliChannel::loopStopQuery()
+{
+    if (_stopQueryAt == 0 || (long)(millis() - _stopQueryAt) < 0)
+        return;
+    if (_queryId != 0)
+        return; // an answer is still on its way, ask afterwards
+    _stopQueryAt = 0;
+    // dimming started again, or the channel is off for us (a late answer must not switch the status on again)
+    if (_dimmDirection != DimmDirection::None || !currentState)
+        return;
+    if (isRgbw() && !_isStaircase && !RgbwHelper::isOff(currentColor))
+    {
+        // an UP/DOWN right after a colour aborts the colour fade (B25): the stored colour once more
+        logDebugP("RGBW: colour again after relative dimming");
+        sendColor(true);
+    }
+    logDebugP("Query actual level after the dimming stop");
+    queryActualLevel();
 }
 
 void DaliChannel::loopStaircase()
@@ -129,6 +336,8 @@ void DaliChannel::loopDimming()
 {
     if (_dimmDirection != DimmDirection::None)
     {
+        // RGBW: like RGB, no wait. An UP/DOWN right after a colour change aborts the colour fade: the colour is sent
+        // again 300 ms after the end of the dimming (loopStopQuery, B25).
         if(millis() - _dimmLast > DimmInterval)
         {
             if (_dimmDirection == DimmDirection::Up)
@@ -145,16 +354,34 @@ void DaliChannel::loopDimming()
                     logDebugP("Dimm Stop at 254");
                     _dimmDirection = DimmDirection::None;
                     updateCurrentDimmValue();
+                    if (isRgbw() && !_isStaircase && currentDimmType == DimmType::Brigthness)
+                        _stopQueryAt = (millis() + 300) | 1; // level query and colour again (B25)
                 }else if (*currentDimmValue >= _max)
                 {
                     logDebugP("Dimm Stop at max %i (%i)", _max, *currentDimmValue);
                     _dimmDirection = DimmDirection::None;
                     updateCurrentDimmValue();
+                    if (isRgbw() && !_isStaircase && currentDimmType == DimmType::Brigthness)
+                        _stopQueryAt = (millis() + 300) | 1; // level query and colour again (B25)
                 }
             }
             else if (_dimmDirection == DimmDirection::Down)
             {
-                if (currentDimmType == DimmType::Brigthness)
+                // RGBW (DA4-M): the DOWN that follows the OFF keeps the driver lit at its minimum (it only went out
+                // when the dimming ended), and the loop kept repeating "DOWN, OFF" every 200 ms until the stop
+                // telegram came: lamp at minimum, bus flooded with status telegrams. So the step that reaches the
+                // minimum sends no further DOWN, switches off and ends the dimming.
+                bool rgbwDown = isRgbw() && !_isStaircase && currentDimmType == DimmType::Brigthness;
+                if (rgbwDown && *currentDimmValue == 0)
+                {
+                    // already off: nothing to dim down
+                    _dimmDirection = DimmDirection::None;
+                    _dimmLast = millis();
+                    return;
+                }
+                bool lastStep = rgbwDown && !isDimmOffLocked() && ((int)*currentDimmValue - 1 <= (int)_min || *currentDimmValue <= 1);
+
+                if (currentDimmType == DimmType::Brigthness && !lastStep)
                 {
                     queryActualLevel();
                     daliMaster.sendCommand(_channelIndex, Dali::Command::DOWN, _isGroup);
@@ -163,13 +390,26 @@ void DaliChannel::loopDimming()
                 *currentDimmValue = *currentDimmValue - 1;
                 if (*currentDimmValue <= _min || *currentDimmValue == 0) {
                     logDebugP("Dimm Stop at: %i", *currentDimmValue);
-                    updateCurrentDimmValue();
+                    if (!lastStep) // lastStep: the status follows below, as "off" (not as the minimum level, "on")
+                        updateCurrentDimmValue();
                     if (isDimmOffLocked()) {
                         logDebugP("Stop here because Dimm off is locked!");
                         _dimmDirection = DimmDirection::None;
                     } else {
                         logDebugP("Turn off device");
-                        daliMaster.sendCommand(_channelIndex, Dali::Command::OFF, _isGroup);
+                        if (isRgbw())
+                        {
+                            rgbwOff();
+                            if (rgbwDown)
+                            {
+                                _dimmDirection = DimmDirection::None;
+                                _queryId = 0; // an answer still on its way (level before the OFF) must not overwrite the status
+                                *currentDimmValue = 0;
+                                setDimmState(0, true, true); // status objects: off, 0 %
+                            }
+                        }
+                        else
+                            daliMaster.sendCommand(_channelIndex, Dali::Command::OFF, _isGroup);
                     }
                 }
             }
@@ -303,8 +543,8 @@ void DaliChannel::processInputKo(GroupObject &ko)
         //logDebugP("Got SHORT KO %i", chanIndex);
     }
 
-    // Case numbers not listed below (1, 4, 7, 10, 13, 16, 19) are read-only
-    // *_state KOs and are never received as input here.
+    // Case numbers not listed below (1, 4, 7, 10, 13, 16, 19) are the *_state KOs; they are handed to us
+    // when we write them ourselves and are ignored here.
     switch (chanIndex)
     {
     case DGW_Koswitch:
@@ -382,6 +622,9 @@ void DaliChannel::koHandleScene(GroupObject &ko)
         return;
     }
 
+    // the driver executes the scene on its own: a pending RGBW window and the level target are dropped
+    if (isRgbw())
+        rgbwCancel();
     daliMaster.sendCommand(_channelIndex, Dali::Command::GO_TO_SCENE | number, _isGroup);
 }
 
@@ -454,6 +697,20 @@ void DaliChannel::koHandleSwitch(GroupObject &ko)
     logDebugP("Switching");
     if (currentIsLocked)
     {
+        if (rgbwRestoreOnUnlock())
+        {
+            // RGBW, restore on unlock: store only, it goes out at the unlock
+            currentState = ko.value(DPT_Switch);
+            if (currentState)
+            {
+                uint8_t onValue = isNight ? _onNight : _onDay;
+                if (onValue == 0)
+                    onValue = isNight ? _lastNightValue : _lastDayValue;
+                currentStep = onValue;
+            }
+            logDebugP("RGBW: locked, stored for the unlock: %s", currentState ? "on" : "off");
+            return;
+        }
         logErrorP("is locked");
         return;
     }
@@ -487,7 +744,10 @@ void DaliChannel::handleSwitchNormal(GroupObject &ko)
         if(!hclBriActivated) {
             logDebugP(isNight ? "Turning on night value" : "Turning on day value");
         }
-        daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+        if (isRgbw())
+            rgbwRequestLevel(onValue); // collection window, then (temporary colour +) DAPC
+        else
+            daliMaster.sendArc(_channelIndex, onValue, _isGroup);
 
         bool hclTempActivated = false;
         if(_hclCurve != HclDisabled && _hclIsAutoMode)
@@ -505,7 +765,10 @@ void DaliChannel::handleSwitchNormal(GroupObject &ko)
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
@@ -580,12 +843,29 @@ void DaliChannel::koHandleDimmRel(GroupObject &ko)
     if (_dimmStep == 0)
     {
         logDebugP("Dimm Stop");
+        const bool alreadyEnded = _dimmDirection == DimmDirection::None;
         _dimmDirection = DimmDirection::None;
         _dimmLast = 0;
         updateCurrentDimmValue();
+        if (alreadyEnded && isRgbw() && !_isStaircase && currentDimmType == DimmType::Brigthness)
+        {
+            // RGBW: the dimming already ended at the maximum or minimum (loopDimming); the colour and the level query
+            // were sent (or are pending) from there. A stop telegram that comes later sends nothing again (B27).
+            logDebugP("RGBW: dimming already ended, stop ignored");
+            return;
+        }
+        // the last UP/DOWN is still running in the driver: ask for the final level shortly after
+        _stopQueryAt = (millis() + 300) | 1; // never 0 (= none)
         return;
     }
 
+    if (isRgbw() && !_isStaircase)
+    {
+        // a pending window goes out first
+        if (_winPending)
+            rgbwFlush();
+    }
+
     currentDimmValue = &currentStep;
     currentDimmType = DimmType::Brigthness;
     _dimmLastStatus = millis();
@@ -605,6 +885,8 @@ void DaliChannel::koHandleDimmRel(GroupObject &ko)
                 daliMaster.sendCommand(_channelIndex, Dali::Command::RECALL_MIN, _isGroup, true);
                 *currentDimmValue = _min;
                 currentState = true;
+                if (isRgbw() && !_isStaircase)
+                    setSwitchState(true, false); // RGBW: "on" right away, not only at the stop (B23, C5)
                 queryActualLevel();
                 logDebugP("starting with min %i", _min);
                 _dimmLast = millis();
@@ -623,6 +905,16 @@ void DaliChannel::koHandleDimmAbs(GroupObject &ko)
     logDebugP("Dimming absolute");
     if (currentIsLocked)
     {
+        if (rgbwRestoreOnUnlock())
+        {
+            // RGBW, restore on unlock: store only, it goes out at the unlock
+            uint8_t arc = DaliHelper::percentToArc((uint8_t)ko.value(Dpt(5, 1)));
+            if (arc > 0)
+                currentStep = arc;
+            currentState = arc > 0;
+            logDebugP("RGBW: locked, stored for the unlock: level %i", arc);
+            return;
+        }
         logErrorP("is locked");
         return;
     }
@@ -633,8 +925,45 @@ void DaliChannel::koHandleDimmAbs(GroupObject &ko)
     uint8_t value = ko.value(Dpt(5, 1));
     logDebugP("Dimming absolute to %i%%", value);
     uint8_t arc = DaliHelper::percentToArc(value);
-    daliMaster.sendArc(_channelIndex, arc, _isGroup);
+    if (isRgbw() && !_isStaircase)
+    {
+        if (arc == 0)
+            rgbwOff();
+        else
+            rgbwRequestLevel(arc); // collection window, then (temporary colour +) DAPC
+    }
+    else
+        daliMaster.sendArc(_channelIndex, arc, _isGroup);
     setDimmState(arc, true, true);
+    // keep the on/off flag in sync, otherwise e.g. a colour telegram after "dimm to 0" is ignored
+    if (!_isStaircase)
+        currentState = arc > 0;
+}
+
+bool DaliChannel::rgbwSliderMode()
+{
+    return isRgbw() && !_isStaircase && (_isGroup ? ParamDGWG_rgbwBri : ParamDGW_rgbwBri) == PT_rgbwBri_colour;
+}
+
+// DALI arc value that belongs to the amplitude (largest channel) of the stored colour
+uint8_t DaliChannel::rgbwArcFromColour()
+{
+    return DaliHelper::percentToArc(RgbwHelper::percentFromAmplitude(RgbwHelper::maxChannel(currentColor)));
+}
+
+// Slider mode: the level changed (absolute / relative dimming, switch on, query of the driver). The colour status
+// follows as mix x level, so that a client that reads the brightness from the colour value sees it.
+void DaliChannel::rgbwSliderStatus(uint8_t arc)
+{
+    if (arc == 0 || !rgbwSliderMode() || RgbwHelper::isOff(_mix) || RgbwHelper::isOff(currentColor))
+        return;
+    if (rgbwArcFromColour() == arc)
+        return; // the colour value already stands for this level (e.g. it just came in)
+    uint8_t amp = RgbwHelper::amplitudeFromPercent(DaliHelper::arcToPercentFloat(arc));
+    if (amp == 0)
+        amp = 1;
+    RgbwHelper::scale(_mix, amp, currentColor);
+    sendRgbwState();
 }
 
 void DaliChannel::koHandleLock(GroupObject &ko)
@@ -655,6 +984,15 @@ void DaliChannel::koHandleLock(GroupObject &ko)
     if (currentIsLocked == value)
         return;
     currentIsLocked = value;
+    if (isRgbw() && !_isStaircase)
+    {
+        // RGBW: own branch with the corrections S1-S4 (README 5.1 plan point 8; 5.8 G4 for the other types)
+        if (currentIsLocked)
+            rgbwLock();
+        else
+            rgbwUnlock();
+        return;
+    }
     uint8_t behave;
     uint8_t behavevalue;
     if (currentIsLocked)
@@ -701,15 +1039,98 @@ void DaliChannel::koHandleLock(GroupObject &ko)
     }
     }
     logDebugP("%i - %i", behavevalue, DaliHelper::percentToArc(behavevalue));
+    if (isRgbw())
+        rgbwCancel();
     daliMaster.sendArc(_channelIndex, behavevalue, _isGroup);
     setDimmState(DaliHelper::percentToArc(behavevalue));
 }
 
+// RGBW lock: "restore on unlock" (parameter, RGBW only)
+bool DaliChannel::rgbwRestoreOnUnlock()
+{
+    return isRgbw() && !_isStaircase && (_isGroup ? ParamDGWG_unlockRestore : ParamDGW_unlockRestore);
+}
+
+// DALI arc value of a lock / unlock behaviour (S1: "Anschalten" = switch-on value, "Ausschalten" = 0; S2: the fixed
+// value is a percentage)
+uint8_t DaliChannel::rgbwLockArc(uint8_t behave, uint8_t percentValue)
+{
+    switch (behave)
+    {
+    case PT_lock_on:
+    {
+        uint8_t onValue = isNight ? _onNight : _onDay;
+        if (onValue == 0)
+            onValue = isNight ? _lastNightValue : _lastDayValue;
+        return onValue;
+    }
+    case PT_lock_value:
+        return DaliHelper::percentToArc(percentValue);
+    default: // PT_lock_off
+        return 0;
+    }
+}
+
+// RGBW lock: level only, no colour (the lamp keeps the colour of the driver). With "restore on unlock" the stored
+// state (on/off, level, colour) stays as it is and only the status follows the lamp.
+void DaliChannel::rgbwLock()
+{
+    // S3: groups read the group parameters
+    uint8_t behave = _isGroup ? ParamDGWG_lockbehave : ParamDGW_lockbehave;
+    uint8_t percent = _isGroup ? ParamDGWG_lockvalue : ParamDGW_lockvalue;
+    logDebugP("RGBW: locking (behaviour %i)", behave);
+    rgbwCancel();
+    _dimmDirection = DimmDirection::None; // a running relative dimming ends with the lock
+    if (behave == PT_lock_no)
+        return;
+    uint8_t arc = rgbwLockArc(behave, percent);
+    daliMaster.sendArc(_channelIndex, arc, _isGroup);
+    if (rgbwRestoreOnUnlock())
+    {
+        setDimmState(arc, false, true);
+        setSwitchState(arc > 0, false);
+    }
+    else
+    {
+        setDimmState(arc, true, true);
+        currentState = arc > 0; // S4
+    }
+}
+
+// RGBW unlock: with "restore on unlock" the stored state (incl. the commands received while locked) goes out as one
+// command; otherwise the unlock behaviour (level only, no colour)
+void DaliChannel::rgbwUnlock()
+{
+    logDebugP("RGBW: unlocking");
+    rgbwCancel();
+    if (rgbwRestoreOnUnlock())
+    {
+        rgbwSendState();
+        setDimmState(currentState ? rgbwStoredLevel() : 0, false, true);
+        setSwitchState(currentState, false);
+        sendRgbwState();
+        return;
+    }
+    uint8_t behave = _isGroup ? ParamDGWG_unlockbehave : ParamDGW_unlockbehave;
+    uint8_t percent = _isGroup ? ParamDGWG_unlockvalue : ParamDGW_unlockvalue;
+    if (behave == PT_lock_no)
+        return;
+    uint8_t arc = rgbwLockArc(behave, percent);
+    daliMaster.sendArc(_channelIndex, arc, _isGroup);
+    setDimmState(arc, true, true);
+    currentState = arc > 0; // S4
+}
+
 void DaliChannel::koHandleColor(GroupObject &ko)
 {
-    logDebugP("Color absolute");
+    logDebugP("Color absolute [%s]", RGBW_BUILD_TAG);
     if (currentIsLocked)
     {
+        if (rgbwRestoreOnUnlock())
+        {
+            handleColorRGBW(ko, true); // RGBW, restore on unlock: store only, it goes out at the unlock
+            return;
+        }
         logErrorP("is locked");
         return;
     }
@@ -738,11 +1159,15 @@ void DaliChannel::koHandleColor(GroupObject &ko)
         case PT_colorType_XYY:
             handleColorXYY(ko);
             break;
+
+        case PT_colorType_RGBW:
+            handleColorRGBW(ko); // switching and status objects are handled there
+            return;
     }
 
     if(!currentState)
         setDimmState(_onDay, true, true); // TODO get real
-    
+
     logDebugP("AutoConf %i %i %i", _isGroup, _isGroup ? ParamDGW_hcl_manu_col : ParamDGWG_hcl_manu_col, _hclIsAutoMode);
 }
 
@@ -780,6 +1205,106 @@ void DaliChannel::handleColorRGB(GroupObject &ko)
     sendColor();
 }
 
+void DaliChannel::handleColorRGBW(GroupObject &ko, bool storeOnly)
+{
+    // DPT 251.600 has 6 bytes: R, G, B, W, reserved, validity bits (mR, mG, mB, mW)
+    if (ko.valueSize() != RgbwHelper::DataLength)
+    {
+        logErrorP("RGBW: unexpected object size %i", (int)ko.valueSize());
+        return;
+    }
+
+    // Decode into a copy first: all channels 0 means "off" and must not overwrite the stored colour.
+    // A single channel keeps the channels that are not marked valid (the colour of the driver is known). A group
+    // always takes a complete colour: its members can also belong to other groups or be changed on their own, so
+    // the colour stored in the group is not reliable. The channels that are not marked valid are 0 there.
+    uint8_t newColor[4] = {0, 0, 0, 0};
+    if (!_isGroup)
+        memcpy(newColor, currentColor, sizeof(newColor));
+    const uint8_t valid = RgbwHelper::decode(ko.valueRef(), newColor);
+    if (valid == 0)
+    {
+        logDebugP("RGBW: no valid channel in telegram, ignored");
+        return;
+    }
+
+    logDebugP("Got RGBW: %i %i %i %i (valid %X)%s", newColor[0], newColor[1], newColor[2], newColor[3], valid, _isGroup && valid != 0x0F ? " group: channels that are not valid are 0" : "");
+
+    if (RgbwHelper::isOff(newColor) && storeOnly)
+    {
+        currentState = false;
+        logDebugP("RGBW: locked, stored for the unlock: off");
+        return;
+    }
+    if (RgbwHelper::isOff(newColor))
+    {
+        // R = G = B = W = 0 -> switch the channel off (status objects report "Off", 0 %).
+        // The last colour stays stored in gateway and driver, so "Schalten = Ein" or the next
+        // colour telegram brings the light back with that colour instead of staying dark.
+        logDebugP("RGBW: all channels 0 -> switching off");
+        rgbwOff();
+        setDimmState(0, true, true);
+        currentState = false;
+        return;
+    }
+
+    memcpy(currentColor, newColor, sizeof(currentColor));
+    const bool slider = rgbwSliderMode();
+    if (slider)
+        RgbwHelper::normalise(currentColor, _mix);
+    if (storeOnly)
+    {
+        if (slider)
+        {
+            uint8_t sliderArc = rgbwArcFromColour();
+            currentStep = sliderArc == 0 ? 1 : sliderArc;
+            currentState = true;
+        }
+        logDebugP("RGBW: locked, stored for the unlock: colour %i %i %i %i", currentColor[0], currentColor[1], currentColor[2], currentColor[3]);
+        return;
+    }
+    sendRgbwState();
+
+    if (slider)
+    {
+        // Brightness from the RGBW value: the largest channel is the level, so a colour > 0 switches on (from off
+        // with the colour, see rgbwRequestLevel) and always goes out together with its level.
+        uint8_t sliderArc = rgbwArcFromColour();
+        if (sliderArc == 0)
+            sliderArc = 1; // a colour > 0 is "on", the smallest level at least
+        logDebugP("RGBW: brightness from the colour value: %i %%, level %i", (int)(RgbwHelper::percentFromAmplitude(RgbwHelper::maxChannel(currentColor)) + 0.5f), sliderArc);
+        _winColour = true;
+        rgbwRequestLevel(sliderArc);
+        setDimmState(sliderArc, true, true);
+        currentState = true;
+    }
+    else
+    {
+        // colour alone, through the collection window (merged with a level or switch-on that follows). At its end:
+        // switched-off channel = ACTIVATE (lamp stays off, status "off"), lit single channel = ACTIVATE, lit group =
+        // temporary colour + DAPC with the group level (rgbwFlush)
+        rgbwRequestColour();
+    }
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
@@ -842,6 +1367,8 @@ void DaliChannel::setTemperature(uint16_t value)
 void DaliChannel::setBrightness(uint8_t value)
 {
     logDebugP("Set Brightness: %i %%", value);
+    if (isRgbw())
+        rgbwCancel(); // HCL changes the level: a pending window is dropped
     daliMaster.sendArc(_channelIndex, DaliHelper::percentToArc(value), _isGroup);
     sendKoStateOnChange(DGW_Kodimm_state, value, Dpt(5, 1));
 }
@@ -867,11 +1394,21 @@ void DaliChannel::queryActualLevel()
     _queryId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_ACTUAL_LEVEL, false, true);
 }
 
-void DaliChannel::sendColor()
+void DaliChannel::sendColor(bool activate)
 {
     // TODO only send every 100ms?
     uint8_t r, g, b;
     uint8_t colorType = _isGroup ? ParamDGWG_colorType : ParamDGW_colorType;
+    if (colorType == PT_colorType_RGBW)
+    {
+        // RGBW is always transmitted as RGB level + white level, "colorSpace" does not apply
+        if (rgbwSliderMode() && !RgbwHelper::isOff(_mix))
+            sendColorAsRGBW(_mix[0], _mix[1], _mix[2], _mix[3], activate); // mix only, the brightness goes as level
+        else
+            sendColorAsRGBW(currentColor[0], currentColor[1], currentColor[2], currentColor[3], activate);
+        return;
+    }
+
     if (colorType == PT_colorType_HSV)
     {
         ColorHelper::hsvToRGB(currentColor[0], currentColor[1], currentColor[2], r, g, b);
@@ -905,6 +1442,31 @@ void DaliChannel::sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b)
     daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
 }
 
+void DaliChannel::sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, bool activate)
+{
+    r = RgbwHelper::daliLevel(r);
+    g = RgbwHelper::daliLevel(g);
+    b = RgbwHelper::daliLevel(b);
+    w = RgbwHelper::daliLevel(w);
+    logDebugP("Send RGBW: %i %i %i %i%s", r, g, b, w, activate ? "" : " (temporary, activated by the following DAPC)");
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
+    // 3. Apply both temporary values at once. Without ACTIVATE the following level command (DAPC) activates them.
+    if (activate)
+        daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
+}
+
 void DaliChannel::sendColorAsXY(uint8_t r, uint8_t g, uint8_t b)
 {
     uint16_t x;
@@ -957,7 +1519,16 @@ void DaliChannel::setSwitchState(bool value, bool isSwitchCommand)
     // bool currentState = knx.getGroupObject(calcKoNumber(_isGroup ? DGWG_Koswitch_state : DGW_Koswitch_state)).value(DPT_Switch);
     // if (value == currentState)
     //     return;
-    knx.getGroupObject(calcKoNumber(_isGroup ? DGWG_Koswitch_state : DGW_Koswitch_state)).value(value, DPT_Switch);
+    GroupObject &stateKo = knx.getGroupObject(calcKoNumber(_isGroup ? DGWG_Koswitch_state : DGW_Koswitch_state));
+    if (isRgbw() && !_isStaircase)
+    {
+        // RGBW: send the switch status only when it changes (otherwise "on" went out with every absolute dimming
+        // command and "off" with every repeated off while dimming down)
+        if (stateKo.valueNoSendCompare(value, DPT_Switch))
+            stateKo.objectWritten();
+    }
+    else
+        stateKo.value(value, DPT_Switch);
 }
 
 void DaliChannel::setDimmState(uint8_t value, bool isDimmCommand, bool isLastCommand)
@@ -976,6 +1547,9 @@ void DaliChannel::setDimmState(uint8_t value, bool isDimmCommand, bool isLastCom
         currentStep = value;
     }
 
+    _lastLevelArc = value;
+    rgbwSliderStatus(value);
+
     float perc = DaliHelper::arcToPercentFloat(value);
 
     GroupObject& ko = knx.getGroupObject(calcKoNumber(DGW_Kodimm_state));
@@ -1002,6 +1576,12 @@ void DaliChannel::updateCurrentDimmValue()
 
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
@@ -1060,13 +1640,35 @@ void DaliChannel::setDimmRef(uint8_t ref)
 void DaliChannel::setGroupState(uint16_t group, bool state)
 {
     if (group == AllGroups || _groups & 1 << group)
+    {
         setSwitchState(state, false);
+        // RGBW: the on/off flag follows what a group or broadcast command did, a pending window is dropped. Locked
+        // with "restore on unlock": the stored state stays (it is sent again at the unlock).
+        if (isRgbw() && !_isStaircase && !(currentIsLocked && rgbwRestoreOnUnlock()))
+        {
+            rgbwCancel();
+            currentState = state;
+        }
+    }
 }
 
 void DaliChannel::setGroupState(uint16_t group, uint8_t value)
 {
     if (group == AllGroups || _groups & (1 << group))
-        setDimmState(value, true, true);
+    {
+        // RGBW locked with "restore on unlock": only the status follows the lamp, the stored state stays
+        const bool keep = isRgbw() && !_isStaircase && currentIsLocked && rgbwRestoreOnUnlock();
+        setDimmState(value, !keep, true);
+        if (keep)
+            setSwitchState(value > 0, false);
+        // RGBW: also at start (level read from the driver): a lamp that is lit is "on" for us, otherwise "brighter"
+        // would jump to the minimum. A pending window is dropped.
+        if (isRgbw() && !_isStaircase && !keep)
+        {
+            rgbwCancel();
+            currentState = value > 0;
+        }
+    }
 }
 
 void DaliChannel::setMinArc(uint8_t min)
diff --git a/src/DaliChannel.h b/src/DaliChannel.h
index 19b49c9..52442ad 100644
--- a/src/DaliChannel.h
+++ b/src/DaliChannel.h
@@ -8,11 +8,19 @@
 #include "DaliConstants.h"
 
 #define DimmInterval 200
+// Version tag of the RGBW extension, shown in the log line "Color absolute [...]" (test aid, remove before a PR)
+#define RGBW_BUILD_TAG "v14b"
+// RGBW collection window (ms): telegrams that arrive within this time (Home Assistant: colour and brightness ~20 ms
+// apart) are merged into one DALI command (temporary colour + DAPC = one fade). Every RGBW command goes through the
+// window, also a colour to a switched-off channel. 0 = send at once. Changeable at runtime: console command "aawin".
+#define RgbwWindowMs 80
 #define DimmStatusInterval 500
 
 class DaliChannel : public OpenKNX::Channel
 {
 	public:
+		// RGBW: collection window (console "aawin")
+		static uint16_t rgbwWindowMs;
         static constexpr uint16_t AllGroups = DaliConstants::AllGroups;
 
         DaliChannel(Dali::Master &master);
@@ -20,6 +28,18 @@ class DaliChannel : public OpenKNX::Channel
 
 		void loop() override;
 		void loop1();
+
+		// RGBW (DPT 251.600), see the concept comment in DaliChannel.cpp
+		bool isRgbw();
+		void rgbwOff();
+		void rgbwCancel();
+		// "Broadcast zurueck": sends the stored state of this RGBW channel or group again (not when locked)
+		void rgbwRestore();
+		// the gateway knows a colour for this channel or group (it got one since the start)
+		bool rgbwColourKnown();
+		// single channel: member of at least one configured RGBW group
+		bool isInRgbwGroup(DaliChannel *groups);
+
 		void setup() override;
 		void processInputKo(GroupObject &ko) override;
 		const bool isConfigured();
@@ -106,17 +126,56 @@ class DaliChannel : public OpenKNX::Channel
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
 
+		// RGBW collection window: newest requested level (0 = none), colour changed, request came while off
+		bool _winPending = false;
+		unsigned long _winAt = 0;
+		uint8_t _winLevel = 0;
+		bool _winColour = false;
+		bool _winFromOff = false;
+		void rgbwRequestLevel(uint8_t arc);
+		void rgbwRequestColour();
+		void rgbwStartWindow();
+		void rgbwFlush();
+		void loopRgbwWindow();
+		// Sends the stored state as one command: on = temporary colour + DAPC (stored level), off = temporary
+		// colour + DAPC 0 (the colour fades in unseen while the lamp goes out)
+		void rgbwSendState();
+		uint8_t rgbwStoredLevel();
+		// Lock of an RGBW channel or group (S1-S4 corrected, README 5.1 plan point 8) and "restore on unlock"
+		bool rgbwRestoreOnUnlock();
+		void rgbwLock();
+		void rgbwUnlock();
+		uint8_t rgbwLockArc(uint8_t behave, uint8_t percentValue);
+
+		// RGBW brightness from the colour value ("slider" mode, parameter rgbwBri): the largest channel of the RGBW
+		// value is the brightness (255 = 100 %), the ratio of the channels is the colour mix. The driver ignores
+		// the amplitude of colour values (it normalises), so the level is sent as an own level command and the
+		// colour as normalised mix (_mix, largest channel 255). The colour status shows mix x level.
+		uint8_t _mix[4] = {0, 0, 0, 0};
+		uint8_t _lastLevelArc = 0; // last level reported through setDimmState (DALI arc value)
+		bool rgbwSliderMode();
+		uint8_t rgbwArcFromColour();
+		void rgbwSliderStatus(uint8_t arc);
+
+		// After the stop of a relative dimming the real level is queried once (the last UP/DOWN is still running
+		// in the driver when the stop arrives, so the level queried during dimming is one step behind). RGBW: the
+		// stored colour is sent again at the same time (an UP/DOWN right after a colour aborts the colour fade, B25).
+		unsigned long _stopQueryAt = 0;
+		void loopStopQuery();
+
+
 		//Turn-on with last value
-		uint8_t _lastDayValue = 100;
-		uint8_t _lastNightValue = 10;
+		// DALI arc values (not percent): 100 % and 10 % until the first level is known
+		uint8_t _lastDayValue = DaliHelper::percentToArc((uint8_t)100);
+		uint8_t _lastNightValue = DaliHelper::percentToArc((uint8_t)10);
 
 		//Group membership
 		uint16_t _groups = 0;
@@ -129,8 +188,10 @@ class DaliChannel : public OpenKNX::Channel
 		void setSwitchState(bool value, bool isSwitchCommand = true);
 		void setDimmState(uint8_t value, bool isDimmCommand = true, bool isLastCommand = false);
 		void updateCurrentDimmValue();
-		void sendColor();
+		void sendColor(bool activate = true); // activate = false: RGBW only, temporary colour without ACTIVATE
 		void sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b);
+		void sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, bool activate = true);
+		void sendRgbwState();
 		void sendColorAsXY(uint8_t r, uint8_t g, uint8_t b);
 		void sendKoStateOnChange(uint16_t koNr, const KNXValue &value, const Dpt &type);
 		void setTemperature(uint16_t value);
@@ -148,6 +209,7 @@ class DaliChannel : public OpenKNX::Channel
 		void handleColorRGB(GroupObject &ko);
 		void handleColorTW(GroupObject &ko);
 		void handleColorXYY(GroupObject &ko);
+		void handleColorRGBW(GroupObject &ko, bool storeOnly = false); // storeOnly: locked, restore on unlock
 		void koHandleColorRel(GroupObject &ko, ColorChannel channel);
 		void koHandleColorAbs(GroupObject &ko, ColorChannel channel);
 		void koHandleHclCurve(GroupObject &ko);
diff --git a/src/DaliModule.cpp b/src/DaliModule.cpp
index 47a905c..bd6d31f 100644
--- a/src/DaliModule.cpp
+++ b/src/DaliModule.cpp
@@ -1,4 +1,6 @@
 #include "DaliModule.h"
+#include "RgbwHelper.h"
+#include <cctype>
 #include "OpenKNX/DateTime.h"
 
 #ifdef ARDUINO_ARCH_ESP32
@@ -117,6 +119,7 @@ void DaliModule::setup1(bool conf) {
     daliMaster.init(REG1_APP_PIN6, REG1_APP_PIN7);
 }
 
+
 #ifdef DALI_NO_TIMER
 bool __isr __time_critical_func(daliTimerInterruptCallback)(repeating_timer *t)
 {
@@ -348,6 +351,10 @@ void DaliModule::showHelp()
     openknx.console.printHelpLine("stepUp", "stepUp xyy => send x times StepUp to evg y");
     openknx.console.printHelpLine("stepDown", "stepDown xyy => send x times StepDown to evg y");
     openknx.console.printHelpLine("getLvl", "getLvl yy => get level of evg y");
+    openknx.console.printHelpLine("getCol", "getCol yy [sel] => DT8 colour query of evg y (diagnosis); sel = DTR0 selector, without: R/G/B/W report (233-236), actual (9-12), temporary (201-204)");
+    openknx.console.printHelpLine("aastat", "aastat => RGBW: read the DT8 automatic activation (gear features/status) of all configured RGBW channels");
+    openknx.console.printHelpLine("aaon", "aaon [yy] => RGBW: switch DT8 automatic activation on permanently (stored in the driver); without yy all configured RGBW channels");
+    openknx.console.printHelpLine("aawin", "aawin nnn => RGBW collection window in ms, telegrams within it go out as one command (default 80, 0 = at once)");
 }
 
 bool DaliModule::processCommand(const std::string cmd, bool diagnoseKo)
@@ -396,12 +403,46 @@ bool DaliModule::processCommand(const std::string cmd, bool diagnoseKo)
         return true;
     }
 
+
+    if (command == "aawin")
+    {
+        if (!hasArg || arg.empty() || arg.length() > 4 || arg.find_first_not_of("0123456789") != std::string::npos)
+        {
+            logErrorP("aawin nnn (ms, 0-1000, 0 = send at once), now %i", DaliChannel::rgbwWindowMs);
+            return true;
+        }
+        int v = std::stoi(arg);
+        if (v > 1000)
+            v = 1000;
+        DaliChannel::rgbwWindowMs = v;
+        logInfoP("RGBW collection window = %i ms", v);
+        return true;
+    }
+
     if (command == "getLvl")
     {
         cmdHandleGetLvl(hasArg, arg);
         return true;
     }
 
+    if (command == "getCol")
+    {
+        cmdHandleGetCol(hasArg, arg);
+        return true;
+    }
+
+    if (command == "aastat")
+    {
+        cmdHandleAaStat();
+        return true;
+    }
+
+    if (command == "aaon")
+    {
+        cmdHandleAaOn(hasArg, arg);
+        return true;
+    }
+
     return false;
 }
 
@@ -467,6 +508,137 @@ void DaliModule::cmdHandleGetLvl(bool hasArg, std::string arg)
         logErrorP("Fehler beim Auslesen %i", resp);
 }
 
+// Diagnosis for the colour of a DT8 driver (RGBW restore at start): asks QUERY_COLOUR_TYPE_FEATURES,
+// QUERY_COLOUR_STATUS and QUERY_COLOUR_VALUE (DTR0 = selector) and logs the raw answers. Changes nothing.
+// QUERY_COLOUR_VALUE answers the high byte, the low byte is in DTR0 afterwards (DTR1 is logged as well).
+void DaliModule::cmdHandleGetCol(bool hasArg, std::string arg)
+{
+    bool ok = hasArg && arg.length() >= 2 && std::isdigit((unsigned char)arg[0]) && std::isdigit((unsigned char)arg[1]);
+    int sel = -1;
+    if (ok && arg.length() > 2)
+    {
+        ok = arg.length() >= 4 && arg.length() <= 6 && arg[2] == ' ' && arg.find_first_not_of("0123456789", 3) == std::string::npos;
+        if (ok)
+            sel = std::stoi(arg.substr(3));
+        if (ok && sel > 255)
+            ok = false;
+    }
+    else if (ok && arg.length() != 2)
+        ok = false;
+    if (!ok)
+    {
+        logErrorP("Argument is invalid!");
+        logIndentUp();
+        logErrorP("getCol yy [sel]");
+        logErrorP("yy  = Address of device (only short address)");
+        logErrorP("sel = DTR0 selector 0-255 for QUERY_COLOUR_VALUE (default: 233-236 report, 9-12 actual, 201-204 temporary; each R,G,B,W)");
+        logIndentDown();
+        return;
+    }
+    uint8_t addr = std::stoi(arg.substr(0, 2));
+    if (addr > 63)
+    {
+        logErrorP("Short Address is invalid!");
+        return;
+    }
+
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+    int16_t features = getInfo(addr, Dali::ExtendedCommandDT8::QUERY_COLOUR_TYPE_FEATURES);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+    int16_t status = getInfo(addr, Dali::ExtendedCommandDT8::QUERY_COLOUR_STATUS);
+    logInfoP("EVG %i colour type features %i (0x%02X), colour status %i (0x%02X)  [-1 = no answer]", addr, features, features & 0xFF, status, status & 0xFF);
+
+    static const uint8_t defaults[] = {233, 234, 235, 236, 9, 10, 11, 12, 201, 202, 203, 204};
+    const uint8_t count = sel >= 0 ? 1 : sizeof(defaults) / sizeof(defaults[0]);
+    for (uint8_t i = 0; i < count; i++)
+    {
+        uint8_t s = sel >= 0 ? (uint8_t)sel : defaults[i];
+        daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, s);
+        daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+        int16_t high = getInfo(addr, Dali::ExtendedCommandDT8::QUERY_COLOUR_VALUE);
+        int16_t dtr0 = getInfo(addr, Dali::Command::QUERY_DTR);
+        int16_t dtr1 = getInfo(addr, Dali::Command::QUERY_DTR1);
+        logInfoP("EVG %i colour value sel %i: answer %i (0x%02X), DTR0 %i, DTR1 %i  [-1 = no answer]", addr, s, high, high & 0xFF, dtr0, dtr1);
+    }
+}
+
+// DT8 "automatic activation" (gear features/status bit 0): a temporary colour is taken over by the next arc power
+// command (DAPC, GO TO LAST ACTIVE LEVEL) without ACTIVATE. RGBW depends on it. Console only, never at start (README 5.1, C4):
+// "aastat" reads it from all configured RGBW channels, "aaon [yy]" switches it on permanently (stored in the driver).
+int16_t DaliModule::rgbwQueryGearFeatures(uint8_t addr)
+{
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+    return getInfo(addr, Dali::ExtendedCommandDT8::QUERY_GEAR_FEATURES);
+}
+
+void DaliModule::cmdHandleAaStat()
+{
+    uint8_t count = 0;
+    for (int i = 0; i < ChannelCount; i++)
+    {
+        if (!channels[i].isConfigured() || !channels[i].isRgbw())
+            continue;
+        count++;
+        int16_t f = rgbwQueryGearFeatures(i);
+        if (f < 0)
+            logErrorP("EVG %i: gear features/status no answer", i);
+        else
+            logInfoP("EVG %i: gear features/status 0x%02X, automatic activation %s", i, f & 0xFF, (f & 0x01) ? "ON" : "OFF");
+    }
+    if (count == 0)
+        logInfoP("no configured RGBW channel");
+}
+
+void DaliModule::cmdHandleAaOn(bool hasArg, std::string arg)
+{
+    int target = -1; // -1 = all configured RGBW channels
+    if (hasArg && !arg.empty())
+    {
+        if (arg.length() > 2 || arg.find_first_not_of("0123456789") != std::string::npos || std::stoi(arg) > 63)
+        {
+            logErrorP("aaon [yy]  (yy = short address 0-63, without = all configured RGBW channels)");
+            return;
+        }
+        target = std::stoi(arg);
+    }
+
+    for (int i = 0; i < ChannelCount; i++)
+    {
+        if (target >= 0 ? i != target : (!channels[i].isConfigured() || !channels[i].isRgbw()))
+            continue;
+
+        int16_t before = rgbwQueryGearFeatures(i);
+        if (before < 0)
+        {
+            logErrorP("EVG %i: no answer, skipped", i);
+            continue;
+        }
+        if (before & 0x01)
+        {
+            logInfoP("EVG %i: automatic activation already ON (0x%02X)", i, before & 0xFF);
+            continue;
+        }
+
+        // STORE GEAR FEATURES/STATUS is a configuration command: it has to be received twice within 100 ms.
+        // Form 1: DTR0, ENABLE DT 8, command, command. Form 2 (if form 1 did not stick): ENABLE DT 8 before each.
+        bool on = false;
+        for (uint8_t form = 1; form <= 2 && !on; form++)
+        {
+            daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, 0x01); // bit 0 = automatic activation
+            daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+            daliMaster.sendCommand(i, Dali::ExtendedCommandDT8::STORE_GEAR_FEATURES);
+            if (form == 2)
+                daliMaster.sendSpecialCommand(Dali::SpecialCommand::ENABLE_DT, 0x08);
+            daliMaster.sendCommand(i, Dali::ExtendedCommandDT8::STORE_GEAR_FEATURES);
+            int16_t after = rgbwQueryGearFeatures(i);
+            on = after >= 0 && (after & 0x01);
+            logInfoP("EVG %i: store form %i, gear features/status now 0x%02X, automatic activation %s", i, form, after & 0xFF, on ? "ON" : "OFF");
+        }
+        if (!on)
+            logErrorP("EVG %i: automatic activation could not be switched on (driver may not support it)", i);
+    }
+}
+
 void DaliModule::cmdHandleScan(bool hasArg, std::string arg)
 {
     if (!hasArg || arg.length() != 4)
@@ -688,6 +860,16 @@ void DaliModule::processGlobalKo(int koNum, GroupObject &ko)
             koHandleDimm(ko);
             break;
 
+        // broadcast RGBW (colour + level in one command, the channels keep their state)
+        case DGW_Kobroadcast_rgbw:
+            koHandleBroadcastRgbw(ko);
+            break;
+
+        // broadcast back: stored states of the RGBW groups and channels again
+        case DGW_Kobroadcast_back:
+            koHandleBroadcastBack(ko);
+            break;
+
         // Tag/Nacht Objekt
         case DGW_Kodaynight:
             koHandleDayNight(ko);
@@ -712,12 +894,111 @@ void DaliModule::koHandleSwitch(GroupObject &ko)
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
 
+// Broadcast on: DAPC 254 to everybody, level only, no colour (README 5.1 plan point 6): every RGBW lamp comes in
+// the colour stored in its driver. The states follow in broadcastGroupState.
+void DaliModule::broadcastOn()
+{
+    rgbwCancelAll();
+    daliMaster.sendArc(BroadcastAddress, 0xFE, true);
+}
+
+// Broadcast off: one DAPC 0 to everybody
+void DaliModule::broadcastOff()
+{
+    rgbwCancelAll();
+    daliMaster.sendArc(BroadcastAddress, 0x00, true);
+}
+
+// Broadcast RGBW (DPT 251.600): colour and level to everybody in ONE command (temporary colour + DAPC to the
+// broadcast address). The level is the largest channel (like the slider mode), the colour the mix of the channels.
+// The colour only reaches RGBW drivers (DT8 RGBWAF), the level reaches all drivers. Channels and groups keep their
+// own stored state and no status is sent: the broadcast is a temporary state of the lamps, "Broadcast zurueck"
+// brings the stored states back. All channels 0 = DAPC 0 to everybody.
+void DaliModule::koHandleBroadcastRgbw(GroupObject &ko)
+{
+    if (ko.valueSize() != RgbwHelper::DataLength)
+    {
+        logErrorP("Broadcast RGBW: unexpected object size %i", (int)ko.valueSize());
+        return;
+    }
+    uint8_t colour[4] = {0, 0, 0, 0};
+    const uint8_t valid = RgbwHelper::decode(ko.valueRef(), colour);
+    if (valid == 0)
+    {
+        logDebugP("Broadcast RGBW: no valid channel in telegram, ignored");
+        return;
+    }
+    rgbwCancelAll();
+    if (RgbwHelper::isOff(colour))
+    {
+        logDebugP("Broadcast RGBW: all channels 0 -> DAPC 0 to everybody");
+        daliMaster.sendArc(BroadcastAddress, 0x00, true);
+        return;
+    }
+    uint8_t mix[4];
+    RgbwHelper::normalise(colour, mix);
+    uint8_t arc = DaliHelper::percentToArc(RgbwHelper::percentFromAmplitude(RgbwHelper::maxChannel(colour)));
+    if (arc == 0)
+        arc = 1;
+    logDebugP("Broadcast RGBW: %i %i %i %i -> mix %i %i %i %i, level %i (one fade)", colour[0], colour[1], colour[2], colour[3], mix[0], mix[1], mix[2], mix[3], arc);
+
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, RgbwHelper::daliLevel(mix[0]));
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, RgbwHelper::daliLevel(mix[1]));
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, RgbwHelper::daliLevel(mix[2]));
+    daliMaster.sendExtendedCommand(BroadcastAddress, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_RGB_LEVEL, true);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, RgbwHelper::daliLevel(mix[3]));
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, 255);
+    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, 255);
+    daliMaster.sendExtendedCommand(BroadcastAddress, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_WAF_LEVEL, true);
+    daliMaster.sendArc(BroadcastAddress, arc, true); // activates the temporary colour (automatic activation)
+}
+
+// Broadcast zurueck: every RGBW group gets its stored state again, then every RGBW channel that is in no configured
+// RGBW group (members get the state of their group: the gateway does not track the members after group commands,
+// 5.8 G3). Locked channels and groups are skipped. Only RGBW (README 5.1, F1); one command each.
+void DaliModule::koHandleBroadcastBack(GroupObject &ko)
+{
+    if (!ko.value(DPT_Switch))
+        return;
+    logDebugP("Broadcast zurueck: RGBW groups and RGBW channels without group get their stored state");
+    // Two passes (B28): first the groups and channels without a known colour (level only), then those with a colour.
+    // A level without colour aborts a running colour fade in a driver that is also in another group (B26), so the
+    // commands with colour come last. Within a pass the order is G0..G15, then the channels; with overlapping groups
+    // the last command wins.
+    for (int pass = 0; pass < 2; pass++)
+    {
+        const bool withColour = pass == 1;
+        for (int i = 0; i < GroupCount; i++)
+            if (groups[i].isConfigured() && groups[i].isRgbw() && groups[i].rgbwColourKnown() == withColour)
+                groups[i].rgbwRestore();
+        for (int i = 0; i < ChannelCount; i++)
+            if (channels[i].isConfigured() && channels[i].isRgbw() && !channels[i].isInRgbwGroup(groups) &&
+                channels[i].rgbwColourKnown() == withColour)
+                channels[i].rgbwRestore();
+    }
+}
+
+// A DALI scene changes levels (and colours) in the drivers: RGBW channels and groups drop a pending collection window
+// and their level target
+void DaliModule::rgbwCancelAll()
+{
+    for (int i = 0; i < ChannelCount; i++)
+        if (channels[i].isConfigured() && channels[i].isRgbw())
+            channels[i].rgbwCancel();
+    for (int i = 0; i < GroupCount; i++)
+        if (groups[i].isConfigured() && groups[i].isRgbw())
+            groups[i].rgbwCancel();
+}
+
 void DaliModule::koHandleDimm(GroupObject &ko)
 {
     uint8_t value = ko.value(Dpt(5, 1));
@@ -800,6 +1081,7 @@ void DaliModule::koHandleScene(GroupObject &ko)
             continue;
 
         logDebugP("Found scene number: %u, save: %u", i, save);
+        rgbwCancelAll(); // a scene changes levels and colours in the drivers
         if (save && !ParamDGWS_save)
         {
             logDebugP("Save not allowed. Skip!");
diff --git a/src/DaliModule.group.templ.xml b/src/DaliModule.group.templ.xml
index ba96936..48a4742 100644
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
@@ -89,6 +89,12 @@
                 <Memory CodeSegment="%AID%_RS-04-00000" Offset="17" BitOffset="1" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%3%CC%035" Name="%C%scene" ParameterType="%AID%_PT-noyes" Text="Szenenobjekt" Value="0" />
+              <Parameter Id="%AID%_P-%T%3%CC%036" Name="%C%rgbwBri" ParameterType="%AID%_PT-rgbwBri" Text="Helligkeit bei RGBW" Value="0">
+                <Memory CodeSegment="%AID%_RS-04-00000" Offset="17" BitOffset="5" />
+              </Parameter>
+              <Parameter Id="%AID%_P-%T%3%CC%037" Name="%C%unlockRestore" ParameterType="%AID%_PT-noyes" Text="Bei Entsperren vorherigen Zustand wiederherstellen" Value="0">
+                <Memory CodeSegment="%AID%_RS-04-00000" Offset="4" BitOffset="2" />
+              </Parameter>
             </Parameters>
             <ParameterRefs>
               <ParameterRef RefId="%AID%_P-%T%3%CC%001" Id="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" Name="%C%name" />
@@ -121,6 +127,8 @@
               <ParameterRef RefId="%AID%_P-%T%3%CC%033" Id="%AID%_P-%T%3%CC%033_R-%T%3%CC%03301" Name="%C%hcl_auto_off" />
               <ParameterRef RefId="%AID%_P-%T%3%CC%034" Id="%AID%_P-%T%3%CC%034_R-%T%3%CC%03401" Name="%C%hcl_auto_day" />
               <ParameterRef RefId="%AID%_P-%T%3%CC%035" Id="%AID%_P-%T%3%CC%035_R-%T%3%CC%03501" Name="%C%scene" />
+              <ParameterRef RefId="%AID%_P-%T%3%CC%036" Id="%AID%_P-%T%3%CC%036_R-%T%3%CC%03601" Name="%C%rgbwBri" />
+              <ParameterRef RefId="%AID%_P-%T%3%CC%037" Id="%AID%_P-%T%3%CC%037_R-%T%3%CC%03701" Name="%C%unlockRestore" />
             </ParameterRefs>
             <ComObjectTable>
               <ComObject Id="%AID%_O-%T%3%CC%000" Name="%C%switch" Text="G%C-1% {{0}}" Number="%K0%" FunctionText="Schalten" ObjectSize="1 Bit" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-1-1" />
@@ -173,6 +181,8 @@
               <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00703" FunctionText="Farbtemperatur Status" DatapointType="DPST-7-600" ObjectSize="2 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%006" Id="%AID%_O-%T%3%CC%006_R-%T%3%CC%00604" FunctionText="Farbe xyY / xy" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00704" FunctionText="Farbe xyY / xy Status" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
+              <ComObjectRef RefId="%AID%_O-%T%3%CC%006" Id="%AID%_O-%T%3%CC%006_R-%T%3%CC%00605" FunctionText="Farbe RGBW" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
+              <ComObjectRef RefId="%AID%_O-%T%3%CC%007" Id="%AID%_O-%T%3%CC%007_R-%T%3%CC%00705" FunctionText="Farbe RGBW Status" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%017" Id="%AID%_O-%T%3%CC%017_R-%T%3%CC%01701" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
               <ComObjectRef RefId="%AID%_O-%T%3%CC%018" Id="%AID%_O-%T%3%CC%018_R-%T%3%CC%01801" TextParameterRefId="%AID%_P-%T%3%CC%001_R-%T%3%CC%00101" />
             </ComObjectRefs>
@@ -288,6 +298,16 @@
                           <ComObjectRefRef RefId="%AID%_O-%T%3%CC%006_R-%T%3%CC%00604" />
                           <ComObjectRefRef RefId="%AID%_O-%T%3%CC%007_R-%T%3%CC%00704" />
                         </when>
+                        <when test="=4">
+                          <ParameterRefRef RefId="%AID%_P-%T%3%CC%036_R-%T%3%CC%03601" />
+                          <choose ParamRefId="%AID%_P-%T%3%CC%036_R-%T%3%CC%03601">
+                            <when test="=1">
+                              <ParameterSeparator Id="%AID%_PS-nnn" Text="Der größte Kanal des RGBW-Werts ist die Helligkeit (z. B. Home Assistant ohne Helligkeitsadresse). Das Objekt &quot;Dimmen absolut&quot; stellt den Pegel weiterhin direkt, der RGBW-Status passt sich an." UIHint="Information" />
+                            </when>
+                          </choose>
+                          <ComObjectRefRef RefId="%AID%_O-%T%3%CC%006_R-%T%3%CC%00605" />
+                          <ComObjectRefRef RefId="%AID%_O-%T%3%CC%007_R-%T%3%CC%00705" />
+                        </when>
                       </choose>
                       <choose ParamRefId="%AID%_P-%T%3%CC%020_R-%T%3%CC%02001">
                         <when test="!=0">
@@ -322,6 +342,16 @@
                           <ParameterRefRef RefId="%AID%_P-%T%3%CC%011_R-%T%3%CC%01101" />
                         </when>
                       </choose>
+                      <choose ParamRefId="%AID%_P-%T%3%CC%019_R-%T%3%CC%01901">
+                        <when test="4">
+                          <ParameterRefRef RefId="%AID%_P-%T%3%CC%037_R-%T%3%CC%03701" />
+                          <choose ParamRefId="%AID%_P-%T%3%CC%037_R-%T%3%CC%03701">
+                            <when test="1">
+                              <ParameterSeparator Id="%AID%_PS-nnn" Text="RGBW: Befehle während der Sperre werden gespeichert. Beim Entsperren kommt der gespeicherte Zustand (Ein/Aus, Helligkeit, Farbe); &quot;Verhalten bei Entsperren&quot; gilt dann nicht." UIHint="Information" />
+                            </when>
+                          </choose>
+                        </when>
+                      </choose>
                     </when>
                   </choose>
                   <choose ParamRefId="%AID%_P-%T%3%CC%004_R-%T%3%CC%00401">
diff --git a/src/DaliModule.h b/src/DaliModule.h
index 1206625..a52928f 100644
--- a/src/DaliModule.h
+++ b/src/DaliModule.h
@@ -63,6 +63,11 @@ class DaliModule : public OpenKNX::Module
 		uint8_t _currentIdentifyDevice = 0;
 #endif
 		bool _currentLockState = false;
+		void broadcastOff();
+		void broadcastOn();
+		void rgbwCancelAll();
+		void koHandleBroadcastRgbw(GroupObject &ko);
+		void koHandleBroadcastBack(GroupObject &ko);
 		int16_t getInfo(byte address, uint8_t command, uint8_t additional = 0);
 	
 		DaliAddressing _addressing{daliMaster};
@@ -121,6 +126,10 @@ class DaliModule : public OpenKNX::Module
 		void cmdHandleStepUp(bool hasArg, std::string arg);
 		void cmdHandleStepDown(bool hasArg, std::string arg);
 		void cmdHandleGetLvl(bool hasArg, std::string arg);
+		void cmdHandleGetCol(bool hasArg, std::string arg);
+		void cmdHandleAaStat();
+		void cmdHandleAaOn(bool hasArg, std::string arg);
+		int16_t rgbwQueryGearFeatures(uint8_t addr);
 
 		void stateHandleAssign(uint8_t *data, uint8_t *resultData, uint8_t &resultLength);
 		void stateHandleScanAndAddress(uint8_t *data, uint8_t *resultData, uint8_t &resultLength);
diff --git a/src/DaliModule.share.xml b/src/DaliModule.share.xml
index 1219e9f..b820fed 100644
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
@@ -127,6 +128,12 @@
                   <Enumeration Text="X/Y Koordinaten" Value="0" Id="%ENID%" op:headerName="xy" />
                 </TypeRestriction>
               </ParameterType>
+              <ParameterType Id="%AID%_PT-rgbwBri" Name="rgbwBri" op:headerExport="define">
+                <TypeRestriction Base="Value" SizeInBit="1">
+                  <Enumeration Text="Über Objekt &quot;Dimmen absolut&quot; (Farbe = nur Mischung)" Value="0" Id="%ENID%" op:headerName="object" />
+                  <Enumeration Text="Aus dem RGBW-Wert (größter Kanal = Helligkeit)" Value="1" Id="%ENID%" op:headerName="colour" />
+                </TypeRestriction>
+              </ParameterType>
               <ParameterType Id="%AID%_PT-fadeTimes" Name="fadeTimes">
                 <TypeRestriction Base="Value" SizeInBit="4">
                   <Enumeration Text="Keine" Value="0" Id="%ENID%" />
@@ -368,6 +375,8 @@
               <ComObject Id="%AID%_O-%T%100003" Name="daynight" Text="Dali: Allgemein" Number="3" FunctionText="Tag/Nacht" ObjectSize="1 Bit" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-1-1" />
               <ComObject Id="%AID%_O-%T%100004" Name="onValue" Text="Dali: Allgemein" Number="4" FunctionText="Einschaltwert Tag" ObjectSize="1 Byte" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-5-1" />
               <ComObject Id="%AID%_O-%T%100005" Name="scenes" Text="Dali: Allgemein" Number="5" FunctionText="Szene" ObjectSize="1 Byte" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-18-1" />
+              <ComObject Id="%AID%_O-%T%100006" Name="broadcast_rgbw" Text="Dali: Broadcast" Number="6" FunctionText="Farbe RGBW (alle RGBW, Pegel an alle)" ObjectSize="6 Bytes" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-251-600" />
+              <ComObject Id="%AID%_O-%T%100007" Name="broadcast_back" Text="Dali: Broadcast" Number="7" FunctionText="Zurück (RGBW-Zustände wiederherstellen)" ObjectSize="1 Bit" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-1-15" />
             </ComObjectTable>
             <ComObjectRefs>
               <ComObjectRef Id="%AID%_O-%T%100001_R-%T%10000101" RefId="%AID%_O-%T%100001" />
@@ -375,6 +384,8 @@
               <ComObjectRef Id="%AID%_O-%T%100003_R-%T%10000301" RefId="%AID%_O-%T%100003" />
               <ComObjectRef Id="%AID%_O-%T%100004_R-%T%10000401" RefId="%AID%_O-%T%100004" />
               <ComObjectRef Id="%AID%_O-%T%100005_R-%T%10000501" RefId="%AID%_O-%T%100005" />
+              <ComObjectRef Id="%AID%_O-%T%100006_R-%T%10000601" RefId="%AID%_O-%T%100006" />
+              <ComObjectRef Id="%AID%_O-%T%100007_R-%T%10000701" RefId="%AID%_O-%T%100007" />
             </ComObjectRefs>
             <Messages>
               <Message Id="%AID%_M-%TT%00001" Name="dali_error" Text="Dali Error: " />
@@ -417,6 +428,8 @@
                 
                 <ComObjectRefRef RefId="%AID%_O-%T%100001_R-%T%10000101" />
                 <ComObjectRefRef RefId="%AID%_O-%T%100002_R-%T%10000201" />
+                <ComObjectRefRef RefId="%AID%_O-%T%100006_R-%T%10000601" />
+                <ComObjectRefRef RefId="%AID%_O-%T%100007_R-%T%10000701" />
                 <ComObjectRefRef RefId="%AID%_O-%T%100003_R-%T%10000301" />
                 <ComObjectRefRef RefId="%AID%_O-%T%100005_R-%T%10000501" />
 
diff --git a/src/DaliModule.single.templ.xml b/src/DaliModule.single.templ.xml
index 78764c5..06e746e 100644
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
@@ -186,6 +186,12 @@
               <Memory CodeSegment="%AID%_RS-04-00000" Offset="21" BitOffset="4" />
               </Parameter>
               <Parameter Id="%AID%_P-%T%2%CC%131" Name="%C%scene" ParameterType="%AID%_PT-noyes" Text="Szenenobjekt" Value="0" />
+              <Parameter Id="%AID%_P-%T%2%CC%132" Name="%C%rgbwBri" ParameterType="%AID%_PT-rgbwBri" Text="Helligkeit bei RGBW" Value="0">
+              <Memory CodeSegment="%AID%_RS-04-00000" Offset="7" BitOffset="5" />
+              </Parameter>
+              <Parameter Id="%AID%_P-%T%2%CC%133" Name="%C%unlockRestore" ParameterType="%AID%_PT-noyes" Text="Bei Entsperren vorherigen Zustand wiederherstellen" Value="0">
+              <Memory CodeSegment="%AID%_RS-04-00000" Offset="7" BitOffset="6" />
+              </Parameter>
             </Parameters>
             <ParameterRefs>
               <ParameterRef RefId="%AID%_P-%T%2%CC%001" Id="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101" Name="%C%deviceType" />
@@ -309,6 +315,8 @@
               <ParameterRef RefId="%AID%_P-%T%2%CC%129" Id="%AID%_P-%T%2%CC%129_R-%T%2%CC%12901" Name="%C%hcl_auto_off" />
               <ParameterRef RefId="%AID%_P-%T%2%CC%130" Id="%AID%_P-%T%2%CC%130_R-%T%2%CC%13001" Name="%C%hcl_auto_day" />
               <ParameterRef RefId="%AID%_P-%T%2%CC%131" Id="%AID%_P-%T%2%CC%131_R-%T%2%CC%13101" Name="%C%scene" />
+              <ParameterRef RefId="%AID%_P-%T%2%CC%132" Id="%AID%_P-%T%2%CC%132_R-%T%2%CC%13201" Name="%C%rgbwBri" />
+              <ParameterRef RefId="%AID%_P-%T%2%CC%133" Id="%AID%_P-%T%2%CC%133_R-%T%2%CC%13301" Name="%C%unlockRestore" />
             </ParameterRefs>
             <ComObjectTable>
               <ComObject Id="%AID%_O-%T%2%CC%000" Name="%C%switch" Text="A%C-1% {{0}}" Number="%K0%" FunctionText="Schalten" ObjectSize="1 Bit" ReadFlag="Disabled" WriteFlag="Enabled" CommunicationFlag="Enabled" TransmitFlag="Disabled" UpdateFlag="Disabled" ReadOnInitFlag="Disabled" DatapointType="DPST-1-1" />
@@ -363,6 +371,8 @@
               <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00703" FunctionText="Farbtemperatur Status" DatapointType="DPST-7-600" ObjectSize="2 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%006" Id="%AID%_O-%T%2%CC%006_R-%T%2%CC%00604" FunctionText="Farbe xyY / xy" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00704" FunctionText="Farbe xyY / xy Status" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
+              <ComObjectRef RefId="%AID%_O-%T%2%CC%006" Id="%AID%_O-%T%2%CC%006_R-%T%2%CC%00605" FunctionText="Farbe RGBW" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
+              <ComObjectRef RefId="%AID%_O-%T%2%CC%007" Id="%AID%_O-%T%2%CC%007_R-%T%2%CC%00705" FunctionText="Farbe RGBW Status" DatapointType="DPST-251-600" ObjectSize="6 Bytes" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%017" Id="%AID%_O-%T%2%CC%017_R-%T%2%CC%01701" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
               <ComObjectRef RefId="%AID%_O-%T%2%CC%018" Id="%AID%_O-%T%2%CC%018_R-%T%2%CC%01801" TextParameterRefId="%AID%_P-%T%2%CC%002_R-%T%2%CC%00201" />
             </ComObjectRefs>
@@ -453,6 +463,16 @@
                           <ParameterRefRef RefId="%AID%_P-%T%2%CC%067_R-%T%2%CC%06701" />
                         </when>
                       </choose>
+                      <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
+                        <when test="4">
+                          <ParameterRefRef RefId="%AID%_P-%T%2%CC%133_R-%T%2%CC%13301" />
+                          <choose ParamRefId="%AID%_P-%T%2%CC%133_R-%T%2%CC%13301">
+                            <when test="1">
+                              <ParameterSeparator Id="%AID%_PS-nnn" Text="RGBW: Befehle während der Sperre werden gespeichert. Beim Entsperren kommt der gespeicherte Zustand (Ein/Aus, Helligkeit, Farbe); &quot;Verhalten bei Entsperren&quot; gilt dann nicht." UIHint="Information" />
+                            </when>
+                          </choose>
+                        </when>
+                      </choose>
                     </when>
                   </choose>
                   <choose ParamRefId="%AID%_P-%T%2%CC%005_R-%T%2%CC%00501">
@@ -502,6 +522,16 @@
                           <ComObjectRefRef RefId="%AID%_O-%T%2%CC%006_R-%T%2%CC%00604" />
                           <ComObjectRefRef RefId="%AID%_O-%T%2%CC%007_R-%T%2%CC%00704" />
                         </when>
+                        <when test="4">
+                          <ParameterRefRef RefId="%AID%_P-%T%2%CC%132_R-%T%2%CC%13201" />
+                          <choose ParamRefId="%AID%_P-%T%2%CC%132_R-%T%2%CC%13201">
+                            <when test="1">
+                              <ParameterSeparator Id="%AID%_PS-nnn" Text="Der größte Kanal des RGBW-Werts ist die Helligkeit (z. B. Home Assistant ohne Helligkeitsadresse). Das Objekt &quot;Dimmen absolut&quot; stellt den Pegel weiterhin direkt, der RGBW-Status passt sich an." UIHint="Information" />
+                            </when>
+                          </choose>
+                          <ComObjectRefRef RefId="%AID%_O-%T%2%CC%006_R-%T%2%CC%00605" />
+                          <ComObjectRefRef RefId="%AID%_O-%T%2%CC%007_R-%T%2%CC%00705" />
+                        </when>
                       </choose>
                       <choose ParamRefId="%AID%_P-%T%2%CC%074_R-%T%2%CC%07401">
                         <when test="!=0">
@@ -676,7 +706,7 @@
                           <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                             <when test="9">
                               <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                <when test="0 1 3">
+                                <when test="0 1 3 4">
                                   <ParameterSeparator Id="%AID%_PS-nnn" Text="Farbe" UIHint="Headline" Cell="1,4" />
                                 </when>
                                 <when test="2">
@@ -693,7 +723,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%090_R-%T%2%CC%09001" Cell="2,4" />
                                     </when>
                                     <when test="2">
@@ -712,7 +742,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%092_R-%T%2%CC%09201" Cell="3,4" />
                                     </when>
                                     <when test="2">
@@ -731,7 +761,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%094_R-%T%2%CC%09401" Cell="4,4" />
                                     </when>
                                     <when test="2">
@@ -750,7 +780,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%096_R-%T%2%CC%09601" Cell="5,4" />
                                     </when>
                                     <when test="2">
@@ -769,7 +799,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%098_R-%T%2%CC%09801" Cell="6,4" />
                                     </when>
                                     <when test="2">
@@ -788,7 +818,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%100_R-%T%2%CC%10001" Cell="7,4" />
                                     </when>
                                     <when test="2">
@@ -807,7 +837,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%102_R-%T%2%CC%10201" Cell="8,4" />
                                     </when>
                                     <when test="2">
@@ -826,7 +856,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%104_R-%T%2%CC%10401" Cell="9,4" />
                                     </when>
                                     <when test="2">
@@ -845,7 +875,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%106_R-%T%2%CC%10601" Cell="10,4" />
                                     </when>
                                     <when test="2">
@@ -864,7 +894,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%108_R-%T%2%CC%10801" Cell="11,4" />
                                     </when>
                                     <when test="2">
@@ -883,7 +913,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%110_R-%T%2%CC%11001" Cell="12,4" />
                                     </when>
                                     <when test="2">
@@ -902,7 +932,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%112_R-%T%2%CC%11201" Cell="13,4" />
                                     </when>
                                     <when test="2">
@@ -921,7 +951,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%114_R-%T%2%CC%11401" Cell="14,4" />
                                     </when>
                                     <when test="2">
@@ -940,7 +970,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%116_R-%T%2%CC%11601" Cell="15,4" />
                                     </when>
                                     <when test="2">
@@ -959,7 +989,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%118_R-%T%2%CC%11801" Cell="16,4" />
                                     </when>
                                     <when test="2">
@@ -978,7 +1008,7 @@
                               <choose ParamRefId="%AID%_P-%T%2%CC%001_R-%T%2%CC%00101">
                                 <when test="9">
                                   <choose ParamRefId="%AID%_P-%T%2%CC%073_R-%T%2%CC%07301">
-                                    <when test="0 1 3">
+                                    <when test="0 1 3 4">
                                       <ParameterRefRef RefId="%AID%_P-%T%2%CC%120_R-%T%2%CC%12001" Cell="17,4" />
                                     </when>
                                     <when test="2">
diff --git a/src/RgbwHelper.h b/src/RgbwHelper.h
new file mode 100644
index 0000000..b259af8
--- /dev/null
+++ b/src/RgbwHelper.h
@@ -0,0 +1,148 @@
+#pragma once
+#include <stdint.h>
+
+/*
+ * Hilfsfunktionen für KNX DPT 251.600 (DPT_Colour_RGBW) und die Übersetzung auf DALI DT8.
+ *
+ * Telegramm (6 Byte). Die Feldreihenfolge entspricht der Definition in knx_master.xml (MSB zuerst):
+ *   Byte 0: Rot    (0..255, die ETS zeigt 0..100 %)
+ *   Byte 1: Grün
+ *   Byte 2: Blau
+ *   Byte 3: Weiß
+ *   Byte 4: reserviert (0x00)
+ *   Byte 5: Bit 7..4 reserviert, Bit 3 = mR, Bit 2 = mG, Bit 1 = mB, Bit 0 = mW  (1 = Wert gültig)
+ *
+ * Bewusst ohne Abhängigkeit zu Arduino/KNX-Stack, damit die Logik auf dem Rechner testbar ist.
+ */
+namespace RgbwHelper
+{
+    constexpr uint8_t DataLength = 6;
+
+    constexpr uint8_t ValidRed = 0x08;
+    constexpr uint8_t ValidGreen = 0x04;
+    constexpr uint8_t ValidBlue = 0x02;
+    constexpr uint8_t ValidWhite = 0x01;
+    constexpr uint8_t ValidAll = 0x0F;
+
+    /*
+     * Übernimmt nur die im Telegramm als gültig markierten Kanäle nach color[0..3] (R, G, B, W).
+     * Nicht markierte Kanäle behalten ihren bisherigen Wert.
+     * Rückgabe: Gültigkeitsmaske (untere 4 Bit). 0 = das Telegramm ändert nichts.
+     */
+    inline uint8_t decode(const uint8_t *data, uint8_t *color)
+    {
+        const uint8_t valid = data[5] & ValidAll;
+        if (valid & ValidRed)
+            color[0] = data[0];
+        if (valid & ValidGreen)
+            color[1] = data[1];
+        if (valid & ValidBlue)
+            color[2] = data[2];
+        if (valid & ValidWhite)
+            color[3] = data[3];
+        return valid;
+    }
+
+    /*
+     * true, wenn alle vier Kanäle (R, G, B, W) 0 sind. Das gilt als "Aus".
+     */
+    inline bool isOff(const uint8_t *color)
+    {
+        return color[0] == 0 && color[1] == 0 && color[2] == 0 && color[3] == 0;
+    }
+
+    /*
+     * Schreibt color[0..3] als 6-Byte-Telegramm (z.B. für das Statusobjekt). Alle Kanäle gelten als gültig.
+     */
+    inline void encode(const uint8_t *color, uint8_t *data)
+    {
+        data[0] = color[0];
+        data[1] = color[1];
+        data[2] = color[2];
+        data[3] = color[3];
+        data[4] = 0x00;
+        data[5] = ValidAll;
+    }
+
+    /*
+     * DALI-Pegel: 255 bedeutet bei DT8 "MASK" (= Kanal nicht ändern), 254 ist der Maximalwert.
+     */
+    inline uint8_t daliLevel(uint8_t value)
+    {
+        return value == 255 ? 254 : value;
+    }
+
+    /*
+     * Gegenrichtung zu daliLevel(): DALI-Pegel (254 = Maximum) -> KNX-Wert (255 = 100 %).
+     */
+    inline uint8_t fromDaliLevel(uint8_t level)
+    {
+        return level == 254 ? 255 : level;
+    }
+
+    /*
+     * Farbe aus den vier Report-Werten (R, G, B, W) des Treibers (QUERY_COLOUR_VALUE, Selektor 233-236).
+     * Nutzbar nur, wenn kein Wert 255 (MASK = unbekannt) ist und mindestens ein Kanal leuchtet: ein Treiber,
+     * der nur Nullen meldet, hat (noch) keine Farbe, und eine Farbe aus lauter Nullen wäre "aus".
+     * Gibt true zurück und schreibt color[0..3], wenn die Farbe übernommen werden kann.
+     */
+    inline bool colourFromDriver(const uint8_t *report, uint8_t *color)
+    {
+        for (uint8_t i = 0; i < 4; i++)
+            if (report[i] == 255)
+                return false;
+        if (report[0] == 0 && report[1] == 0 && report[2] == 0 && report[3] == 0)
+            return false;
+        for (uint8_t i = 0; i < 4; i++)
+            color[i] = fromDaliLevel(report[i]);
+        return true;
+    }
+
+    /*
+     * Helligkeit aus dem RGBW-Wert ("Slider"-Modus): Der größte Kanal ist die Helligkeit (255 = 100 %), das Verhältnis
+     * der Kanäle zueinander ist die Farbmischung. Der DA4-M wertet die Amplitude der Farbwerte nicht aus (er
+     * normiert), die Helligkeit muss deshalb als eigener Pegel gesendet werden.
+     */
+    inline uint8_t maxChannel(const uint8_t *color)
+    {
+        uint8_t m = color[0];
+        for (uint8_t i = 1; i < 4; i++)
+            if (color[i] > m)
+                m = color[i];
+        return m;
+    }
+
+    /*
+     * Farbmischung: color so skaliert, dass der größte Kanal 255 ist (alle 0 bleibt 0).
+     */
+    inline void normalise(const uint8_t *color, uint8_t *mix)
+    {
+        const uint8_t m = maxChannel(color);
+        for (uint8_t i = 0; i < 4; i++)
+            mix[i] = m == 0 ? 0 : (uint8_t)(((uint16_t)color[i] * 255 + m / 2) / m);
+    }
+
+    /*
+     * Gegenrichtung: Farbmischung (größter Kanal 255) auf die Amplitude amp (= größter Kanal) skalieren.
+     */
+    inline void scale(const uint8_t *mix, uint8_t amp, uint8_t *color)
+    {
+        for (uint8_t i = 0; i < 4; i++)
+            color[i] = (uint8_t)(((uint16_t)mix[i] * amp + 127) / 255);
+    }
+
+    // Amplitude (0..255) <-> Helligkeit in Prozent (0..100, linear)
+    inline float percentFromAmplitude(uint8_t amp)
+    {
+        return amp * 100.0f / 255.0f;
+    }
+
+    inline uint8_t amplitudeFromPercent(float percent)
+    {
+        if (percent <= 0.0f)
+            return 0;
+        if (percent >= 100.0f)
+            return 255;
+        return (uint8_t)(percent * 2.55f + 0.5f);
+    }
+}
diff --git a/test/test_rgbw_helper.cpp b/test/test_rgbw_helper.cpp
new file mode 100644
index 0000000..a9a3018
--- /dev/null
+++ b/test/test_rgbw_helper.cpp
@@ -0,0 +1,136 @@
+#include <stdlib.h>
+// Host test for src/RgbwHelper.h (no Arduino/KNX needed):
+//   g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
+#include <cstdio>
+#include <cstring>
+#include "RgbwHelper.h"
+
+static int failures = 0;
+#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
+
+int main()
+{
+    uint8_t color[4] = {1, 2, 3, 4};
+
+    // all channels valid
+    { uint8_t d[6] = {10, 20, 30, 40, 0, 0x0F};
+      CHECK(RgbwHelper::decode(d, color) == 0x0F);
+      CHECK(color[0] == 10 && color[1] == 20 && color[2] == 30 && color[3] == 40); }
+
+    // only white valid -> RGB untouched
+    { uint8_t d[6] = {99, 99, 99, 200, 0, 0x01};
+      CHECK(RgbwHelper::decode(d, color) == 0x01);
+      CHECK(color[0] == 10 && color[1] == 20 && color[2] == 30 && color[3] == 200); }
+
+    // only red valid
+    { uint8_t d[6] = {77, 99, 99, 99, 0, 0x08};
+      CHECK(RgbwHelper::decode(d, color) == 0x08);
+      CHECK(color[0] == 77 && color[1] == 20 && color[2] == 30 && color[3] == 200); }
+
+    // only green + blue valid
+    { uint8_t d[6] = {99, 5, 6, 99, 0, 0x06};
+      CHECK(RgbwHelper::decode(d, color) == 0x06);
+      CHECK(color[0] == 77 && color[1] == 5 && color[2] == 6 && color[3] == 200); }
+
+    // nothing valid -> nothing changes
+    { uint8_t before[4]; memcpy(before, color, 4);
+      uint8_t d[6] = {1, 1, 1, 1, 0, 0x00};
+      CHECK(RgbwHelper::decode(d, color) == 0);
+      CHECK(memcmp(before, color, 4) == 0); }
+
+    // reserved upper bits of byte 5 are ignored
+    { uint8_t d[6] = {1, 2, 3, 4, 0, 0xF0};
+      CHECK(RgbwHelper::decode(d, color) == 0); }
+
+    // encode
+    { uint8_t c[4] = {1, 2, 3, 4}; uint8_t d[6];
+      RgbwHelper::encode(c, d);
+      const uint8_t exp[6] = {1, 2, 3, 4, 0, 0x0F};
+      CHECK(memcmp(d, exp, 6) == 0); }
+
+    // isOff: only true when R, G, B and W are all 0
+    { uint8_t z[4] = {0, 0, 0, 0}; CHECK(RgbwHelper::isOff(z)); }
+    { uint8_t w[4] = {0, 0, 0, 1}; CHECK(!RgbwHelper::isOff(w)); }
+    { uint8_t r[4] = {1, 0, 0, 0}; CHECK(!RgbwHelper::isOff(r)); }
+
+    // all channels 0 in the telegram -> "off", but a partial 0 (only white) is not "off" while RGB is set
+    { uint8_t c[4] = {10, 0, 0, 50}; uint8_t d[6] = {0, 0, 0, 0, 0, 0x01};
+      RgbwHelper::decode(d, c);
+      CHECK(!RgbwHelper::isOff(c)); }
+    { uint8_t c[4] = {0, 0, 0, 50}; uint8_t d[6] = {0, 0, 0, 0, 0, 0x01};
+      RgbwHelper::decode(d, c);
+      CHECK(RgbwHelper::isOff(c)); }
+
+    // DALI level: 255 is MASK
+    CHECK(RgbwHelper::daliLevel(255) == 254);
+    CHECK(RgbwHelper::daliLevel(254) == 254);
+    CHECK(RgbwHelper::daliLevel(0) == 0);
+    CHECK(RgbwHelper::fromDaliLevel(254) == 255);
+    CHECK(RgbwHelper::fromDaliLevel(128) == 128);
+    CHECK(RgbwHelper::fromDaliLevel(0) == 0);
+
+    // colour from the driver report values (R, G, B, W; 255 = MASK = unknown)
+    { const uint8_t rep[4] = {0, 0, 254, 0}; uint8_t c[4] = {9, 9, 9, 9};
+      CHECK(RgbwHelper::colourFromDriver(rep, c));
+      CHECK(c[0] == 0 && c[1] == 0 && c[2] == 255 && c[3] == 0); }
+    { const uint8_t rep[4] = {254, 0, 0, 254}; uint8_t c[4];
+      CHECK(RgbwHelper::colourFromDriver(rep, c));
+      CHECK(c[0] == 255 && c[1] == 0 && c[2] == 0 && c[3] == 255); }
+    { const uint8_t rep[4] = {0, 0, 0, 0}; uint8_t c[4] = {7, 7, 7, 7};
+      CHECK(!RgbwHelper::colourFromDriver(rep, c));
+      CHECK(c[0] == 7 && c[3] == 7); }                // unchanged
+    { const uint8_t rep[4] = {255, 255, 255, 255}; uint8_t c[4] = {7, 7, 7, 7};
+      CHECK(!RgbwHelper::colourFromDriver(rep, c)); }
+    { const uint8_t rep[4] = {0, 0, 254, 255}; uint8_t c[4];
+      CHECK(!RgbwHelper::colourFromDriver(rep, c)); } // one MASK is enough to reject
+
+    // slider mode: brightness = largest channel, colour = ratio
+    { const uint8_t c[4] = {128, 0, 0, 128}; CHECK(RgbwHelper::maxChannel(c) == 128); }
+    { const uint8_t c[4] = {0, 0, 0, 0};     CHECK(RgbwHelper::maxChannel(c) == 0); }
+    { const uint8_t c[4] = {5, 0, 0, 5}; uint8_t m[4];
+      RgbwHelper::normalise(c, m);
+      CHECK(m[0] == 255 && m[1] == 0 && m[2] == 0 && m[3] == 255); }
+    { const uint8_t c[4] = {241, 0, 13, 241}; uint8_t m[4];
+      RgbwHelper::normalise(c, m);
+      CHECK(m[0] == 255 && m[1] == 0 && m[2] == 14 && m[3] == 255); }
+    { const uint8_t c[4] = {0, 0, 0, 0}; uint8_t m[4] = {9, 9, 9, 9};
+      RgbwHelper::normalise(c, m);
+      CHECK(RgbwHelper::isOff(m)); }
+    { const uint8_t m[4] = {255, 0, 0, 255}; uint8_t c[4];
+      RgbwHelper::scale(m, 128, c);
+      CHECK(c[0] == 128 && c[1] == 0 && c[2] == 0 && c[3] == 128);
+      RgbwHelper::scale(m, 255, c);
+      CHECK(c[0] == 255 && c[3] == 255);
+      RgbwHelper::scale(m, 5, c);
+      CHECK(c[0] == 5 && c[3] == 5); }
+    { const uint8_t m[4] = {255, 102, 0, 51}; uint8_t c[4];
+      RgbwHelper::scale(m, 5, c);
+      CHECK(c[0] == 5 && c[1] == 2 && c[2] == 0 && c[3] == 1); }
+    // round trip colour -> mix -> same amplitude gives the colour back (within 1)
+    for (int a = 1; a < 256; a++)
+    {
+        const uint8_t c[4] = {(uint8_t)a, (uint8_t)(a / 3), 0, (uint8_t)(a / 2)};
+        uint8_t m[4], b[4];
+        RgbwHelper::normalise(c, m);
+        RgbwHelper::scale(m, RgbwHelper::maxChannel(c), b);
+        for (int i = 0; i < 4; i++)
+            CHECK(abs((int)b[i] - (int)c[i]) <= 1);
+        CHECK(RgbwHelper::maxChannel(m) == 255);
+    }
+    CHECK(RgbwHelper::amplitudeFromPercent(0) == 0);
+    CHECK(RgbwHelper::amplitudeFromPercent(100) == 255);
+    CHECK(RgbwHelper::amplitudeFromPercent(50) == 128);
+    CHECK(RgbwHelper::amplitudeFromPercent(0.1f) == 0 || RgbwHelper::amplitudeFromPercent(0.1f) == 1);
+    CHECK(RgbwHelper::percentFromAmplitude(255) > 99.99f && RgbwHelper::percentFromAmplitude(255) < 100.01f);
+    CHECK(RgbwHelper::percentFromAmplitude(0) == 0.0f);
+    for (int a = 0; a < 256; a++)
+        CHECK(RgbwHelper::amplitudeFromPercent(RgbwHelper::percentFromAmplitude(a)) == a);
+
+    // group: decode on an all-0 base gives a complete colour, channels that are not valid stay 0
+    { uint8_t d[6] = {99, 99, 200, 99, 0, 0x02}; uint8_t c[4] = {0, 0, 0, 0};
+      CHECK(RgbwHelper::decode(d, c) == 0x02);
+      CHECK(c[0] == 0 && c[1] == 0 && c[2] == 200 && c[3] == 0); }
+
+    printf(failures ? "%d FAILED\n" : "all tests ok\n", failures);
+    return failures ? 1 : 0;
+}
```
