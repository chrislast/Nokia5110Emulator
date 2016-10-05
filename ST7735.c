// ST7735.c
//===========================================================================
//
//  Code written for TM4C123GH6PM using LCD pin configuration of 
//  BOOSTXL-EDUMKII (using Launchpad SSI2 for ST7735 LCD)
//  https://github.com/chrislast/Nokia5110Emulator
//
//  Description:
//  This source file could be used as the basis for a standalone driver for the
//  BOOSTXL-EDUMKII ST7735 LCD but it's main purpose is to provide emulation
//  of the Nokia5110 display allowing Launchpad software written for that device
//  to be displayed in a 84x48 window on the ST7735 LCD
//
//  Usage:
//  1. Remove Nokia5110.c from your 6.03x project workspace (no need to delete the file)
//  2. Add this file ST7735.c to your 6.03x project workspace
//  3. Depending on your project directory location on disk you may need to 
//     change the #include path to "../inc/tm4c123gh6pm.h" below
//  4. Compile, Flash, Run!
// 
//  Notes:
//  1. No header file is required for Nokia 5110 emulation as it implements the interface  
//     described in your existing Nokia5110.h
//  2. Please note that all other hardware interfaces e.g. sound, buttons, leds, slide pot etc.
//     are not implemented by this software
//
//============================================================================
//   TM4C123G Launchpad pin usage
//   PIN#  GPIO  description
//   #31 | PF4 | LCD_RS
//   #17 | PF0 | LCD_RESET
//   #13 | PA4 | LCD_CS_NOT (or SPI SS)
//   #15 | PB7 | LCD_MOSI (hardware SPI)
//   #14 | PB6 | not used (would be MISO)
//   #7  | PB4 | LCD_SCK (hardware SPI)
//
//  CRYSTALFONTZ CFAF128128B-0145T 128X128 SPI COLOR 1.45" TFT
//
//  ref: https://www.crystalfontz.com/product/cfaf128128b0145t
//
//  2016 - 10 - 04 Chris Last
//       with thanks to  Brent A. Crosby whose 
//                 driver for Arduino R3 I based this on
// Some original Nokia5110 source functions are
// Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
//    You may use, edit, run or distribute this file
//    as long as the above copyright notice remains
//
//===========================================================================
//
//For more information, please refer to 
//============================================================================
//
// Display is Crystalfontz CFAF128128B-0145T
//   https://www.crystalfontz.com/product/cfaf128128b0145t
//
// The controller is a Sitronix ST7735S
//   http://www.crystalfontz.com/controllers/Sitronix/ST7735S/
//
//============================================================================

#include <stdint.h>
#include "Nokia5110.h"
#include "../inc/tm4c123gh6pm.h"

// Declare the Emulator Nokia5110 replacement functions 
void Nokia5110Emu_Init(void);
void Nokia5110Emu_OutChar(unsigned char data);
void Nokia5110Emu_SetCursor(unsigned char newX, unsigned char newY);
void Nokia5110Emu_Clear(void);
void Nokia5110Emu_DrawFullImage(const char *ptr);
// Declare some private functions for SPI control 
static void delay(unsigned long msec);
static void SPI_transfer(uint8_t byte);
static void SPI_sendCommand(uint8_t command);
static void SPI_sendData(uint8_t data);
// private functions for LCD window control
static void LCD_resize_window (uint16_t xsize, uint16_t ysize);
static void LCD_reset_window(void);
static void LCD_send_data(const char* buffer);

void Initialize_LCD(void);
void Initialize_SPI(void);
void Initialize_Launchpad(void);

// Define some constants
#define BIT(X)    (1L<<(X))

#define LCD_RS_B BIT(4)
#define LCD_RESET_B BIT(0)
#define LCD_CS_B BIT(4)
#define LCD_MOSI_B BIT(7)
#define LCD_SCK_B BIT(4)

#define CLR_RS    (GPIO_PORTF_DATA_R &= ~LCD_RS_B)
#define SET_RS    (GPIO_PORTF_DATA_R |=  LCD_RS_B)
#define CLR_RESET (GPIO_PORTF_DATA_R &= ~LCD_RESET_B)
#define SET_RESET (GPIO_PORTF_DATA_R |=  LCD_RESET_B)
#define CLR_CS    (GPIO_PORTA_DATA_R &= ~LCD_CS_B)
#define SET_CS    (GPIO_PORTA_DATA_R |=  LCD_CS_B)
#define CLR_MOSI  (GPIO_PORTB_DATA_R &= ~LCD_MOSI_B)
#define SET_MOSI  (GPIO_PORTB_DATA_R |=  LCD_MOSI_B)
#define CLR_SCK   (GPIO_PORTB_DATA_R &= ~LCD_SCK_B)
#define SET_SCK   (GPIO_PORTB_DATA_R |=  LCD_SCK_B)

// Maximum dimensions of the Nokia LCD, although the pixels are
// numbered from zero to (MAX-1).  Address may automatically
// be incremented after each transmission.
#define NOKIA_MAX_X                   84
#define NOKIA_MAX_Y                   48

// Maximum dimensions of the ST7735, although the pixels are
// numbered from zero to (MAX-1).  Address may automatically
// be incremented after each transmission.
#define ST7735_MAX_X            128
#define ST7735_MAX_Y            128

// This table contains the hex values that represent pixels
// for a font that is 6 pixels wide and 8 pixels high
#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8

// 12-bit RGB value to represent pixel on or pixel off
//                     RGB 
#define PIXEL_ON   (0x0000)  // Black
#define PIXEL_OFF  (0x0FFF)  // White

