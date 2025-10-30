/* first_pass.c - legality check + 24-bit encoding
 * ---------------------------------------------------------------
 *  • Builds code[] (instruction image) and data[] (data image)
 *  • Records placeholders for DIRECT / RELATIVE operands
 *  • Symbol table, ICF, DCF fully resolved by end of pass-1
 * -------------------------------------------------------------- */

#include "symbols.h"
#include "opcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- Word type and masking ---------- */
typedef unsigned long Word;
#define WORD_MASK 0xFFFFFFul

/* ---------- configuration ---------- */
#define MAX_CODE 4096
#define MAX_DATA 4096
#define MAX_LINE_LENGTH 80
#define ARE_A 4 /* ARE bits = 100 (A=1, R=0, E=0) */
#define ARE_R 2 /* ARE bits = 010 (A=0, R=1, E=0) */
#define ARE_E 1 /* ARE bits = 001 (A=0, R=0, E=1) */

/* ---------- code & data images (shared with 2nd pass) ---------- */
Word code[MAX_CODE];
int cw = 0; /* instruction words */
Word data[MAX_DATA];
int dw = 0; /* data words        */

/* ---------- placeholder list (shared with 2nd pass) ------------ */
typedef struct
{
    int wordIndex;  /* index in code[]        */
    int instrIC;    /* IC of header word      */
    int mode;       /* 1 = DIR , 2 = REL      */
    char label[31]; /* label (no &)           */
} Placeholder;

Placeholder placeholders[1000];
int n_placeholders = 0;

/* this function resets the assembler state to initial values to prepare for a new assembly */
void reset_assembler_state(void)
{
    cw = 0;
    dw = 0;
    n_placeholders = 0;
    free_symbol_table();
}

/* ---------- helpers (label, classify, …) ----------------------- */
static int is_label(const char *line, char label_name[31])
{
    const char *colon_pos = strchr(line, ':');
    size_t len;
    size_t char_index;

    if (!colon_pos)
        return 0;                     /*no colon found - no label*/
    len = (size_t)(colon_pos - line); /* how many chars before colon*/

    if (len > 30)
        return -1; /* too long */
    if (len == 0 || !isalpha((unsigned char)line[0]))
        return 0;

    for (char_index = 1; char_index < len; ++char_index)
        if (!isalnum((unsigned char)line[char_index]))
            return 0;

    strncpy(label_name, line, len);
    label_name[len] = '\0';
    return 1;
}

static const char *after_label(const char *line)
{
    const char *colon_pos = strchr(line, ':');
    const char *current_char;
    if (!colon_pos)
        return line;              /* no label */
    current_char = colon_pos + 1; /* start after colon */
    /* skip spaces after colon */
    while (*current_char && isspace((unsigned char)*current_char))
        ++current_char;
    return current_char; /* return position after label */
}

/* 0 instr | 1 .data/.string | 2 .extern | 3 .entry */
static int classify(const char *body)
{
    if (*body == '.')
    {
        if (strncmp(body, ".data", 5) == 0 && (isspace((unsigned char)body[5]) || body[5] == '\0'))
            return 1;
        if (strncmp(body, ".string", 7) == 0 && (isspace((unsigned char)body[7]) || body[7] == '\0'))
            return 1;
        if (!strncmp(body, ".extern", 7))
            return 2;
        if (!strncmp(body, ".entry", 6))
            return 3;
    }
    return 0;
}

/* Check if operand is a register (r0-r7) */
static int is_register(const char *operand)
{
    return operand[0] == 'r' && operand[1] >= '0' && operand[1] <= '7' && operand[2] == '\0';
}

/* Get register number (0-7) or -1 if not a register */
static int reg_num(const char *operand)
{
    if (is_register(operand))
    {
        return operand[1] - '0';
    }
    else
    {
        return -1;
    }
}

/* 0 #imm | 1 DIR | 2 REL (&) | 3 REG | -1 none */
static int addr_mode(const char *operand)
{
    if (!operand[0]) /* empty operand */
        return -1;
    if (operand[0] == '#') /* immediate mode */
        return 0;
    if (operand[0] == '&') /* relative mode */
        return 2;
    if (is_register(operand)) /* register mode */
        return 3;
    return 1; /* direct mode */
}

