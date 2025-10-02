/*--------------------------------------------------------------------------------------

 DMD.cpp - Function and support library for the Freetronics DMD, a 512 LED matrix display
           panel arranged in a 32 x 16 layout. Port na Raspberry Pi + lgpio.

--------------------------------------------------------------------------------------*/
#include "DMD.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*--------------------------------------------------------------------------------------
 Setup and instantiation of DMD library
--------------------------------------------------------------------------------------*/
DMD::DMD(uint8_t panelsWide, uint8_t panelsHigh) {
    DisplaysWide  = panelsWide;
    DisplaysHigh  = panelsHigh;
    DisplaysTotal = DisplaysWide * DisplaysHigh;
    row1 = DisplaysTotal << 4;
    row2 = DisplaysTotal << 5;
    row3 = ((DisplaysTotal << 2) * 3) << 2;
    bDMDScreenRAM = (uint8_t*) malloc(DisplaysTotal * DMD_RAM_SIZE_BYTES);

    // init GPIO + SPI
    hChip = lgGpiochipOpen(0);
    if (hChip < 0) { perror("lgGpiochipOpen"); exit(1); }

    lgGpioClaimOutput(hChip, 0, PIN_DMD_A, 0);
    lgGpioClaimOutput(hChip, 0, PIN_DMD_B, 0);
    lgGpioClaimOutput(hChip, 0, PIN_DMD_CLK, 0);
    lgGpioClaimOutput(hChip, 0, PIN_DMD_SCLK, 0);
    lgGpioClaimOutput(hChip, 0, PIN_DMD_R_DATA, 1);
    lgGpioClaimOutput(hChip, 0, PIN_DMD_nOE, 0);

    hSpi = lgSpiOpen(SPI_BUS, SPI_CHIP, SPI_SPEED, 0);
    if (hSpi < 0) { perror("lgSpiOpen"); exit(1); }

    clearScreen(true);
    bDMDByte = 0;
}

DMD::~DMD() {
    lgSpiClose(hSpi);
    lgGpiochipClose(hChip);
    free(bDMDScreenRAM);
}

/*--------------------------------------------------------------------------------------
 Set or clear a pixel
--------------------------------------------------------------------------------------*/
void DMD::writePixel(unsigned int bX, unsigned int bY, uint8_t bGraphicsMode, uint8_t bPixel) {
    unsigned int uiDMDRAMPointer;

    if (bX >= (DMD_PIXELS_ACROSS*DisplaysWide) || bY >= (DMD_PIXELS_DOWN * DisplaysHigh)) return;

    uint8_t panel = (bX / DMD_PIXELS_ACROSS) + (DisplaysWide * (bY / DMD_PIXELS_DOWN));
    bX = (bX % DMD_PIXELS_ACROSS) + (panel << 5);
    bY = bY % DMD_PIXELS_DOWN;

    uiDMDRAMPointer = bX/8 + bY*(DisplaysTotal<<2);
    uint8_t lookup = bPixelLookupTable[bX & 0x07];

    switch (bGraphicsMode) {
        case GRAPHICS_NORMAL:
            if (bPixel) bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
            else bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
            break;
        case GRAPHICS_INVERSE:
            if (!bPixel) bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
            else bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
            break;
        case GRAPHICS_TOGGLE:
            if (bPixel) {
                if ((bDMDScreenRAM[uiDMDRAMPointer] & lookup) == 0)
                    bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
                else
                    bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
            }
            break;
        case GRAPHICS_OR:
            if (bPixel) bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
            break;
        case GRAPHICS_NOR:
            if (bPixel && ((bDMDScreenRAM[uiDMDRAMPointer] & lookup) == 0))
                bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
            break;
    }
}

/*--------------------------------------------------------------------------------------
 Tekst
--------------------------------------------------------------------------------------*/
void DMD::drawString(int bX, int bY, const char *bChars, uint8_t length, uint8_t bGraphicsMode) {
    if (bX >= (DMD_PIXELS_ACROSS*DisplaysWide) || bY >= DMD_PIXELS_DOWN * DisplaysHigh) return;
    uint8_t height = *(this->Font + FONT_HEIGHT);
    if (bY+height<0) return;

    int strWidth = 0;
    this->drawLine(bX -1 , bY, bX -1 , bY + height, GRAPHICS_INVERSE);

    for (int i = 0; i < length; i++) {
        int charWide = this->drawChar(bX+strWidth, bY, bChars[i], bGraphicsMode);
        if (charWide > 0) {
            strWidth += charWide ;
            this->drawLine(bX + strWidth , bY, bX + strWidth , bY + height, GRAPHICS_INVERSE);
            strWidth++;
        } else if (charWide < 0) return;
        if ((bX + strWidth) >= DMD_PIXELS_ACROSS * DisplaysWide || bY >= DMD_PIXELS_DOWN * DisplaysHigh) return;
    }
}

