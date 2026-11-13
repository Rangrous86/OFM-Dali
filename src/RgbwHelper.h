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
