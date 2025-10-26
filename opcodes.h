#ifndef OPCODES_H
#define OPCODES_H

/* bit 0 = mode 0 (#imm) , bit 1 = mode 1 (direct) ,
   bit 2 = mode 2 (&relative) , bit 3 = mode 3 (register) */
typedef struct {
    const char *name;
    int  opcode;
    int  funct;
    unsigned srcMask;   /* allowed modes for source operand */
    unsigned dstMask;   /* allowed modes for dest   operand */
    int  nOperands;     /* 0, 1, or 2 */
} OpInfo;

const OpInfo *find_opcode(const char *name);
#endif

