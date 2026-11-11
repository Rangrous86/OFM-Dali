// Host test for src/RgbwHelper.h (no Arduino/KNX needed):
//   g++ -std=c++11 -Wall -Isrc test/test_rgbw_helper.cpp -o /tmp/test_rgbw && /tmp/test_rgbw
#include <cstdio>
#include <cstring>
#include "RgbwHelper.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main()
{
    uint8_t color[4] = {1, 2, 3, 4};

    // all channels valid
    { uint8_t d[6] = {10, 20, 30, 40, 0, 0x0F};
      CHECK(RgbwHelper::decode(d, color) == 0x0F);
      CHECK(color[0] == 10 && color[1] == 20 && color[2] == 30 && color[3] == 40); }

    // only white valid -> RGB untouched
    { uint8_t d[6] = {99, 99, 99, 200, 0, 0x01};
      CHECK(RgbwHelper::decode(d, color) == 0x01);
      CHECK(color[0] == 10 && color[1] == 20 && color[2] == 30 && color[3] == 200); }

    // only red valid
    { uint8_t d[6] = {77, 99, 99, 99, 0, 0x08};
      CHECK(RgbwHelper::decode(d, color) == 0x08);
      CHECK(color[0] == 77 && color[1] == 20 && color[2] == 30 && color[3] == 200); }

    // only green + blue valid
    { uint8_t d[6] = {99, 5, 6, 99, 0, 0x06};
      CHECK(RgbwHelper::decode(d, color) == 0x06);
      CHECK(color[0] == 77 && color[1] == 5 && color[2] == 6 && color[3] == 200); }

    // nothing valid -> nothing changes
    { uint8_t before[4]; memcpy(before, color, 4);
      uint8_t d[6] = {1, 1, 1, 1, 0, 0x00};
      CHECK(RgbwHelper::decode(d, color) == 0);
      CHECK(memcmp(before, color, 4) == 0); }

    // reserved upper bits of byte 5 are ignored
    { uint8_t d[6] = {1, 2, 3, 4, 0, 0xF0};
      CHECK(RgbwHelper::decode(d, color) == 0); }

    // encode
    { uint8_t c[4] = {1, 2, 3, 4}; uint8_t d[6];
      RgbwHelper::encode(c, d);
      const uint8_t exp[6] = {1, 2, 3, 4, 0, 0x0F};
      CHECK(memcmp(d, exp, 6) == 0); }

    // DALI level: 255 is MASK
    CHECK(RgbwHelper::daliLevel(255) == 254);
    CHECK(RgbwHelper::daliLevel(254) == 254);
    CHECK(RgbwHelper::daliLevel(0) == 0);

    printf(failures ? "%d FAILED\n" : "all tests ok\n", failures);
    return failures ? 1 : 0;
}