// Declare Module-private data
// modified ASCII character table with explicit blank column
static const char ASCII6[][6] = {
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} // 20
  ,{0x00, 0x00, 0x5f, 0x00, 0x00, 0x00} // 21 !
  ,{0x00, 0x07, 0x00, 0x07, 0x00, 0x00} // 22 "
  ,{0x14, 0x7f, 0x14, 0x7f, 0x14, 0x00} // 23 #
  ,{0x24, 0x2a, 0x7f, 0x2a, 0x12, 0x00} // 24 $
  ,{0x23, 0x13, 0x08, 0x64, 0x62, 0x00} // 25 %
  ,{0x36, 0x49, 0x55, 0x22, 0x50, 0x00} // 26 &
  ,{0x00, 0x05, 0x03, 0x00, 0x00, 0x00} // 27 '
  ,{0x00, 0x1c, 0x22, 0x41, 0x00, 0x00} // 28 (
  ,{0x00, 0x41, 0x22, 0x1c, 0x00, 0x00} // 29 )
  ,{0x14, 0x08, 0x3e, 0x08, 0x14, 0x00} // 2a *
  ,{0x08, 0x08, 0x3e, 0x08, 0x08, 0x00} // 2b +
  ,{0x00, 0x50, 0x30, 0x00, 0x00, 0x00} // 2c ,
  ,{0x08, 0x08, 0x08, 0x08, 0x08, 0x00} // 2d -
  ,{0x00, 0x60, 0x60, 0x00, 0x00, 0x00} // 2e .
  ,{0x20, 0x10, 0x08, 0x04, 0x02, 0x00} // 2f /
  ,{0x3e, 0x51, 0x49, 0x45, 0x3e, 0x00} // 30 0
  ,{0x00, 0x42, 0x7f, 0x40, 0x00, 0x00} // 31 1
  ,{0x42, 0x61, 0x51, 0x49, 0x46, 0x00} // 32 2
  ,{0x21, 0x41, 0x45, 0x4b, 0x31, 0x00} // 33 3
  ,{0x18, 0x14, 0x12, 0x7f, 0x10, 0x00} // 34 4
  ,{0x27, 0x45, 0x45, 0x45, 0x39, 0x00} // 35 5
  ,{0x3c, 0x4a, 0x49, 0x49, 0x30, 0x00} // 36 6
  ,{0x01, 0x71, 0x09, 0x05, 0x03, 0x00} // 37 7
  ,{0x36, 0x49, 0x49, 0x49, 0x36, 0x00} // 38 8
  ,{0x06, 0x49, 0x49, 0x29, 0x1e, 0x00} // 39 9
  ,{0x00, 0x36, 0x36, 0x00, 0x00, 0x00} // 3a :
  ,{0x00, 0x56, 0x36, 0x00, 0x00, 0x00} // 3b ;
  ,{0x08, 0x14, 0x22, 0x41, 0x00, 0x00} // 3c <
  ,{0x14, 0x14, 0x14, 0x14, 0x14, 0x00} // 3d =
  ,{0x00, 0x41, 0x22, 0x14, 0x08, 0x00} // 3e >
  ,{0x02, 0x01, 0x51, 0x09, 0x06, 0x00} // 3f ?
  ,{0x32, 0x49, 0x79, 0x41, 0x3e, 0x00} // 40 @
  ,{0x7e, 0x11, 0x11, 0x11, 0x7e, 0x00} // 41 A
  ,{0x7f, 0x49, 0x49, 0x49, 0x36, 0x00} // 42 B
  ,{0x3e, 0x41, 0x41, 0x41, 0x22, 0x00} // 43 C
  ,{0x7f, 0x41, 0x41, 0x22, 0x1c, 0x00} // 44 D
  ,{0x7f, 0x49, 0x49, 0x49, 0x41, 0x00} // 45 E
  ,{0x7f, 0x09, 0x09, 0x09, 0x01, 0x00} // 46 F
  ,{0x3e, 0x41, 0x49, 0x49, 0x7a, 0x00} // 47 G
  ,{0x7f, 0x08, 0x08, 0x08, 0x7f, 0x00} // 48 H
  ,{0x00, 0x41, 0x7f, 0x41, 0x00, 0x00} // 49 I
  ,{0x20, 0x40, 0x41, 0x3f, 0x01, 0x00} // 4a J
  ,{0x7f, 0x08, 0x14, 0x22, 0x41, 0x00} // 4b K
  ,{0x7f, 0x40, 0x40, 0x40, 0x40, 0x00} // 4c L
  ,{0x7f, 0x02, 0x0c, 0x02, 0x7f, 0x00} // 4d M
  ,{0x7f, 0x04, 0x08, 0x10, 0x7f, 0x00} // 4e N
  ,{0x3e, 0x41, 0x41, 0x41, 0x3e, 0x00} // 4f O
  ,{0x7f, 0x09, 0x09, 0x09, 0x06, 0x00} // 50 P
  ,{0x3e, 0x41, 0x51, 0x21, 0x5e, 0x00} // 51 Q
  ,{0x7f, 0x09, 0x19, 0x29, 0x46, 0x00} // 52 R
  ,{0x46, 0x49, 0x49, 0x49, 0x31, 0x00} // 53 S
  ,{0x01, 0x01, 0x7f, 0x01, 0x01, 0x00} // 54 T
  ,{0x3f, 0x40, 0x40, 0x40, 0x3f, 0x00} // 55 U
  ,{0x1f, 0x20, 0x40, 0x20, 0x1f, 0x00} // 56 V
  ,{0x3f, 0x40, 0x38, 0x40, 0x3f, 0x00} // 57 W
  ,{0x63, 0x14, 0x08, 0x14, 0x63, 0x00} // 58 X
  ,{0x07, 0x08, 0x70, 0x08, 0x07, 0x00} // 59 Y
  ,{0x61, 0x51, 0x49, 0x45, 0x43, 0x00} // 5a Z
  ,{0x00, 0x7f, 0x41, 0x41, 0x00, 0x00} // 5b [
  ,{0x02, 0x04, 0x08, 0x10, 0x20, 0x00} // 5c '\'
  ,{0x00, 0x41, 0x41, 0x7f, 0x00, 0x00} // 5d ]
  ,{0x04, 0x02, 0x01, 0x02, 0x04, 0x00} // 5e ^
  ,{0x40, 0x40, 0x40, 0x40, 0x40, 0x00} // 5f _
  ,{0x00, 0x01, 0x02, 0x04, 0x00, 0x00} // 60 `
  ,{0x20, 0x54, 0x54, 0x54, 0x78, 0x00} // 61 a
  ,{0x7f, 0x48, 0x44, 0x44, 0x38, 0x00} // 62 b
  ,{0x38, 0x44, 0x44, 0x44, 0x20, 0x00} // 63 c
  ,{0x38, 0x44, 0x44, 0x48, 0x7f, 0x00} // 64 d
  ,{0x38, 0x54, 0x54, 0x54, 0x18, 0x00} // 65 e
  ,{0x08, 0x7e, 0x09, 0x01, 0x02, 0x00} // 66 f
  ,{0x0c, 0x52, 0x52, 0x52, 0x3e, 0x00} // 67 g
  ,{0x7f, 0x08, 0x04, 0x04, 0x78, 0x00} // 68 h
  ,{0x00, 0x44, 0x7d, 0x40, 0x00, 0x00} // 69 i
  ,{0x20, 0x40, 0x44, 0x3d, 0x00, 0x00} // 6a j
  ,{0x7f, 0x10, 0x28, 0x44, 0x00, 0x00} // 6b k
  ,{0x00, 0x41, 0x7f, 0x40, 0x00, 0x00} // 6c l
  ,{0x7c, 0x04, 0x18, 0x04, 0x78, 0x00} // 6d m
  ,{0x7c, 0x08, 0x04, 0x04, 0x78, 0x00} // 6e n
  ,{0x38, 0x44, 0x44, 0x44, 0x38, 0x00} // 6f o
  ,{0x7c, 0x14, 0x14, 0x14, 0x08, 0x00} // 70 p
  ,{0x08, 0x14, 0x14, 0x18, 0x7c, 0x00} // 71 q
  ,{0x7c, 0x08, 0x04, 0x04, 0x08, 0x00} // 72 r
  ,{0x48, 0x54, 0x54, 0x54, 0x20, 0x00} // 73 s
  ,{0x04, 0x3f, 0x44, 0x40, 0x20, 0x00} // 74 t
  ,{0x3c, 0x40, 0x40, 0x20, 0x7c, 0x00} // 75 u
  ,{0x1c, 0x20, 0x40, 0x20, 0x1c, 0x00} // 76 v
  ,{0x3c, 0x40, 0x30, 0x40, 0x3c, 0x00} // 77 w
  ,{0x44, 0x28, 0x10, 0x28, 0x44, 0x00} // 78 x
  ,{0x0c, 0x50, 0x50, 0x50, 0x3c, 0x00} // 79 y
  ,{0x44, 0x64, 0x54, 0x4c, 0x44, 0x00} // 7a z
  ,{0x00, 0x08, 0x36, 0x41, 0x00, 0x00} // 7b {
  ,{0x00, 0x00, 0x7f, 0x00, 0x00, 0x00} // 7c |
  ,{0x00, 0x41, 0x36, 0x08, 0x00, 0x00} // 7d }
  ,{0x10, 0x08, 0x08, 0x10, 0x08, 0x00} // 7e ~
