/*
	sconsole - 2010 Scognito (scognito@gmail.com)
*/

#include "sconsole.h"
#include "console_fonts.h"

void sconsoleInit(int bgColor, int fgColor, int screenWidth, int screenHeight)
{
	sconsole.bgColor = bgColor;
	sconsole.fgColor = fgColor;
	sconsole.curX = 0;
	sconsole.curY = 0;
	sconsole.screenWidth = screenWidth;
	sconsole.screenHeight = screenHeight;
}

void print(int x, int y, char *text, uint32_t * buffer)
{

	int i = 0;
	int tempx = 0;		// scans the font bitmap
	int tempy = 0;
	int oldConsoleX = sconsole.curX;
	char c;

	sconsole.curX = x;
	sconsole.curY = y;

	while (*text != '\0') {

		c = *text;

		if (c == '\n') {
			sconsole.curX = x;
			sconsole.curY += FONT_H * 2;
		} else {

			if (c < 32 || c > 132)
				c = 180;

			for (i = 0; i < FONT_W * FONT_H; i++) {

				if (consoleFont[-32 + c][i] == 0) {
					if (sconsole.bgColor != FONT_COLOR_NONE) {
						buffer[(sconsole.curY +
							tempy) *
						       sconsole.screenWidth +
						       sconsole.curX + 1 +
						       tempx] =
						    sconsole.bgColor;
						buffer[(sconsole.curY + tempy +
							1) *
						       sconsole.screenWidth +
						       sconsole.curX + tempx] =
						    sconsole.bgColor;
						buffer[(sconsole.curY + tempy +
							1) *
						       sconsole.screenWidth +
						       sconsole.curX + 1 +
						       tempx] =
						    sconsole.bgColor;
						buffer[(sconsole.curY +
							tempy) *
						       sconsole.screenWidth +
						       sconsole.curX + tempx] =
						    sconsole.bgColor;
					}
				} else {
					if (sconsole.fgColor != FONT_COLOR_NONE) {
						buffer[(sconsole.curY +
							tempy + 1) *
						       sconsole.screenWidth +
						       sconsole.curX + tempx] =
						    sconsole.fgColor;
						buffer[(sconsole.curY +
							tempy + 1) *
						       sconsole.screenWidth +
						       sconsole.curX + 1 +
						       tempx] =
						    sconsole.fgColor;
						buffer[(sconsole.curY +
							tempy) *
						       sconsole.screenWidth +
						       sconsole.curX + 1 +
						       tempx] =
						    sconsole.fgColor;
						buffer[(sconsole.curY +
							tempy) *
						       sconsole.screenWidth +
						       sconsole.curX + tempx] =
						    sconsole.fgColor;
					}
				}

				tempx += 2;
				if (tempx == FONT_W * 2) {
					tempx = 0;
					tempy += 2;
				}
			}
			tempy = 0;
			sconsole.curX += FONT_W * 2;
			if (sconsole.curX >= sconsole.screenWidth) {
				sconsole.curX = 0;
				sconsole.curY += FONT_H * 2;
			}
		}
		++text;
	}

	sconsole.curY += FONT_H * 2;
	sconsole.curX = oldConsoleX;
}
