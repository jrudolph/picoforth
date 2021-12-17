.syntax unified
.cpu cortex-m0plus
.thumb

#include "pico/asm_helper.S"

.section .text,"x"

// forth conventions:
//   stack contains user values
//   r7 return stack pointer

regular_func forth_repl
    push {r4, lr}
    ldr r4, =input_buffer
    mov r0, r4
    bl read_line
    adds r0, '0'
    bl put_char
    ldrb r0, [r4]
    bl put_char
    pop {r4, pc}


.section .data
here:            .word forth_code_area
base:            .word 16
state:           .word 0
latest:          .word 0
input_ptr:       .word input_buffer
input_buffer:    .space 256
return_stack:    .space 1000
forth_code_area: .space 1000