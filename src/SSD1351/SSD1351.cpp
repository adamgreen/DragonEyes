// SSD1351 OLED 128x128 display library
// Code originates from:
//    Fast ST7789 IPS 240x240 SPI display library
//    (c) 2019 by Pawel A. Hernik
// Updated in 2020 by Adam Green (https://github.com/adamgreen):
//    Switched to SSD1351 OLED controller.
//    Ported to use mbed SDK.
//    Customized SPI code path for faster pixel transmits on LPC1768.

#include "SSD1351.h"
#include <limits.h>

// If this bit is set in the argument count of the init list then the last argument is the number of milliseconds to
// delay after sending the command.
#define ST_CMD_DELAY   0x80


static const uint8_t initSSD1351[] = {
    16, // 16 commands in list.
    SSD1351_CMD_COMMANDLOCK,
    1, // Set command lock, 1 arg
    0x12,
    SSD1351_CMD_COMMANDLOCK,
    1, // Set command lock, 1 arg
    0xB1,
    SSD1351_CMD_DISPLAYOFF,
    0, // Display off, no args
    SSD1351_CMD_CLOCKDIV,
    1,
    0xF1, // 7:4 = Oscillator Freq, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
    SSD1351_CMD_MUXRATIO,
    1,
    127,
    SSD1351_CMD_DISPLAYOFFSET,
    1,
    0x0,
    SSD1351_CMD_SETGPIO,
    1,
    0x00,
    SSD1351_CMD_FUNCTIONSELECT,
    1,
    0x01, // internal (diode drop)
    SSD1351_CMD_PRECHARGE,
    1,
    0x32,
    SSD1351_CMD_VCOMH,
    1,
    0x05,
    SSD1351_CMD_NORMALDISPLAY,
    0,
    SSD1351_CMD_CONTRASTABC,
    3,
    0xC8,
    0x80,
    0xC8,
    SSD1351_CMD_CONTRASTMASTER,
    1,
    0x0F,
    SSD1351_CMD_SETVSL,
    3,
    0xA0,
    0xB5,
    0x55,
    SSD1351_CMD_PRECHARGE2,
    1,
    0x01,
    SSD1351_CMD_DISPLAYON,
    0  // Main screen turn on
    }; // END OF COMMAND LIST

// Don't need to do anything for SPI_START/END on LPC1768 devices.
#define SPI_START
#define SPI_END

// macros for fast DC and CS state changes
// Make sure that last byte has been successfully trasnmitted before pulling CS inactive by calling m_pSpi->flush().
#define DC_DATA     m_dcPin = 1
#define DC_COMMAND  m_dcPin = 0
#define CS_IDLE     m_pSpi->flush(); m_csPin = 1
#define CS_ACTIVE   m_csPin = 0

inline void SSD1351::writeSPI(uint8_t c)
{
    m_pSpi->transmit(c);
}

// ----------------------------------------------------------
SSD1351::SSD1351(uint16_t width, uint16_t height,
                 FastSpiWriter* pSpi, PinName dcPin, PinName rstPin, PinName csPin) :
    Adafruit_GFX(width, height),
    m_dcPin(dcPin),
    m_rstPin(rstPin),
    m_csPin(csPin)
{
  _width  = width;
  _height = height;
  m_pSpi = pSpi;
}

// ----------------------------------------------------------
void SSD1351::init()
{
  commonInit();
  displayInit(initSSD1351);
  setRotation(0);
}