//  ,{0x78, 0x46, 0x41, 0x46, 0x78, 0x00} // 7f DEL
  ,{0x1f, 0x24, 0x7c, 0x24, 0x1f, 0x00} // 7f UT sign
};
// LCD RAM data window data 
static uint8_t LCD_cursor_x;
static uint8_t LCD_cursor_y;
static uint8_t LCD_window_x;
static uint8_t LCD_window_width;
static uint8_t LCD_window_y;
static uint8_t LCD_window_height;

// Declare the Global screen buffer originally defined in Nokia 5110.c
char Screen[SCREENW*SCREENH/8]; // buffer stores the next image to be printed on the screen

/**************************** Code *******************************************/

//============================================================================
// busy delay loop for simple timing of time sensitive wait states
// Subroutine to delay in units of milliseconds
// Inputs:  Number of milliseconds to delay
// Outputs: None
// Notes:   assumes 80 MHz clock
static void delay(unsigned long msec)
{
	  unsigned long j;
	  while (msec-- > 0)
				for (j=6000; j>0; j--);
}

//============================================================================
// Send one byte over Hardware SPI
static void SPI_transfer(uint8_t byte)
{
    // wait until SSI2 not busy/transmit FIFO empty
    while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
    SSI2_DR_R = byte; // send a byte
    // wait until SSI2 not busy/transmit FIFO empty
    while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
}
//============================================================================
// Send a command on Hardware SPI
static void SPI_sendCommand(uint8_t command)
{
  // Select the LCD's command register
  CLR_RS;
  // Select the LCD controller
  CLR_CS;
  //Send the command via SPI:
  SPI_transfer(command);
  // Deselect the LCD controller
  CLR_CS;
}
//============================================================================
// Send data on Hardware SPI
static void SPI_sendData(uint8_t data)
{
  // Select the LCD's data register
  SET_RS;
  // Select the LCD controller
  CLR_CS;
  //Send the command via SPI:
  SPI_transfer(data);
  // Deselect the LCD controller
  CLR_CS;
}

//----------------------------------------------------------------------------
// Defines for the ST7735 registers. Unused commands are commented out
// ref: https://www.crystalfontz.com/products/document/3277/ST7735_V2.1_20100505.pdf

/************* SYSTEM FUNCTION COMMANDS ************************
#define ST7735_NOP      (0x00) // No Operation
#define ST7735_SWRESET  (0x01) // software Reset
#define ST7735_RDDID    (0x04) // Read Display ID
#define ST7735_RDDST    (0x09) // Read Display Status
#define ST7735_RDDPM    (0x0A) // Read Display Power Mode
#define ST7735_RDDMADCTL (0x0B) // Read Display MADCTL
#define ST7735_RDDCOLMOD (0x0C) // Read Display Pixel Format
#define ST7735_RDDIM    (0x0D) // Read Display Image Mode
#define ST7735_RDDSM    (0x0E) // Read Display Signal Mode
#define ST7735_RDDSR    (0x0F) // Read Display Self-Diagnostic Result
#define ST7735_SLPIN    (0x10) // Sleep In & Booster Off */
#define ST7735_SLPOUT   (0x11) // Sleep Out & Booster On
#define ST7735_PTLON    (0x12) /* Partial Mode On 
#define ST7735_NORON    (0x13) // Partial Mode Off (Normal)
#define ST7735_INVOFF   (0x20) // Diplay Inversion Off (Normal)
#define ST7735_INVON    (0x21) // Display Inversion On
#define ST7735_GAMSET   (0x26) // Gamma Curve Select */
#define ST7735_DISPOFF  (0x28) // Display Off
#define ST7735_DISPON   (0x29) // Display On
#define ST7735_CASET    (0x2A) // Column Address Set
#define ST7735_RASET    (0x2B) // Row Address Set
#define ST7735_RAMWR    (0x2C) /* Memory Write
#define ST7735_RGBSET   (0x2D) // LUT for 4k,65k,262k colour display */
#define ST7735_RAMRD    (0x2E) // Memory Read
#define ST7735_PTLAR    (0x30) /* Partial Start/End Address
#define ST7735_SCRLAR   (0x33) // Scroll Area Set */
#define ST7735_TEOFF    (0x34) // Tearing Effect Line Off
#define ST7735_TEON     (0x35) // Tearing Effect Mode Set & On
#define ST7735_MADCTL   (0x36) /* Memory Data Acess Control
#define ST7735_VSCSAD   (0x37) // Scroll Start Address of RAM
#define ST7735_IDMOFF   (0x38) // Idle Mode Off
#define ST7735_IDMON    (0x39) // Idle Mode On */
#define ST7735_COLMOD   (0x3A) /* Interface Pixel Format
#define ST7735_RDID1    (0xDA) // Read ID1
#define ST7735_RDID2    (0xDB) // Read ID2
#define ST7735_RDID3    (0xDC) // Read ID3 */

/*********** PANEL FUNCTION COMMANDS *****************/
#define ST7735_FRMCTR1  (0xB1) // In Normal Mode (Full Colours)
#define ST7735_FRMCTR2  (0xB2) // In Idle Mode (8 colours)
#define ST7735_FRMCTR3  (0xB3) // In Partial Mode + Full Colous
#define ST7735_INVCTR   (0xB4) // Display Inversion Control
#define ST7735_PWCTR1   (0xC0) // Power Control Setting
#define ST7735_PWCTR2   (0xC1) // Power Control Setting
#define ST7735_PWCTR3   (0xC2) // Power Control Setting in Normal Mode
#define ST7735_PWCTR4   (0xC3) // Power Control Setting in Idle Mode
#define ST7735_PWCTR5   (0xC4) // Power Control Setting in Partial Mode
#define ST7735_VMCTR1   (0xC5) /* VCOM Control 1
#define ST7735_VMOFCTR  (0xC7) // Set VCOM Offset Control
#define ST7735_WRID2    (0xD1) // Set LCM Version Code
#define ST7735_WRID3    (0xD2) // Customer Project Code
#define ST7735_NVCTR1   (0xD9) // NVM Control Status
#define ST7735_NVCTR1   (0xDE) // NVM Read Command
#define ST7735_NVCTR1   (0xDF) // NVM Write Command */
#define ST7735_GAMCTRP1 (0xE0) // Gamma Adjust +ve Polarity
#define ST7735_GAMCTRN1 (0xE1) // Gamma Adjust -ve Polarity