static void split_ops(const char *b, char *o1, char *o2)
{
    int i;

    while (*b && !isspace((unsigned char)*b)) ++b;   /* skip mnemonic */
    while (*b &&  isspace((unsigned char)*b)) ++b;   /* spaces after */

    /* first operand */
    i = 0;
    while (*b && *b != ',' && !isspace((unsigned char)*b) && i < 30)
        o1[i++] = *b++;
    o1[i] = '\0';

    /* skip spaces */
    while (*b && isspace((unsigned char)*b)) ++b;

    /* if there isn't a comma -> no second operand */
    if (*b != ',') { o2[0] = '\0'; return; }

    /* comma */
    ++b;
    while (*b && isspace((unsigned char)*b)) ++b;

    /* second operand */
    i = 0;
    while (*b && *b != ',' && !isspace((unsigned char)*b) && i < 30)
        o2[i++] = *b++;
    o2[i] = '\0';
}

/* Check if name is reserved (opcode or register) */
static int is_reserved_name(const char *name)
{
    const OpInfo *op = find_opcode(name);
    if (op)
        return 1;
    if (is_register(name))
        return 1;
    return 0;
}

/* taking care of errors */
static int first_pass_errors = 0;

/* pass the error count in othar modules */
int get_first_pass_errors(void)
{
    return first_pass_errors;
}

