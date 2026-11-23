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
    inline uint8_t knxFromDaliLevel(uint8_t level)
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
            color[i] = knxFromDaliLevel(report[i]);
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