//----------------------------------------------------------------------------
// Initialize the ST7735 LCD
void Initialize_LCD(void)
{
  //Reset the LCD controller
  CLR_RESET;
  delay(1);//10µS min
  SET_RESET;
  delay(150);

  //SLPOUT (11h): Sleep Out ("Sleep Out"  is chingrish for "wake")
  //The DC/DC converter is enabled, Internal display oscillator
  //is started, and panel scanning is started.
  SPI_sendCommand(ST7735_SLPOUT);
  delay(120);

  //FRMCTR1 (B1h): Frame Rate Control (In normal mode/ Full colors)
  //Set the frame frequency of the full colors normal mode.
  // * Frame rate=fosc/((RTNA + 20) x (LINE + FPA + BPA))
  // * 1 < FPA(front porch) + BPA(back porch) ; Back porch ?0
  //Note: fosc = 333kHz
  SPI_sendCommand(ST7735_FRMCTR1);//In normal mode(Full colors)
  SPI_sendData(0x01);//RTNB: set 1-line period
  SPI_sendData(0x2c);//FPB:  front porch
  SPI_sendData(0x2d);//BPB:  back porch

  //FRMCTR2 (B2h): Frame Rate Control (In Idle mode/ 8-colors)
  //Set the frame frequency of the Idle mode.
  // * Frame rate=fosc/((RTNB + 20) x (LINE + FPB + BPB))
  // * 1 < FPB(front porch) + BPB(back porch) ; Back porch ?0
  //Note: fosc = 333kHz
  SPI_sendCommand(ST7735_FRMCTR2);//In Idle mode (8-colors)
  SPI_sendData(0x01);//RTNB: set 1-line period
  SPI_sendData(0x2c);//FPB:  front porch
  SPI_sendData(0x2d);//BPB:  back porch

  //FRMCTR3 (B3h): Frame Rate Control (In Partial mode/ full colors)
  //Set the frame frequency of the Partial mode/ full colors.
  // * 1st parameter to 3rd parameter are used in line inversion mode.
  // * 4th parameter to 6th parameter are used in frame inversion mode.
  // * Frame rate=fosc/((RTNC + 20) x (LINE + FPC + BPC))
  // * 1 < FPC(front porch) + BPC(back porch) ; Back porch ?0
  //Note: fosc = 333kHz
  SPI_sendCommand(ST7735_FRMCTR3);//In partial mode + Full colors
  SPI_sendData(0x01);//RTNB: set 1-line period
  SPI_sendData(0x2c);//FPB:  front porch
  SPI_sendData(0x2d);//BPB:  back porch
  SPI_sendData(0x01);//RTNB: set 1-line period
  SPI_sendData(0x2c);//FPB:  front porch
  SPI_sendData(0x2d);//BPB:  back porch

  //INVCTR (B4h): Display Inversion Control
  SPI_sendCommand(ST7735_INVCTR);
  SPI_sendData(0x07);
  // 0000 0ABC
  // |||| ||||-- NLC: Inversion setting in full Colors partial mode
  // |||| |||         (0=Line Inversion, 1 = Frame Inversion)
  // |||| |||--- NLB: Inversion setting in idle mode
  // |||| ||          (0=Line Inversion, 1 = Frame Inversion)
  // |||| ||---- NLA: Inversion setting in full Colors normal mode
  // |||| |----- Unused: 0

  //PWCTR1 (C0h): Power Control 1
  SPI_sendCommand(ST7735_PWCTR1);
  SPI_sendData(0x02);// VRH[4:0] (0-31) Sets GVDD
                     // VRH=0x00 => GVDD=5.0v
                     // VRH=0x1F => GVDD=3.0v
                     // Each tick is a variable step:
                     // VRH[4:0] |  VRH | GVDD
                     //   00000b | 0x00 | 5.00v
                     //   00001b | 0x01 | 4.75v
                     //   00010b | 0x02 | 4.70v <<<<<
                     //   00011b | 0x03 | 4.65v
                     //   00100b | 0x04 | 4.60v
                     //   00101b | 0x05 | 4.55v
                     //   00110b | 0x06 | 4.50v
                     //   00111b | 0x07 | 4.45v
                     //   01000b | 0x08 | 4.40v
                     //   01001b | 0x09 | 4.35v
                     //   01010b | 0x0A | 4.30v
                     //   01011b | 0x0B | 4.25v
                     //   01100b | 0x0C | 4.20v
                     //   01101b | 0x0D | 4.15v
                     //   01110b | 0x0E | 4.10v
                     //   01111b | 0x0F | 4.05v
                     //   10000b | 0x10 | 4.00v
                     //   10001b | 0x11 | 3.95v
                     //   10010b | 0x12 | 3.90v
                     //   10011b | 0x13 | 3.85v
                     //   10100b | 0x14 | 3.80v
                     //   10101b | 0x15 | 3.75v
                     //   10110b | 0x16 | 3.70v
                     //   10111b | 0x17 | 3.65v
                     //   11000b | 0x18 | 3.60v
                     //   11001b | 0x19 | 3.55v
                     //   11010b | 0x1A | 3.50v
                     //   11011b | 0x1B | 3.45v
                     //   11100b | 0x1C | 3.40v
                     //   11101b | 0x1D | 3.35v
                     //   11110b | 0x1E | 3.25v
                     //   11111b | 0x1F | 3.00v
  SPI_sendData(0x02);// 010i i000
                     // |||| ||||-- Unused: 0
                     // |||| |----- IB_SEL0:
                     // ||||------- IB_SEL1:
                     // |||-------- Unused: 010
                     // IB_SEL[1:0] | IB_SEL | AVDD
                     //         00b | 0x00   | 2.5µA   <<<<<
                     //         01b | 0x01   | 2.0µA
                     //         10b | 0x02   | 1.5µA
                     //         11b | 0x03   | 1.0µA

  //PWCTR2 (C1h): Power Control 2
  // * Set the VGH and VGL supply power level
  //Restriction: VGH-VGL <= 32V
  SPI_sendCommand(ST7735_PWCTR2);
  SPI_sendData(0xC5);// BT[2:0] (0-15) Sets GVDD
                     // BT[2:0] |    VGH      |     VGL
                     //    000b | 4X |  9.80v | -3X |  -7.35v
                     //    001b | 4X |  9.80v | -4X |  -9.80v
                     //    010b | 5X | 12.25v | -3X |  -7.35v
                     //    011b | 5X | 12.25v | -4X |  -9.80v
                     //    100b | 5X | 12.25v | -5X | -12.25v
                     //    101b | 6X | 14.70v | -3X |  -7.35v   <<<<<
                     //    110b | 6X | 14.70v | -4X |  -9.80v
                     //    111b | 6X | 14.70v | -5X | -12.25v

  //PWCTR3 (C2h): Power Control 3 (in Normal mode/ Full colors)
  // * Set the amount of current in Operational amplifier in
  //   normal mode/full colors.
  // * Adjust the amount of fixed current from the fixed current
  //   source in the operational amplifier for the source driver.
  // * Set the Booster circuit Step-up cycle in Normal mode/ full
  //   colors.
  SPI_sendCommand(ST7735_PWCTR3);
  SPI_sendData(0x0D);// AP[2:0] Sets Operational Amplifier Bias Current
                     // AP[2:0] | Function
                     //    000b | Off
                     //    001b | Small
                     //    010b | Medium Low
                     //    011b | Medium
                     //    100b | Medium High
                     //    101b | Large          <<<<<
                     //    110b | reserved
                     //    111b | reserved
  SPI_sendData(0x00);// DC[2:0] Booster Frequency
                     // DC[2:0] | Circuit 1 | Circuit 2,4
                     //    000b | BCLK / 1  | BCLK / 1  <<<<<
                     //    001b | BCLK / 1  | BCLK / 2
                     //    010b | BCLK / 1  | BCLK / 4
                     //    011b | BCLK / 2  | BCLK / 2
                     //    100b | BCLK / 2  | BCLK / 4
                     //    101b | BCLK / 4  | BCLK / 4
                     //    110b | BCLK / 4  | BCLK / 8
                     //    111b | BCLK / 4  | BCLK / 16

  //PWCTR4 (C3h): Power Control 4 (in Idle mode/ 8-colors)
  // * Set the amount of current in Operational amplifier in
  //   normal mode/full colors.
  // * Adjust the amount of fixed current from the fixed current
  //   source in the operational amplifier for the source driver.
  // * Set the Booster circuit Step-up cycle in Normal mode/ full
  //   colors.
  SPI_sendCommand(ST7735_PWCTR4);
  SPI_sendData(0x8D);// AP[2:0] Sets Operational Amplifier Bias Current
                     // AP[2:0] | Function
                     //    000b | Off
                     //    001b | Small
                     //    010b | Medium Low
                     //    011b | Medium
                     //    100b | Medium High
                     //    101b | Large          <<<<<
                     //    110b | reserved
                     //    111b | reserved
  SPI_sendData(0x1A);// DC[2:0] Booster Frequency
                     // DC[2:0] | Circuit 1 | Circuit 2,4
                     //    000b | BCLK / 1  | BCLK / 1
                     //    001b | BCLK / 1  | BCLK / 2
                     //    010b | BCLK / 1  | BCLK / 4  <<<<<
                     //    011b | BCLK / 2  | BCLK / 2
                     //    100b | BCLK / 2  | BCLK / 4
                     //    101b | BCLK / 4  | BCLK / 4
                     //    110b | BCLK / 4  | BCLK / 8
                     //    111b | BCLK / 4  | BCLK / 16

  //PPWCTR5 (C4h): Power Control 5 (in Partial mode/ full-colors)
  // * Set the amount of current in Operational amplifier in
  //   normal mode/full colors.
  // * Adjust the amount of fixed current from the fixed current
  //   source in the operational amplifier for the source driver.
  // * Set the Booster circuit Step-up cycle in Normal mode/ full
  //   colors.
  SPI_sendCommand(ST7735_PWCTR5);
  SPI_sendData(0x8D);// AP[2:0] Sets Operational Amplifier Bias Current
                     // AP[2:0] | Function
                     //    000b | Off
                     //    001b | Small
                     //    010b | Medium Low
                     //    011b | Medium
                     //    100b | Medium High
                     //    101b | Large          <<<<<
                     //    110b | reserved
                     //    111b | reserved
  SPI_sendData(0xEE);// DC[2:0] Booster Frequency
                     // DC[2:0] | Circuit 1 | Circuit 2,4
                     //    000b | BCLK / 1  | BCLK / 1
                     //    001b | BCLK / 1  | BCLK / 2
                     //    010b | BCLK / 1  | BCLK / 4
                     //    011b | BCLK / 2  | BCLK / 2
                     //    100b | BCLK / 2  | BCLK / 4
                     //    101b | BCLK / 4  | BCLK / 4
                     //    110b | BCLK / 4  | BCLK / 8  <<<<<
                     //    111b | BCLK / 4  | BCLK / 16

  //VMCTR1 (C5h): VCOM Control 1
  SPI_sendCommand(ST7735_VMCTR1);
  SPI_sendData(0x51);// Default: 0x51 => +4.525
                     // VMH[6:0] (0-100) Sets VCOMH
                     // VMH=0x00 => VCOMH= +2.5v
                     // VMH=0x64 => VCOMH= +5.0v
  SPI_sendData(0x4D);// Default: 0x4D => -0.575
                     // VML[6:0] (4-100) Sets VCOML
                     // VML=0x04 => VCOML= -2.4v
                     // VML=0x64 => VCOML=  0.0v

  //GMCTRP1 (E0h): Gamma ‘+’polarity Correction Characteristics Setting
  SPI_sendCommand(ST7735_GAMCTRP1);
  SPI_sendData(0x0a);
  SPI_sendData(0x1c);
  SPI_sendData(0x0c);
  SPI_sendData(0x14);
  SPI_sendData(0x33);
  SPI_sendData(0x2b);
  SPI_sendData(0x24);
  SPI_sendData(0x28);
  SPI_sendData(0x27);
  SPI_sendData(0x25);
  SPI_sendData(0x2C);
  SPI_sendData(0x39);
  SPI_sendData(0x00);
  SPI_sendData(0x05);
  SPI_sendData(0x03);
  SPI_sendData(0x0d);

  //GMCTRN1 (E1h): Gamma ‘-’polarity Correction Characteristics Setting
  SPI_sendCommand(ST7735_GAMCTRN1);
  SPI_sendData(0x0a);
  SPI_sendData(0x1c);
  SPI_sendData(0x0c);
  SPI_sendData(0x14);
  SPI_sendData(0x33);
  SPI_sendData(0x2b);
  SPI_sendData(0x24);
  SPI_sendData(0x28);
  SPI_sendData(0x27);
  SPI_sendData(0x25);
  SPI_sendData(0x2D);
  SPI_sendData(0x3a);
  SPI_sendData(0x00);
  SPI_sendData(0x05);
  SPI_sendData(0x03);
  SPI_sendData(0x0d);

  //COLMOD (3Ah): Interface Pixel Format
  // * This command is used to define the format of RGB picture
  //   data, which is to be transferred via the MCU interface.
  SPI_sendCommand(ST7735_COLMOD);
  SPI_sendData(0x06);// Default: 0x06 => 18-bit/pixel
                     // IFPF[2:0] MCU Interface Color Format
                     // IFPF[2:0] | Format
                     //      000b | reserved
                     //      001b | reserved
                     //      010b | reserved
                     //      011b | 12-bit/pixel
                     //      100b | reserved
                     //      101b | 16-bit/pixel
                     //      110b | 18-bit/pixel   <<<<<
                     //      111b | reserved

  //DISPON (29h): Display On
  // * This command is used to recover from DISPLAY OFF mode. Output
  //   from the Frame Memory is enabled.
  // * This command makes no change of contents of frame memory.
  // * This command does not change any other status.
  // * The delay time between DISPON and DISPOFF needs 120ms at least
  SPI_sendCommand(ST7735_DISPON); //Display On
  delay(1);

  //MADCTL (36h): Memory Data Access Control
  SPI_sendCommand(ST7735_MADCTL);
  SPI_sendData(0xC0);// YXVL RH--
                     // |||| ||||-- Unused: 0
                     // |||| ||---- MH: Horizontal Refresh Order
                     // |||| |        0 = left to right
                     // |||| |        1 = right to left
                     // |||| |----- RGB: RGB vs BGR Order
                     // ||||          0 = RGB color filter panel
                     // ||||          1 = BGR color filter panel
                     // ||||------- ML: Vertical Refresh Order
                     // |||           0 = top to bottom
                     // |||           1 = bottom to top
                     // |||-------- MV: Row / Column Exchange
                     // ||--------- MX: Column Address Order  <<<<<
                     // |---------- MY: Row Address Order

	// Use 12-bit color
	SPI_sendCommand(ST7735_COLMOD);
  SPI_sendData(0x03);// Use 0x03 => 12-bit/pixel
                     // IFPF[2:0] MCU Interface Color Format
                     // IFPF[2:0] | Format
                     //      011b | 12-bit/pixel RRRRGGGG BBBBRRRR GGGGBBBB

}

