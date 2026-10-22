/* Small file adapter around Torch's standalone VPK0 decoder. */
#include "vpk0.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    FILE *in = fopen(argv[1], "rb");
    if (!in) return 3;
    if (fseek(in, 0, SEEK_END)) return 4;
    long size = ftell(in);
    if (size < 8 || size > 16777216) return 4;
    rewind(in);
    unsigned char *src = malloc((size_t)size);
    if (!src || fread(src, 1, (size_t)size, in) != (size_t)size) return 5;
    fclose(in);
    uint32_t decoded = vpk0_decoded_size(src, (size_t)size);
    if (!decoded || decoded > 16 * 1024 * 1024) return 6;
    unsigned char *dst = malloc(decoded);
    if (!dst || vpk0_decode(src, (size_t)size, dst, decoded) != decoded) return 7;
    FILE *out = fopen(argv[2], "wb");
    if (!out) return 8;
    int ok = fwrite(dst, 1, decoded, out) == decoded;
    if (fclose(out)) ok = 0;
    free(src);
    free(dst);
    return ok ? 0 : 9;
}
