/*This program reads in the .bin file to mem[] and prints the result*/
#include <stdio.h>

#define MAXMEMORY 65536 /* maximum number of data words in memory */
static FILE *bin_file;

/* Overall processor state */
typedef struct _state_t {
  /* memory */
  unsigned char mem[MAXMEMORY];
} state_t;

int main()
{
    printf("Hello, World!\n");
    
    state_t *state;
    int data_count;
    
    bin_file = fopen("simple.bin","r");
    
    /* read machine-code file into instruction/data memory (starting at address 0) */
    int i=0;
    while (!feof(bin_file)) {
        if (fread(&state->mem[i], 1, 1, bin_file) != 0) {
            i++;
        } else if (!feof(bin_file)) {
            printf("error: cannot read address 0x%X from binary file\n", i);
         }
    }
    data_count = i;
    
    for (i=0;i<data_count;i++) {
        printf("%u\n", state->mem[i]);
    }

    return 0;
}