//============================================================================
//
//  Restrict the size of LCD data memory which can be written to match the size
//  of data being written e.g. 6x8 for a char, 84x48 for Nokia Emulator Screen,
//  128x128 for full ST7735 LCD
//
static void LCD_resize_window (uint16_t xsize, uint16_t ysize)
{
	// window width must be an even number because two adjacent pixels will be written at a time
	// so stay here forever for debug purposes if xsize is an odd number
	while (xsize&1) {}; // infinite loop
	// Store these values as a global to record current window size
  LCD_window_width = xsize;
  LCD_window_height = ysize;

  // CASET (2Ah): Column Address Set
  // * The value of XS [15:0] and XE [15:0] are referred when RAMWR
  //   command comes.
  // * Each value represents one column line in the Frame Memory.
  // * XS [15:0] always must be equal to or less than XE [15:0]
  SPI_sendCommand(ST7735_CASET); //Column address set
  // Write the parameters for the "column address set" command
  SPI_sendData((0x00 + LCD_window_x + LCD_cursor_x) >> 8);                          //Start MSB = XS[15:8]
  SPI_sendData((0x02 + LCD_window_x + LCD_cursor_x) & 0xFF);                        //Start LSB = XS[ 7:0]
  SPI_sendData((0x00 + LCD_window_x + LCD_cursor_x + LCD_window_width - 1) >> 8);   //End MSB   = XE[15:8]
  SPI_sendData((0x02 + LCD_window_x + LCD_cursor_x + LCD_window_width - 1) & 0xFF); //End LSB   = XE[ 7:0]
  // Write the "row address set" command to the LCD
  // RASET (2Bh): Row Address Set
  // * The value of YS [15:0] and YE [15:0] are referred when RAMWR
  //   command comes.
  // * Each value represents one row line in the Frame Memory.
  // * YS [15:0] always must be equal to or less than YE [15:0]
  SPI_sendCommand(ST7735_RASET); //Row address set
  // Write the parameters for the "row address set" command
  SPI_sendData((0x00 + LCD_window_y + LCD_cursor_y) >> 8);                           //Start MSB = YS[15:8]
  SPI_sendData((0x01 + LCD_window_y + LCD_cursor_y) & 0xFF);                         //Start LSB = YS[ 7:0]
  SPI_sendData((0x00 + LCD_window_y + LCD_cursor_y + LCD_window_height - 1) >> 8);   //End MSB   = YE[15:8]
  SPI_sendData((0x01 + LCD_window_y + LCD_cursor_y + LCD_window_height - 1) & 0xFF); //End LSB   = YE[ 7:0]
  // Prepare the ST7735 LCD to receive pixel data
  // RAMWR (2Ch): Memory Write
  SPI_sendCommand(ST7735_RAMWR); //write data
}

