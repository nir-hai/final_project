/* pre_assembler.c - here we will expand the macro , we gonna clean whitspace 
 * it will make <input>.am with macros expanded, the comments or blank lines removed,
 * and normalized spacing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pre_assembler.h"

#define MAX_LINE_LEN 81
#define MAX_MACRO_BODY 10000
#define MAX_MACRO_NAME 31

typedef struct MacroNode { /* this is node for linked list of macros */
    char name[MAX_MACRO_NAME + 1];
    char *body;
    struct MacroNode *next;
} MacroNode;

static MacroNode *macro_head = NULL; /*head of linked list*/

/* declarations */
static void free_macros(void);
static MacroNode *find_macro(const char *name);
static int is_valid_macro_name(const char *name);
static void clean_line(const char *input, char *output);
static char *get_first_word(const char *line);

/* new helpers: declare (reserve) a macro at 'mcro', then attach body at 'mcroend' */
static MacroNode *declare_macro(const char *name);
static int set_macro_body(MacroNode *m, const char *body);

/*take first word from a line */
static char *get_first_word(const char *line) {
    static char word[MAX_MACRO_NAME + 1];
    int i = 0;
    int j = 0;
    
    /* skip opening spaces */
    while (line[i] && isspace((unsigned char)line[i])) i++;
    
    /* take first word */
    while (line[i] && !isspace((unsigned char)line[i]) && line[i] != ':' && j < MAX_MACRO_NAME) {
        word[j++] = line[i++];
    }
    word[j] = '\0';
    
    return word;
}

/* this frees up all the mcros */
static void free_macros(void) {
    MacroNode *current = macro_head;
    MacroNode *next;
    
    while (current != NULL) {
        next = current->next;
        free(current->body);
        free(current);
        current = next;
    }
    macro_head = NULL;
}

