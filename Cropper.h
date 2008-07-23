#pragma once

#define TITLE TEXT("Cropper - Click to select, ESC to Save")
#define CROPWNDCLASS TEXT("CropFrame")
#define TOOLWNDCLASS TEXT("ToolFrame")

#define ID_BTN_VARIABLE					100
#define ID_BTN_FIXED					101
#define	ID_BTN_FIXED_RATIO				102
#define ID_EDIT_WIDTH					103
#define ID_EDIT_HEIGHT					104
#define ID_EDIT_HEIGHT					104
#define ID_BTN_SAVE						105
#define ID_BTN_RESIZE					106
#define ID_EDIT_OUTPUTWIDTH				107
#define ID_EDIT_OUTPUTHEIGHT			108

enum SelectionMode {
	VARIABLE = 1,
	FIXED_SIZE,
	FIXED_RATIO
};

typedef struct {
	// HWNDs
	HWND hwndCropFrame, hwndToolFrame;
	HWND hwndLblSelectionMode;
	HWND hwndBtnVariable, hwndBtnFixed, hwndBtnFixedRatio;
	HWND hwndLblWidth, hwndLblHeight;
	HWND hwndEditWidth, hwndEditHeight;
	HWND hwndBtnResizeOutput;
	HWND hwndLblOutputWidth, hwndLblOutputHeight;
	HWND hwndEditOutputWidth, hwndEditOutputHeight;
	HWND hwndBtnSave;

	// Edit WndProcs
	WNDPROC editProc;

	// The loaded image
	Image *img;

	// Used to prevent painting while the window is being resized
	BOOL allowPaint;

	// Used to indicate that the buffer needs to be regenerated after
	// the window has been rezied
	BOOL redraw;

	// The secondary buffer that contains the scaled image
	HDC hdcMem;

	SelectionMode selectionMode;
	INT modeWidth, modeHeight;

	BOOL resizeOutput;

	// Position of the mouse
	INT x, y;
	// Position of the image in the window
	INT imageLeft, imageTop, imageWidth, imageHeight;
	// Position of the rectangle in the window coordinate system
	INT rectLeft, rectTop, rectWidth, rectHeight;
	// Used to correctly resize the rectangle
	INT oldLeft, oldTop, oldWidth, oldHeight;
} ApplicationContext;