//============================================================================
//
//  Reset the cursor to Nokia 5110 (0,0) and Nokia5110 window size
//
static void LCD_reset_window(void)
{
	LCD_cursor_x = 0;
	LCD_cursor_y = 0;
	LCD_window_x = (ST7735_MAX_X - NOKIA_MAX_X)/2;
	LCD_window_y = (ST7735_MAX_Y - NOKIA_MAX_Y)/2;
  LCD_resize_window (NOKIA_MAX_X, NOKIA_MAX_Y);
}

//============================================================================
//
// Write one entire window of data to the LCD
// If buffer is a null pointer it will fill the window with "off" pixels instead
// Two pixels of data are processed at a time as 12-bit colour data format combines
// two 12-bit pixels into three data bytes for speed and efficiency
//
static void LCD_send_data(const char* buffer)
{
	int i;
  // Select the LCD's data register
  SET_RS;
  // Select the LCD controller
  CLR_CS;
	// Send exactly one window of data from buffer
	for (i=0; i < LCD_window_height * LCD_window_width; i+=2)
	{
		unsigned long pixel1, pixel2;
		// Check single bit of buffer data relating to next pixel
		if ( buffer && (buffer[i/LCD_window_width/8*LCD_window_width+(i%LCD_window_width)+0] & (1<<(i/LCD_window_width)%8)))
			pixel1 = PIXEL_ON;
		else
			pixel1 = PIXEL_OFF;
		// Check single bit of buffer data relating to next pixel + 1
		if ( buffer && (buffer[i/LCD_window_width/8*LCD_window_width+(i%LCD_window_width)+1] & (1<<(i/LCD_window_width)%8)))
			pixel2 = PIXEL_ON;
		else
			pixel2 = PIXEL_OFF;
		// Send both pixels to LCD over SSI
		while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
    SSI2_DR_R = (pixel1&0x0FF0)>>4;                          // Send Red1/Green1 
		while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
    SSI2_DR_R = ((pixel1&0x000F)<<4) | ((pixel2&0x0F00)>>8); // Send Blue1/Red2
		while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
    SSI2_DR_R = pixel2&0x00FF;                               // Send Green2/Blue2
	}
	while((SSI2_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
  // De-Select the LCD controller
  CLR_CS;
}

//============================================================================
//
//  Initialise the SPI specific settings of the launchpad
//
void Initialize_SPI(void)
{
	volatile unsigned long temp;
	// Enable the SSI2 module using the RCGCSSI register (see page 345 of TM4C datasheet).
	SYSCTL_RCGCSSI_R |= SYSCTL_RCGCSSI_R2; // Enable SSI2 Clock
	temp = SYSCTL_RCGCSSI_R;
  // Configure SSI2
  SSI2_CR1_R &= ~SSI_CR1_SSE;           // disable SSI2
  SSI2_CR1_R &= ~SSI_CR1_MS;            // master mode
                                        // configure for system clock/PLL baud clock source
  SSI2_CC_R = (SSI2_CC_R&~SSI_CC_CS_M)+SSI_CC_CS_SYSPLL;
                                        // clock divider for 3.125 MHz SSIClk (50 MHz PIOSC/16)
  SSI2_CPSR_R = (SSI2_CPSR_R&~SSI_CPSR_CPSDVSR_M)+16;
                                        // clock divider for 8 MHz SSIClk (80 MHz PLL/24)
                                        // SysClk/(CPSDVSR*(1+SCR))
                                        // 80/(10*(1+0)) = 8 MHz (slower than 4 MHz)
  SSI2_CPSR_R = (SSI2_CPSR_R&~SSI_CPSR_CPSDVSR_M)+10; // must be even number
  SSI2_CR0_R &= ~(SSI_CR0_SCR_M |       // SCR = 0 (8 Mbps data rate)
                  SSI_CR0_SPH |         // SPH = 0
                  SSI_CR0_SPO);         // SPO = 0
                                        // FRF = Freescale format
  SSI2_CR0_R = (SSI2_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
                                        // DSS = 8-bit data
  SSI2_CR0_R = (SSI2_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
  SSI2_CR1_R |= SSI_CR1_SSE;            // enable SSI2
}

//============================================================================
//
//  Initialise the GPIOs on the launchpad used to control the LCD
//
void Initialize_Launchpad()
{
// LCD SPI & control lines
//      TM4C123G
//   #31   |  PF4 | LCD_RS
//   #17   |  PF0 | LCD_RESET
//   #13   |  PA4 | LCD_CS_NOT (or SPI SS)
//   #15   |  PB7 | LCD_MOSI (hardware SPI)
//   #14   |  PB6 | not used (would be MISO)
//   #7    |  PB4 | LCD_SCK (hardware SPI)
	
// 1. Enable Port Clocks RCGCGPIO
	SYSCTL_RCGCGPIO_R |= (SYSCTL_RCGCGPIO_R5|SYSCTL_RCGCGPIO_R1|SYSCTL_RCGCGPIO_R0);
	delay(1);
// unlock PF0 pin to remove default NMI function 
	GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;
	GPIO_PORTA_CR_R |= LCD_CS_B;
	GPIO_PORTB_CR_R |= (LCD_MOSI_B|LCD_SCK_B);
	GPIO_PORTF_CR_R |= (LCD_RS_B|LCD_RESET_B);
// 2. Set data directions 1=output 0=input GPIODIR
	GPIO_PORTA_DIR_R |= LCD_CS_B;
	GPIO_PORTB_DIR_R |= (LCD_MOSI_B|LCD_SCK_B);
	GPIO_PORTF_DIR_R |= (LCD_RS_B|LCD_RESET_B);
// 3. Set Alternate function selections GPIOAFSEL
	GPIO_PORTA_AFSEL_R &= ~(LCD_CS_B);
	GPIO_PORTB_AFSEL_R |=  (LCD_MOSI_B|LCD_SCK_B);
	GPIO_PORTB_PCTL_R  |= (GPIO_PCTL_PB4_SSI2CLK|GPIO_PCTL_PB7_SSI2TX);
	GPIO_PORTF_AFSEL_R &= ~(LCD_RS_B|LCD_RESET_B);
// 4. Set current drive strength GPIODRxR
// 5. Set pull-up/pull-down/open-drain  GPIOPUR/PDR/ODR
// 6. Enable digital/analog functions GPIODEN/GPIOAMSEL
	GPIO_PORTA_DEN_R |= LCD_CS_B;
	GPIO_PORTB_DEN_R |= (LCD_MOSI_B|LCD_SCK_B);
	GPIO_PORTF_DEN_R |= (LCD_RS_B|LCD_RESET_B);
// 7. Configure Interrupts GPIOIS, GPIOIBE, GPIOBE, GPIOEV, and GPIOIM

  //Drive the ports to a reasonable starting state.
  CLR_RESET;
  CLR_RS;
  SET_CS;
  CLR_MOSI;
  CLR_SCK;

  // initialize SPI. By default the clock is 1.2MHz. The chip is good to 10 MHz
  Initialize_SPI();

	// Increase SSI clock to 8MHz maximum
	// Not yet implemented but 1.2MHz gives ~40Hz? refresh in 84x48 window
	// using 12-bit color pixel data
}

//============================================================================

//********Nokia5110Emu_OutString*****************
// Print a string of characters to the Nokia 5110 84x48 LCD.
// The string will automatically wrap, so padding spaces may
// be needed to make the output look optimal.
// inputs: ptr  pointer to NULL-terminated ASCII string
// outputs: none
void Nokia5110Emu_OutString(unsigned char *ptr)
{
  while(*ptr)
	{
    Nokia5110Emu_OutChar((unsigned char)*ptr);
    ptr = ptr + 1;
  }
}

//********Nokia5110Emu_Init*****************
// Replace the Nokia5110 Initialisation with
// a ST7735 initialisation and draw the external
// frame surrounding the emulator window
void Nokia5110Emu_Init(void)
{
	int i;
	unsigned char label1[] = " Nokia 5110 ";
	unsigned char label2[] =  " Emulator ";

	// Initialize the Launchpad and SPI
	Initialize_Launchpad();

	//Initialize the ST7735 LCD controller
  Initialize_LCD();
	
	// Initialise static data
	LCD_cursor_x = 0;
  LCD_cursor_y = 0;
  LCD_window_x = 0;
  LCD_window_y = 0;

  // Fill full 128x128 ST7735 display with an RGB pattern
	LCD_resize_window(ST7735_MAX_X, ST7735_MAX_Y);
  for (i = 0; i < (ST7735_MAX_X * ST7735_MAX_Y / 2); i++)
  {
		// Send two 12 bit/pixel colour pixels
    SPI_sendData(i); // R1,G1
    SPI_sendData(~i); // B1,R2
    SPI_sendData(i/64); // G2,B2
  }
	
	// Display Emulator label
	LCD_cursor_x = (ST7735_MAX_X-12*CHAR_WIDTH)/2; // Centre 12 chars of label1
	LCD_cursor_y = (ST7735_MAX_Y-NOKIA_MAX_Y)/2-3*CHAR_HEIGHT; // 3 rows above emulator window
	Nokia5110Emu_OutString(label1);
	LCD_cursor_x = (ST7735_MAX_X-10*CHAR_WIDTH)/2; // Centre 10 chars of label2
	LCD_cursor_y = (ST7735_MAX_Y-NOKIA_MAX_Y)/2-2*CHAR_HEIGHT; // 2 rows above emulator window
	Nokia5110Emu_OutString(label2);

	// Initialise the Nokia 5110 emulator window
  Nokia5110Emu_Clear();
}

//********Nokia5110Emu_OutChar*****************
// Print a character to the Nokia 5110 84x48 LCD.  The
// character will be printed at the current cursor position,
// the cursor will automatically be updated, and it will
// wrap to the next row or back to the top if necessary.
// One blank column of pixels will be printed on either side
// of the character for readability.  Since characters are 8
// pixels tall and 6 pixels wide, 14 characters fit per row,
// and there are six rows.
// inputs: data  character to print
// outputs: none
// Note this is slightly more compact than the original which only
// allowed 12 chars per row
void Nokia5110Emu_OutChar(unsigned char data)
{
  // save the original draw window size
	uint16_t height = LCD_window_height;
	uint16_t width = LCD_window_width;

	// Reduce the update window to the size of one ascii character
  LCD_resize_window (CHAR_WIDTH, CHAR_HEIGHT);

	// Write the character data
	LCD_send_data( ASCII6[data-' '] );

	// Restore the window to it's previous size
	LCD_window_height = height;
	LCD_window_width = width;

	// Advance the text cursor to the next character position
	LCD_cursor_x += CHAR_WIDTH;
	if ( LCD_cursor_x + CHAR_WIDTH > LCD_window_width )
	{
		LCD_cursor_x = 0;
		LCD_cursor_y += CHAR_HEIGHT;
		if ( LCD_cursor_y + CHAR_HEIGHT > LCD_window_height )
			LCD_cursor_y = 0;
	}
}

//********Nokia5110Emu_SetCursor*****************
// Move the cursor to the desired X- and Y-position.  The
// next character will be printed here.  X=0 is the leftmost
// column.  Y=0 is the top row.
// inputs: newX  new X-position of the cursor (0<=newX<=13)
//         newY  new Y-position of the cursor (0<=newY<=5)
// outputs: none
void Nokia5110Emu_SetCursor(unsigned char newX, unsigned char newY)
{
	if ( newX * CHAR_WIDTH + CHAR_WIDTH <= NOKIA_MAX_X )
		LCD_cursor_x = newX*CHAR_WIDTH;
	if ( newY * CHAR_HEIGHT + CHAR_HEIGHT <= NOKIA_MAX_Y )
		LCD_cursor_y = newY*CHAR_HEIGHT;
}

//********Nokia5110Emu_Clear*****************
// Clear the LCD by writing off pixels to the emulator window and 
// reset the text cursor to (0,0) (top left corner of screen).
// inputs: none
// outputs: none
void Nokia5110Emu_Clear(void)
{
  LCD_reset_window();
  LCD_send_data( (void *)0 );
}

//********Nokia5110Emu_DrawFullImage*****************
// Fill the whole screen by drawing a 84x48 bitmap image.
// inputs: ptr  pointer to 504 byte bitmap
// outputs: none
void Nokia5110Emu_DrawFullImage(const char *ptr)
{
  LCD_reset_window();
	LCD_send_data(ptr);
}

//********Nokia5110_Init*****************
// Pass the command to EDUMKII ST7735 Nokia5110 Emulator
void Nokia5110_Init(void)
{
	Nokia5110Emu_Init();
}

//********Nokia5110_OutChar*****************
// Wrapper to the new emulator function
void Nokia5110_OutChar(unsigned char data)
{
  Nokia5110Emu_OutChar(data);
}

//********Nokia5110_OutString*****************
// Print a string of characters to the Nokia 5110 84x48 LCD.
// The string will automatically wrap, so padding spaces may
// be needed to make the output look optimal.
// inputs: ptr  pointer to NULL-terminated ASCII string
// outputs: none
void Nokia5110_OutString(char *ptr)
	{
  while(*ptr)
	{
    Nokia5110_OutChar(*ptr);
    ptr = ptr + 1;
  }
}

//********Nokia5110_OutUDec*****************
// Output a 16-bit number in unsigned decimal format with a
// fixed size of five right-justified digits of output.
// Inputs: n  16-bit unsigned number
// Outputs: none
void Nokia5110_OutUDec(unsigned short n)
{
  if(n < 10){
    Nokia5110_OutString("    ");
    Nokia5110_OutChar(n+'0'); /* n is between 0 and 9 */
  } else if(n<100){
    Nokia5110_OutString("   ");
    Nokia5110_OutChar(n/10+'0'); /* tens digit */
    Nokia5110_OutChar(n%10+'0'); /* ones digit */
  } else if(n<1000){
    Nokia5110_OutString("  ");
    Nokia5110_OutChar(n/100+'0'); /* hundreds digit */
    n = n%100;
    Nokia5110_OutChar(n/10+'0'); /* tens digit */
    Nokia5110_OutChar(n%10+'0'); /* ones digit */
  }
  else if(n<10000){
    Nokia5110_OutChar(' ');
    Nokia5110_OutChar(n/1000+'0'); /* thousands digit */
    n = n%1000;
    Nokia5110_OutChar(n/100+'0'); /* hundreds digit */
    n = n%100;
    Nokia5110_OutChar(n/10+'0'); /* tens digit */
    Nokia5110_OutChar(n%10+'0'); /* ones digit */
  }
  else {
    Nokia5110_OutChar(n/10000+'0'); /* ten-thousands digit */
    n = n%10000;
    Nokia5110_OutChar(n/1000+'0'); /* thousands digit */
    n = n%1000;
    Nokia5110_OutChar(n/100+'0'); /* hundreds digit */
    n = n%100;
    Nokia5110_OutChar(n/10+'0'); /* tens digit */
    Nokia5110_OutChar(n%10+'0'); /* ones digit */
  }
}

//********Nokia5110_SetCursor*****************
// Wrapper to new emulator function
void Nokia5110_SetCursor(unsigned char newX, unsigned char newY)
{
  Nokia5110Emu_SetCursor(newX, newY);
}

//********Nokia5110_Clear*****************
// Clear the LCD by writing zeros to the entire screen and
// reset the cursor to (0,0) (top left corner of screen).
// inputs: none
// outputs: none
void Nokia5110_Clear(void)
{
  Nokia5110Emu_Clear();
}

//********Nokia5110_DrawFullImage*****************
// Fill the whole screen by drawing a 48x84 bitmap image.
// inputs: ptr  pointer to 504 byte bitmap
// outputs: none
// assumes: LCD is in default horizontal addressing mode (V = 0)
void Nokia5110_DrawFullImage(const char *ptr)
{
  Nokia5110_SetCursor(0, 0);
  Nokia5110Emu_DrawFullImage(ptr);
}

//********Nokia5110_PrintBMP*****************
// Bitmaps defined above were created for the LM3S1968 or
// LM3S8962's 4-bit grayscale OLED display.  They also
// still contain their header data and may contain padding
// to preserve 4-byte alignment.  This function takes a
// bitmap in the previously described format and puts its
// image data in the proper location in the buffer so the
// image will appear on the screen after the next call to
//   Nokia5110_DisplayBuffer();
// The interface and operation of this process is modeled
// after RIT128x96x4_BMP(x, y, image);
// inputs: xpos      horizontal position of bottom left corner of image, columns from the left edge
//                     must be less than 84
//                     0 is on the left; 82 is near the right
//         ypos      vertical position of bottom left corner of image, rows from the top edge
//                     must be less than 48
//                     2 is near the top; 47 is at the bottom
//         ptr       pointer to a 16 color BMP image
//         threshold grayscale colors above this number make corresponding pixel 'on'
//                     0 to 14
//                     0 is fine for ships, explosions, projectiles, and bunkers
// outputs: none
void Nokia5110_PrintBMP(unsigned char xpos, unsigned char ypos, const unsigned char *ptr, unsigned char threshold)
{
  long width = ptr[18], height = ptr[22], i, j;
  unsigned short screenx, screeny;
  unsigned char mask;
  // check for clipping
  if((height <= 0) ||              // bitmap is unexpectedly encoded in top-to-bottom pixel order
     ((width%2) != 0) ||           // must be even number of columns
     ((xpos + width) > SCREENW) || // right side cut off
     (ypos < (height - 1)) ||      // top cut off
     (ypos > SCREENH))           { // bottom cut off
    return;
  }
  if(threshold > 14){
    threshold = 14;             // only full 'on' turns pixel on
  }
  // bitmaps are encoded backwards, so start at the bottom left corner of the image
  screeny = ypos/8;
  screenx = xpos + SCREENW*screeny;
  mask = ypos%8;                // row 0 to 7
  mask = 0x01<<mask;            // now stores a mask 0x01 to 0x80
  j = ptr[10];                  // byte 10 contains the offset where image data can be found
  for(i=1; i<=(width*height/2); i=i+1){
    // the left pixel is in the upper 4 bits
    if(((ptr[j]>>4)&0xF) > threshold){
      Screen[screenx] |= mask;
    } else{
      Screen[screenx] &= ~mask;
    }
    screenx = screenx + 1;
    // the right pixel is in the lower 4 bits
    if((ptr[j]&0xF) > threshold){
      Screen[screenx] |= mask;
    } else{
      Screen[screenx] &= ~mask;
    }
    screenx = screenx + 1;
    j = j + 1;
    if((i%(width/2)) == 0){     // at the end of a row
      if(mask > 0x01){
        mask = mask>>1;
      } else{
        mask = 0x80;
        screeny = screeny - 1;
      }
      screenx = xpos + SCREENW*screeny;
      // bitmaps are 32-bit word aligned
      switch((width/2)%4){      // skip any padding
        case 0: j = j + 0; break;
        case 1: j = j + 3; break;
        case 2: j = j + 2; break;
        case 3: j = j + 1; break;
      }
    }
  }
}

// There is a buffer in RAM that holds one screen
// This routine clears this buffer
void Nokia5110_ClearBuffer(void)
{
	int i;
  for(i=0; i<SCREENW*SCREENH/8; i++)
    Screen[i] = 0;              // clear buffer
}

//********Nokia5110_DisplayBuffer*****************
// Fill the whole screen by drawing a 84x48 screen image.
// inputs: none
// outputs: none
// assumes: LCD is in default horizontal addressing mode (V = 0)
void Nokia5110_DisplayBuffer(void)
{
  Nokia5110_DrawFullImage(Screen);
}
