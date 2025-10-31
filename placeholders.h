/* placeholders.h */
#ifndef PLACEHOLDERS_H
#define PLACEHOLDERS_H

/* Shared placeholder descriptor between passes */
typedef struct
{
    int wordIndex;   /* index in code[]              */
    int instrIC;     /* IC of the header word        */
    int mode;        /* 1 = DIRECT , 2 = RELATIVE    */
    char label[31];  /* label (without leading '&')  */
    int line;        /* source line number            */
} Placeholder;

/* Defined in first_pass.c, used also by second_pass.c */
extern Placeholder placeholders[1000];
extern int n_placeholders;

#endif /* PLACEHOLDERS_H */


