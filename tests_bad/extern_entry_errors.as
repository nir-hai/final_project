
.extern EXT1 extra         ;  extra stuff after extern name  
.extern 9BAD               ;  illegal extern name 
.extern                    ; missing extern name 
.extern X                  ; 

.entry START extra         ;  extra stuff after entry name 
.entry BAD$NAME            ;  illegal entry name (invalid char) 
.entry                     ;  missing entry name 
.entry X                   ; extern cannot be entry 
.entry UNDEF               ;  undefined entry 

START:  stop               
MAIN:   jmp &X             ; relative to extern is forbidden 