/*--------------------------------------------------------------------------------------
 Marquee
--------------------------------------------------------------------------------------*/
void DMD::drawMarquee(const char *bChars, uint8_t length, int left, int top) {
    marqueeWidth = 0;
    for (int i = 0; i < length; i++) {
        marqueeText[i] = bChars[i];
        marqueeWidth += charWidth(bChars[i]) + 1;
    }
    marqueeHeight = *(this->Font + FONT_HEIGHT);
    marqueeText[length] = '\0';
    marqueeOffsetY = top;
    marqueeOffsetX = left;
    marqueeLength = length;
    drawString(marqueeOffsetX, marqueeOffsetY, marqueeText, marqueeLength, GRAPHICS_NORMAL);
}

bool DMD::stepMarquee(int amountX, int amountY) {
    bool ret=false;
    marqueeOffsetX += amountX;
    marqueeOffsetY += amountY;
    if (marqueeOffsetX < -marqueeWidth) {
        marqueeOffsetX = DMD_PIXELS_ACROSS * DisplaysWide;
        clearScreen(true);
        ret=true;
    } else if (marqueeOffsetX > DMD_PIXELS_ACROSS * DisplaysWide) {
        marqueeOffsetX = -marqueeWidth;
        clearScreen(true);
        ret=true;
    }
    if (marqueeOffsetY < -marqueeHeight) {
        marqueeOffsetY = DMD_PIXELS_DOWN * DisplaysHigh;
        clearScreen(true);
        ret=true;
    } else if (marqueeOffsetY > DMD_PIXELS_DOWN * DisplaysHigh) {
        marqueeOffsetY = -marqueeHeight;
        clearScreen(true);
        ret=true;
    }

    if (amountY==0 && amountX==-1) {
        for (int i=0; i<DMD_RAM_SIZE_BYTES*DisplaysTotal;i++) {
            if ((i%(DisplaysWide*4)) == (DisplaysWide*4) -1)
                bDMDScreenRAM[i]=(bDMDScreenRAM[i]<<1)+1;
            else
                bDMDScreenRAM[i]=(bDMDScreenRAM[i]<<1) + ((bDMDScreenRAM[i+1] & 0x80) >>7);
        }
        int strWidth=marqueeOffsetX;
        for (uint8_t i=0; i < marqueeLength; i++) {
            int wide = charWidth(marqueeText[i]);
            if (strWidth+wide >= DisplaysWide*DMD_PIXELS_ACROSS) {
                drawChar(strWidth, marqueeOffsetY,marqueeText[i],GRAPHICS_NORMAL);
                return ret;
            }
            strWidth += wide+1;
        }
    } else if (amountY==0 && amountX==1) {
        for (int i=(DMD_RAM_SIZE_BYTES*DisplaysTotal)-1; i>=0;i--) {
            if ((i%(DisplaysWide*4)) == 0)
                bDMDScreenRAM[i]=(bDMDScreenRAM[i]>>1)+128;
            else
                bDMDScreenRAM[i]=(bDMDScreenRAM[i]>>1) + ((bDMDScreenRAM[i-1] & 1) <<7);
        }
        int strWidth=marqueeOffsetX;
        for (uint8_t i=0; i < marqueeLength; i++) {
            int wide = charWidth(marqueeText[i]);
            if (strWidth+wide >= 0) {
                drawChar(strWidth, marqueeOffsetY,marqueeText[i],GRAPHICS_NORMAL);
                return ret;
            }
            strWidth += wide+1;
        }
    } else {
        drawString(marqueeOffsetX, marqueeOffsetY, marqueeText, marqueeLength, GRAPHICS_NORMAL);
    }
    return ret;
}

/*--------------------------------------------------------------------------------------
 Clear the screen
--------------------------------------------------------------------------------------*/
void DMD::clearScreen(uint8_t bNormal) {
    if (bNormal) memset(bDMDScreenRAM,0xFF,DMD_RAM_SIZE_BYTES*DisplaysTotal);
    else memset(bDMDScreenRAM,0x00,DMD_RAM_SIZE_BYTES*DisplaysTotal);
}

