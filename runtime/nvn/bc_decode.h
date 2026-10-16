#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Linear BC blocks -> tightly packed texels. Caller owns *pixels on success.
bool bc_decode_image(uint32_t format, const uint8_t *blocks, uint32_t w, uint32_t h,
                     uint32_t depth, uint8_t **pixels, size_t *row_bytes);
