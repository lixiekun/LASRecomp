#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "bc_decode.h"
int main(void) {
    // Two BC1 blocks: solid red and solid green. Clip a 5x3 image across the second block.
    const unsigned char blocks[16] = {0x00,0xf8,0,0,0,0,0,0, 0xe0,0x07,0,0,0,0,0,0};
    unsigned char *p; size_t row;
    assert(bc_decode_image(0x41, blocks, 5, 3, 1, &p, &row)); assert(row == 20);
    for (int y=0;y<3;y++) {
        assert(p[y*row]==255 && p[y*row+1]==0 && p[y*row+2]==0 && p[y*row+3]==255);
        assert(p[y*row+16]==0 && p[y*row+17]==255 && p[y*row+18]==0 && p[y*row+19]==255);
    }
    free(p);
    // Signed BC5 endpoint -127 / +127 must preserve negative normal components.
    unsigned char signed_block[16] = {0x81,0x7f,0,0,0,0,0,0, 0x7f,0x81,0,0,0,0,0,0};
    assert(bc_decode_image(0x4c, signed_block, 1, 1, 1, &p, &row));
    assert(row==2 && (signed char)p[0]==-127 && (signed char)p[1]==127);free(p);
    assert(!bc_decode_image(0,blocks,1,1,1,&p,&row));
    assert(!bc_decode_image(0x41,blocks,0,1,1,&p,&row));
    return 0;
}
