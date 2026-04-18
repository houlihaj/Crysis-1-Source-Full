// PhysTestbed.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "PhysTestbed.h"
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HDC g_hDC;
HWND g_hWnd;
HGLRC g_glrc;
int g_bAnimate;
char *g_arg[3];
int g_bFast=1,g_bShowColl=1,g_bShowProfiler=1,g_moveMode=0;
extern HANDLE g_hThreadActive;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void InitPhysics();
void ShutdownPhysics();
void SaveWorld(const char *fworld);
void ReloadWorld(const char *fworld);
void ReloadWorldAndGeometries(const char *fworld,const char *fgeoms);
void EvolveWorld();
void RenderWorld(HWND hWnd, HDC hDC);
void OnCameraMove(float dx, float dy);
void OnCameraRotate(float dx, float dy);
void SelectNextProfVisInfo();
void SelectPrevProfVisInfo();
void ExpandProfVisInfo(int bExpand);
void SelectObject(float dx,float dy);

BOOL SetupPixelFormat(HDC hDC)
{
  static PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
    1,                              // version number
     PFD_DRAW_TO_WINDOW |            // support bitmap
     PFD_SUPPORT_OPENGL |             // support OpenGL
		 PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,                  // RGBA type
    24,                             // 24-bit color depth
    0, 0, 0, 0, 0, 0,               // color bits ignored
    0,                              // no alpha buffer
    0,                              // shift bit ignored
    0,                              // no accumulation buffer
    0, 0, 0, 0,                     // accum bits ignored
    32,                             // 32-bit z-buffer
    0,                              // no stencil buffer
    0,                              // no auxiliary buffer
    PFD_MAIN_PLANE,                 // main layer
    0,                              // reserved
    0, 0, 0                         // layer masks ignored
  };
	int pixelformat;
	HBITMAP hBmp = (HBITMAP)GetCurrentObject(hDC, OBJ_BITMAP);
	BITMAP bmp;	GetObject(hBmp, sizeof(BITMAP), &bmp);
	pfd.cColorBits=bmp.bmPlanes*bmp.bmBitsPixel;
	if (!(pixelformat=ChoosePixelFormat(hDC, &pfd))) { 
		MessageBox(0,"ChoosePixelFormat failed",0,0); 
		return FALSE; 
	}
	if (!SetPixelFormat(hDC, pixelformat, &pfd)) { 
		MessageBox(0,"SetPixelFormat failed",0,0); 
		return FALSE; 
	}

  return TRUE;
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PHYSTESTBED, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	int i,bString,bMoving; char *ptr;
	for(i=0,ptr=lpCmdLine; i<3; i++) {
		for(g_arg[i]=ptr,bString=0; *ptr && (bString || !isspace(*ptr)); ptr++)
			bString ^= (*ptr=='"');
		if (*ptr)
			*ptr++ = 0;
	}

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
		return FALSE;

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_PHYSTESTBED);

	do {
		float dx=0,dy=0;
		if (GetForegroundWindow()==g_hWnd) {
			if (GetAsyncKeyState('A')<0) dx -= 1;
			if (GetAsyncKeyState('D')<0) dx += 1;
			if (GetAsyncKeyState('W')<0) dy += 1;
			if (GetAsyncKeyState('S')<0) dy -= 1;
		}
		bMoving = dx*dx+dy*dy>0;
		if (bMoving)
			OnCameraMove(dx,dy);

		//if (g_bAnimate)
		//	EvolveWorld();

		if (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (msg.message==WM_QUIT) 
				break;
		}	else if (g_bAnimate | bMoving)
			RenderWorld(g_hWnd,g_hDC);
	} while (true);

	return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_PHYSTESTBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)IDC_PHYSTESTBED;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	InitPhysics();

	g_hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!g_hWnd)
		return FALSE;

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	return TRUE;
}

