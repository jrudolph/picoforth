.syntax unified
.cpu cortex-m0plus
.thumb

#include "pico/asm_helper.S"

.section .text


regular_func doublize
    //push {lr}
    add r0, r0
    //bl plus1
    ldr r1, back //=#0x10000677
    //ldr r1, =#0x10000677
    bx r1

.align 2
back: .word	0x10000677
