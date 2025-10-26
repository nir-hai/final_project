mcro INIT
    mov #1, r0
mcroend

.extern ext_func
.entry start

start: INIT
       lea data, r1
       jsr ext_func
       stop

data: .data 42