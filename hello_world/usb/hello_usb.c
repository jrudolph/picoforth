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

const uint CLOCK_PIN = 15;
const uint DATA_PIN = 14;

/* Example code to drive a 16x2 LCD panel via a I2C bridge chip (e.g. PCF8574)

   NOTE: The panel must be capable of being driven at 3.3v NOT 5v. The Pico
   GPIO (and therefor I2C) cannot be used at 5v.

   You will need to use a level shifter on the I2C lines if you want to run the
   board at 5v.

   Connections on Raspberry Pi Pico board, other boards may vary.

   GPIO 4 (pin 6)-> SDA on LCD bridge board
   GPIO 5 (pin 7)-> SCL on LCD bridge board
   3.3v (pin 36) -> VCC on LCD bridge board
   GND (pin 38)  -> GND on LCD bridge board
*/
// commands
const int LCD_CLEARDISPLAY = 0x01;
const int LCD_RETURNHOME = 0x02;
const int LCD_ENTRYMODESET = 0x04;
const int LCD_DISPLAYCONTROL = 0x08;
const int LCD_CURSORSHIFT = 0x10;
const int LCD_FUNCTIONSET = 0x20;
const int LCD_SETCGRAMADDR = 0x40;
const int LCD_SETDDRAMADDR = 0x80;

// flags for display entry mode
const int LCD_ENTRYSHIFTINCREMENT = 0x01;
const int LCD_ENTRYLEFT = 0x02;

// flags for display and cursor control
const int LCD_BLINKON = 0x01;
const int LCD_CURSORON = 0x02;
const int LCD_DISPLAYON = 0x04;

// flags for display and cursor shift
const int LCD_MOVERIGHT = 0x04;
const int LCD_DISPLAYMOVE = 0x08;

// flags for function set
const int LCD_5x10DOTS = 0x04;
const int LCD_2LINE = 0x08;
const int LCD_8BITMODE = 0x10;

// flag for backlight control
const int LCD_BACKLIGHT = 0x00;

const int LCD_ENABLE_BIT = 0x80;

// By default these LCD display drivers are on bus address 0x27
static int addr = 0x27;

// Modes for lcd_send_byte
#define LCD_CHARACTER  0x10
#define LCD_COMMAND    0x00

#define MAX_LINES      4
#define MAX_CHARS      20

/* Quick helper function for single byte transfers */
void i2c_write_byte(uint8_t val) {
#ifdef i2c_default
    i2c_write_blocking(i2c_default, addr, &val, 1, false);
#endif
}

void lcd_toggle_enable(uint8_t val) {
    // Toggle enable pin on LCD display
    // We cannot do this too quickly or things don't work
#define DELAY_US 600
    sleep_us(DELAY_US);
    i2c_write_byte(val | LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
    i2c_write_byte(val & ~LCD_ENABLE_BIT);
    sleep_us(DELAY_US);
}

// The display is sent a byte as two separate nibble transfers
void lcd_send_byte(uint8_t val, int mode) {
    uint8_t high = mode | ((val & 0xF0) >> 4) | LCD_BACKLIGHT;
    uint8_t low = mode | (val & 0x0F) | LCD_BACKLIGHT;

    i2c_write_byte(high);
    lcd_toggle_enable(high);
    i2c_write_byte(low);
    lcd_toggle_enable(low);
}

void lcd_clear(void) {
    lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND);
}

// go to location on LCD
void lcd_set_cursor(int line, int position) {
    //uint8_t val = position; 
    //if (line == 0) ;
    //else /* if (line == 1) */ val += 0x40;
    //else if (line == 2) val += 0x14;
    //else if (line == 3) val += 0x54; */
    //lcd_send_byte(LCD_SETDDRAMADDR | val, LCD_COMMAND);
    int addr;
    if (line == 0) addr = position ;
    else if (line == 1) addr = 0x40 + position;
    else if (line == 2) addr = 0x14 + position;
    else addr = 0x54 + position;
    lcd_send_byte(addr | 0x80, LCD_COMMAND);
}

static void inline lcd_char(char val) {
    lcd_send_byte(val, LCD_CHARACTER);
}

void lcd_string(const char *s) {
    while (*s) {
        lcd_char(*s++);
    }
}

void lcd_init() {
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x03, LCD_COMMAND);
    lcd_send_byte(0x02, LCD_COMMAND);

    lcd_send_byte(LCD_ENTRYMODESET | LCD_ENTRYLEFT, LCD_COMMAND);
    lcd_send_byte(LCD_FUNCTIONSET | LCD_2LINE, LCD_COMMAND);
    lcd_send_byte(LCD_DISPLAYCONTROL | LCD_DISPLAYON, LCD_COMMAND);
    lcd_clear();
}

int main2() {
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
    #warning i2c/lcd_1602_i2c example requires a board with I2C pins
#else
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    //bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    lcd_init();

    static char *message[] =
            {
                    "RP2040 by", "Raspberry Pi",
                    "A brand new", "microcontroller",
                    "Twin core M0", "Full C SDK",
                    "More power in", "your product",
                    "More beans", "than Heinz!",
                    "but really...", "who's Heinz???"
            };

    //while (1) {
        for (int m = 0; m < sizeof(message) / sizeof(message[0]); m += MAX_LINES) {
            for (int line = 0; line < MAX_LINES; line++) {
                lcd_set_cursor(line, (MAX_CHARS / 2) - strlen(message[m + line]) / 2);
                lcd_string(message[m + line]);
            }
            sleep_ms(2000);
            lcd_clear();
        }
    //}

    return 0;
#endif
}
void write_led(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg & 0x7f;  // remove read bit as this is a write
    buf[1] = data;
    spi_write_blocking(spi_default, buf, 2);
    spi_write_blocking(spi_default, buf, 2);
    spi_write_blocking(spi_default, buf, 2);
    spi_write_blocking(spi_default, buf, 2);
    sleep_ms(1);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
    sleep_ms(1);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);
}

