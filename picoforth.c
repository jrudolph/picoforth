#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "ps2.pio.h"

#include "tama-mini02-font.h"
#include "font8x8_basic.h"

uint LCDA0_PIN = 13;
uint LCDRES_PIN = 12;
uint LCDCS1_PIN = 17;
const uint LCDCLK_PIN = 18;
const uint LCDTX_PIN = 19;

void lcdcommand(uint8_t cmd) {
    printf("CMD: %x\n", cmd);
    gpio_put(LCDA0_PIN, 0);
    spi_write_blocking (spi_default, &cmd, 1);
}

void lcddata_send(uint8_t cmd) {
    gpio_put(LCDA0_PIN, 1);
    spi_write_blocking (spi_default, &cmd, 1);
}

bool current_buffer = 0;
uint8_t frame_buffers[2][8][128]; // 8 lines of 128 columns of 8 pixel height = 64 * 128 bit
uint8_t (*frame_buf)[8][128] = &frame_buffers[0];

int col = 0;
void lcddata(uint8_t cmd) {
    uint8_t l = (col >> 7) & 0x7;
    (*frame_buf)[l][(col++)&0x7f] = cmd;
}
void new_line() {
    uint8_t line = ((col >> 7) + 1) & 0x07;
    col = line << 7;
    for (int c = 0; c < 128; c++)
        (*frame_buf)[line][c] = 0;
}
void paint_buffer() {
    for (uint8_t l = 0; l < 8; l++) {
        lcdcommand(0xb0 | l);
        lcdcommand(0x10); // select column 4
        lcdcommand(0x04);
        for (uint8_t c = 0; c < 128; c++)
            lcddata_send(frame_buffers[0][l][c] | frame_buffers[1][l][c]);
    }
    current_buffer = !current_buffer;
    void *old_buf = frame_buf;
    frame_buf = &(frame_buffers[current_buffer & 1]);
    memcpy(frame_buf, old_buf, 128 * 8);
}

void lcdchar(char c) {
    //const unsigned char *glyph = font8x8_basic_cols[c];
    const unsigned char *glyph = tama_font[c - ' '];
    lcddata(glyph[0]);
    lcddata(glyph[1]);
    if (glyph[2]) lcddata(glyph[2]);
    if (glyph[3]) lcddata(glyph[3]);
    if (glyph[4]) lcddata(glyph[4]);
    lcddata(0);
    //lcddata(glyph[5]);
    //lcddata(glyph[6]);
    //lcddata(glyph[7]);
}
uint8_t char_width(char c) {
    const unsigned char *glyph = tama_font[c - ' '];
    return 3 + (glyph[2] != 0) + (glyph[3] != 0) + (glyph[4] != 0);
}

void lcdstring(char *str) {
    size_t len = strnlen(str, 100);
    for (int i = 0; i < len; i++)
        lcdchar(str[i]);
}

void transpose_font(char (*source_font)[][8], char (*target_font)[][8], int num_chars) {
    for (int ch = 0; ch < 128; ch++) {
        for (int col = 0; col < 8; col++) {
            char c = 0;
            for (int row = 0; row < 8; row++) {
                c |= ((((*source_font)[ch][row]) >> col) & 1) << row;
            }
            (*target_font)[ch][col] = c;
        }
    }
}

void lcdinit() {
    transpose_font(&font8x8_basic, &font8x8_basic_cols, 128);

    printf("a0: %d res: %d cs: %d\n", LCDA0_PIN, LCDRES_PIN, LCDCS1_PIN);
    gpio_put(LCDCS1_PIN, 0);

    gpio_put(LCDRES_PIN, 0);
    sleep_ms(500);
    gpio_put(LCDRES_PIN, 1);
    sleep_ms(1);

    //lcdcommand(0xa5);
    lcdcommand(0xe2); // RESET
    lcdcommand(0xa2); // bias 1/9, 0xa3 would be 1/7 in which case the display is almost black
    lcdcommand(0xa1); // horiz flip
    lcdcommand(0xc8); // vertical flip
    lcdcommand(0x40); // top left start
    lcdcommand(0x2f); // all power on (adafruit does this one at a time, but it doesn't seem to be required)
    //lcdcommand(0x22); // resistor divider contrast 2 (0..7) (values don't make a difference)
    //lcdcommand(0x81); // dynamic contrast (values don't make a difference)
    //lcdcommand(31);

    /* for (int i = 0; i < 8; i += 1) {
        lcdcommand(0x28 | i); // resistor divider contrast 2 (0..7)
    } */

    //lcdstring("Hello");
    //lcdcommand(0xb1);
    sleep_ms(100);

    //lcdstring("> ");
    lcdcommand(0xaf); // display on
    paint_buffer();
}

