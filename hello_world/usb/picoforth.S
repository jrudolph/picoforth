.syntax unified
.cpu cortex-m0plus
.thumb

#include "pico/asm_helper.S"

// taking inspiration from https://niedzejkob.p4.team/bootstrap/miniforth/#keyboard-input for now

.section .text."x"

// forth conventions:
//   stack contains user values
//   r8 pointer to DOCOL (because it's so hard to assemble otherwise) FIXME: it might get mangled by using C funcs
//   r7 return stack pointer
//   r6 contains top of stack
//   r5 threaded code pointer, values need to have thumb bit set, so that we can use `bx` directly

regular_func forth_repl
    push {r4, r5, r6, r7, lr}
    ldr r0, =DOCOL
    mov r8, r0
    ldr r0, =#0xcafebabe
    ldr r6, =#0xdeadbeef
    push {r0} // bottom of forth stack
next_line:
    ldr r7, =return_stack
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
    ldr r5, =input_ptr
    ldr r4, [r5]
    ldrb r0, [r4]
    ands r0, r0
    beq next_line
    mov r0, r4
    bl parse_word
    movs r4, r0 // save number if needed later
    bl find_word
    beq number // Z is set when nothing was found
    
    // execute word
    // setup threaded code return
    ldr r5, =return_pointer
    bx r0

.align 2
return_pointer:
    .word next_word + 1

    b next_word

number:
    push {r6}
    movs r6, r4 // push number literal
    b next_word // go to next word in line

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
    subs r0, #1 // FIXME: because we already added one before
    str r0, [r1]
    movs r0, r4
    movs r1, r5
    pop {r4, r5, pc}
.pool

// write word r0 to `here` and put back the increased value
regular_func COMMA
    ldr r1, =here
    ldr r2, [r1]
    stmia r2!, {r0}
    str r2, [r1]
    bx lr

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

// allocate a new word and enter compilation mode
// afterwards:
//   - latest should point to here
def_word COLON,":"
    ldr r1, =here
    ldr r2, [r1] // start of allocation area

    ldr r1, =latest
    ldr r0, [r1] // old latest
    push {r1, r2}
    bl COMMA     // write old latest into link field
    pop {r1, r2}
    str r2, [r1] // *latest = *here (before writing anything)

    ldr r1, =input_ptr
    ldr r0, [r1]
    bl parse_word
    
    // write entry
    ldr r0, =here
    ldr r0, [r0] // load current here (after writing link field)
    
    strb r1, [r0]
    adds r0, #1

copy_next_char:
    tst r1, r1
    beq copy_done
    ldrb r3, [r2]
    strb r3, [r0]
    adds r0, #1
    adds r2, #1
    subs r1, #1
    b copy_next_char

copy_done:
    // align and write back here
    adds r0, #3
    movs r1, #3
    bics r0, r1

    // assemble call to DOCOL
    // basically this:
    // mov r0, r8 -> 40 46
    // blx r0     -> 80 47
    // or 0x47804640
    ldr r1, =#0x47804640
    stmia r0!, {r1}

    ldr r1, =here
    str r0, [r1]
    b RBRACK    
        
    // r0: number
    // r1: char count
    // r2: start of word

def_word SEMICOLON,";"
    // write EXIT
    ldr r0, =EXIT+1
    bl COMMA
    b LBRACK

def_word LBRACK,"[" // FIXME: , F_IMMEDIATE
    ldr r0, =state
    movs r1, #0
    str r1, [r0]
    NEXT

def_word RBRACK,"]"
    ldr r0, =state
    movs r1, #1
    str r1, [r0]
    NEXT

// example for compiled colon word
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

.section .data
here:            .word forth_code_area
base:            .word 16
state:           .word 0 // 0 is interpretation mode | 1 is compilation mode
latest:          .word latest_predefined
input_ptr:       .word input_buffer
input_buffer:    .space 256
return_stack:    .space 1000
forth_code_area: .space 1000