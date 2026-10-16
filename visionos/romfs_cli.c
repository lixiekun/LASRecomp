// Build the same private data image as the desktop setup, on the development Mac.
#include <stdio.h>
#include "romfs_build.h"
int main(int argc, char **argv) {
    char why[256];
    if (argc != 3) { fprintf(stderr, "usage: romfs-build <romfs-directory> <output-image>\n"); return 2; }
    if (!romfs_build(argv[1], argv[2], NULL, why, sizeof why)) {
        fprintf(stderr, "%s\n", why); return 1;
    }
    return 0;
}
