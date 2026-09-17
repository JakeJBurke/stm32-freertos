#include <stdint.h>

void *memset(void *s, int c, uint32_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s2 = (const uint8_t *)src;
    while (n--) *d++ = *s2++;
    return dst;
}