/* --------------------------------------------------------------- */
void first_pass(const char *am)
{
    FILE *src; 
    int IC = 100, DC = 0; /* IC = Instruction Counter starts at 100, DC = Data Counter starts at 0 */
    char line[81]; /* line buffer */
    int ln = 0; /* line number */
    char label[31]; /* label buffer */
    int has_lab; /* 1=has label, 0=no label, -1=label too long */
    const char *body; /* pointer to line body (after label) */
    int kind; /* 0=instr, 1=data/string, 2=extern, 3=entry */
    char op_name[16]; /* opcode name */
    const OpInfo *op;  /* pointer to opcode info */
    char src_op[31], dst_op[31]; /* source and destination operands */
    int sm, dm, nOps;  /* source mode, dest mode, number of operands */
    Word w;  /* the 24 bits word we are building */ 
    int headerIC; /* IC of the current instruction header word */
    long numeric_value; /* For storing parsed numbers */
    first_pass_errors = 0; /* reset error counter */

    src = fopen(am, "r");
    if (!src)
    {
        perror(am);
        first_pass_errors++;
        return;
    }

    init_symbol_table();

    while (fgets(line, sizeof line, src))
    {
        ++ln;
        /* we remove newline for more cleaner error messages */
        line[strcspn(line, "\r\n")] = '\0';

        /* check line length */
        if (strlen(line) > MAX_LINE_LENGTH) {
            printf("ERROR in line %d: line exceeds %d characters (%zu chars): \"%.20s...\"\n", 
                   ln, MAX_LINE_LENGTH, strlen(line), line);
            first_pass_errors++;
            continue;
        }

        has_lab = is_label(line, label);

        if (has_lab == -1)
        { /* label correctness validation */
            printf("ERROR in line %d: label too long (over 30 characters): \"%s\"\n", ln, line);
            first_pass_errors++;
            continue;
        }
        else if (!has_lab && strchr(line, ':'))
        {
            printf("ERROR in line %d: invalid label format: \"%s\"\n", ln, line);
            first_pass_errors++;
            continue;
        }
        body = after_label(line);
        while (*body && isspace((unsigned char)*body))
            ++body;
        if (*body == '\0' || *body == ';')
        {
            IC=100+cw;
            continue;
        }
        kind = classify(body);

        /* ---- symbol insertion ------------------------------------------------ */
        if (has_lab && kind != 2)
        {
            int addr; /* address to assign to label */
            char attr; /* attribute to assign to label */

            if (kind == 1)
            {
                addr = DC;
                attr = 'D';
            }
            else
            {
                addr = IC;
                attr = 'C';
            }

            /* Check for reserved names */
            if (is_reserved_name(label))
            {
                printf("ERROR: in line %d: label is conflicts with reserved name: \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            if (add_symbol(label, addr, attr) != 0)
            {
                printf("ERROR: in line %d: ther is duplicate label: \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }
        }

        /* ---------------- instructions ---------------- */
        if (kind == 0)
        {
            sscanf(body, "%15s", op_name);
            op = find_opcode(op_name);
            if (!op)
            {
                printf("ERROR in line %d: ther isunknown instruction: \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            split_ops(body, src_op, dst_op);

            if (op->nOperands == 1 && dst_op[0] == '\0')
            {
                strcpy(dst_op, src_op);
                src_op[0] = '\0';
            }

            sm = addr_mode(src_op);
            dm = addr_mode(dst_op);
            nOps = 0;
            if (src_op[0] != '\0')
                nOps++;
            if (dst_op[0] != '\0')
                nOps++;

            if (nOps != op->nOperands)
            {
                if (nOps > op->nOperands)
                {
                    printf("ERROR in lien %d: extra operand \"%s\"\n", ln, line);
                }
                else
                {
                    printf("ERROR in line %d: missing operand \"%s\"\n", ln, line);
                }
                first_pass_errors++;
                continue;
            }

            /* check source addressing mode */
            if (sm >= 0 && !(op->srcMask & (1 << sm)))
            {
                printf("ERROR on line %d: Invalid source addressing mode \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }
            /* check destination addressing mode */
            if (dm >= 0 && !(op->dstMask & (1 << dm)))
            {
                printf("ERROR on line %d: Invalid destination addressing mode \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            /* ---- header word ---- */
            w = 0; /* start with empty 24 bit word */
            w |= ((Word)op->opcode & 0x3F) << 18; /* insert opcode */

            /* source mode */
            if (sm >= 0) /* there IS a source operand */
            {
                w |= ((Word)(sm & 0x3)) << 16; /* insert actual mode (0,1,2,3) */
            }
            else /* no source operand (sm = -1) */
            {
                w |= ((Word)(0 & 0x3)) << 16; /* insert 0 (no source) */
            }

            /* source register */
            if (sm == 3) /* register mode */
            {
                w |= ((Word)(reg_num(src_op) & 0x7)) << 13; /* insert register number */
            }
            else
            {
                w |= ((Word)(0 & 0x7)) << 13; /* insert 0 (no register) */
            }

            /* destination mode */
            if (dm >= 0) /* there IS a destination operand */
            {
                w |= ((Word)(dm & 0x3)) << 11; /* insert actual mode (0,1,2,3) */
            }
            else
            {
                w |= ((Word)(0 & 0x3)) << 11; /* insert 0 (no destination) */
            }

            /* destination register */
            if (dm == 3) /* register mode */
            {
                w |= ((Word)(reg_num(dst_op) & 0x7)) << 8; /* insert register number */
            }
            else
            {
                w |= ((Word)(0 & 0x7)) << 8; /* insert 0 (no register) */
            }

            /* function code */
            if (op->funct < 0) /* no funct field */
            {
                w |= ((Word)(0 & 0x1F)) << 3; /* insert 0 (no funct) */
            }
            else
            {
                w |= ((Word)(op->funct & 0x1F)) << 3; /* insert actual funct */
            }
            w |= ARE_A;                               /* insert ARE = 100 (Absolute) */

            headerIC = IC; /* remember IC of this instruction */
            code [cw]= w & WORD_MASK; /* store header word */
            cw++;
            /* ---- extra words ---- */
            if (sm == 0)
            { /* immediate */
                numeric_value = strtol(src_op + 1, NULL, 10);
                code[cw++] = (((Word)(numeric_value & 0x1FFFFF) << 3) | ARE_A) & WORD_MASK;
            }
            else if (sm >= 0 && sm != 3)
            {
                code[cw++] = 0;
                placeholders[n_placeholders].wordIndex = cw - 1;
                placeholders[n_placeholders].instrIC = headerIC;
                placeholders[n_placeholders].mode = sm;
                /* For source operand */
                if (sm == 2)
                {
                    strncpy(placeholders[n_placeholders].label, src_op + 1, 30);
                }
                else
                {
                    strncpy(placeholders[n_placeholders].label, src_op, 30);
                }
                placeholders[n_placeholders].label[30] = '\0';
                n_placeholders++;
            }

            if (dm == 0)
            {
                numeric_value = strtol(dst_op + 1, NULL, 10);
                code[cw++] = (((Word)(numeric_value & 0x1FFFFF) << 3) | ARE_A) & WORD_MASK;
            }
            else if (dm >= 0 && dm != 3)
            {
                code[cw++] = 0;
                placeholders[n_placeholders].wordIndex = cw - 1;
                placeholders[n_placeholders].instrIC = headerIC;
                placeholders[n_placeholders].mode = dm;
                /* For destination operand */
                if (dm == 2)
                {
                    strncpy(placeholders[n_placeholders].label, dst_op + 1, 30);
                }
                else
                {
                    strncpy(placeholders[n_placeholders].label, dst_op, 30);
                }
                placeholders[n_placeholders].label[30] = '\0';
                n_placeholders++;
            }
            IC = 100 + cw;
        }

        /* ---------------- data / string ---------------- */
        else if (kind == 1)
        {
            if (strncmp(body, ".data", 5) == 0)
            {
                const char *data_ptr;
                const char *number_end;
                
                data_ptr = body + 5;
                while (1)
                {
                    while (*data_ptr && (isspace((unsigned char)*data_ptr) || *data_ptr == ','))
                        ++data_ptr;
                    if (!*data_ptr || *data_ptr == '\n')
                        break;
                    numeric_value = strtol(data_ptr, (char **)&number_end, 10);
                    if (data_ptr == number_end)
                    {
                        printf("ERROR: bad number in line %d: \"%s\"\n", ln, line);
                        first_pass_errors++;
                        goto fail;
                    }
                    data[dw++] = (Word)(numeric_value & 0xFFFFFF);
                    ++DC;
                    data_ptr = number_end;
                }
            }
            else
            { /* .string */
                const char *open_quote;
                const char *close_quote;
                const char *char_ptr;
                
                open_quote = strchr(body, '"');
                close_quote = open_quote ? strrchr(open_quote + 1, '"') : NULL;
                
                if (!open_quote || !close_quote || close_quote == open_quote + 1)
                {
                    printf("ERROR-  bad .string on line %d: \"%s\"\n", ln, line);
                    first_pass_errors++;
                    goto fail;
                }
                
                for (char_ptr = open_quote + 1; char_ptr < close_quote; ++char_ptr)
                {
                    data[dw++] = (Word)(*char_ptr & 0xFF);
                    ++DC;
                }
                data[dw++] = 0; /* Raw zero terminator */
                ++DC;
            }
        }

        /* ---------------- .extern ---------------- */
        else if (kind == 2)
        {
            const char *p = body + 7;
            char extern_name[31];
            size_t l = 0;

            while (*p && isspace((unsigned char)*p)) ++p;

            if (*p == '\0') {
                printf("ERROR in line %d: missing name after .extern: \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }
            if (!isalpha((unsigned char)*p)) {
                printf("ERROR in line %d: invalid symbol after .extern (must start with a letter): \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            l = 1;
            while (l < 30 && isalnum((unsigned char)p[l])) ++l;

            if (l == 30 && isalnum((unsigned char)p[l])) {
                printf("ERROR in line %d: symbol name too long (max 30): \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            strncpy(extern_name, p, l);
            extern_name[l] = '\0';

            p += l;
            while (*p && isspace((unsigned char)*p)) ++p;
            if (*p != '\0') {
                printf("ERROR in line %d: '.extern' takes exactly one symbol (letters/digits only): \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            if (is_reserved_name(extern_name)) {
                printf("ERROR in line %d: extern name conflicts with reserved word/register: \"%s\"\n", ln, line);
                first_pass_errors++;
                continue;
            }

            if (add_symbol(extern_name, 0, 'E') != 0)
            {
                printf("ERROR in line %d: duplicate extern symbol \"%s\": \"%s\"\n", ln, extern_name, line);
                first_pass_errors++;
                continue;
            }
        }
        /* .entry ignored here */
    }

    if (first_pass_errors == 0)
    {
        relocate_data_symbols(IC);
    }

    fclose(src);

    if (first_pass_errors > 0)
    {
        printf("First pass completed with %d error(s). No output files will be generated.\n", first_pass_errors);
    }

    return;

fail:
    first_pass_errors++;
    fclose(src);
}

