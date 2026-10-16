#include "bc_decode.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define BCDEC_IMPLEMENTATION
#define BCDEC_BC4BC5_PRECISE
#include "../../third_party/bcdec.h"

bool bc_decode_image(uint32_t f, const uint8_t *src, uint32_t w, uint32_t h,
                     uint32_t d, uint8_t **pixels, size_t *row_bytes) {
    if (f < 0x41 || f > 0x50 || !src || !w || !h || !d || !pixels || !row_bytes) return false;
    unsigned bytes = f == 0x49 || f == 0x4A ? 1 : f == 0x4B || f == 0x4C ? 2 : f >= 0x4F ? 8 : 4;
    unsigned block_bytes = f == 0x41 || f == 0x42 || f == 0x45 || f == 0x46 || f == 0x49 || f == 0x4A ? 8 : 16;
    size_t row = (size_t)w * bytes;
    if (row / bytes != w || row > SIZE_MAX / h || row * h > SIZE_MAX / d) return false;
    uint8_t *out = malloc(row * h * d);
    if (!out) return false;
    uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
    for (uint32_t z = 0; z < d; z++) for (uint32_t y = 0; y < bh; y++) for (uint32_t x = 0; x < bw; x++) {
        const uint8_t *block = src + (((size_t)z * bh + y) * bw + x) * block_bytes;
        uint16_t texels16[64] = {0};
        uint8_t *texels = (uint8_t *)texels16;
        switch (f) {
            case 0x41: case 0x42: case 0x45: case 0x46: bcdec_bc1(block, texels, 16); break;
            case 0x43: case 0x47: bcdec_bc2(block, texels, 16); break;
            case 0x44: case 0x48: bcdec_bc3(block, texels, 16); break;
            case 0x49: case 0x4A: bcdec_bc4(block, texels, 4, f == 0x4A); break;
            case 0x4B: case 0x4C: bcdec_bc5(block, texels, 8, f == 0x4C); break;
            case 0x4D: case 0x4E: bcdec_bc7(block, texels, 16); break;
            case 0x4F: case 0x50: {
                uint16_t rgb[48];
                bcdec_bc6h_half(block, rgb, 12, f == 0x4F);
                for (int i = 0; i < 16; i++) {
                    memcpy(texels16 + i * 4, rgb + i * 3, 6);
                    texels16[i * 4 + 3] = 0x3C00; // half-float alpha 1
                }
                break;
            }
        }
        unsigned width = w - x * 4 < 4 ? w - x * 4 : 4;
        unsigned height = h - y * 4 < 4 ? h - y * 4 : 4;
        for (unsigned j = 0; j < height; j++)
            memcpy(out + ((size_t)z * h + y * 4 + j) * row + x * 4 * bytes,
                   texels + j * 4 * bytes, width * bytes);
    }
    *pixels = out; *row_bytes = row;
    return true;
}
