#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct { uint8_t r, g, b, a; } Pixel;

static void rect(Pixel *pixels, int size, int x, int y, int w, int h, Pixel color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > size) w = size - x;
    if (y + h > size) h = size - y;
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            pixels[(size_t)row * size + col] = color;
}

static void glyph(Pixel *pixels, int size, int x, int y, int scale, const char rows[5][4], Pixel color) {
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 3; col++)
            if (rows[row][col] == '1') rect(pixels, size, x + col * scale, y + row * scale, scale, scale, color);
}

static int write_chunk(FILE *file, const char type[4], const uint8_t *data, uint32_t length) {
    uint8_t header[4] = { (uint8_t)(length >> 24), (uint8_t)(length >> 16), (uint8_t)(length >> 8), (uint8_t)length };
    if (fwrite(header, 1, 4, file) != 4 || fwrite(type, 1, 4, file) != 4) return 0;
    if (length && fwrite(data, 1, length, file) != length) return 0;
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (length) crc = crc32(crc, data, length);
    uint8_t crcBytes[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc };
    return fwrite(crcBytes, 1, 4, file) == 4;
}

static int write_icon(const char *path, int size) {
    const Pixel bg = { 7, 16, 13, 255 };
    const Pixel screen = { 12, 28, 22, 255 };
    const Pixel green = { 91, 255, 145, 255 };
    const Pixel dim = { 35, 91, 64, 255 };
    const Pixel cyan = { 59, 223, 211, 255 };
    const Pixel amber = { 255, 195, 79, 255 };
    Pixel *pixels = malloc((size_t)size * size * sizeof(Pixel));
    if (!pixels) return 0;
    for (int i = 0; i < size * size; i++) pixels[i] = bg;

    int unit = size / 64;
    if (unit < 1) unit = 1;
    int margin = 5 * unit;
    rect(pixels, size, margin, margin, size - 2 * margin, 2 * unit, green);
    rect(pixels, size, margin, size - margin - 2 * unit, size - 2 * margin, 2 * unit, green);
    rect(pixels, size, margin, margin, 2 * unit, size - 2 * margin, green);
    rect(pixels, size, size - margin - 2 * unit, margin, 2 * unit, size - 2 * margin, green);

    int inset = 11 * unit;
    rect(pixels, size, inset, inset, size - 2 * inset, size - 2 * inset, screen);
    rect(pixels, size, inset, inset, size - 2 * inset, 3 * unit, dim);
    for (int dot = 0; dot < 3; dot++)
        rect(pixels, size, inset + (5 + dot * 4) * unit, inset + unit, 2 * unit, unit,
             dot == 0 ? green : (dot == 1 ? cyan : amber));

    int scale = size / 23;
    if (scale < 2) scale = 2;
    int textWidth = 16 * scale;
    int x = (size - textWidth) / 2;
    int y = size / 2 - (5 * scale) / 2 - 2 * unit;
    static const char I[5][4] = { "111", "010", "010", "010", "111" };
    static const char T[5][4] = { "111", "010", "010", "010", "010" };
    static const char O[5][4] = { "111", "101", "101", "101", "111" };
    static const char P[5][4] = { "110", "101", "110", "100", "100" };
    glyph(pixels, size, x, y, scale, I, green);
    glyph(pixels, size, x + 4 * scale, y, scale, T, green);
    glyph(pixels, size, x + 8 * scale, y, scale, O, green);
    glyph(pixels, size, x + 12 * scale, y, scale, P, green);

    int meterX = inset + 5 * unit;
    int meterW = size - 2 * inset - 10 * unit;
    int meterY = size - inset - 10 * unit;
    rect(pixels, size, meterX, meterY, meterW, 2 * unit, dim);
    rect(pixels, size, meterX, meterY, meterW * 3 / 5, 2 * unit, green);
    rect(pixels, size, meterX, meterY + 4 * unit, meterW, 2 * unit, dim);
    rect(pixels, size, meterX, meterY + 4 * unit, meterW * 2 / 5, 2 * unit, cyan);

    size_t rowBytes = 1 + (size_t)size * 4;
    size_t rawLength = rowBytes * (size_t)size;
    uint8_t *raw = malloc(rawLength);
    if (!raw) { free(pixels); return 0; }
    for (int row = 0; row < size; row++) {
        uint8_t *line = raw + (size_t)row * rowBytes;
        line[0] = 0;
        memcpy(line + 1, pixels + (size_t)row * size, (size_t)size * 4);
    }

    uLongf compressedLength = compressBound((uLong)rawLength);
    uint8_t *compressed = malloc(compressedLength);
    if (!compressed || compress2(compressed, &compressedLength, raw, (uLong)rawLength, Z_BEST_COMPRESSION) != Z_OK) {
        free(compressed); free(raw); free(pixels); return 0;
    }
    FILE *file = fopen(path, "wb");
    if (!file) { free(compressed); free(raw); free(pixels); return 0; }
    const uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    uint8_t ihdr[13] = {
        (uint8_t)(size >> 24), (uint8_t)(size >> 16), (uint8_t)(size >> 8), (uint8_t)size,
        (uint8_t)(size >> 24), (uint8_t)(size >> 16), (uint8_t)(size >> 8), (uint8_t)size,
        8, 6, 0, 0, 0
    };
    int okay = fwrite(signature, 1, sizeof(signature), file) == sizeof(signature) &&
               write_chunk(file, "IHDR", ihdr, sizeof(ihdr)) &&
               write_chunk(file, "IDAT", compressed, (uint32_t)compressedLength) &&
               write_chunk(file, "IEND", NULL, 0);
    fclose(file);
    free(compressed); free(raw); free(pixels);
    return okay;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: make_icon APP_BUNDLE_PATH\n");
        return 2;
    }
    struct { int size; const char *name; } icons[] = {
        { 120, "Icon-60@2x.png" }, { 180, "Icon-60@3x.png" }, { 152, "Icon-76@2x.png" }
    };
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], icons[i].name);
        if (!write_icon(path, icons[i].size)) {
            fprintf(stderr, "could not write %s\n", path);
            return 1;
        }
    }
    return 0;
}