/*  this find  macro by name */
static MacroNode *find_macro(const char *name) {
    MacroNode *p;
    for (p = macro_head; p != NULL; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}


/* here we check if name is valid for a mcro */
static int is_valid_macro_name(const char *name) {
    int i;
    if (name == NULL || name[0] == '\0') return 0;
    if (!isalpha((unsigned char)name[0])) return 0;
    
    for (i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') return 0;  
    }
    
    /* check against reserved words */
    if (strcmp(name, "mov") == 0 || strcmp(name, "cmp") == 0 ||
        strcmp(name, "add") == 0 || strcmp(name, "sub") == 0 ||
        strcmp(name, "lea") == 0 || strcmp(name, "clr") == 0 ||
        strcmp(name, "not") == 0 || strcmp(name, "inc") == 0 ||
        strcmp(name, "dec") == 0 || strcmp(name, "jmp") == 0 ||
        strcmp(name, "bne") == 0 || strcmp(name, "red") == 0 ||
        strcmp(name, "prn") == 0 || strcmp(name, "jsr") == 0 ||
        strcmp(name, "rts") == 0 || strcmp(name, "stop") == 0 ||
        strcmp(name, "data") == 0 || strcmp(name, "string") == 0 ||
        strcmp(name, "entry") == 0 || strcmp(name, "extern") == 0 ||
        strcmp(name, "mcro") == 0 || strcmp(name, "mcroend") == 0) {
        return 0;
    }
    return 1;
}

/* take a line and remove extra spaces and comments */
static void clean_line(const char *input, char *output) {
    int i = 0;
    int j = 0;
    int in_space = 0;
    
    /* skip openning spaces */
    while (input[i] && isspace((unsigned char)input[i])) i++;
    
    /* copy non-comment part */
    while (input[i] && input[i] != ';') {
        if (isspace((unsigned char)input[i])) {
            if (!in_space && j > 0) {
                output[j++] = ' ';
                in_space = 1;
            }
        } else {
            output[j++] = input[i];
            in_space = 0;
        }
        i++;
    }
    
    /* remove ending space */
    if (j > 0 && output[j-1] == ' ') j--;
    
    output[j] = '\0';
}

/* check if a name exists as a label in the source */
static int name_exists_as_label(const char *name, FILE *file) {
    long pos = ftell(file); /*as used in the book file position func*/
    char line[MAX_LINE_LEN];
    char label[MAX_MACRO_NAME + 1];
    const char *colon;
    int len;
    char *start;  
    
    /* back to start of file */
    rewind(file);
    
    while (fgets(line, sizeof(line), file)) {
        colon = strchr(line, ':');
        if (colon) {
            len = (int)(colon - line);
            if (len > 0 && len <= MAX_MACRO_NAME) {
                strncpy(label, line, (size_t)len);
                label[len] = '\0';
                
                /*  cut opening whitespace */
                start = label;  
                while (*start && isspace((unsigned char)*start)) start++;
                
                if (strcmp(start, name) == 0) {
                    fseek(file, pos, SEEK_SET); /* go back to position */
                    return 1; /* we find a label */
                }
            }
        }
    }
    fseek(file, pos, SEEK_SET); /* go back to position */
    return 0; /* did nt found*/
}

/* reserve a macro name at 'mcro' time (no body yet) */
static MacroNode *declare_macro(const char *name) {
    MacroNode *m;
    if (find_macro(name) != NULL) return NULL; /* duplicate */
    m = (MacroNode *)malloc(sizeof(MacroNode));
    if (!m) return NULL;
    strncpy(m->name, name, MAX_MACRO_NAME);
    m->name[MAX_MACRO_NAME] = '\0';
    m->body = NULL;
    m->next = macro_head;
    macro_head = m;
    return m;
}

/* attach body at 'mcroend' */
static int set_macro_body(MacroNode *m, const char *body) {
    size_t n = strlen(body);
    char *b = (char *)malloc(n + 1);
    if (!b) return 0;
    memcpy(b, body, n + 1);
    if (m->body) free(m->body);
    m->body = b;
    return 1;
}

/* this is the main pre assembler*/
int pre_assembler_main(const char *in_path) {
    /* declare variables */
    FILE *in_file, *out_file; /* input and output file pointers */
    char out_path[512];
    char char_line[MAX_LINE_LEN];
    char processed_line[MAX_LINE_LEN];
    char *first_word;
    int inside = 0; /* flag we inside macro definition */
    int errors = 0; /* count errors */
    char current_name[MAX_MACRO_NAME + 1];
    char current_body[MAX_MACRO_BODY];
    int line_no = 0;
    char *name_start, *name_end, *extra;
    int len;
    MacroNode *found;
    MacroNode *current_decl = NULL; /* macro currently being defined */
    
    /*this create output filename */
    strcpy(out_path, in_path);
    if (strlen(out_path) > 3 && strcmp(out_path + strlen(out_path) - 3, ".as") == 0) {
        /* we need longer than 3 chars and then we check they .as*/
        strcpy(out_path + strlen(out_path) - 3, ".am");
    } else {
        strcat(out_path, ".am");
    }
    
    in_file = fopen(in_path, "r");
    if (in_file == NULL) { /* input file is not found */
        printf("%s: No such file or directory\n", in_path);
        return 1;
    }
    
    out_file = fopen(out_path, "w");
    if (out_file == NULL) { /* output file cannot be created */
        printf("Cannot create output file %s\n", out_path);
        fclose(in_file);
        return 1;
    }
    
    current_body[0] = '\0'; /* start the current mcro body*/
    
    /* here we every line */
    while (fgets(char_line, sizeof(char_line), in_file)) {
        line_no++;
        
        /* we check line length ( ignore /n) */
        {
            int llen = (int)strlen(char_line);
            if (llen > 0 && char_line[llen-1] == '\n') {
                llen--; /* don't count newline */
            }
            if (llen > 80) {
                printf("ERROR in line %d: Line too long (above 80 characters): \"%.40s...\"\n", 
                       line_no, char_line);
                errors++;
                continue;
            }
            /* fgets might cut the line for long lines */
            if (llen == MAX_LINE_LEN - 1 && char_line[llen-1] != '\n') {
                int c;
                printf("ERROR in line %d: the line too long (above 80 characters)\n", line_no);
                errors++;
                /* skip rest of this line */
                while ((c = fgetc(in_file)) != '\n' && c != EOF) { /* skip */ }
                continue;
            }
        }
        
        clean_line(char_line, processed_line);
        first_word = get_first_word(processed_line); /* get first word */
        
        /* skip blank lines and comments */
        if (strlen(processed_line) == 0) {
            /* don't write blank lines to output .. skip them */
            continue;
        }
        
        /* Check for macro usage (only when not inside macro definition) */
        if (!inside) {
            const char *colon = strchr(processed_line, ':');
            const char *macro_word = colon ? get_first_word(colon + 1) : first_word;
            
            found = find_macro(macro_word);
            if (found != NULL) {
                fprintf(out_file, "; SRCLINE %d\n", line_no);  // Add this line
                if (colon) {
                    /* Write label part first WITHOUT newline */
                    fwrite(processed_line, 1, (size_t)(colon - processed_line + 1), out_file);
                    fputc(' ', out_file);  /* Add space instead of newline */
                }
                /* expand macro */
                fputs(found->body, out_file);
                continue;
            }
        }
        
        /* Check for macro definition start */
        if (strcmp(first_word, "mcro") == 0) {
            if (inside) {
                printf("Error in line %d: 'mcro' inside another macro definition\n", line_no);
                errors++;
                continue;
            }

            /* Extract macro name */
            name_start = processed_line + 4; /* Skip "mcro" */
            while (*name_start && isspace((unsigned char)*name_start)) name_start++;
            
            name_end = name_start;
            while (*name_end && !isspace((unsigned char)*name_end)) name_end++;
            
            len = (int)(name_end - name_start);
            if (len <= 0 || len > MAX_MACRO_NAME) {
                printf("Error in line %d: Missing/too-long macro name\n", line_no);
                errors++;
                continue;
            }
            
            strncpy(current_name, name_start, (size_t)len);
            current_name[len] = '\0';
            
            /* Check for extra characters */
            while (*name_end && isspace((unsigned char)*name_end)) name_end++;
            if (*name_end != '\0') {
                printf("Error in line %d: Extra characters after macro name\n", line_no);
                errors++;
                continue;
            }
            
            /* Validate macro name */
            if (!is_valid_macro_name(current_name)) {
                printf("Error in line %d: Illegal macro name '%s'\n", line_no, current_name);
                errors++;
                continue;
            }
            
            /* Check for redefinition (or reserve immediately) */
            if (name_exists_as_label(current_name, in_file)) {
                printf("Error in line %d: Macro name '%s' conflicts with existing symbol\n", line_no, current_name);
                errors++;
                continue;
            }

            current_decl = declare_macro(current_name);
            if (!current_decl) {
                printf("Error in line %d: Macro redefinition: '%s'\n", line_no, current_name);
                errors++;
                continue;
            }
            
            inside = 1;
            current_body[0] = '\0';
            continue;
        }
        
        if (strcmp(first_word, "mcroend") == 0) {
            /* If mcroend appears with no active macro, ignore it silently */
            if (!inside || !current_decl) {
                continue;
            }

            /* look for extra characters */
            extra = processed_line + 7; /* Skip "mcroend" */
            while (*extra && isspace((unsigned char)*extra)) extra++;
            if (*extra != '\0') {
                printf("Error in line %d: Extra characters after 'mcroend'\n", line_no);
                errors++;
                /* still close the macro block to resync */
            } else {
                if (!set_macro_body(current_decl, current_body)) {
                    printf("Error in line %d: Failed to save macro '%s'\n", line_no, current_decl->name);
                    errors++;
                }
            }
            
            inside = 0;
            current_decl = NULL;
            continue;
        }

        /* Handle lines inside macro definition */
        if (inside) {
            clean_line(char_line, processed_line);      /* normalize + strip comments */
            if (processed_line[0] != '\0') {            /* skip blank lines */
                size_t need = strlen(current_body) + strlen(processed_line) + 2;
                if (need < MAX_MACRO_BODY) {
                    strcat(current_body, processed_line);
                    strcat(current_body, "\n");         /* add exactly one newline */
                } else {
                    printf("Error in line %d: Macro body too long\n", line_no);
                    errors++;
                    /* force-close to avoid spillover */
                    inside = 0;
                    current_decl = NULL;
                }
            }
            continue;                                 
        }

        /* normal line - just forward to .am */
        fprintf(out_file, "; SRCLINE %d\n", line_no);  // Add this line
        fputs(processed_line, out_file);
        fputc('\n', out_file);       
    }

    /* NOTE: No error if EOF while 'inside' a macro (missing 'mcroend' is tolerated) */
    
    fclose(in_file);
    fclose(out_file);
    free_macros();
    
    if (errors > 0) {
        remove(out_path);
        printf("Pre-assembler failed with %d error(s). No .am file generated.\n", errors);
        return 1;
    }
    return 0;
}

