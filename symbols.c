/* symbols.c - clean linked-list symbol table */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

static SymbolNode *symbol_head = NULL;

/* Initialize symbol table */
void init_symbol_table(void) {
    /* Table is already initialized as NULL */
}

/* Add a symbol to the table */
int add_symbol(const char *name, int value, char attr) {
    SymbolNode *new_node;
    SymbolNode *current;
    
    /* Check if symbol already exists */
    for (current = symbol_head; current != NULL; current = current->next) {
        if (strcmp(current->symbol.name, name) == 0) {
            return 1; /* Symbol already exists - return non-zero for error */
        }
    }
    
    /* Create new symbol node */
    new_node = (SymbolNode *)malloc(sizeof(SymbolNode));
    if (new_node == NULL) {
        return 1; /* Memory allocation failed */
    }
    
    /* Initialize symbol */
    strncpy(new_node->symbol.name, name, 30);
    new_node->symbol.name[30] = '\0';
    new_node->symbol.value = value;
    new_node->symbol.attr = attr;
    
    /* Add to front of list */
    new_node->next = symbol_head;
    symbol_head = new_node;
    
    return 0; /* Success */
}

/* Find a symbol by name */
const Symbol *find_symbol(const char *name) {
    SymbolNode *current;
    
    for (current = symbol_head; current != NULL; current = current->next) {
        if (strcmp(current->symbol.name, name) == 0) {
            return &(current->symbol);
        }
    }
    
    return NULL; /* Not found */
}

/* Relocate data symbols by adding offset to their values */
void relocate_data_symbols(int offset) {
    SymbolNode *p;
    
    for (p = symbol_head; p != NULL; p = p->next) {
        if (p->symbol.attr == 'D') {
            p->symbol.value += offset;
        }
    }
}

/* Mark a symbol as entry (change its attribute to 'R') */
int mark_entry(const char *name) {
    SymbolNode *p;
    for (p = symbol_head; p != NULL; p = p->next) {
        if (strcmp(p->symbol.name, name) == 0) {
            if (p->symbol.attr == 'E') {
                return -2; /* Cannot mark extern as entry */
            }
            p->symbol.attr = 'R';
            return 0; /* Success */
        }
    }
    return -1; /* Symbol not found */
}

/* Free all symbols */
void free_symbol_table(void) {
    SymbolNode *current = symbol_head;
    SymbolNode *next;
    
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    
    symbol_head = NULL;
}

