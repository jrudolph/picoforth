/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "ps2.pio.h"
#include "clocked_input.pio.h"

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
    //sleep_ms(1);
    spi_write_blocking (spi_default, &cmd, 1);
    //sleep_ms(1000);
}

void lcddata_send(uint8_t cmd) {
    gpio_put(LCDA0_PIN, 1);
    //sleep_ms(1);
    spi_write_blocking (spi_default, &cmd, 1);
}

bool current_buffer = 0;
uint8_t frame_buffers[2][8][128]; // 8 lines of 128 columns of 8 pixel height = 64 * 128 bit
uint8_t (*frame_buf)[8][128] = &frame_buffers[0];

int col = 0;
int line = 0;
void lcddata(uint8_t cmd) {
    //lcddata_send(cmd);
    uint8_t l = (col >> 7) & 0x7;
    if (col == 128) {
        uint8_t x = 12;
        x++;
        sleep_ms(1);
    }
    (*frame_buf)[l][(col++)&0x7f] = cmd;
    /* col += 1;

    int new_line = col >> 7;
    if (line != new_line) {
        line = new_line & 0x7;
        //lcdcommand(0xb0 + line);
        //lcdcommand(0x10); // column address 4
        //lcdcommand(0x04);
    } */
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

void animate_pixels() {
    for (int l = 7; l >= 0; l--) {
        for (int c = 0; c < 128; c++) {
            uint8_t data = (*frame_buf)[l][c];
            //frame_buf[l][c] = data << 1;
            //if ((data & 0x80) && l < 7)
            //    frame_buf[l + 1][c] = frame_buf[l + 1][c] | 0x01;
            uint8_t new_data = 0;
            if (data) {
                if ((data & 0x80))
                    for (int p = 7; p >= 0; p--) {
                        bool is_set = (data >> p) & 1;
                        if (is_set) {
                            bool below;
                            if (p == 7)
                                if (l == 7) below = 1;
                                else below = (*frame_buf)[l + 1][c] & 1;
                            else below = (new_data >> (p + 1)) & 1;

                            if (below) new_data |= 1 << p; // keep pixel if below is also set
                            else { // shift one down
                                if (p == 7)
                                    if (l < 7)
                                        (*frame_buf)[l + 1][c] |= 1; // set highest pixel of row below
                                    else ; //we would need to set line 8 which has already pixels set by definition
                                else
                                new_data |= 1 << (p + 1);
                            }
                        }
                    }
                else
                    new_data = data << 1; // we can shift directly;
            }
            (*frame_buf)[l][c] = new_data;
        }
    }
}

int lcdrun(/* uint a0, uint res, uint cs1 */) {
    /* LCDA0_PIN = a0;
    LCDRES_PIN = res;
    LCDCS1_PIN = cs1; */
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
    lcdcommand(0x22); // resistor divider contrast 2 (0..7) (values don't make a difference)
    lcdcommand(0xaf); // display on
    lcdcommand(0x81); // dynamic contrast (values don't make a difference)
    lcdcommand(31);

    /* for (int i = 0; i < 8; i += 1) {
        lcdcommand(0x28 | i); // resistor divider contrast 2 (0..7)
    } */

        //     spi.write([
    //  0xA3, // bias 7 (0xA2 for bias 9)
    //  0xA0, // no horiz flip
    //  0xC8, // vertical flip
    //  0x40, // top left start
    //  0x2F, // all power on (adafruit does this one at a time, but it doesn't seem to be required)
    //  0x20|2, // resistor divider contrast 2 (0..7)
    //  0xAF, // display on
    //  0x81, // dynamic contrast
    //  31 // 0..63

    //lcdstring("Hello");
    //lcdcommand(0xb1);
    sleep_ms(100);

    // lcddata(0xaa);
    // lcddata(0x55);
    // lcddata(0xaa);
    // lcddata(0x55);
    // lcddata(0xaa);
    // lcddata(0x55);
    // lcddata(0xaa);

    //lcdcommand(0x04); // reset to column 3
    lcdstring("Hello Werld! ");
    lcdstring("Das ist ein extrem langer Text, den ich hier jetzt mal ausfuehrlich wiedergeben moechte, um zu sehen, ");
    lcdstring("wie der Speicher organisiert ist.");
    // sleep_ms(1000);
    // for (int i = 0; i < 64; i++) {
    //     lcdcommand(0x40 + i);
    //     sleep_ms(20);
    // }

    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    paint_buffer();

    sleep_ms(1000);
    for (int i = 0; i < 1000; i++) {
        animate_pixels();
        paint_buffer();
        sleep_ms(31);
    }

    printf("Display finished!\n");
    sleep_ms(2000);
}

int main() {
    stdio_init_all();
    
    /* sleep_ms(1000);
    printf("Hello, world!\n"); */

    spi_init(spi_default, 2000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    gpio_init(LCDCS1_PIN);
    gpio_set_dir(LCDCS1_PIN, GPIO_OUT);
    gpio_init(LCDA0_PIN);
    gpio_set_dir(LCDA0_PIN, GPIO_OUT);
    gpio_init(LCDRES_PIN);
    gpio_set_dir(LCDRES_PIN, GPIO_OUT);

    //lcdrun(12, 13, 17);
    //lcdrun(12, 17, 13);
    //lcdrun(13, 12, 17);
    lcdrun(/* 17, 12, 13 */);
    //lcdrun(13, 17, 12);
    //lcdrun(17, 12, 13);
    //lcdrun(17, 13, 12);
    while(true);
}
