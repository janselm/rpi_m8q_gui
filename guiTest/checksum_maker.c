#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void calculateUBXChecksum(const uint8_t* msg, uint16_t length, uint8_t* ck_a, uint8_t* ck_b) {
    *ck_a = 0;
    *ck_b = 0;
    for(uint16_t i = 0; i < length; i++) {
        *ck_a = *ck_a + msg[i];
        *ck_b = *ck_b + *ck_a;
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <hex values of UBX message>\n", argv[0]);
        return 1;
    }

    uint16_t length = argc - 1;
    uint8_t msg[length];
    
    for(int i = 1; i < argc; i++) {
        msg[i-1] = (uint8_t)strtol(argv[i], NULL, 16);
    }
    
    uint8_t ck_a, ck_b;
    calculateUBXChecksum(msg, length, &ck_a, &ck_b);
    
    printf("Checksum: CK_A = 0x%02X, CK_B = 0x%02X\n", ck_a, ck_b);

    return 0;
}
