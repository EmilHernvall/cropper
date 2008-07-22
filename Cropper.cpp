#include "stdafx.h"
#include "Cropper.h"

// Helper function from another file
INT GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

// Forward declarations
LRESULT CALLBACK CropProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ToolProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK NumOnlyEditProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                       LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASSEX wcex;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Image *img;
	OPENFILENAME ofn;
	TCHAR szFile[MAX_PATH*2+1];
	ApplicationContext ctx;

	UNREFERENCED_PARAMETER(hPrevInstance);

	// Initialize GDI+
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Window class for main crop window
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = 0; //CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = CropProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = CROPWNDCLASS;
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex)) {
		MessageBox(0, TEXT("Could Not Register Window"), TEXT("Oh Oh..."),
		           MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Window class for the tool window
	wcex.lpfnWndProc = ToolProc;
	wcex.lpszClassName = TOOLWNDCLASS;
	wcex.hbrBackground = (HBRUSH)COLOR_WINDOW;
	if (!RegisterClassEx(&wcex)) {
		MessageBox(0, TEXT("Could Not Register Window"), TEXT("Oh Oh..."),
		           MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Check if a filename was providing as a command line argument,
	// and open that file if that's the case.
	if (_tcslen(lpCmdLine) > 0) {
		_tcsncpy_s(szFile, sizeof(szFile)/sizeof(*szFile), 
			lpCmdLine+1, _tcslen(lpCmdLine)-2);
	} 
	// Show load dialogue
	else {
		ZeroMemory(&ofn, sizeof(ofn));

		szFile[0] = '\0';

		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = L"All Known Filetypes (JPEG,PNG,BMP,GIF,TIFF)\0*.jpg;*.png;*.bmp;*.gif;*.tiff\0"
			L"JPEG\0*.jpg\0"
			L"PNG\0*.png\0"
			L"BMP\0*.bmp\0"
			L"GIF\0*.gif\0"
			L"TIFF\0*.tiff\0";
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(*szFile);
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = 0;
		ofn.lpstrTitle = TEXT("Open");

		if (GetOpenFileName(&ofn) == FALSE) {
			return 0;
		}
	}

	// Load image
	img = Image::FromFile(szFile, FALSE);
	if (img->GetLastStatus() != Ok) {
		MessageBox(NULL, TEXT("Failed to load image!"), TEXT("Epic Fail!"),
			       MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Initialize application context
	ctx.allowPaint = TRUE;
	ctx.hdcMem = NULL;
	ctx.img = img;
	ctx.redraw = TRUE;
	ctx.rectTop = -1;
	ctx.rectLeft = -1;
	ctx.rectWidth = -1;
	ctx.rectHeight = -1;
	ctx.oldWidth = -1;
	ctx.oldHeight = -1;
	ctx.x = -1;
	ctx.y = -1;
	ctx.selectionMode = VARIABLE;

	// Create main window
	ctx.hwndCropFrame = CreateWindow(CROPWNDCLASS, TITLE, WS_OVERLAPPEDWINDOW,
						CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, 
						hInstance, NULL);
	if (!ctx.hwndCropFrame) {
		return 0;
	}

	// Store application context
	SetWindowLong(ctx.hwndCropFrame, GWL_USERDATA, (LONG)&ctx);

	// Display window
	ShowWindow(ctx.hwndCropFrame, nCmdShow);
	UpdateWindow(ctx.hwndCropFrame);

	// Create tool frame
	ctx.hwndToolFrame = CreateWindow(TOOLWNDCLASS, TEXT("Tools"), 0,
						CW_USEDEFAULT, 0, 150, 270, ctx.hwndCropFrame, NULL, 
						hInstance, NULL);
	if (!ctx.hwndToolFrame) {
		return 0;
	}

	// Store application context
	SetWindowLong(ctx.hwndToolFrame, GWL_USERDATA, (LONG)&ctx);

	// Send a second message to indicate that the application 
	// context has been set and that it's clear to create the
	// sub windows.
	SendMessage(ctx.hwndToolFrame, WM_USER, 0, 0);

	// Show toolframe
	ShowWindow(ctx.hwndToolFrame, nCmdShow);
	UpdateWindow(ctx.hwndToolFrame);

	// Main loop
	while(GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

VOID Save(HWND hWnd, ApplicationContext *ctx)
{
	OPENFILENAME ofn;
	TCHAR szFile[MAX_PATH+1];
	INT imageWidth, imageHeight;
	DOUBLE widthRatio, heightRatio;
	INT realTop, realLeft, realWidth, realHeight;
	TCHAR szBuffer[2*MAX_PATH+1];
	CLSID encoderClsid;
	EncoderParameters encoderParameters;
	ULONG quality;

	// Don't allow the user to save the image unless there's an selected area
	if (ctx->rectLeft == -1) {
		MessageBox(hWnd, TEXT("You need to select an area to crop."), 
			TEXT("Cropper"), MB_ICONINFORMATION);
		return;
	}

	// Show the save as dialog
	ZeroMemory(&ofn, sizeof(ofn));

	szFile[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"JPEG\0*.jpg\0"
		L"PNG\0*.png\0"
		L"BMP\0*.bmp\0"
		L"TIFF\0*.tiff\0";

	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile) / sizeof(*szFile);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_OVERWRITEPROMPT;
	ofn.lpstrTitle = TEXT("Save As");

	if (GetSaveFileName(&ofn) == TRUE) {

		// Calculate the area to crop
		imageWidth = ctx->img->GetWidth();
		imageHeight = ctx->img->GetHeight();

		widthRatio = imageWidth / (double)ctx->imageWidth;
		heightRatio = imageHeight / (double)ctx->imageHeight;

		realLeft = (int)(widthRatio * (ctx->rectLeft - ctx->imageLeft));
		realTop = (int)(heightRatio * (ctx->rectTop - ctx->imageTop));
		realWidth = (int)(widthRatio * ctx->rectWidth);
		realHeight = (int)(heightRatio * ctx->rectHeight);

		// Retrieve the chosen filename, and check extension. Find the correct
		// encoder.
		_tcsncpy_s(szBuffer, sizeof(szBuffer)/sizeof(*szBuffer), 
			szFile, sizeof(szFile)/sizeof(*szFile));
		LPTSTR szExt = _tcschr(szBuffer, _T('.'));
		switch (ofn.nFilterIndex) {
			case 4:
				if (szExt == NULL) {
					_tcscat_s(szBuffer, sizeof(szBuffer)/sizeof(*szBuffer), TEXT(".tiff"));
				}
				GetEncoderClsid(L"image/tiff", &encoderClsid);
				break;
			case 3:
				if (szExt == NULL) {
					_tcscat_s(szBuffer, sizeof(szBuffer)/sizeof(*szBuffer), TEXT(".bmp"));
				}
				GetEncoderClsid(L"image/bmp", &encoderClsid);
				break;
			case 2:
				if (szExt == NULL) {
					_tcscat_s(szBuffer, sizeof(szBuffer)/sizeof(*szBuffer), TEXT(".png"));
				}
				GetEncoderClsid(L"image/png", &encoderClsid);
				break;
			case 1:
			default:
				if (szExt == NULL) {
					_tcscat_s(szBuffer, sizeof(szBuffer)/sizeof(*szBuffer), TEXT(".jpg"));
				}
				GetEncoderClsid(L"image/jpeg", &encoderClsid);
		}

		// Crop the image.		
		Bitmap bmp(realWidth, realHeight);
		Graphics g(&bmp);
		g.DrawImage(ctx->img, Rect(0, 0, realWidth, realHeight), 
			realLeft, realTop, realWidth, realHeight, UnitPixel);

		quality = 90;

		encoderParameters.Count = 1;
		encoderParameters.Parameter[0].Guid = EncoderQuality;
		encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
		encoderParameters.Parameter[0].NumberOfValues = 1;
		encoderParameters.Parameter[0].Value = &quality;

		Status stat = bmp.Save(szBuffer, &encoderClsid, &encoderParameters);

		// Quit if successful
		if (stat == Ok) {
			PostQuitMessage(0);
		} 
		// Complain otherwise
		else {
			MessageBox(hWnd, TEXT("Save failed!"), TEXT("Status"), MB_ICONEXCLAMATION);
		}
	}
}

VOID Paint(HWND hWnd, ApplicationContext *ctx)
{
	int windowWidth, windowHeight, imageWidth, imageHeight;
	int newWidth, newHeight, top, left;
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rc;
	HBITMAP hbmMem = NULL;
	HANDLE hOld = NULL;
	DOUBLE widthRatio, heightRatio;

	// Only paint if we're not resizing the window
	if (ctx->allowPaint) {

		// Calculate size and position of image
		GetClientRect(hWnd, &rc);

		windowWidth = rc.right - rc.left;
		windowHeight = rc.bottom - rc.top;

		imageWidth = ctx->img->GetWidth();
		imageHeight = ctx->img->GetHeight();

		if (windowWidth > windowHeight) {
			// Calc 1
			newWidth = (int)(imageWidth / (double)imageHeight * windowHeight);
			newHeight = windowHeight;

			top = 0;
			left = (windowWidth - newWidth) / 2;

			if (newWidth > windowWidth) {
				// Calc 2
				newWidth = windowWidth;
				newHeight = (int)(imageHeight / (double)imageWidth * windowWidth);

				top = (windowHeight - newHeight) / 2;
				left = 0;
			}
		} else {
			// Calc 2
			newWidth = windowWidth;
			newHeight = (int)(imageHeight / (double)imageWidth * windowWidth);

			top = (windowHeight - newHeight) / 2;
			left = 0;

			if (newHeight > windowHeight) {
				// Calc 1
				newWidth = (int)(imageWidth / (double)imageHeight * windowHeight);
				newHeight = windowHeight;

				top = 0;
				left = (windowWidth - newWidth) / 2;
			}
		}

		hdc = BeginPaint(hWnd, &ps);

		// This is the double buffering. We only regenerate
		// the image buffer when the size of the window has changed
		// and the image needs to be rescaled (the only operation that
		// takes real time).
		if (ctx->redraw) {
			if (ctx->hdcMem == NULL) {
				DeleteDC(ctx->hdcMem);
			}

			// Draw loading screen to display while doing the rescaling
			Graphics graphics(hdc);
			graphics.FillRectangle(new SolidBrush(Color::Pink), RectF::RectF(0, 0, 
				(Gdiplus::REAL)windowWidth, (Gdiplus::REAL)windowHeight));
			FontFamily fontFamily(L"Arial");
			Font font(&fontFamily, 16, FontStyleRegular, UnitPixel);
			graphics.DrawString(TEXT("Loading..."), 10, &font, 
				PointF::PointF(0,0), new SolidBrush(Color::Black));

			// Create a new buffer
			ctx->hdcMem = CreateCompatibleDC(hdc);
			hbmMem = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
			hOld = SelectObject(ctx->hdcMem, hbmMem);

			// Rescale image
			Graphics memgraphics(ctx->hdcMem);
			memgraphics.FillRectangle(new SolidBrush(Color::White), 
				RectF::RectF(0, 0, (Gdiplus::REAL)windowWidth, (Gdiplus::REAL)windowHeight));
			memgraphics.DrawImage(ctx->img, RectF::RectF((Gdiplus::REAL)left, (Gdiplus::REAL)top, 
				(Gdiplus::REAL)newWidth, (Gdiplus::REAL)newHeight));

			SelectObject(ctx->hdcMem, hOld);
			DeleteObject(hbmMem);

			// Next time we won't have to redraw (unless the user resizes the window again)
			ctx->redraw = FALSE;

			ctx->imageLeft = left;
			ctx->imageTop = top;
			ctx->imageWidth = newWidth;
			ctx->imageHeight = newHeight;
		}

		// Transfer the buffer to the screen
		BitBlt(hdc, 0, 0, windowWidth, windowHeight, ctx->hdcMem, 0, 0, SRCCOPY);

		// Draw the rectangle. This won't have any effect while mouse movement occurs,
		// since the area is masked out.
		if (ctx->rectLeft != -1) {
			if (ctx->oldWidth != -1) {
				widthRatio = ctx->imageWidth / (double)ctx->oldWidth;
				heightRatio = ctx->imageHeight / (double)ctx->oldHeight;

				ctx->rectLeft = ctx->imageLeft + 
					(int)((ctx->rectLeft - ctx->oldLeft) * widthRatio);
				ctx->rectWidth = (int)(ctx->rectWidth * widthRatio);
				ctx->rectTop = ctx->imageTop + 
					(int)((ctx->rectTop - ctx->oldTop) * heightRatio);
				ctx->rectHeight = (int)(ctx->rectHeight * heightRatio);

				ctx->oldWidth = -1;
				ctx->oldHeight = -1;
			}

			Graphics graphics(hdc);
			graphics.DrawRectangle(new Pen(Color::Red, 1), 
				RectF((Gdiplus::REAL)ctx->rectLeft, (Gdiplus::REAL)ctx->rectTop, 
					  (Gdiplus::REAL)ctx->rectWidth, (Gdiplus::REAL)ctx->rectHeight));
		}

		EndPaint(hWnd, &ps);
	}
}

INT GetX(ApplicationContext *ctx, INT x)
{
	if (x < ctx->imageLeft) {
		x = ctx->imageLeft;
	} else if (x > ctx->imageLeft + ctx->imageWidth) {
		x = ctx->imageLeft + ctx->imageWidth;
	}

	return x;
}

INT GetY(ApplicationContext *ctx, INT y)
{
	if (y < ctx->imageTop) {
		y = ctx->imageTop;
	} else if (y >= ctx->imageTop + ctx->imageHeight) {
		y = ctx->imageTop + ctx->imageHeight - 1;
	}

	return y;
}

VOID MouseMove(HWND hWnd, ApplicationContext *ctx, int x, int y)
{
	int newWidth, newHeight, top, left, right, bottom;
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rc, rc2;

	if (ctx->x == -1) {
		return;
	}

	// Calculate the selection rectangle
	switch (ctx->selectionMode)
	{
	case FIXED_SIZE:
		right = GetX(ctx, x + ctx->modeWidth);
		bottom = GetY(ctx, y + ctx->modeHeight);
		left = right - ctx->modeWidth;
		top = bottom - ctx->modeHeight;
		newWidth = ctx->modeWidth;
		newHeight = ctx->modeHeight;
		break;
	case FIXED_RATIO:
		newWidth = abs(x - ctx->x);
		newHeight = abs(y - ctx->y);

		if (newWidth/(double)newHeight > ctx->modeWidth/(double)ctx->modeHeight) {
			newWidth = (ctx->x < x ? x - ctx->x : ctx->x - x);
			newHeight = (int)(ctx->modeHeight/(double)ctx->modeWidth * newWidth);
		} else {
			newHeight = (ctx->y < y ? y - ctx->y : ctx->y - y);
			newWidth = (int)(ctx->modeWidth/(double)ctx->modeHeight * newHeight);
		}

		if (x < ctx->x) {
			left = ctx->x - newWidth;
			right = ctx->x;
		} else {
			left = ctx->x;
			right = left + newWidth;
		}

		if (y < ctx->y) {
			top = ctx->y - newHeight;
			bottom = ctx->y;
		} else {
			top = ctx->y;
			bottom = top + newHeight;
		}
		
		break;
	case VARIABLE:
	default:
		left = (ctx->x < x ? ctx->x : x);
		top = (ctx->y < y ? ctx->y : y);
		right = (ctx->x < x ? x : ctx->x);
		bottom = (ctx->y < y ? y : ctx->y);
		newWidth = (ctx->x < x ? x - ctx->x : ctx->x - x);
		newHeight = (ctx->y < y ? y - ctx->y : ctx->y - y);
		break;
	}

	if (left <= ctx->imageLeft) {
		return;
	}

	if (top <= ctx->imageTop) {
		return;
	}

	if (right >= ctx->imageLeft + ctx->imageWidth) {
		return;
	}

	if (bottom >= ctx->imageTop + ctx->imageHeight) {
		return;
	}

	// Store for later use
	ctx->rectLeft = left;
	ctx->rectTop = top;
	ctx->rectWidth = newWidth;
	ctx->rectHeight = newHeight;

	// Invalidate the entire client area
	GetClientRect(hWnd, &rc);
	InvalidateRect(hWnd, &rc, FALSE);

	// Validate the area to be encompassed by the rectangle
	// including the rectangle borders.
	rc.left = left;
	rc.top = top;
	rc.right = right + 1;
	rc.bottom = bottom + 1;
	ValidateRect(hWnd, &rc);

	// Invalidate everything inside the rectangle borders
	rc2.left = rc.left + 1;
	rc2.top = rc.top + 1;
	rc2.right = rc.right - 1;
	rc2.bottom = rc.bottom - 1;
	InvalidateRect(hWnd, &rc2, FALSE);

	// We have now invalidated everything except the rectangle
	// that will be drawn later.
	SendMessage(hWnd, WM_PAINT, 0, 0);

	// Since WM_PAINT has revalidated the area, we have to reinvalidate
	// the area where we want to draw.
	InvalidateRect(hWnd, &rc, FALSE);

	// Paint the rectangle.
	hdc = BeginPaint(hWnd, &ps);
	Graphics graphics(hdc);
	graphics.DrawRectangle(new Pen(Color::Red, 1), 
		RectF((Gdiplus::REAL)left, (Gdiplus::REAL)top, 
		      (Gdiplus::REAL)newWidth, (Gdiplus::REAL)newHeight));
	EndPaint(hWnd, &ps);
}

LRESULT CALLBACK CropProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rc;
	ApplicationContext *ctx;
	INT x, y;

	ctx = (ApplicationContext*)GetWindowLong(hWnd, GWL_USERDATA);
	if (ctx == NULL) {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	switch (message)
	{
	// Resize the selection even when the maximize or restore buttons are
	// used.
	case WM_SYSCOMMAND:
		switch (wParam) {
		case SC_RESTORE:
		case SC_MAXIMIZE:
		case 0xF012:
		case 0xF122:
			ctx->oldLeft = ctx->imageLeft;
			ctx->oldTop = ctx->imageTop;
			ctx->oldWidth = ctx->imageWidth;
			ctx->oldHeight = ctx->imageHeight;
			break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	// When the window has been resized, we have to rescale the 
	// image, and redraw the entire client area
	case WM_SIZE:
		ctx->redraw = TRUE;
		GetClientRect(hWnd, &rc);
		InvalidateRect(hWnd, &rc, TRUE);
		break;
	case WM_PAINT:
		Paint(hWnd, ctx);
		break;
	// We don't allow repainting while resizing the window.
	case WM_ENTERSIZEMOVE:
		ctx->allowPaint = FALSE;

		ctx->oldLeft = ctx->imageLeft;
		ctx->oldTop = ctx->imageTop;
		ctx->oldWidth = ctx->imageWidth;
		ctx->oldHeight = ctx->imageHeight;
		return 0;
	// We're done resizing, so we can reactivate painting.
	case WM_EXITSIZEMOVE:
		ctx->allowPaint = TRUE;
		return 0;
	// Store the mouse down coordinate when the left mouse button
	// is pressed in order to draw the selection rectangle
	case WM_LBUTTONDOWN:
		ctx->x = GetX(ctx, LOWORD(lParam));
		ctx->y = GetY(ctx, HIWORD(lParam));
		break;
	// Check if the mouse has moved. If not, hide the selection rectangle.
	case WM_LBUTTONUP:
		x = GetX(ctx, LOWORD(lParam));
		y = GetY(ctx, HIWORD(lParam));
		if (ctx->x == x && ctx->y == y) {
			ctx->rectWidth = -1;
			ctx->rectHeight = -1;
			ctx->rectTop = -1;
			ctx->rectLeft = -1;
			GetClientRect(hWnd, &rc);
			InvalidateRect(hWnd, &rc, TRUE);
		}
		break;
	// Update the selection rectangle
	case WM_MOUSEMOVE:
		// If the left button is pressed
		if ((wParam & MK_LBUTTON) > 0) {
			x = GetX(ctx, LOWORD(lParam));
			y = GetY(ctx, HIWORD(lParam));
			MouseMove(hWnd, ctx, x, y);
		}
		return 0;
	// Check for an escape
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			Save(hWnd, ctx);
		}
		break;
	// Exit the application when the window is closed
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

LRESULT CALLBACK ToolProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	INT wmId, wmEvent, value;
	ApplicationContext *ctx;
	TCHAR szSizeBuffer[10];

	ctx = (ApplicationContext*)GetWindowLong(hWnd, GWL_USERDATA);
	if (ctx == NULL) {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	switch (message)
	{
	case WM_USER:
		
		ctx->hwndLblSelectionMode = CreateWindow(L"STATIC", L"Selection mode:",
							WS_VISIBLE | WS_CHILD,
							10, 10, 130, 20, hWnd, NULL, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->hwndBtnVariable = CreateWindow(L"BUTTON", L"Variable size",
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
							10, 30, 130, 20, hWnd, (HMENU)ID_BTN_VARIABLE, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->hwndBtnFixed = CreateWindow(L"BUTTON", L"Fixed size",
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
							10, 50, 130, 20, hWnd, (HMENU)ID_BTN_FIXED, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->hwndBtnFixedRatio = CreateWindow(L"BUTTON", L"Fixed ratio",
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
							10, 70, 130, 20, hWnd, (HMENU)ID_BTN_FIXED_RATIO, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		SendMessage(ctx->hwndBtnVariable, BM_SETCHECK, BST_CHECKED, 0);

		ctx->hwndLblWidth = CreateWindow(L"STATIC", L"Width:",
							WS_VISIBLE | WS_CHILD,
							10, 100, 130, 20, hWnd, NULL, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->hwndEditWidth = CreateWindow(L"EDIT", NULL,
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER,
							10, 120, 125, 20, hWnd, (HMENU)ID_EDIT_WIDTH, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->wndProcWidth = (WNDPROC)SetWindowLong(ctx->hwndEditWidth, GWL_WNDPROC, (LONG)NumOnlyEditProc);
		SetWindowLong(ctx->hwndEditWidth, GWL_USERDATA, (LONG)ctx);
		EnableWindow(ctx->hwndEditWidth, FALSE);
		SendMessage(ctx->hwndEditWidth, EM_LIMITTEXT, 6, 0);

		ctx->hwndLblHeight = CreateWindow(L"STATIC", L"Height:",
							WS_VISIBLE | WS_CHILD,
							10, 145, 130, 20, hWnd, NULL, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->hwndEditHeight = CreateWindow(L"EDIT", NULL,
							WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER,
							10, 165, 125, 20, hWnd, (HMENU)ID_EDIT_HEIGHT, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		ctx->wndProcHeight = (WNDPROC)SetWindowLong(ctx->hwndEditHeight, GWL_WNDPROC, (LONG)NumOnlyEditProc);
		SetWindowLong(ctx->hwndEditHeight, GWL_USERDATA, (LONG)ctx);
		EnableWindow(ctx->hwndEditHeight, FALSE);
		SendMessage(ctx->hwndEditHeight, EM_LIMITTEXT, 6, 0);

		ctx->hwndBtnSave = CreateWindow(L"BUTTON", L"Save",
							WS_TABSTOP | WS_VISIBLE | WS_CHILD,
							10, 200, 125, 30, hWnd, (HMENU)ID_BTN_SAVE, 
							(HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE), NULL);

		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId) {
		case ID_BTN_VARIABLE:
			EnableWindow(ctx->hwndEditWidth, FALSE);
			EnableWindow(ctx->hwndEditHeight, FALSE);
			ctx->selectionMode = VARIABLE;
			return 0;
		case ID_BTN_FIXED:
			EnableWindow(ctx->hwndEditWidth, TRUE);
			EnableWindow(ctx->hwndEditHeight, TRUE);
			ctx->selectionMode = FIXED_SIZE;
			return 0;
		case ID_BTN_FIXED_RATIO:
			EnableWindow(ctx->hwndEditWidth, TRUE);
			ctx->selectionMode = FIXED_RATIO;
			EnableWindow(ctx->hwndEditHeight, TRUE);
			return 0;
		case ID_EDIT_WIDTH:
			switch (wmEvent)
			{
			case EN_CHANGE:
				SendMessage(ctx->hwndEditWidth, WM_GETTEXT, 9, (LPARAM)szSizeBuffer);
				value = _wtoi(szSizeBuffer);
				if (value < ctx->imageWidth) {
					ctx->modeWidth = value;
				} else {
					wsprintf(szSizeBuffer, L"%d", ctx->imageWidth - 1);
					SendMessage(ctx->hwndEditWidth, WM_SETTEXT, 0, (LPARAM)szSizeBuffer);
				}
				break;
			}
			return 0;
		case ID_EDIT_HEIGHT:
			switch (wmEvent)
			{
			case EN_CHANGE:
				SendMessage(ctx->hwndEditHeight, WM_GETTEXT, 9, (LPARAM)szSizeBuffer);
				value = _wtoi(szSizeBuffer);
				if (value < ctx->imageHeight) {
					ctx->modeHeight = value;
				} else {
					wsprintf(szSizeBuffer, L"%d", ctx->imageHeight - 1);
					SendMessage(ctx->hwndEditHeight, WM_SETTEXT, 0, (LPARAM)szSizeBuffer);
				}
				break;
			}
			return 0;
		case ID_BTN_SAVE:
			Save(ctx->hwndCropFrame, ctx);
			return 0;
		}
		break;
	default:
	return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

LRESULT CALLBACK NumOnlyEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ApplicationContext *ctx = (ApplicationContext*)GetWindowLong(hWnd, GWL_USERDATA);

	switch (msg)
	{
	case WM_CHAR:
		if (wParam == 9) {
			if (ctx->hwndEditWidth == hWnd) {
				SetFocus(ctx->hwndEditHeight);
			}
		}

		if (!iswdigit(wParam) && wParam != 8) {
			return 0;
		}
	}

	if (ctx->hwndEditWidth == hWnd) {
		return CallWindowProc(ctx->wndProcWidth, hWnd, msg, wParam, lParam);
	} else {
		return CallWindowProc(ctx->wndProcHeight, hWnd, msg, wParam, lParam);
	}
}
