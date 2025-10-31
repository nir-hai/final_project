.extern EXT
MAIN:   jmp &EXT        ; illegal: relative to extern
        mov X,r1        ; X is undefined
        jmp &Y          ; Y is undefined (and relative)
        stop