/*--------------------------------------------------------------------------------------
 Draw a line
--------------------------------------------------------------------------------------*/
void DMD::drawLine(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    int dy = y2 - y1;
    int dx = x2 - x1;
    int stepx, stepy;

    if (dy < 0) { dy = -dy; stepy = -1; } else { stepy = 1; }
    if (dx < 0) { dx = -dx; stepx = -1; } else { stepx = 1; }
    dy <<= 1; dx <<= 1;

    writePixel(x1, y1, bGraphicsMode, true);
    if (dx > dy) {
        int fraction = dy - (dx >> 1);
        while (x1 != x2) {
            if (fraction >= 0) { y1 += stepy; fraction -= dx; }
            x1 += stepx; fraction += dy;
            writePixel(x1, y1, bGraphicsMode, true);
        }
    } else {
        int fraction = dx - (dy >> 1);
        while (y1 != y2) {
            if (fraction >= 0) { x1 += stepx; fraction -= dy; }
            y1 += stepy; fraction += dx;
            writePixel(x1, y1, bGraphicsMode, true);
        }
    }
}

/*--------------------------------------------------------------------------------------
 Circle
--------------------------------------------------------------------------------------*/
void DMD::drawCircle(int xCenter, int yCenter, int radius, uint8_t bGraphicsMode) {
    int x = 0;
    int y = radius;
    int p = (5 - radius * 4) / 4;
    drawCircleSub(xCenter, yCenter, x, y, bGraphicsMode);
    while (x < y) {
        x++;
        if (p < 0) { p += 2 * x + 1; }
        else { y--; p += 2 * (x - y) + 1; }
        drawCircleSub(xCenter, yCenter, x, y, bGraphicsMode);
    }
}

void DMD::drawCircleSub(int cx, int cy, int x, int y, uint8_t bGraphicsMode) {
    if (x == 0) {
        writePixel(cx, cy + y, bGraphicsMode, true);
        writePixel(cx, cy - y, bGraphicsMode, true);
        writePixel(cx + y, cy, bGraphicsMode, true);
        writePixel(cx - y, cy, bGraphicsMode, true);
    } else if (x == y) {
        writePixel(cx + x, cy + y, bGraphicsMode, true);
        writePixel(cx - x, cy + y, bGraphicsMode, true);
        writePixel(cx + x, cy - y, bGraphicsMode, true);
        writePixel(cx - x, cy - y, bGraphicsMode, true);
    } else if (x < y) {
        writePixel(cx + x, cy + y, bGraphicsMode, true);
        writePixel(cx - x, cy + y, bGraphicsMode, true);
        writePixel(cx + x, cy - y, bGraphicsMode, true);
        writePixel(cx - x, cy - y, bGraphicsMode, true);
        writePixel(cx + y, cy + x, bGraphicsMode, true);
        writePixel(cx - y, cy + x, bGraphicsMode, true);
        writePixel(cx + y, cy - x, bGraphicsMode, true);
        writePixel(cx - y, cy - x, bGraphicsMode, true);
    }
}

/*--------------------------------------------------------------------------------------
 Box
--------------------------------------------------------------------------------------*/
void DMD::drawBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    drawLine(x1, y1, x2, y1, bGraphicsMode);
    drawLine(x2, y1, x2, y2, bGraphicsMode);
    drawLine(x2, y2, x1, y2, bGraphicsMode);
    drawLine(x1, y2, x1, y1, bGraphicsMode);
}

void DMD::drawFilledBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    for (int b = x1; b <= x2; b++) drawLine(b, y1, b, y2, bGraphicsMode);
}

/*--------------------------------------------------------------------------------------
 Test patterns
--------------------------------------------------------------------------------------*/
void DMD::drawTestPattern(uint8_t bPattern) {
    unsigned int ui;
    int numPixels=DisplaysTotal * DMD_PIXELS_ACROSS * DMD_PIXELS_DOWN;
    int pixelsWide=DMD_PIXELS_ACROSS*DisplaysWide;
    for (ui = 0; ui < numPixels; ui++) {
        switch (bPattern) {
            case PATTERN_ALT_0:
                writePixel((ui & (pixelsWide-1)), ((ui & ~(pixelsWide-1)) / pixelsWide), GRAPHICS_NORMAL, (ui & pixelsWide)?!(ui&1):(ui&1));
                break;
            case PATTERN_ALT_1:
                writePixel((ui & (pixelsWide-1)), ((ui & ~(pixelsWide-1)) / pixelsWide), GRAPHICS_NORMAL, (ui & pixelsWide)?(ui&1):!(ui&1));
                break;
            case PATTERN_STRIPE_0:
                writePixel((ui & (pixelsWide-1)), ((ui & ~(pixelsWide-1)) / pixelsWide), GRAPHICS_NORMAL, ui & 1);
                break;
            case PATTERN_STRIPE_1:
                writePixel((ui & (pixelsWide-1)), ((ui & ~(pixelsWide-1)) / pixelsWide), GRAPHICS_NORMAL, !(ui & 1));
                break;
        }
    }
}

