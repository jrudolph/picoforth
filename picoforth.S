.syntax unified
.cpu cortex-m0plus
.thumb

#include "pico/asm_helper.S"

// This whole project was spawned after reading about miniforth at
// https://niedzejkob.p4.team/bootstrap/miniforth/#keyboard-input. This is heavily inspired and
// in some cases an almost literal translation from x86 assembler.

.section .text."x"

// our register conventions while running forth code:
//   stack contains user values
//   r7 return stack pointer
//   r6 contains top of stack
//   r5 threaded code pointer, values need to have thumb bit set, so that we can use `bx` directly

regular_func forth_repl
    push {r4, r5, r6, r7, lr}
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
    // r0: pointer to word code, r1: flags
    ldr r2, =state
    ldr r2, [r2]
    tst r0, r0
    beq number
    
    orrs r1, r1, r2 // FIXME: only compare against F_IMMEDIATE not against all flags
    beq compile_word

    // execute word
    // setup threaded code return
    ldr r5, =return_pointer
    bx r0

.align 2
return_pointer:
    .word next_word + 1

    b next_word

compile_word:
    bl COMMA
    b next_word

number:
    tst r2, r2
    beq compile_number

    push {r6}
    movs r6, r4 // push number literal
    b next_word // go to next word in line

compile_number:
    ldr r0, =LIT + 1
    bl COMMA
    mov r0, r4
    bl COMMA
    b next_word

    pop {r4, r5, r6, r7, pc}

// returns
// r0: entry point for forth word or 0 if none found
// r1: flags
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
    movs r4, LENGTH_MASK
    ands r0, r0, r4
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

    ldrb r1, [r3, #4]
    movs r2, FLAGS_MASK
    ands r1, r1, r2

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
    NEXT

LIT:
    push {r6}
    ldm r5!, {r6}
    // TODO: NEXT does another `ldm r5!, {r1}` directly afterwards, so it would be nice coalesce them
    // that's currently not possible because we would need a register > r6 to be loaded afterwards
    // but r7 is already taken (but maybe let's use a different calling convention later on. We
    // have full control over the calling convention for assembled FORTH words.
    NEXT

.set F_IMMEDIATE, 0x80
.set LENGTH_MASK, 0x3f
.set FLAGS_MASK, 0xc0

.macro def_word label,name,flags=0
.align 2
___header_\label:
1:
.word 1f            // pointer to next dict entry
.byte 3f - 2f + \flags     // size of name
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

def_word SWAP,swap
    pop {r1}
    push {r6}
    mov r6, r1
    NEXT

def_word UDOT,u.
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
    str r2, [r1] // *latest = *here (before writing anything)
   
    bl COMMA     // write old latest into link field

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
    // What we want is just
    //   bl DOCOL
    //
    // This instruction uses addressing relative to PC.
    // 
    // However, since COLON lives in flash and the generated code in SRAM
    // the difference is ~ 0x10000000 which is much more than the supported
    // 25 bit of range that bl offers.
    //
    // Unfortunately, there are no instructions that allow to have big enough
    // immediate operands, so even if the address of DOCOL is known here, it
    // is pretty hard to reify it into the generated code.
    //
    // A former solution was to keep the address of DOCOL in r8 for now,
    // which is easily accessible from generated code. However, r8 is not necessarily
    // safe to be used, since it can be mangled by any calls to C functions or interrupt
    // handlers, so we would need to make sure to always save r8 before calling any external functions!
    // 
    // Translation would be easy, though, and basically this:
    // mov r0, r8 -> 40 46 // cannot call from r8 directly
    // blx r0     -> 80 47
    // or 0x47804640
    //
    // ldr r1, =#0x47804640
    // stmia r0!, {r1}
    //
    // An alternative is using a trampoline inside the .data section which is easy enough to
    // reach with a relative jump, so let's try this
    // What we want to assemble is
    //   bl DOCOL_TRAMPOLINE
    // We assume that it is a forward jump because we try to put DOCOL_TRAMPOLINE behind the
    // forth_code_area
    // Then `bl [PC, #0]` is 0xf000f800, jumps up to 4096 bytes forward should be representable by
    // just ORing the relative position with that base instruction. Let's try this!
    ldr r1, =#0xf000f800
    ldr r2, =DOCOL_TRAMPOLINE
    subs r2, r0     // relative address to the new code's position
    subs r2, 4      // it's one word too large because we need the address after we have written
    lsrs r2, r2, #1 // lowest bit isn't encoded in the instruction (redundant because of alignment)
    orrs r1, r1, r2 // build instruction
    rev r1, r1      // reverse bytes...
    rev16 r1, r1    // oh, yes, rev destroyed the little-endian byte order of the halfwords, it would probably be easier to use strh twice, instead
    stmia r0!, {r1} // store instruction

    ldr r1, =here
    str r0, [r1]
    b RBRACK    
        
def_word SEMICOLON,";",F_IMMEDIATE
    // write EXIT
    ldr r0, =EXIT+1
    bl COMMA
    b LBRACK

.set INTERPRETER_MODE, 1
.set COMPILER_MODE, 0

def_word LBRACK,"[",F_IMMEDIATE
    ldr r0, =state
    movs r1, INTERPRETER_MODE
    str r1, [r0]
    NEXT

def_word RBRACK,"]"
    ldr r0, =state
    movs r1, COMPILER_MODE
    str r1, [r0]
    NEXT

// example for encoding of compiled colon word
def_word DOUBLE,double
    bl DOCOL
    do LIT
    do 0x1234
    do DUP
    do PLUS
    do EXIT

def_word PEEK,"@" // (addr -- val)
    ldr r6, [r6]
    NEXT

def_word POKE,"!" // (val addr -- )
    pop {r0,r1}
    str r0, [r6]
    movs r1, r6
    NEXT

def_word CPEEK,"c@" // (addr -- ch)
    ldrb r6, [r6]
    NEXT

def_word CPOKE,"c!" // (ch addr -- )
    pop {r0,r1}
    strb r0, [r6]
    movs r1, r6
    NEXT

def_word TO_R,">r" // (val -- )
    stmia r7!, {r6}
    pop {r6}
    NEXT

def_word FROM_R,"r>" // ( -- val)
    push {r6}
    subs r7, #4
    ldr r6, [r7]
    NEXT

def_word EMIT,emit // (ch -- )
    mov r0, r6
    bl put_char
    pop {r6}
    NEXT

// final word definition needs to be written manually
.align 2
1:
.word 0
.word 0

.section .data
here:            .word forth_code_area
base:            .word 16
state:           .word INTERPRETER_MODE
latest:          .word latest_predefined
input_ptr:       .word input_buffer
input_buffer:    .space 256
return_stack:    .space 1000
forth_code_area: .space 1000

// trampoline for code living in .data to be able to call DOCOL
DOCOL_TRAMPOLINE:
    ldr r0, =DOCOL
    bx r0
.pool
