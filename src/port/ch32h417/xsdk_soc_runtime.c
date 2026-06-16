#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0U; i < count; i++)
    {
        d[i] = s[i];
    }

    return dest;
}

void *memset(void *dest, int value, size_t count)
{
    uint8_t *d = (uint8_t *)dest;
    uint8_t byte = (uint8_t)value;

    for (size_t i = 0U; i < count; i++)
    {
        d[i] = byte;
    }

    return dest;
}

size_t strlen(const char *str)
{
    const char *s = str;
    while (*s != '\0')
    {
        s++;
    }
    return (size_t)(s - str);
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0U; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }
    return dest;
}

// EOF /////////////////////////////////////////////////////////////////////////////
