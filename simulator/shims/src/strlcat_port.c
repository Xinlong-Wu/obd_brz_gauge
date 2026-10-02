/* Simulator shim: strlcpy/strlcat for Linux glibc < 2.38 (ui_theme.c uses
 * strlcat). macOS/BSD libc provides them natively; this TU compiles empty
 * there. On glibc >= 2.38 our strong definition simply wins the link. */
#if !defined(__APPLE__) && !defined(__OpenBSD__) && !defined(__FreeBSD__)

#include <string.h>
#include <stdio.h>
#include <stdint.h>

size_t strlcpy(char *dst, const char *src, size_t dsize)
{
    if (!dst || !src || dsize == 0) {
        if (src) return strlen(src);
        return 0;
    }
    size_t srclen = strlen(src);
    size_t copy = srclen < dsize - 1 ? srclen : dsize - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return srclen;
}

size_t strlcat(char *dst, const char *src, size_t dsize)
{
    if (!dst || !src || dsize == 0) {
        if (src) return strlen(src);
        return 0;
    }
    size_t dlen = strnlen(dst, dsize);
    if (dlen == dsize) return dlen + strlen(src);
    return dlen + strlcpy(dst + dlen, src, dsize - dlen);
}

#endif /* !macOS/BSD */
