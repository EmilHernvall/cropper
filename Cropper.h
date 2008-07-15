#pragma once

#define TITLE TEXT("Cropper")
#define WNDCLASS TEXT("CropFrame")

typedef struct {
	// The loaded image
	Image *img;
	// Used to prevent painting while the window is being resized
	BOOL allowPaint;
	// Used to indicate that the buffer needs to be regenerated after
	// the window has been rezied
	BOOL redraw;
	// The secondary buffer that contains the scaled image
	HDC hdcMem;

	// Position of the mouse
	INT x, y;
	// Position of the image in the window
	INT imageLeft, imageTop, imageWidth, imageHeight;
	// Position of the rectangle in the window coordinate system
	INT rectLeft, rectTop, rectWidth, rectHeight;
} ApplicationContext;
