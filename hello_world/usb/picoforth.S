.syntax unified
.cpu cortex-m0plus
.thumb

#include "pico/asm_helper.S"

// taking inspiration from https://niedzejkob.p4.team/bootstrap/miniforth/#keyboard-input for now

.section .text."x"

// forth conventions:
//   stack contains user values
//   r7 return stack pointer
//   r6 contains top of stack

regular_func forth_repl
    push {r4, r5, lr}
next_line:
    ldr r4, =input_buffer
    ldr r5, =input_ptr
    str r4, [r5]        // set ptr to start of buffer
    mov r0, r4
    bl read_line
next_word:
    ldr r4, [r5]
    ldrb r0, [r4]
    ands r0, r0
    beq next_line
    mov r0, r4
    bl parse_word

    pop {r4, r5, pc}

regular_func parse_word
    // r0: start of input buffer
    // r1: scratch
    // r2: start of word
    // r3: scratch
    // r4: number literal
    // r5: char count

    push {r4, r5, lr}
    subs r0, #1
whitespace:
    adds r0, #1
    ldrb r1, [r0]
    cmp r1, ' '
    beq whitespace

    mov r2, r0 // save start of word
    movs r4, #0
    movs r5, #0

word_chars:
    movs r3, #0x20
    mvns r3, r3
    ldrb r1, [r0]
    adds r0, #1

    ands r1, r3 // if '\0' or ' '
    beq done

    adds r5, #1
    subs r1, # '0' & ~ 0x20
    cmp r1, #9
    ble digit
    subs r1, # 'A' - ('0' & ~0x20) - 10
digit:
    ldr r3, =#base
    ldr r3, [r3]
    muls r4, r4, r3
    sxtb r1, r1
    adds r4, r1
    b word_chars

done:
    ldr r1, =#input_ptr
    str r0, [r1]
    movs r0, r4
    movs r1, r5
    pop {r4, r5, pc}

.section .data
here:            .word forth_code_area
base:            .word 16
state:           .word 0
latest:          .word 0
input_ptr:       .word input_buffer
input_buffer:    .space 256
return_stack:    .space 1000
forth_code_area: .space 1000