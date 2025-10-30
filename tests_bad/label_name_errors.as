
1BAD:      mov r0, r1           
BAD$NAME:  stop                  
mov:       rts                   
THIS_LABEL_NAME_IS_WAY_TOO_LONG_ABCDEFG: stop 
FOO:   mov r0, r1
FOO:   add r2, r3         

BAR:   .data 1, 2, 3
BAR:   .string "x"   
              