/* second_pass.c - handles .entry and patches all placeholders,
 * then writes .ob and .ext files when assembly succeeds.
 * -------------------------------------------------------------- */

#include "symbols.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>


/* Word type matching first_pass.c */
typedef unsigned long Word;
#define WORD_MASK 0xFFFFFFul

/* ARE bit definitions */
#define ARE_A    4                /* ARE bits = 100 */
#define ARE_R    2                /* ARE bits = 010 */
#define ARE_E    1                /* ARE bits = 001 */

/* ---- data exported by first_pass ---- */
extern Word code[];   extern int cw;
extern Word data[];   extern int dw;

typedef struct { 
    int wordIndex, instrIC, mode; 
    char label[31]; 
} Placeholder;
extern Placeholder placeholders[];  
extern int n_placeholders;

/* ---- collect extern references ---- */
typedef struct { 
    char name[31]; 
    int addr; 
} ExtRef;
static ExtRef ext_refs[1000];  
static int n_ext = 0;

/* ---- collect entry symbols while scanning .entry ---- */
typedef struct { 
    char name[31]; 
    int value; 
} Entry;
static Entry entries[1000]; 
static int n_ent = 0;

/* ---- Global error counter ---- */
static int second_pass_errors = 0;

/* ---- Error counter function ---- */
int get_second_pass_errors(void) {
    return second_pass_errors;
}

/* helper: skip leading label */
static const char *after_label(const char *s)
{
    const char *p = strchr(s, ':'); 
    if (!p) return s;
    for (++p; *p && isspace((unsigned char)*p); ++p) ; 
    return p;
}

/* write object file */
static void write_ob(const char *base)
{
    char fn[260]; 
    FILE *f;
    int addr;
    int i;
    
    sprintf(fn, "%s.ob", base);
    f = fopen(fn, "w"); 
    if (!f) {
        perror(fn);
        return;
    }

    fprintf(f, "%d %d\n", cw, dw);

    addr = 100;
    for (i = 0; i < cw; ++i, ++addr)
        fprintf(f, "%07d %06lx\n", addr, code[i] & WORD_MASK);
    for (i = 0; i < dw; ++i, ++addr)
        fprintf(f, "%07d %06lx\n", addr, data[i] & WORD_MASK);
    fclose(f);
}

/* write ext file */
static void write_ext(const char *base)
{
    char fn[260]; 
    FILE *f;
    int i;
    
    if (n_ext == 0) return;
    sprintf(fn, "%s.ext", base);
    f = fopen(fn, "w"); 
    if (!f) {
        perror(fn);
        return;
    }
    for (i = 0; i < n_ext; ++i)
        fprintf(f, "%s %07d\n", ext_refs[i].name, ext_refs[i].addr);
    fclose(f);
}

/* write ent file */
static void write_ent(const char *base)
{
    char fn[260]; 
    FILE *f;
    int i;
    
    if (n_ent == 0) return;
    sprintf(fn, "%s.ent", base);
    f = fopen(fn, "w"); 
    if (!f) {
        perror(fn);
        return;
    }
    for (i = 0; i < n_ent; ++i)
        fprintf(f, "%s %07d\n", entries[i].name, entries[i].value);
    fclose(f);
}

extern int get_first_pass_errors(void);

