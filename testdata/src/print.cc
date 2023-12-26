#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main() {
    FILE* r = fopen("bigprof.profraw", "r");

    FILE* w = fopen("bigendian.profout", "wb");

    char buffer[5000];

    size_t numread = fread(buffer, sizeof(unsigned char), 5000, r);
    printf("numread is %zu\n", numread);

    int i = 0;
    for (i = 0; i < numread; i += 2) {
        char hex[5]="0x";
        hex[2] = buffer[i];
        if (i + 1 < numread) {
        hex[3] = buffer[i + 1];
        hex[4] = '\0';
        } else {
            hex[3] = '\0';
        }
        printf("hex string is %s\n", hex);
        int n = strtol(hex, NULL, 16);
        uint8_t val = n;
        printf("Number %o\n", val);
        fwrite(&val, 1, 1, w);
    }
    printf("numread is %zu\n", numread);

    fclose(r);
    fclose(w);

    return 0;
}