// ----------------------------------------------------------
void SSD1351::writeCmd(uint8_t c)
{
  DC_COMMAND;
  CS_ACTIVE;
  SPI_START;

  writeSPI(c);

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::writeData(uint8_t c)
{
  DC_DATA;
  CS_ACTIVE;
  SPI_START;

  writeSPI(c);

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::displayInit(const uint8_t *addr)
{
  uint8_t  numCommands, numArgs;
  uint16_t ms;
  numCommands = pgm_read_byte(addr++);   // Number of commands to follow
  while(numCommands--) {                 // For each command...
    writeCmd(pgm_read_byte(addr++));     //   Read, issue command
    numArgs  = pgm_read_byte(addr++);    //   Number of args to follow
    ms       = numArgs & ST_CMD_DELAY;   //   If hibit set, delay follows args
    numArgs &= ~ST_CMD_DELAY;            //   Mask out delay bit
    while(numArgs--) writeData(pgm_read_byte(addr++));

    if(ms) {
      ms = pgm_read_byte(addr++); // Read post-command delay time (ms)
      if(ms == 255) ms = 500;     // If 255, delay for 500 ms
      wait_ms(ms);
    }
  }
}

void SSD1351::commonInit()
{
  const int bitsPerWrite = 8;
  const int spiMode = 0;
  m_pSpi->format(bitsPerWrite, spiMode);
  m_pSpi->frequency(96000000/5);

  if (m_rstPin.is_connected()) {
    CS_ACTIVE;
    m_rstPin = 1;
    wait_ms(50);
    m_rstPin = 0;
    wait_ms(50);
    m_rstPin = 1;
    wait_ms(50);
  }
}

// ----------------------------------------------------------
void SSD1351::setRotation(uint8_t r)
{
  // madctl bits:
  // 6,7 Color depth (01 = 64K)
  // 5   Odd/even split COM (0: disable, 1: enable)
  // 4   Scan direction (0: top-down, 1: bottom-up)
  // 3   Reserved
  // 2   Color remap (0: A->B->C, 1: C->B->A)
  // 1   Column remap (0: 0-127, 1: 127-0)
  // 0   Address increment (0: horizontal, 1: vertical)
  uint8_t madctl = 0b01100100; // 64K, enable split, CBA

  rotation = r & 3; // Clip input to valid range

  switch (rotation) {
  case 0:
    madctl |= 0b00010000; // Scan bottom-up
    _width = WIDTH;
    _height = HEIGHT;
    break;
  case 1:
    madctl |= 0b00010011; // Scan bottom-up, column remap 127-0, vertical
    _width = HEIGHT;
    _height = WIDTH;
    break;
  case 2:
    madctl |= 0b00000010; // Column remap 127-0
    _width = WIDTH;
    _height = HEIGHT;
    break;
  case 3:
    madctl |= 0b00000001; // Vertical
    _width = HEIGHT;
    _height = WIDTH;
    break;
  }

  writeCmd(SSD1351_CMD_SETREMAP);
  writeData(madctl);
  uint8_t startline = (rotation < 2) ? HEIGHT : 0;
  writeCmd(SSD1351_CMD_STARTLINE);
  writeData(startline);
}

void SSD1351::mirrorDisplay(bool mirror)
{
  if (!mirror) {
    setRotation(rotation);
    return;
  }

  // madctl bits:
  // 6,7 Color depth (01 = 64K)
  // 5   Odd/even split COM (0: disable, 1: enable)
  // 4   Scan direction (0: top-down, 1: bottom-up)
  // 3   Reserved
  // 2   Color remap (0: A->B->C, 1: C->B->A)
  // 1   Column remap (0: 0-127, 1: 127-0)
  // 0   Address increment (0: horizontal, 1: vertical)
  const uint8_t mirrorOLED[] = { 0b01110110, // 64k color depth, odd/even split, bottom-up, color remap, column remap, column increment
                                 0b01100111, // 64k color depth, odd/even split, top-down, color remap, column remap, row increment
                                 0b01100100, // 64k color depth, odd/even split, top-down, color remap, column increment
                                 0b01110101 }; // 64k color depth, odd/even split, bottom-up, color remap, row increment
  writeCmd(SSD1351_CMD_SETREMAP);
  writeData(mirrorOLED[rotation]);
}

// ----------------------------------------------------------
void SSD1351::setAddrWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  if (rotation & 1) { // Vertical address increment mode
    uint16_t t1 = x1; x1 = y1; y1 = t1;
    uint16_t t2 = x2; x2 = y2; y2 = t2;
  }
  writeCmd(SSD1351_CMD_SETCOLUMN);
  writeData(x1);
  writeData(x2);

  writeCmd(SSD1351_CMD_SETROW);
  writeData(y1);
  writeData(y2);
  writeCmd(SSD1351_CMD_WRITERAM); // Begin write
}

// ----------------------------------------------------------
void SSD1351::pushColor(uint16_t color)
{
  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  writeSPI(color >> 8); writeSPI(color);

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  if(x<0 ||x>=_width || y<0 || y>=_height) return;
  setAddrWindow(x,y,x+1,y+1);

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  writeSPI(color >> 8); writeSPI(color);

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  if(x>=_width || y>=_height || h<=0) return;
  if(y+h-1>=_height) h=_height-y;
  setAddrWindow(x, y, x, y+h-1);

  uint8_t hi = color >> 8, lo = color;

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  uint8_t num8 = h>>3;
  while(num8--) {
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
  }
  num8 = (uint8_t)h & 7;
  while(num8--) { writeSPI(hi); writeSPI(lo); }

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::drawFastHLine(int16_t x, int16_t y, int16_t w,  uint16_t color)
{
  if(x>=_width || y>=_height || w<=0) return;
  if(x+w-1>=_width)  w=_width-x;
  setAddrWindow(x, y, x+w-1, y);

  uint8_t hi = color >> 8, lo = color;

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  uint8_t num8 = w>>3;
  while(num8--) {
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
  }
  num8 = (uint8_t)w & 7;
  while(num8--) { writeSPI(hi); writeSPI(lo); }

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
void SSD1351::fillScreen(uint16_t color)
{
  fillRect(0, 0,  _width, _height, color);
}

// ----------------------------------------------------------
void SSD1351::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if(x>=_width || y>=_height || w<=0 || h<=0) return;
  if(x+w-1>=_width)  w=_width -x;
  if(y+h-1>=_height) h=_height-y;
  setAddrWindow(x, y, x+w-1, y+h-1);

  uint8_t hi = color >> 8, lo = color;

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  uint32_t num = (uint32_t)w*h;
  uint16_t num16 = num>>4;
  while(num16--) {
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
    writeSPI(hi); writeSPI(lo);
  }
  uint8_t num8 = num & 0xf;
  while(num8--) { writeSPI(hi); writeSPI(lo); }

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
// draws image from RAM
void SSD1351::drawImage(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *img16)
{
  if(x>=_width || y>=_height || w<=0 || h<=0) return;
  setAddrWindow(x, y, x+w-1, y+h-1);

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  uint32_t num = (uint32_t)w*h;
  uint16_t num16 = num>>3;
  uint8_t *img = (uint8_t *)img16;
  while(num16--) {
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
    writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2;
  }
  uint8_t num8 = num & 0x7;
  while(num8--) { writeSPI(*(img+1)); writeSPI(*(img+0)); img+=2; }

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
// draws image from flash.
void SSD1351::drawImageF(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *img16)
{
  if(x>=_width || y>=_height || w<=0 || h<=0) return;
  setAddrWindow(x, y, x+w-1, y+h-1);

  SPI_START;
  DC_DATA;
  CS_ACTIVE;

  uint32_t num = (uint32_t)w*h;
  uint16_t num16 = num>>3;
  uint8_t *img = (uint8_t *)img16;
  while(num16--) {
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
    writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2;
  }
  uint8_t num8 = num & 0x7;
  while(num8--) { writeSPI(pgm_read_byte(img+1)); writeSPI(pgm_read_byte(img+0)); img+=2; }

  CS_IDLE;
  SPI_END;
}

// ----------------------------------------------------------
// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t SSD1351::Color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ----------------------------------------------------------
void SSD1351::invertDisplay(bool mode)
{
  writeCmd(mode ? SSD1351_CMD_INVERTDISPLAY : SSD1351_CMD_NORMALDISPLAY);

  //writeCmd(!mode ? ST7789_INVON : ST7789_INVOFF);  // modes inverted?
}

// ----------------------------------------------------------
void SSD1351::enableDisplay(bool mode)
{
  writeCmd(mode ? SSD1351_CMD_DISPLAYON : SSD1351_CMD_DISPLAYOFF);
  //writeCmd(mode ? ST7789_DISPON : ST7789_DISPOFF);
}

// ------------------------------------------------
// Input a value 0 to 511 (85*6) to get a color value.
// The colours are a transition R - Y - G - C - B - M - R.
void SSD1351::rgbWheel(int idx, uint8_t *_r, uint8_t *_g, uint8_t *_b)
{
  idx &= 0x1ff;
  if(idx < 85) { // R->Y
    *_r = 255; *_g = idx * 3; *_b = 0;
    return;
  } else if(idx < 85*2) { // Y->G
    idx -= 85*1;
    *_r = 255 - idx * 3; *_g = 255; *_b = 0;
    return;
  } else if(idx < 85*3) { // G->C
    idx -= 85*2;
    *_r = 0; *_g = 255; *_b = idx * 3;
    return;
  } else if(idx < 85*4) { // C->B
    idx -= 85*3;
    *_r = 0; *_g = 255 - idx * 3; *_b = 255;
    return;
  } else if(idx < 85*5) { // B->M
    idx -= 85*4;
    *_r = idx * 3; *_g = 0; *_b = 255;
    return;
  } else { // M->R
    idx -= 85*5;
    *_r = 255; *_g = 0; *_b = 255 - idx * 3;
   return;
  }
}

uint16_t SSD1351::rgbWheel(int idx)
{
  uint8_t r,g,b;
  rgbWheel(idx, &r,&g,&b);
  return RGBto565(r,g,b);
}

// ------------------------------------------------