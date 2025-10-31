MAIN:  mov #5, #7
       lea r3, r4
       lea LABEL, #5
LABEL: .data 1,2
       prn &LABEL
       jmp r3
       red #2
       add r1, #3
       stop