int leds() {
    spi_init(spi_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

/*         (_SHUTDOWN, 0),
        (_DISPLAYTEST, 0),
        (_SCANLIMIT, 7),
        (_DECODEMODE, 0),
        (_SHUTDOWN, 1),
 */
    write_led(0x0c, 0); // shutdown mode
    write_led(0x0f, 0); // display test off
    write_led(0x0b, 7); // scanlimit 7
    write_led(0x09, 0); // decode mode 0
    write_led(0x0c, 1); // normal mode
    //write_led(0x08, 0x0f); // high intensity

    for (int i = 0; i < 8; i++) {
        write_led(0x0a, i); // raise intensity for each loop cycle

        write_led(0x01, 0xaa);
        write_led(0x02, 0x55);
        write_led(0x03, 0xaa);
        write_led(0x04, 0x55);
        write_led(0x05, 0xaa);
        write_led(0x06, 0x55);
        write_led(0x07, 0xaa);
        write_led(0x08, 0x55);

        sleep_ms(200);

        write_led(0x01, 0x55);
        write_led(0x02, 0xaa);
        write_led(0x03, 0x55);
        write_led(0x04, 0xaa);
        write_led(0x05, 0x55);
        write_led(0x06, 0xaa);
        write_led(0x07, 0x55);
        write_led(0x08, 0xaa);

        sleep_ms(200);
    }

    for (int i = 0; i < 100; i++) {
        if (i & 0x10)
            write_led(0x0a, ~i & 0x0f);
        else
            write_led(0x0a, i & 0x0f); // raise intensity for each loop cycle
        sleep_ms(40);
    }
}

void display() {
    // 12 DC
    // 14 CLK
    // 15 DIN
    // 17 CS

    spi_init(spi1, 100 * 1000);
    gpio_set_function(14, GPIO_FUNC_SPI); // CLK
    gpio_set_function(15, GPIO_FUNC_SPI); // TX

    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);
}

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

int col = 0;
int line = 0;
void lcddata(uint8_t cmd) {
    lcddata_send(cmd);
    col += 1;

    int new_line = col >> 7;
    if (line != new_line) {
        line = new_line & 0x7;
        lcdcommand(0xb0 + line);
        lcdcommand(0x10); // column address 3
        lcdcommand(0x04);
    }
}

void lcdchar(char c) {
    //const unsigned char *glyph = font8x8_basic_cols[c];
    const unsigned char *glyph = tama_font[c - ' '];
    lcddata(glyph[0]);
    lcddata(glyph[1]);
    lcddata(glyph[2]);
    lcddata(glyph[3]);
    lcddata(glyph[4]);
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
    lcdcommand(0xa2); // bias 9
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
    //sleep_ms(100);

    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);
    lcddata(0x55);
    lcddata(0xaa);

    lcdcommand(0x04); // reset to column 3
    lcdstring("Hello World! ");
    lcdstring("Das ist ein extrem langer Text, den ich hier jetzt mal ausfuehrlich widergeben moechte, um zu sehen, ");
    lcdstring("wie der Speicher organisiert ist.");

    /* sleep_ms(1000);
    for (int i = 0; i < 64; i++) {
        lcdcommand(0x40 + i);
        sleep_ms(20);
    } */

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

    printf("Display finished!\n");
    sleep_ms(2000);
}

int main() {
    stdio_init_all();
    
    /* sleep_ms(1000);
    printf("Hello, world!\n"); */

    spi_init(spi_default, 1000 * 1000);
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

int main3() {
    stdio_init_all();
    //gpio_init(CLOCK_PIN);
    //gpio_init(DATA_PIN);
    //gpio_set_dir(CLOCK_PIN, GPIO_IN);
    //gpio_set_dir(DATA_PIN, GPIO_IN);
    
    sleep_ms(1000);
    printf("Hello, world!\n");
    leds();
    printf("Animation finished!\n");
    sleep_ms(3000);
    printf("Starting LCD display code!\n");
    main2();
    printf("Now reading keys\n");

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ps2_program);
    uint sm = pio_claim_unused_sm(pio, true);
    ps2_program_init(pio, sm, offset, CLOCK_PIN, DATA_PIN);
    /* PIO pio = pio0;
    uint offset = pio_add_program(pio, &clocked_input_program);
    uint sm = pio_claim_unused_sm(pio, true);
    clocked_input_program_init(pio, sm, offset, DATA_PIN); */

    while(true) {
        uint32_t rxdata = pio_sm_get_blocking(pio, sm);
        printf("DATA: %08x\n", rxdata);
    }
    /* int counter = 0;
    bool bits[11];
    int bitPos = 0;
    
    while (true) {
        while(gpio_get(CLOCK_PIN)) ; // wait for falling edge
        while(!gpio_get(CLOCK_PIN)) ; // wait for rising edge
        bits[bitPos++] = gpio_get(DATA_PIN);
        
        if (bitPos == 11) {
            printf("DATA [%4d]: %d%d%d%d%d%d%d%d%d%d%d\n", counter++, 
                bits[0],
                bits[1], bits[2], bits[3], bits[4], bits[5], bits[6], bits[7], bits[8],
                bits[9],
                bits[10]);
            bitPos = 0;
        }
    } */
    return 0;
}
