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
| `koHandleColor` (nur RGBW) | Ist `currentState` false: `sendArc(Einschaltwert Tag/Nacht bzw. letzter Wert)`, `setDimmState(percentToArc(..))`, `currentState = true`. Die anderen Farbtypen behalten das alte Verhalten. |
| `handleColorRGBW` | Telegramm wird zuerst in eine Kopie dekodiert. Sind danach R, G, B und W alle 0 (`RgbwHelper::isOff`), wird `sendArc(0)` gesendet, `setDimmState(0,…)` setzt *Schalten Status* = Aus und *Dimmen Status* = 0 %, `currentState = false`. Die Farbe 0/0/0/0 geht **nicht** an den Treiber, die gespeicherte Farbe bleibt erhalten. |
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

```diff
diff --git a/src/DaliChannel.cpp b/src/DaliChannel.cpp
index dffeaaf..7fa4c5d 100644
--- a/src/DaliChannel.cpp
+++ b/src/DaliChannel.cpp
@@ -1,4 +1,5 @@
 #include "DaliChannel.h"
+#include "RgbwHelper.h"
 #include "OpenKNX.h"
 
 DaliChannel::DaliChannel(Dali::Master &_daliMaster) : daliMaster(_daliMaster) {}
@@ -635,6 +636,9 @@ void DaliChannel::koHandleDimmAbs(GroupObject &ko)
     uint8_t arc = DaliHelper::percentToArc(value);
     daliMaster.sendArc(_channelIndex, arc, _isGroup);
     setDimmState(arc, true, true);
+    // keep the on/off flag in sync, otherwise e.g. a colour telegram after "dimm to 0" is ignored
+    if (!_isStaircase)
+        currentState = arc > 0;
 }
 
 void DaliChannel::koHandleLock(GroupObject &ko)
@@ -738,11 +742,32 @@ void DaliChannel::koHandleColor(GroupObject &ko)
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
+            daliMaster.sendArc(_channelIndex, onValue, _isGroup);
+            setDimmState(DaliHelper::percentToArc(onValue), true, true);
+            currentState = true;
+        }
+        else
+            setDimmState(_onDay, true, true); // TODO get real
+    }
+
     logDebugP("AutoConf %i %i %i", _isGroup, _isGroup ? ParamDGW_hcl_manu_col : ParamDGWG_hcl_manu_col, _hclIsAutoMode);
 }
 
@@ -780,6 +805,62 @@ void DaliChannel::handleColorRGB(GroupObject &ko)
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
+        daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
+        setDimmState(0, true, true);
+        currentState = false;
+        return false; // already handled, do not switch on again
+    }
+
+    memcpy(currentColor, newColor, sizeof(currentColor));
+    sendRgbwState();
+    sendColor();
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
@@ -872,6 +953,13 @@ void DaliChannel::sendColor()
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
@@ -905,6 +993,30 @@ void DaliChannel::sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b)
     daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
 }
 
+void DaliChannel::sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
+{
+    r = RgbwHelper::daliLevel(r);
+    g = RgbwHelper::daliLevel(g);
+    b = RgbwHelper::daliLevel(b);
+    w = RgbwHelper::daliLevel(w);
+    logDebugP("Send RGBW: %i %i %i %i", r, g, b, w);
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
@@ -1002,6 +1114,12 @@ void DaliChannel::updateCurrentDimmValue()
 
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
index 19b49c9..d5154f5 100644
--- a/src/DaliChannel.h
+++ b/src/DaliChannel.h
@@ -106,8 +106,8 @@ class DaliChannel : public OpenKNX::Channel
 		uint8_t currentStep = 0;
 		//Current lock state
 		bool currentIsLocked = false;
-		//Current color
-		uint8_t currentColor[4];
+		//Current color (R, G, B, W). With xyY the 4 bytes are used as two 16 bit values.
+		uint8_t currentColor[4] = {0, 0, 0, 0};
 
 		//Querying current state
 		uint32_t _queryId = 0;
@@ -131,6 +131,8 @@ class DaliChannel : public OpenKNX::Channel
 		void updateCurrentDimmValue();
 		void sendColor();
 		void sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b);
+		void sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
+		void sendRgbwState();
 		void sendColorAsXY(uint8_t r, uint8_t g, uint8_t b);
 		void sendKoStateOnChange(uint16_t koNr, const KNXValue &value, const Dpt &type);
 		void setTemperature(uint16_t value);
@@ -148,6 +150,7 @@ class DaliChannel : public OpenKNX::Channel
 		void handleColorRGB(GroupObject &ko);
 		void handleColorTW(GroupObject &ko);
 		void handleColorXYY(GroupObject &ko);
+		bool handleColorRGBW(GroupObject &ko);
 		void koHandleColorRel(GroupObject &ko, ColorChannel channel);
 		void koHandleColorAbs(GroupObject &ko, ColorChannel channel);
 		void koHandleHclCurve(GroupObject &ko);
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
