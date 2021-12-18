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
//   r5 threaded code pointer, values need to have thumb bit set, so that we can use `bx` directly

regular_func forth_repl
    push {r4, r5, r6, lr}
next_line:
    ldr r4, =input_buffer
    ldr r5, =input_ptr
    str r4, [r5]        // set ptr to start of buffer
    movs r0, '>'
    bl put_char
    movs r0, ' '
    bl put_char
    mov r0, r4
    bl read_line
next_word:
    ldr r4, [r5]
    ldrb r0, [r4]
    ands r0, r0
    beq next_line
    mov r0, r4
    bl parse_word
    movs r4, r0 // save number if needed later
    bl find_word
    beq number // Z is set when nothing was found
    bl execute_word
    b next_word

number:
    //push {r6}
    ldr r1, =forth_stack_ptr
    ldr r0, [r1]
    subs r0, #4
    str r4, [r0]
    str r0, [r1]
    //movs r6, r4 // push number literal
    b next_word // go to next word in line

    pop {r4, r5, r6, pc}

// sets up registers to interpret a word
regular_func execute_word
    push {r4, r5, r6, r7, lr}
    ldr r7, =return_stack
    ldr r5, =return_pointer
    
    // setup forth code stack
    mov r4, sp // keep existing in r4 during forth execution
    ldr r1, =forth_stack_ptr
    ldr r1, [r1]    
    
    mov sp, r1
    pop {r6}
    bx r0

.align 2
return_pointer:
    .word 1f + 1
1:
    push {r6}
    mov r1, sp
    ldr r0, =forth_stack_ptr
    str r1, [r0]
    mov sp, r4 // reset old SP

    pop {r4, r5, r6, r7, pc}

// returns
// r0: entry point for forth word or 0 if none found
regular_func find_word
// r1: input length
// r2: input char buf
// r3: entry pointer
    push {r4, r5, r6, r7, lr}
    ldr r3, =latest

next_entry:
    ldr r3, [r3]
    tst r3, r3
    beq not_found // found end of list

    // compare size first
    ldrb r0, [r3, #4] // load size field
    cmp r0, r1
    bne next_entry // if size doesn't match go directly to next entry

    // then compare chars
    // r0: dict name pointer
    // r5: idx
    // r6: dict char
    // r7: input char
    adds r0, r3, #5 // start of dict entry
    movs r5, #0
next_char:
    ldrb r6, [r0, r5]
    ldrb r7, [r2, r5]
    cmp r6, r7
    bne next_entry // char didn't match

    adds r5, #1
    cmp r5, r1
    bne next_char

    adds r0, r0, r5 // points after last char
    // need to align to next half-word
    adds r0, #1
    movs r1, 1
    orrs r0, r1 // set thumb bit for the upcoming jump

not_found:
    pop {r4, r5, r6, r7, pc}

// returns:
// r0: number
// r1: char count
// r2: start of word
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
.pool

.macro NEXT
    ldm r5!, {r1}
    bx r1
.endm

regular_func DOCOL
    stm r7!, {r5}
    mov r5, lr
    subs r5, #1 // clear thumb bit
    NEXT

EXIT:
    subs r7, #4
    ldr r5, [r7]
    ldr r1, [r5]
    bx r1

LIT:
    push {r6}
    ldm r5!, {r6}
    // TODO: NEXT does another `ldm r5!, {r1}` directly afterwards, so it would be nice coalesce them
    // that's currently not possible because we would need a register > r6 to be loaded afterwards
    // but r7 is already taken (but maybe let's use a different calling convention later on. We
    // have full control over the calling convention for assembled FORTH words.
    NEXT

.macro def_word label,name
.align 2
1:
.word 1f            // pointer to next dict entry
.byte 3f -2f        // size of name
2:
.ascii "\name"     // name
3:
.align 1
\label:
.endm

.macro do label
    .word \label + 1 // set thumb bit
.endm

.align 4
latest_predefined:
def_word MINUS,-
    pop {r1}
    subs r6, r1, r6
    NEXT

def_word PLUS,+
    pop {r1}
    adds r6, r1
    NEXT

def_word DUP,dup
    push {r6}
    NEXT

def_word DROP,drop
    pop {r6}
    NEXT

def_word DOT,.
    movs r0, r6
    bl print_number
    pop {r6}
    NEXT

def_word DOUBLE,double
    bl DOCOL
    do LIT
    do 0x1234
    do DUP
    do PLUS
    do EXIT

// final word definition needs to be written manually
1:
.word 0
.word 0
//.byte 10
//.ascii "          "

regular_func exec_double_test
    // setup forth env
    ldr r7, =return_stack
    ldr r5, =return
    movs r6, #12 // initial stack val
next:    
    b DOUBLE
return:
    .word finished + 1
finished:
    bl finished

.section .data
here:            .word forth_code_area
base:            .word 16
state:           .word 0
latest:          .word latest_predefined
input_ptr:       .word input_buffer
input_buffer:    .space 256
return_stack:    .space 1000
forth_stack:     .space 1000
forth_stack_ptr: .word forth_stack_ptr - 4// initially points to itself, i.e. on top of the forth_stack area
forth_code_area: .space 1000