void second_pass(const char *am_path)
{
    FILE *src;
    char line[256]; 
    int ln = 0;
    const char *body;
    const char *p;
    char name[31];
    size_t l;
    int rc;
    const Symbol *s;
    int i;
    const Placeholder *ph;
    const Symbol *sym;
    int off;
    char base[260];
    char *dot;
    
    /* Reset error counter for this file */
    second_pass_errors = 0;
    
    /* Don't proceed if first pass had errors */
    if (get_first_pass_errors() > 0) {
        printf("Second pass skipped due to first pass errors.\n");
        return;
    }
    
    /* Reset counters for this file */
    n_ext = 0;
    n_ent = 0;
    
    /* -------- scan .am file for .entry ------------------ */
    src = fopen(am_path, "r"); 
    if (!src) {
        perror(am_path);
        return;
    }

    while (fgets(line, sizeof line, src)) {
        ++ln;
        body = after_label(line);
        while (*body && isspace((unsigned char)*body)) ++body;

        if (*body == '.' && (strncmp(body, ".data", 5) == 0 ||
                          strncmp(body, ".string", 7) == 0 ||
                          strncmp(body, ".extern", 7) == 0))
            continue;

        if (strncmp(body, ".entry", 6) == 0) {
            p = body + 6; 
            while (*p && isspace((unsigned char)*p)) ++p;

            if (*p == '\0') {
                printf("Error: missing name after .entry (l%d)\n", ln);
                second_pass_errors++;
                continue;
            }
            if (!isalpha((unsigned char)*p)) {
                printf("Error: invalid entry name (must start with a letter) (l%d)\n", ln);
                second_pass_errors++;
                continue;
            }

            /* read letters/digits only (NO underscore), up to 30 */
            l = 1;
            while (l < 30 && isalnum((unsigned char)p[l])) ++l;

            if (l == 30 && isalnum((unsigned char)p[l])) {
                printf("Error: entry name too long (max 30) (l%d)\n", ln);
                second_pass_errors++;
                continue;
            }

            strncpy(name, p, l);
            name[l] = '\0';

            /* no trailing tokens (also catches commas) */
            p += l;
            while (*p && isspace((unsigned char)*p)) ++p;
            if (*p != '\0') {
                printf("Error: '.entry' takes exactly one symbol (letters/digits only) (l%d)\n", ln);
                second_pass_errors++;
                continue;
            }

            rc = mark_entry(name);
            if (rc == -1) {
                printf( "Error: undefined entry \"%s\" (l%d)\n", name, ln);
                second_pass_errors++;
            } else if (rc == -2) {
                printf( "Error: extern \"%s\" cannot be entry (l%d)\n", name, ln);
                second_pass_errors++;
            } else {
                s = find_symbol(name);
                if (s && n_ent < 1000) {
                    strncpy(entries[n_ent].name, name, 30);
                    entries[n_ent].name[30] = '\0';
                    entries[n_ent].value = s->value;
                    n_ent++;
                }
            }
        }
    }
    fclose(src);
    
    /* -------- patch placeholders ------------------------ */
    for (i = 0; i < n_placeholders; ++i) {
        ph = &placeholders[i];
        
        sym = find_symbol(ph->label);
        if (!sym) {
            printf( "Error: undefined symbol \"%s\"\n", ph->label);
            second_pass_errors++;
            continue;
        }

        if (ph->mode == 1) {            /* DIRECT */
            if (sym->attr == 'E') {
                code[ph->wordIndex] = ARE_E & WORD_MASK;
                if (n_ext < 1000) {
                    strncpy(ext_refs[n_ext].name, sym->name, 30);
                    ext_refs[n_ext].name[30] = '\0';
                    ext_refs[n_ext].addr = 100 + ph->wordIndex;
                    n_ext++;
                }
            } else {
                code[ph->wordIndex] = (((Word)(sym->value & 0x1FFFFF) << 3) | ARE_R) & WORD_MASK;
            }
        } else if (ph->mode == 2) {      /* RELATIVE */
            if (sym->attr == 'E') {
                printf( "Error: extern \"%s\" used with '&'\n", ph->label);
                second_pass_errors++;
                continue;
            }
            off = sym->value - ph->instrIC;
            code[ph->wordIndex] = (((Word)(off & 0x1FFFFF) << 3) | ARE_A) & WORD_MASK;
        }
    }

    /* -------- write output files if no errors ----------- */
    if (second_pass_errors == 0) {
        /* get base name (strip trailing .am) */
        strncpy(base, am_path, sizeof base);
        base[sizeof base - 1] = '\0';
        dot = strrchr(base, '.'); 
        if (dot && strcmp(dot, ".am") == 0) 
            *dot = '\0';
        
        write_ob(base);
        write_ext(base);
        write_ent(base);
        printf("Assembly completed successfully - files written.\n");
    } else {
        printf("Second pass completed with %d error(s); no output files generated.\n", second_pass_errors);
    } 
}

