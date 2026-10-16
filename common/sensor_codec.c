#include "sensor_codec.h"

int ze03_decode(const uint8_t *f, size_t size, uint8_t *gas, uint16_t *ppm100)
{
    if (!f || size != 9 || f[0] != 0xff || f[1] != 0x86 ||
        (f[4] != 0x02 && f[4] != 0x03) || f[5] > 2)
        return 0;
    uint8_t sum = 0;
    for (unsigned i = 1; i < 9; ++i) sum += f[i];
    if (sum) return 0;
    uint32_t value = ((uint32_t)f[2] << 8) | f[3];
    value *= f[5] == 0 ? 100 : f[5] == 1 ? 10 : 1;
    if (value > 65535) return 0;
    *gas = f[4];
    *ppm100 = (uint16_t)value;
    return 1;
}

int zs03_decode(const uint8_t f[5], int16_t *temperature10, uint16_t *humidity10)
{
    if ((uint8_t)(f[0] + f[1] + f[2] + f[3]) != f[4] ||
        f[1] != 0 || (f[3] & 0x7f) != 0)
        return 0;
    int temperature = (f[3] & 0x80) ? -(int)f[2] : f[2];
    if (temperature < -20 || temperature > 60) return 0;
    if (temperature >= 0 && (f[0] < 20 || f[0] > 90)) return 0;
    *temperature10 = (int16_t)(temperature * 10);
    *humidity10 = (uint16_t)(f[0] * 10);
    return 1;
}
