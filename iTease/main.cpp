#include "stdinc.h"
#include "common.h"
#include "cpp/string.h"
#include "system.h"
#include "application.h"
#include "server.h"
#include "logging.h"
#include "resource.h"
#include <future>
#include <ShellAPI.h>
#pragma comment (lib, "shell32.lib")
#include <direct.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hWnd;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM RegisterWindowClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int serverPort = 36900;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	RegisterWindowClass(hInstance);

	// Perform application initialization
	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	// Windows wants this
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	{
		iTease::Application app(GetCommandLineW());
		// Create a debug console like a badass
		#ifdef _DEBUG
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		#endif

		// Initialise app with command line params
		app.opt.port = serverPort = std::stoi(app.get_args().at("port"));

		if (app.init()) {
			std::atomic<bool> exit = false;
			std::mutex active_thread;
			iTease::Server server(app);
			/*auto server_thread = std::thread([&]() {
				active_thread.lock();
				active_thread.unlock();
				while (!exit) {
					active_thread.lock();
					active_thread.unlock();
					Sleep(1);
				}
			});*/

		#ifdef _DEBUG
			auto console_thread = std::thread([&]() {
				while (!exit) {
					std::string cmd;
					if (!std::getline(std::cin, cmd).fail()) {
						std::lock_guard<std::mutex> lock(active_thread);
						if (cmd == "exit") {
							std::cout << "Exiting..." << std::endl;
							std::this_thread::sleep_for(std::chrono::seconds(3));
							exit = true;
						}
					}
				}
			});
		#endif

			//
			HANDLE hFileUpdateWatcher;
			hFileUpdateWatcher = FindFirstChangeNotification(L"system", TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);

			// The 'Windows' loop
			//active_thread.unlock();
			while (WM_QUIT != msg.message) {
				{
					std::lock_guard<std::mutex> lock(active_thread);
					if (exit) break;
					if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
					if (WM_QUIT != msg.message) {
						try {
							if (hFileUpdateWatcher == INVALID_HANDLE_VALUE)
								throw std::exception("FindFirstChangeNotification function failed");
							if (WaitForSingleObject(hFileUpdateWatcher, 0) == WAIT_OBJECT_0) {
								app.partial_restart();

								if (FindNextChangeNotification(hFileUpdateWatcher) == FALSE)
									throw std::exception("FindFirstChangeNotification function failed");
							}

							server.run();
							// run until app wants to exit
							app.run();
							if (exit || !app.is_running()) break;
						}
						/*catch (iTease::JS::ExceptionInfo& ex) {
							auto str = std::string("JavaScript exception:\n") + ex.what();
							ITEASE_LOGERROR(str);
							MessageBox(NULL, L"A fatal error occurred.", L"iTease Application", MB_OK | MB_ICONERROR);
							PostQuitMessage(1);
						}*/
						catch (const std::bad_alloc&) {
							// yo momma so poor she ran out of RAM
							MessageBox(NULL, L"An out-of-memory error occurred.", L"iTease Application", MB_OK | MB_ICONERROR);
							break;
						}
						catch (const std::exception& ex) {
							// oh snap!
							std::wstring str = L"A fatal error occurred.\n" + iTease::to_wstring(ex.what());
							MessageBox(NULL, L"A fatal error occurred.", L"iTease Application", MB_OK | MB_ICONERROR);
							break;
						}
					}
				}

				// chill for a millisec
				Sleep(1);
			}
			exit = true;
			FindCloseChangeNotification(hFileUpdateWatcher);
			// we should probably wait for the other threads before getting out of here
			console_thread.join();
			//server_thread.join();
		}
		/*else {
			MessageBox(NULL, L"Failed to initalize application", L"iTease Application", MB_OK | MB_ICONERROR);
		}*/
	}

	if (IsWindow(hWnd))
		DestroyWindow(hWnd);
	return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM RegisterWindowClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = L"";
	wcex.lpszClassName = L"iTease";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance;

	if (hWnd = CreateWindow(L"iTease", L"", 0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr)) {
		UpdateWindow(hWnd);
		return TRUE;
	}
	return FALSE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
const UINT WM_TRAY = WM_USER + 1;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE: {
		NOTIFYICONDATA stData;
		ZeroMemory(&stData, sizeof(stData));
		stData.cbSize = sizeof(stData);
		stData.hWnd = hWnd;
		stData.dwState = NIS_SHAREDICON;
		stData.uVersion = NOTIFYICON_VERSION;
		stData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		stData.uCallbackMessage = WM_TRAY;
		stData.uTimeout = 15000;
		stData.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
		Shell_NotifyIcon(NIM_ADD, &stData);
		Shell_NotifyIcon(NIM_SETVERSION, &stData);
		return 0;
	}
	case WM_TRAY: {
		switch (lParam) {
		case WM_LBUTTONUP:
		case WM_RBUTTONUP: {
			HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU1));
			if (hMenu) {
				HMENU hSubMenu = GetSubMenu(hMenu, 0);
				if (hSubMenu) {
					POINT stPoint;
					GetCursorPos(&stPoint);
					SetForegroundWindow(hWnd);
					TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, stPoint.x, stPoint.y, 0, hWnd, NULL);
					PostMessage(hWnd, WM_NULL, 0, 0);
				}
				DestroyMenu(hMenu);
			}
			break;
		}
		default: break;
		}
		return 0;
	}
	case WM_COMMAND: {
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId) {
		case ID_ROOT_OPEN:
			ShellExecute(0, 0, std::wstring(L"http://127.0.0.1:" + std::to_wstring(serverPort)).c_str(), 0, 0, SW_SHOW);
			break;
		case ID_ROOT_EXIT:
			// Exit? Pretty self explanatory...
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		break;
	}
	default: return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message) {
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}