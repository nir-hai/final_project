#include "opcodes.h"
#include <string.h>

/* Create bitmask for allowed addressing modes */
#define MAKE_ADDR_MASK(imm, dir, rel, reg) \
    ((imm)<<0 | (dir)<<1 | (rel)<<2 | (reg)<<3)
    /* bit 0: immediate (#5)
       bit 1: direct (label)  
       bit 2: relative (%label)
       bit 3: register (r1) */

static const OpInfo opcode_table[] = {
  { "mov",  0, 0, MAKE_ADDR_MASK(1,1,0,1), MAKE_ADDR_MASK(0,1,0,1), 2 },
  { "cmp",  1, 0, MAKE_ADDR_MASK(1,1,0,1), MAKE_ADDR_MASK(1,1,0,1), 2 },
  { "add",  2, 1, MAKE_ADDR_MASK(1,1,0,1), MAKE_ADDR_MASK(0,1,0,1), 2 },
  { "sub",  2, 2, MAKE_ADDR_MASK(1,1,0,1), MAKE_ADDR_MASK(0,1,0,1), 2 },
  { "lea",  4, 0, MAKE_ADDR_MASK(0,1,0,0), MAKE_ADDR_MASK(0,1,0,1), 2 },
  { "clr",  5, 1, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,0,1), 1 },
  { "not",  5, 2, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,0,1), 1 },
  { "inc",  5, 3, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,0,1), 1 },
  { "dec",  5, 4, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,0,1), 1 },
  { "jmp",  9, 1, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,1,0), 1 },
  { "bne",  9, 2, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,1,0), 1 },
  { "jsr",  9, 3, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,1,0), 1 },
  { "red", 12, 0, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,1,0,1), 1 },
  { "prn", 13, 0, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(1,1,0,1), 1 },
  { "rts", 14, 0, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,0,0,0), 0 },
  { "stop",15, 0, MAKE_ADDR_MASK(0,0,0,0), MAKE_ADDR_MASK(0,0,0,0), 0 }
};

const OpInfo *find_opcode(const char *name) {
    size_t i, n = sizeof(opcode_table)/sizeof(opcode_table[0]);
    for (i = 0; i < n; ++i)
        if (strcmp(opcode_table[i].name, name) == 0)
            return &opcode_table[i];
    return NULL;
}
