#ifndef SYMBOLS_H
#define SYMBOLS_H

typedef struct {
    char name[31];
    int  value;
    char attr;      /* 'C' = code, 'D' = data, 'E' = external, 'R' = relocatable entry */
} Symbol;

typedef struct SymbolNode {
    Symbol symbol;
    struct SymbolNode *next;
} SymbolNode;

void init_symbol_table(void);
int add_symbol(const char *name, int value, char attr);
const Symbol *find_symbol(const char *name);
void relocate_data_symbols(int offset);
int mark_entry(const char *name);
void free_symbol_table(void);

#endif