/*--------------------------------------------------------------------------------------
 Scan display by SPI
--------------------------------------------------------------------------------------*/
void DMD::scanDisplayBySPI() {
    if (lgGpioRead(hChip, PIN_OTHER_SPI_nCS) == 1) {
        int rowsize=DisplaysTotal<<2;
        int offset=rowsize * bDMDByte;
        for (int i=0;i<rowsize;i++) {
            uint8_t bytes[4] = {
                bDMDScreenRAM[offset+i+row3],
                bDMDScreenRAM[offset+i+row2],
                bDMDScreenRAM[offset+i+row1],
                bDMDScreenRAM[offset+i]
            };
            lgSpiWrite(hSpi, (char*)bytes, 4);
        }

        OE_DMD_ROWS_OFF(hChip);
        LATCH_DMD_SHIFT_REG_TO_OUTPUT(hChip);
        switch (bDMDByte) {
            case 0: LIGHT_DMD_ROW_01_05_09_13(hChip); bDMDByte=1; break;
            case 1: LIGHT_DMD_ROW_02_06_10_14(hChip); bDMDByte=2; break;
            case 2: LIGHT_DMD_ROW_03_07_11_15(hChip); bDMDByte=3; break;
            case 3: LIGHT_DMD_ROW_04_08_12_16(hChip); bDMDByte=0; break;
        }
        OE_DMD_ROWS_ON(hChip);
    }
}

/*--------------------------------------------------------------------------------------
 Font handling
--------------------------------------------------------------------------------------*/
void DMD::selectFont(const uint8_t * font) { this->Font = font; }

int DMD::drawChar(const int bX, const int bY, const unsigned char letter, uint8_t bGraphicsMode) {
    if (bX > (DMD_PIXELS_ACROSS*DisplaysWide) || bY > (DMD_PIXELS_DOWN*DisplaysHigh) ) return -1;
    unsigned char c = letter;
    uint8_t height = *(this->Font + FONT_HEIGHT);
    if (c == ' ') {
        int charWide = charWidth(' ');
        this->drawFilledBox(bX, bY, bX + charWide, bY + height, GRAPHICS_INVERSE);
        return charWide;
    }
    uint8_t width = 0;
    uint8_t bytes = (height + 7) / 8;

    uint8_t firstChar = *(this->Font + FONT_FIRST_CHAR);
    uint8_t charCount = *(this->Font + FONT_CHAR_COUNT);

    uint16_t index = 0;

    if (c < firstChar || c >= (firstChar + charCount)) return 0;
    c -= firstChar;

    if (*(this->Font + FONT_LENGTH) == 0 && *(this->Font + FONT_LENGTH + 1) == 0) {
        width = *(this->Font + FONT_FIXED_WIDTH);
        index = c * bytes * width + FONT_WIDTH_TABLE;
    } else {
        for (uint8_t i = 0; i < c; i++) {
            index += *(this->Font + FONT_WIDTH_TABLE + i);
        }
        index = index * bytes + charCount + FONT_WIDTH_TABLE;
        width = *(this->Font + FONT_WIDTH_TABLE + c);
    }
    if (bX < -width || bY < -height) return width;

    for (uint8_t j = 0; j < width; j++) {
        for (uint8_t i = bytes - 1; i < 254; i--) {
            uint8_t data = *(this->Font + index + j + (i * width));
            int offset = (i * 8);
            if ((i == bytes - 1) && bytes > 1) offset = height - 8;
            for (uint8_t k = 0; k < 8; k++) {
                if ((offset+k >= i*8) && (offset+k <= height)) {
                    if (data & (1 << k)) writePixel(bX + j, bY + offset + k, bGraphicsMode, true);
                    else writePixel(bX + j, bY + offset + k, bGraphicsMode, false);
                }
            }
        }
    }
    return width;
}

int DMD::charWidth(const unsigned char letter) {
    unsigned char c = letter;
    if (c == ' ') c = 'n';
    uint8_t width = 0;

    uint8_t firstChar = *(this->Font + FONT_FIRST_CHAR);
    uint8_t charCount = *(this->Font + FONT_CHAR_COUNT);

    if (c < firstChar || c >= (firstChar + charCount)) return 0;
    c -= firstChar;

    if (*(this->Font + FONT_LENGTH) == 0 && *(this->Font + FONT_LENGTH + 1) == 0) {
        width = *(this->Font + FONT_FIXED_WIDTH);
    } else {
        width = *(this->Font + FONT_WIDTH_TABLE + c);
    }
    return width;
}