/*
	sconsole - 2010 Scognito (scognito@gmail.com)
*/

#ifndef _SCONSOLE_
#define _SCONSOLE_

#include <psl1ght/lv2.h>

#define FONT_W 8
#define FONT_H 16

#define FONT_COLOR_NONE  -1
#define FONT_COLOR_BLACK 0x00000000
#define FONT_COLOR_WHITE 0x00ffffff
#define FONT_COLOR_RED   0x00ff0000
#define FONT_COLOR_GREEN 0x0000ff00
#define FONT_COLOR_BLUE  0x000000ff

typedef struct{

	int curX;
	int curY;
	int fgColor;
	int bgColor;
	int screenWidth;
	int screenHeight;
} s_console;

s_console sconsole;

void sconsoleInit(int bgColor, int fgColor, int screenWidth, int screenHeight);
void print(int x, int y, char* text, uint32_t* buffer);

#endif
