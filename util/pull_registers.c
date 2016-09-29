/*
This program reads in the .bin file to mem[] and prints the result.
It also attempts to pull the register from the instruction.
*/
#include <stdio.h>

#define MAXMEMORY 65536 /* maximum number of data words in memory */
static FILE *bin_file;

#define FIELD_OPCODE(instr) ((unsigned int)(((instr)>>26)&0x003F))
#define FIELD_FUNC(instr)   ((unsigned int)((instr)&0x07FF))
#define FIELD_R1(instr)     ((unsigned int)(((instr)>>21)&0x001F))
#define FIELD_R2(instr)     ((unsigned int)(((instr)>>16)&0x001F))
#define FIELD_R3(instr)     ((unsigned int)(((instr)>>11)&0x001F))
#define FIELD_IMM(instr)    ((signed short)((instr)&0xFFFF))
#define FIELD_IMMU(instr)   ((unsigned short)((instr)&0xFFFF))
#define FIELD_OFFSET(instr) ((signed int)(((instr)&0x02000000)?((instr)|0xFC000000):((instr)&0x03FFFFFF)))


/* Overall processor state */
typedef struct _state_t {
  /* memory */
  unsigned char mem[MAXMEMORY];
} state_t;

typedef struct _op_info_t {
  const char *name;
  const int fu_group_num;
  const int operation;
  const int data_type;
} op_info_t;

/* union to handle multiple fixed-point types */
typedef union _int_t {
  signed long w;
  unsigned long wu;
} int_t;

typedef union _operand_t {
  int_t integer;
  float flt;
} operand_t;


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
    int j = 0;
    int instr = (state->mem[j]<<24)|(state->mem[j+1]<<16)|(state->mem[j+2]<<8)|(state->mem[j+3]);
    
    printf("0x%x\n",(unsigned int)instr);
    printf("Data Count: %u\n", data_count);
    printf("R1: %u\n", FIELD_R1(instr));
    printf("R2: %u\n", FIELD_R2(instr));
    printf("R3: %u\n", FIELD_R3(instr));
    printf("FIELD_OPCODE: %u\n", FIELD_OPCODE(instr));
    printf("FIELD_FUNC: %u\n", FIELD_FUNC(instr));
    printf("FIELD_IMM: %d\n", FIELD_IMM(instr));
    printf("FIELD_IMMU: %u\n", FIELD_IMMU(instr));
    printf("FIELD_OFFSET: %x\n", FIELD_OFFSET(instr));

    return 0;
}