void UpdateMenuItems(HWND hWnd)
{
	CheckMenuItem(GetMenu(hWnd), ID_ANIMATE, MF_BYCOMMAND | (g_bAnimate ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(GetMenu(hWnd), ID_FASTMOVEMENT, MF_BYCOMMAND | (g_bFast ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(GetMenu(hWnd), ID_SLOWMOVEMENT, MF_BYCOMMAND | (g_bFast ? MF_UNCHECKED:MF_CHECKED));
	CheckMenuItem(GetMenu(hWnd), ID_DISPLAYCOLLISIONS, MF_BYCOMMAND | (g_bShowColl ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(GetMenu(hWnd), ID_DISPLAYPROFILER, MF_BYCOMMAND | (g_bShowProfiler ? MF_CHECKED:MF_UNCHECKED));
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	RECT rect;
	float dx,dy;

	switch (message) {
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId) {
				case ID_ANIMATE: g_bAnimate^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); 
						 if (g_bAnimate) 
							 ReleaseMutex(g_hThreadActive);
						 else
							 WaitForSingleObject(g_hThreadActive,INFINITE);
						 break;
				case ID_FASTMOVEMENT: g_bFast=1; UpdateMenuItems(hWnd); break;
				case ID_SLOWMOVEMENT: g_bFast=0; UpdateMenuItems(hWnd); break;
				case ID_DISPLAYCOLLISIONS: g_bShowColl^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				case ID_DISPLAYPROFILER: g_bShowProfiler^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				case ID_RELOADWORLD: ReloadWorld(g_arg[0]); RenderWorld(hWnd,g_hDC); break;
				case ID_RELOADWORLD1: ReloadWorld(g_arg[2]); RenderWorld(hWnd,g_hDC); break;
				case ID_RELOADGEOMETRIES: ReloadWorldAndGeometries(g_arg[0],g_arg[1]); RenderWorld(hWnd,g_hDC); break;
				case ID_SAVEWORLD: SaveWorld(g_arg[2]); break;
				case IDM_ABOUT:
					DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
					break;
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

		case WM_CREATE: {
			g_hDC = GetDC(hWnd);
			if (!SetupPixelFormat(g_hDC)) return -1;
			if (!(g_glrc=wglCreateContext(g_hDC)))
				MessageBox(0,"wglCreateContext failed",0,0);
			else if (!wglMakeCurrent(g_hDC,g_glrc))
				MessageBox(0,"wglMakeCurrent failed",0,0);
			else {
				SendMessage(hWnd,WM_COMMAND,ID_RELOADGEOMETRIES,0);
				return 0;
			}
			return -1;
		}

		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			RenderWorld(hWnd,g_hDC);
			break;

		case WM_RBUTTONDOWN:
			if (GetCapture()!=hWnd) {
				GetClientRect(hWnd, &rect);
				rect.left = rect.left+rect.right>>1; rect.top = rect.top+rect.bottom>>1;
				ClientToScreen(hWnd,(LPPOINT)&rect);
				SetCursorPos(rect.left,rect.top);
				SetCapture(hWnd);	g_moveMode = 1;
			}
			break;

		case WM_LBUTTONDOWN:
			if (GetCapture()!=hWnd) {
				SetCapture(hWnd);	g_moveMode = 0;
				SendMessage(hWnd, WM_MOUSEMOVE, wParam,lParam);
			}
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_CANCELMODE:
			if (GetCapture()==hWnd)
				ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			if (GetCapture()==hWnd) {
				GetClientRect(hWnd, &rect);
				dx = (float)((short)LOWORD(lParam)-(rect.left+rect.right>>1))/rect.bottom;
				dy = (float)((short)HIWORD(lParam)-(rect.top+rect.bottom>>1))/rect.bottom;
				if (g_moveMode==0)
					SelectObject(dx*0.825f,dy*0.825f);
				else if (dx*dx+dy*dy>0) {
					OnCameraRotate(dx,dy);
					rect.left = rect.left+rect.right>>1; rect.top = rect.top+rect.bottom>>1;
					ClientToScreen(hWnd,(LPPOINT)&rect);
					SetCursorPos(rect.left,rect.top);
				}	else break;
				RenderWorld(hWnd,g_hDC);
			}
			break;

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_SPACE: if (!g_bAnimate) EvolveWorld(); break;
				case VK_DOWN: if (g_bShowProfiler) SelectNextProfVisInfo(); break;
				case VK_UP: if (g_bShowProfiler) SelectPrevProfVisInfo(); break;
				case VK_LEFT: if (g_bShowProfiler) ExpandProfVisInfo(0); break;
				case VK_RIGHT: if (g_bShowProfiler) ExpandProfVisInfo(1); break;
				default: return DefWindowProc(hWnd, message, wParam, lParam);
			}
			RenderWorld(hWnd,g_hDC);
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_DESTROY:
			ShutdownPhysics();
			ReleaseDC(hWnd, g_hDC);
			wglDeleteContext(g_glrc);
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
