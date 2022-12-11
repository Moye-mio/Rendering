#include "math.hpp"
#include "d3d.hpp"
#include "cbuffer.hpp"
#include "camera.hpp"
#include "lines.hpp"
#include "renderer.hpp"
#include <Windows.h>
#include <windowsx.h>

D3D*		g_D3D = NULL;
Camera*		g_Camera = NULL;
Lines*		g_Lines = NULL;
Renderer*	g_Renderer = NULL;

//// ---------------------------------------
//// Window-related code
//// ---------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow, const std::string& WindowTitle, LONG Width, LONG Height, HWND& hWnd)
{
	// Register class
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = DefWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "SmallDOOClass";
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	RECT rc = { 0, 0, Width, Height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	hWnd = CreateWindow("SmallDOOClass", WindowTitle.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!hWnd)
		return E_FAIL;

	ShowWindow(hWnd, nCmdShow);
	return S_OK;
}

// ---------------------------------------
// Update and Render
// ---------------------------------------
void Render()
{
	// Collect objects needed later
	ID3D11DeviceContext* immediateContext = g_D3D->GetImmediateContext();
	ID3D11RenderTargetView* rtvBackbuffer = g_D3D->GetRtvBackbuffer();
	ID3D11DepthStencilView* dsvBackbuffer = g_D3D->GetDsvBackbuffer();

	// Clear the backbuffer and the depth buffer
	float clearColor[4] = { 1,1,1,1 };
	immediateContext->ClearRenderTargetView(rtvBackbuffer, clearColor);
	immediateContext->ClearDepthStencilView(dsvBackbuffer, D3D11_CLEAR_DEPTH, 1, 0);

	// Render the scene with decoupled opacity optimization
	g_Renderer->Draw(immediateContext, g_D3D, g_Lines, g_Camera);

	// Swap
	g_D3D->GetSwapChain()->Present(0, 0);
}

// =============================================================
// =============================================================
// =============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Allocate a debug console and redirect standard streams
	AllocConsole();
	FILE *pCout, *pCerr;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	freopen_s(&pCerr, "CONOUT$", "w", stderr);

	printf("Decoupled Opacity Optimization Demo\n");
	printf("===================================\n\n");
	printf("Move the camera by holding the right mouse button and move back and forth with 'W' and 'S'\n\n");
	printf("Use '0', '1' or '2' as command line argument to select a data set:\n");
	printf("   0 = data/tornado.obj (default)\n");
	printf("   1 = data/rings.obj\n");
	printf("   2 = data/heli.obj\n");

	// =============================================================
	// initialize
	// =============================================================
	int datasetIndex = 0;
	datasetIndex = atoi(lpCmdLine);		// read 0, 1 or 2 from command line
	datasetIndex = 1;

	std::string path;
	Vec3f eye, lookAt;
	Vec2i resolution(700, 700);
	float q, r, lambda, stripWidth;
	int totalNumCPs, smoothingIterations;
	switch (datasetIndex)
	{
	default:
	case 0:
		path = std::string("data\\tornado.obj");
		eye = Vec3f(11, 21, -25);
		lookAt = Vec3f(10, 10, 10);
		q = 60;
		r = 500;
		lambda = 1;
		stripWidth = 0.05f;
		totalNumCPs = 10000;
		smoothingIterations = 10;
		break;
	case 1:
		path = std::string("data\\rings.obj");
		eye = Vec3f(40, 16, 12);
		lookAt = Vec3f(10, 10, 10);
		q = 80;
		r = 40;
		lambda = 1.5f;
		stripWidth = 0.03f;
		totalNumCPs = 10000;
		smoothingIterations = 10;
		break;
	case 2:
		path = std::string("data\\heli.obj");
		eye = Vec3f(40, 4, 17);
		lookAt = Vec3f(10, 10, 10);
		q = 100;
		r = 200;
		lambda = 2;
		stripWidth = 0.03f;
		totalNumCPs = 10000;
		smoothingIterations = 10;
		break;
	}
	printf(("Currently using: " + path + "\n\n").c_str());

	// Create the window
	HWND hWnd;
	const std::string windowTitle = "Fourier Opacity Optimization Demo";
	if (FAILED(InitWindow(hInstance, nCmdShow, windowTitle, resolution.x, resolution.y, hWnd)))
		return -1;

	// Initialize the objects
	g_D3D = new D3D(hWnd);
	g_Camera = new Camera(eye, lookAt, (float)resolution.x / (float)resolution.y, hWnd);
	g_Lines = new Lines(path, totalNumCPs);
	g_Renderer = new Renderer(q, r, lambda, stripWidth, smoothingIterations);

	// Create D3D resources
	ID3D11Device* device = g_D3D->GetDevice();
	g_Camera->Create(device);
	g_Lines->Create(device);
	g_Renderer->D3DCreateDevice(device);
	g_Renderer->D3DCreateSwapChain(device, &g_D3D->GetBackBufferSurfaceDesc());
	
	// ---------------------------------------
	// Enter the main loop
	// ---------------------------------------
	LARGE_INTEGER timerLast, timerCurrent, frequency;
	QueryPerformanceCounter(&timerCurrent);
	timerLast = timerCurrent;

	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		// handle windows messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// close window on 'ESC'
		if (GetAsyncKeyState(VK_ESCAPE) != 0) break;
		
		// get elapsed time
		QueryPerformanceCounter(&timerCurrent);
		QueryPerformanceFrequency(&frequency);
		LONGLONG timeElapsed = timerCurrent.QuadPart - timerLast.QuadPart;
		double elapsedS = (double)timeElapsed / (double)frequency.QuadPart;
		timerLast = timerCurrent;

		// update the camera
		g_Camera->Update(elapsedS);
		// render the scene
		Render();

		printf("\rfps: %i      ", (int)(1.0 / elapsedS));
		std::string newWindowTitleTemp = windowTitle + "      fps: " + std::to_string((int)(1.0 / elapsedS));
		LPCSTR newWindowTitle = newWindowTitleTemp.c_str();
		SetWindowText(hWnd, newWindowTitle);
	}
	
	// Clean up before closing.
	delete g_Camera;
	delete g_Lines;
	delete g_Renderer;
	delete g_D3D;

	return 0;
}