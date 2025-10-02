#ifndef DMD_H_
#define DMD_H_

#include <stdint.h>
#include <string.h>
#include <lgpio.h>

// ============================================================================
// KONFIGURACJA PINÓW RASPBERRY PI (BCM numbering)
// ============================================================================
#define PIN_DMD_nOE       5
#define PIN_DMD_A         6
#define PIN_DMD_B         13
#define PIN_DMD_CLK       11    // SPI0_SCLK (sprzętowy)
#define PIN_DMD_SCLK      19
#define PIN_DMD_R_DATA    10    // SPI0_MOSI (sprzętowy)
#define PIN_OTHER_SPI_nCS 8     // SPI0_CE0

// ============================================================================
// Makra sterujące GPIO
// ============================================================================
#define LIGHT_DMD_ROW_01_05_09_13(chip) { lgGpioWrite(chip, PIN_DMD_B, 0); lgGpioWrite(chip, PIN_DMD_A, 0); }
#define LIGHT_DMD_ROW_02_06_10_14(chip) { lgGpioWrite(chip, PIN_DMD_B, 0); lgGpioWrite(chip, PIN_DMD_A, 1); }
#define LIGHT_DMD_ROW_03_07_11_15(chip) { lgGpioWrite(chip, PIN_DMD_B, 1); lgGpioWrite(chip, PIN_DMD_A, 0); }
#define LIGHT_DMD_ROW_04_08_12_16(chip) { lgGpioWrite(chip, PIN_DMD_B, 1); lgGpioWrite(chip, PIN_DMD_A, 1); }

#define LATCH_DMD_SHIFT_REG_TO_OUTPUT(chip) { lgGpioWrite(chip, PIN_DMD_SCLK, 1); lgGpioWrite(chip, PIN_DMD_SCLK, 0); }
#define OE_DMD_ROWS_OFF(chip) { lgGpioWrite(chip, PIN_DMD_nOE, 0); }
#define OE_DMD_ROWS_ON(chip)  { lgGpioWrite(chip, PIN_DMD_nOE, 1); }

// ============================================================================
// Tryby grafiki
// ============================================================================
#define GRAPHICS_NORMAL    0
#define GRAPHICS_INVERSE   1
#define GRAPHICS_TOGGLE    2
#define GRAPHICS_OR        3
#define GRAPHICS_NOR       4

// Wzory testowe
#define PATTERN_ALT_0     0
#define PATTERN_ALT_1     1
#define PATTERN_STRIPE_0  2
#define PATTERN_STRIPE_1  3

// ============================================================================
// Rozmiar panelu
// ============================================================================
#define DMD_PIXELS_ACROSS 32
#define DMD_PIXELS_DOWN   16
#define DMD_BITSPERPIXEL  1
#define DMD_RAM_SIZE_BYTES ((DMD_PIXELS_ACROSS * DMD_BITSPERPIXEL / 8) * DMD_PIXELS_DOWN)

static uint8_t bPixelLookupTable[8] =
{
   0x80,   // bit 7
   0x40,   // bit 6
   0x20,   // bit 5
   0x10,   // bit 4
   0x08,   // bit 3
   0x04,   // bit 2
   0x02,   // bit 1
   0x01    // bit 0
};

// ============================================================================
// Stałe dla fontów
// ============================================================================
#define FONT_LENGTH             0
#define FONT_FIXED_WIDTH        2
#define FONT_HEIGHT             3
#define FONT_FIRST_CHAR         4
#define FONT_CHAR_COUNT         5
#define FONT_WIDTH_TABLE        6

typedef uint8_t (*FontCallback)(const uint8_t*);

// ============================================================================
// Klasa główna DMD
// ============================================================================
class DMD {
public:
    DMD(uint8_t panelsWide, uint8_t panelsHigh);
    ~DMD();

    // Pixel / grafika
    void writePixel(unsigned int bX, unsigned int bY, uint8_t bGraphicsMode, uint8_t bPixel);
    void clearScreen(uint8_t bNormal);

    // Tekst
    void drawString(int bX, int bY, const char* bChars, uint8_t length, uint8_t bGraphicsMode);
    void selectFont(const uint8_t* font);
    int drawChar(const int bX, const int bY, const unsigned char letter, uint8_t bGraphicsMode);
    int charWidth(const unsigned char letter);

    // Efekty przewijania
    void drawMarquee(const char* bChars, uint8_t length, int left, int top);
    bool stepMarquee(int amountX, int amountY);

    // Kształty
    void drawLine(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawCircle(int xCenter, int yCenter, int radius, uint8_t bGraphicsMode);
    void drawBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawFilledBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawTestPattern(uint8_t bPattern);

    // Aktualizacja
    void scanDisplayBySPI();

private:
    void drawCircleSub(int cx, int cy, int x, int y, uint8_t bGraphicsMode);

    // Bufor RAM dla ekranu
    uint8_t *bDMDScreenRAM;

    // Czcionka
    const uint8_t* Font;

    // Tekst przewijany
    char marqueeText[256];
    uint8_t marqueeLength;
    int marqueeWidth;
    int marqueeHeight;
    int marqueeOffsetX;
    int marqueeOffsetY;

    // Informacje o wyświetlaczu
    uint8_t DisplaysWide;
    uint8_t DisplaysHigh;
    uint8_t DisplaysTotal;
    volatile uint8_t bDMDByte;

    // Handlery lgpio
    int hChip;
    int hSpi;
};

#endif /* DMD_H_ */