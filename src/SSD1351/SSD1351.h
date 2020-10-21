// SSD1351 OLED 128x128 display library
// Code originates from:
//    Fast ST7789 IPS 240x240 SPI display library
//    (c) 2019 by Pawel A. Hernik
// Updated in 2020 by Adam Green (https://github.com/adamgreen):
//    Switched to SSD1351 OLED controller.
//    Ported to use mbed SDK.
//    Customized SPI code path for faster pixel transmits on LPC1768.

#ifndef _SSD1351_FAST_H_
#define _SSD1351_FAST_H_


#include <mbed.h>
#include <Adafruit_GFX.h>

// Macro to convert AVR pgm_read_*() calls to simple dereferences on ARM.
#define pgm_read_word(ADDR) (*(uint16_t*)(ADDR))
#define pgm_read_byte(ADDR) (*(uint8_t*)(ADDR))


#define SSD1351_CMD_SETCOLUMN 0x15      ///< See datasheet
#define SSD1351_CMD_SETROW 0x75         ///< See datasheet
#define SSD1351_CMD_WRITERAM 0x5C       ///< See datasheet
#define SSD1351_CMD_READRAM 0x5D        ///< Not currently used
#define SSD1351_CMD_SETREMAP 0xA0       ///< See datasheet
#define SSD1351_CMD_STARTLINE 0xA1      ///< See datasheet
#define SSD1351_CMD_DISPLAYOFFSET 0xA2  ///< See datasheet
#define SSD1351_CMD_DISPLAYALLOFF 0xA4  ///< Not currently used
#define SSD1351_CMD_DISPLAYALLON 0xA5   ///< Not currently used
#define SSD1351_CMD_NORMALDISPLAY 0xA6  ///< See datasheet
#define SSD1351_CMD_INVERTDISPLAY 0xA7  ///< See datasheet
#define SSD1351_CMD_FUNCTIONSELECT 0xAB ///< See datasheet
#define SSD1351_CMD_DISPLAYOFF 0xAE     ///< See datasheet
#define SSD1351_CMD_DISPLAYON 0xAF      ///< See datasheet
#define SSD1351_CMD_PRECHARGE 0xB1      ///< See datasheet
#define SSD1351_CMD_DISPLAYENHANCE 0xB2 ///< Not currently used
#define SSD1351_CMD_CLOCKDIV 0xB3       ///< See datasheet
#define SSD1351_CMD_SETVSL 0xB4         ///< See datasheet
#define SSD1351_CMD_SETGPIO 0xB5        ///< See datasheet
#define SSD1351_CMD_PRECHARGE2 0xB6     ///< See datasheet
#define SSD1351_CMD_SETGRAY 0xB8        ///< Not currently used
#define SSD1351_CMD_USELUT 0xB9         ///< Not currently used
#define SSD1351_CMD_PRECHARGELEVEL 0xBB ///< Not currently used
#define SSD1351_CMD_VCOMH 0xBE          ///< See datasheet
#define SSD1351_CMD_CONTRASTABC 0xC1    ///< See datasheet
#define SSD1351_CMD_CONTRASTMASTER 0xC7 ///< See datasheet
#define SSD1351_CMD_MUXRATIO 0xCA       ///< See datasheet
#define SSD1351_CMD_COMMANDLOCK 0xFD    ///< See datasheet
#define SSD1351_CMD_HORIZSCROLL 0x96    ///< Not currently used
#define SSD1351_CMD_STOPSCROLL 0x9E     ///< Not currently used
#define SSD1351_CMD_STARTSCROLL 0x9F    ///< Not currently used


// Color definitions
#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800
#define TFT_GREEN 0x07E0
#define TFT_BLUE 0x001F
#define TFT_CYAN 0x07FF
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW 0xFFE0
#define TFT_ORANGE 0xFC00

#define RGBto565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))


// A faster version of the SPI master class for mbed-1768 devices when only transmitting (as for displays).
class FastSpiWriter : public SPI
{
  public:
    FastSpiWriter(PinName mosi, PinName miso, PinName sclk, PinName ssel) : SPI(mosi, miso, sclk, ssel)
    {
    }

    inline void transmit(int value)
    {
      while (!transmitFifoNotFull()) {
      }
      _spi.spi->DR = value;
    }

    inline void flush()
    {
      while (busy()) {
      }
    }

  protected:
    inline int transmitFifoNotFull()
    {
        return _spi.spi->SR & (1 << 1);
    }

    inline int busy()
    {
        return _spi.spi->SR & (1 << 4);
    }
};


class SSD1351 : public Adafruit_GFX
{
 public:
  SSD1351(uint16_t width, uint16_t height,
          FastSpiWriter* pSpi, PinName dcPin, PinName rstPin = NC, PinName csPin = NC);

  void init();
  void begin() { init(); }
  void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
  void pushColor(uint16_t color);
  void fillScreen(uint16_t color=TFT_BLACK);
  void clearScreen() { fillScreen(TFT_BLACK); }
  void cls() { fillScreen(TFT_BLACK); }
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawImage(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t *img);
  void drawImageF(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *img16);
  void drawImageF(int16_t x, int16_t y, const uint16_t *img16) { drawImageF(x,y,pgm_read_word(img16),pgm_read_word(img16+1),img16+3); }
  void setRotation(uint8_t r);
  void mirrorDisplay(bool mirror);
  void invertDisplay(bool mode);
  void enableDisplay(bool mode);

  uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return Color565(r, g, b); }
  void rgbWheel(int idx, uint8_t *_r, uint8_t *_g, uint8_t *_b);
  uint16_t rgbWheel(int idx);

 protected:
  void displayInit(const uint8_t *addr);
  void writeSPI(uint8_t);
  void writeCmd(uint8_t c);
  void writeData(uint8_t d);
  void commonInit();

 private:
  FastSpiWriter*        m_pSpi;
  DigitalOut            m_dcPin;
  DigitalOut            m_rstPin;
  DigitalOut            m_csPin;
};

#endif
