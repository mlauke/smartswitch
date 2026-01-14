#pragma once

#include <stdlib.h>

static int cmp_uint16(const void *a, const void *b)
{
    return *(const uint16_t *)a - *(const uint16_t *)b;
}

static uint16_t median_uint16(uint16_t *values, size_t count)
{
    uint16_t median;

    qsort(values, count, sizeof(uint16_t), cmp_uint16);

    if (count & 1) {
        median = values[count / 2];
    } else {
        median = (uint16_t)(
            ((uint16_t)values[count / 2 - 1] +
             (uint16_t)values[count / 2]) / 2
        );
    }
    return median;
}