#define CLOCK_PIN 15
#define DATA_PIN 14

// mapping generated with scala code, copied from table at https://techdocs.altium.com/display/FPGA/PS2+Keyboard+Scan+Codes
// val codeMap = "11621e32642552e63673d83e946045-4e=55q15w1de24r2dt2cy35u3ci43o44p4d[54]5b\\5da1cs1bd23f2bg34h33j3bk42l4b;4c'52z1ax22c21v2ab32n31m3a,41.49/4a 29".grouped(3).map { case d => (java.lang.Integer.parseInt(d.tail, 16),d.head) }.toMap
// (0 until 128).map { i => codeMap.getOrElse(i, '?') }.mkString // needs fixing for escaped chars
char code_to_char_lower[128] =
    "?????????????????????q1???zsaw2??cxde43?? vftr5??nbhgy6???mju78??,kio09??./l;p-???'\n[=?????]?\\??????????????????????????????????";
char code_to_char[128] =
    "?????????????????????Q!???ZSAW@??CXDE$#?? VFTR%??NBHGY^???MJU&*??<KIO)(??>?L:P_???\"\n{+?????}?|??????????????????????????????????";

void put_char(uint8_t ch) {
    if (ch == '\n') new_line();
    else lcdchar(ch);
    paint_buffer();
}

void print_number(uint32_t num) {
    char buffer[100];
    snprintf(buffer, 100, "0x%x", num);
    lcdstring(buffer);
    put_char(' ');
    paint_buffer();
}

PIO pio;
uint sm;

uint8_t read_line(char* buffer) {
    uint8_t read = 0;
    bool shift_pressed = false;
    while(read < 256) {
        uint8_t code = ps2_program_getc(pio, sm);
        if (code == 0x5a) { // enter
            put_char('\n');
            buffer[read] = 0;
            return read;
        }
        else if (code == 0x59 || code == 0x12) { // right shift / left shift
            shift_pressed = true;
        }
        else if (code == 0x66) { // backspace
            if (read) {
                read -= 1;
                uint8_t w = char_width(buffer[read]);
                col -= w;
                for (int i = 0; i < w; i++)
                    lcddata(0);
                paint_buffer();
                paint_buffer();
                col -= w;
            }
        }
        else if (code < 128) {
            char ch;
            if (shift_pressed) ch = code_to_char[code];
            else ch = code_to_char_lower[code];

            buffer[read++] = ch;
            put_char(ch);
        }
        else if (code == 0xf0) {
            // skip release code
            code = ps2_program_getc(pio, sm);
            if (code == 0x59 || code == 0x12)
                shift_pressed = false;
        }
    }
}

void init_keyboard() {
    gpio_init(CLOCK_PIN);
    gpio_init(DATA_PIN);
    gpio_set_dir(CLOCK_PIN, GPIO_IN);
    gpio_set_dir(DATA_PIN, GPIO_IN);

    pio = pio0;
    uint offset = pio_add_program(pio, &ps2_program);
    sm = pio_claim_unused_sm(pio, true);
    ps2_program_init(pio, sm, offset, CLOCK_PIN, DATA_PIN);
}

void forth_repl();
void exec_double_test();
void forth_init() {
    lcdinit();
    init_keyboard();
    //exec_double_test();

    while(true)
        forth_repl();
    /* char buffer[256];

    while(true) {
        uint8_t read = read_line(buffer);
        snprintf(buffer, 100, "Read %d chars", read);
        lcdstring(buffer);
        put_char('\n');
    } */
}

int main() {
    stdio_init_all();
    
    spi_init(spi_default, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    gpio_init(LCDCS1_PIN);
    gpio_set_dir(LCDCS1_PIN, GPIO_OUT);
    gpio_init(LCDA0_PIN);
    gpio_set_dir(LCDA0_PIN, GPIO_OUT);
    gpio_init(LCDRES_PIN);
    gpio_set_dir(LCDRES_PIN, GPIO_OUT);

    forth_init();

    while(true) tight_loop_contents();
}
