#pragma once

#include <stdlib.h>
#include <stdint.h>

static int cmp_uint16(const void *a, const void *b)
{
    return *(const uint16_t *)a - *(const uint16_t *)b;
}

static uint16_t median_uint16(uint16_t *values, size_t count)
{
    uint16_t median;

    qsort(values, count, sizeof(uint16_t), cmp_uint16);

    if (count & 1)
    {
        median = values[count / 2];
    }
    else
    {
        median = (uint16_t)(((uint16_t)values[count / 2 - 1] + (uint16_t)values[count / 2]) / 2);
    }
    return median;
}

enum ArgType
{
    ARG_INT,
    ARG_FLT,
    ARG_STR,
};

struct Arg
{
    enum ArgType type;
    void *value;
};

// esp8266 does not support positional printf
static void format_indexed(char *out, size_t out_size,
                           const char *fmt,
                           struct Arg *args)
{
    char *dst = out;
    const char *p = fmt;

    while (*p && (dst - out) < (int)(out_size - 1))
    {

        if (*p == '%' && p[1] >= '0' && p[1] <= '9')
        {
            int idx = p[1] - '0';
            struct Arg *a = &args[idx];

            switch (a->type)
            {
            case ARG_INT:
                dst += snprintf(dst, out_size - (dst - out), "%d", *(uint16_t *)a->value);
                break;
            case ARG_FLT:
                dst += snprintf(dst, out_size - (dst - out), "%.2f", *(float *)a->value);
                break;
            case ARG_STR:
                dst += snprintf(dst, out_size - (dst - out), "%s", (const char *)a->value);
                break;
            }
            p += 2;
        }
        else
        {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
}
