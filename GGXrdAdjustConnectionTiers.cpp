
#ifndef UNICODE
#define UNICODE
#endif
#include "framework.h"
#include "GGXrdAdjustConnectionTiers.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlobj.h> // for CLSID_FileOpenDialog
#include <atlbase.h> // for CComPtr
#include <vector>
#include <algorithm>
#include <string>
#include <Psapi.h>
#include <VersionHelpers.h>
#include "WinError.h"
#include "VdfReader.h"
#include <unordered_map>
#include "SharedFunctions.h"
#include "ExePatcher.h"

using namespace std::literals;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::vector<HWND> whiteBgLabels;
HBRUSH hbrBkgnd = NULL;
HFONT font = NULL;
HWND hWndButton = NULL;
HWND hWndBackupCheckbox = NULL;
HWND mainWindow = NULL;
HWND backupWillbeCreatedMsg1 = NULL;
HWND backupWillbeCreatedMsg2 = NULL;
HWND backupWillbeCreatedMsg3 = NULL;
WNDPROC OldWndProc = NULL;
HWND hWndT4UpperLimitInclusive = NULL;
HWND hWndTNLowerLimitInclusive[3] = { NULL };
HWND hWndTNUpperLimitInclusive[3] = { NULL };
HWND hwndT0LowerLimitNonInclusive = NULL;
const wchar_t* ERROR_STR = L"Error";
SCROLLINFO scrollInfo { 0 };
int scrollNMax = 0;
int scrollNPage = 0;
int windowClientRectHeight = 0;
int wholeWindowConentsHeight = 0;
bool initializationDone = false;
int currentPingValues[5] = { 0 };
wchar_t pingFieldName[5][32] = { L'\0' };
bool somethingWentWrongWhenPatching = false;

// The patch of the GuiltyGearXrd.exe file that was obtained when the GuiltyGearXrd.exe process was seen working.
std::wstring ggProcessModulePath;
std::wstring steamLibraryPath;

// The Y position at which the next text row using addTextRow must be created.
int nextTextRowY = 5;

// Used to restart adding text rows from some fixed Y position.
int nextTextRowYOriginal = 5;

// These variables are used for text measurement when creating controls, to determine their sizes.
HDC hdc = NULL;
HGDIOBJ oldObj = NULL;

// Specifies text color for each static control.
std::unordered_map<HWND, COLORREF> controlColorMap;

// Stores the result of the GetTempPath() call.
wchar_t tempPath[MAX_PATH] { L'\0' };

// The text rows that have been added beneath the "Patch GuiltyGear" button.
// They must be cleared on each button press so that the new output can be displayed
// in that same place.
std::vector<HWND> rowsToRemove;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    NewWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK setFontOnChildWindow(HWND hwnd, LPARAM lParam);
void handlePatchButton();
void handleBackupCheckbox();
HANDLE findOpenGgProcess(bool* foundButFailedToOpen);
void fillGgProcessModulePath(HANDLE ggProcess);
bool validateInstallPath(std::wstring& path);
#define stringAndItsLength(str) str, (int)wcslen(str)
void beginInsertRows();
void finishInsertRows();
HWND addTextRow(const wchar_t* txt);
HWND addTextInCurrentRow(const wchar_t* txt, int x, SIZE* textSz);
void printRemarks(std::vector<std::wstring>& remarks);
void fillInInitialValues();
void deletePreviousMessages();
void onVScroll(HWND hWnd, WPARAM wParam);
void setScrollNMaxNPage(bool isBeingInitialized);
void setScrollNMaxNPage(HWND hWnd, bool isBeingInitialized, int pageSize, int wholeSize);
void onMouseWheel(HWND hWnd, WPARAM wParam);
void onSize(HWND hWnd, LPARAM lParam);
bool parsePingValue(HWND hwnd, const wchar_t* name, int* outPing, HWND mainWindow, bool silenceErrors);
void updateOtherPingFields(int exceptThis);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GGXRDADJUSTCONNECTIONTIERS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }
    
    fillInInitialValues();

    HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_GGXRDADJUSTCONNECTIONTIERS));

    MSG msg;

    // Main message loop:
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GGXRDADJUSTCONNECTIONTIERS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GGXRDADJUSTCONNECTIONTIERS);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	mainWindow = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!mainWindow)
	{
		return FALSE;
	}

	ShowWindow(mainWindow, nCmdShow);
	UpdateWindow(mainWindow);
	
	hbrBkgnd = GetSysColorBrush(COLOR_WINDOW);
	TEXTMETRIC textMetric { 0 };
	NONCLIENTMETRICSW nonClientMetrics { 0 };
	nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICSW);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &nonClientMetrics, NULL);
	font = CreateFontIndirectW(&nonClientMetrics.lfCaptionFont);
	
	
	beginInsertRows();
	
	addTextRow(L"App for patching file Binaries\\Win32\\GuiltyGearXrd.exe,");
	
	addTextRow(L"so that ranges of pings that correspond to connection tiers T4, T3, T2, T1, and T0 can be adjusted");
	
	addTextRow(L"for Guilty Gear Xrd -REVELATOR- Rev 2 version 2211 (works as of 2024 December 5).");
	
	addTextRow(L"This affects the listing of 'Friend'/'Player'/'Private' (same thing called different names) lobbies,");
	
	addTextRow(L"the filtered results of those lobbies, and the connection tier");
	
	addTextRow(L"icon next to a player's name when inside a 'Friend' lobby (may also affect player icons in other");
	
	addTextRow(L"types of lobbies - not tested). May affect ranked match and world lobbies - not tested.");
	
	backupWillbeCreatedMsg1 = addTextRow(L"A backup of GuiltyGearXrd.exe will be created,");
	
	backupWillbeCreatedMsg2 = addTextRow(L"so in case the game stops working after patching, substitute the backup copy of GuiltyGearXrd.exe");
	
	backupWillbeCreatedMsg3 = addTextRow(L"for the current one.");
	
	const wchar_t* title = L"Don't create a backup";
	SIZE textSz{0};
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	hWndBackupCheckbox = CreateWindowW(WC_BUTTONW, title,
		WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | BS_CHECKBOX,
		5, nextTextRowY, textSz.cx + 20, textSz.cy + 8, mainWindow, NULL, hInst,
		NULL);
	nextTextRowY += textSz.cy + 8 + 10;
	
	addTextRow(L"GuiltyGearXrd.exe should be located in the game's installation directory.");
	
	addTextRow(L"");
	
	addTextRow(L"Current ping tier values (parsed from the EXE):");
	addTextRow(L"You may edit these and then press 'Patch GuiltyGear' to apply the changes.");
	
	int x = 5;
	title = L"T4) From 0ms to ";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	addTextInCurrentRow(title, x, &textSz);
	x += textSz.cx;
	
	title = L"000";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	hWndT4UpperLimitInclusive = CreateWindowW(WC_EDITW, L"",
		WS_BORDER | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | ES_NUMBER,
		x, nextTextRowY, textSz.cx * 2, textSz.cy, mainWindow, NULL, hInst,
		NULL);
	OldWndProc = (WNDPROC)SetWindowLongPtr (hWndT4UpperLimitInclusive, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
	SendMessageW(hWndT4UpperLimitInclusive, EM_SETLIMITTEXT, 3, 0);
	x += textSz.cx * 2;
	
	title = L"ms;";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	addTextInCurrentRow(title, x, &textSz);

	nextTextRowY += textSz.cy + 10;
	
	for (int i = 2; i >= 0; --i) {
		wchar_t strbuf[] = L"T_) From ";
		strbuf[1] = L'0' + i + 1;
		x = 5;
		title = strbuf;
		GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
		addTextInCurrentRow(title, x, &textSz);
		x += textSz.cx;
		
		title = L"000";
		GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
		hWndTNLowerLimitInclusive[i] = CreateWindowW(WC_EDITW, L"",
			WS_BORDER | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | ES_READONLY,
			x, nextTextRowY, textSz.cx * 2, textSz.cy, mainWindow, NULL, hInst,
			NULL);
		(WNDPROC)SetWindowLongPtr (hWndTNLowerLimitInclusive[i], GWLP_WNDPROC, (LONG_PTR)NewWndProc);
		x += textSz.cx * 2;
		
		title = L"ms to ";
		GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
		addTextInCurrentRow(title, x, &textSz);
		x += textSz.cx;
		
		title = L"000";
		GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
		hWndTNUpperLimitInclusive[i] = CreateWindowW(WC_EDITW, L"",
			WS_BORDER | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | ES_NUMBER,
			x, nextTextRowY, textSz.cx * 2, textSz.cy, mainWindow, NULL, hInst,
			NULL);
		OldWndProc = (WNDPROC)SetWindowLongPtr (hWndTNUpperLimitInclusive[i], GWLP_WNDPROC, (LONG_PTR)NewWndProc);
		SendMessageW(hWndTNUpperLimitInclusive[i], EM_SETLIMITTEXT, 3, 0);
		x += textSz.cx * 2;
		
		title = L"ms;";
		GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
		addTextInCurrentRow(title, x, &textSz);
	
		nextTextRowY += textSz.cy + 10;
	}
	
	x = 5;
	title = L"T0) Everything above ";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	addTextInCurrentRow(title, x, &textSz);
	x += textSz.cx;
	
	title = L"000";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	hwndT0LowerLimitNonInclusive = CreateWindowW(WC_EDITW, L"",
		WS_BORDER | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | ES_READONLY,
		x, nextTextRowY, textSz.cx * 2, textSz.cy, mainWindow, NULL, hInst,
		NULL);
	(WNDPROC)SetWindowLongPtr (hwndT0LowerLimitNonInclusive, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
	x += textSz.cx * 2;
	
	title = L"ms.";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	addTextInCurrentRow(title, x, &textSz);
	
	nextTextRowY += textSz.cy + 10;
	
	title = L"Patch GuiltyGear";
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	hWndButton = CreateWindowW(WC_BUTTONW, title,
		WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
		5, nextTextRowY, textSz.cx + 20, textSz.cy + 8, mainWindow, NULL, hInst,
		NULL);
	nextTextRowY += textSz.cy + 8;
	nextTextRowYOriginal = nextTextRowY;
	wholeWindowConentsHeight = nextTextRowY;
	initializationDone = true;
	RECT clientRect;
	GetClientRect(mainWindow, &clientRect);
	windowClientRectHeight = clientRect.bottom - clientRect.top;
	setScrollNMaxNPage(true);
	
	wcscpy_s(pingFieldName[4], L"T4 upper limit");
	for (int i = 0; i < 5; ++i) {
		pingFieldName[4][1] = L'0' + i;
		if (i != 4) {
			wcscpy_s(pingFieldName[i], pingFieldName[4]);
		}
	}

	finishInsertRows();
	EnumChildWindows(mainWindow, setFontOnChildWindow, (LPARAM)font);
	return TRUE;
}

LRESULT CALLBACK NewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CHAR:
	{
		if (wParam == '\r' || wParam == '\n') {
			return 0;
		}
		if (wParam == 1) {
			// Q: why 1?
			// A: You are all too young ;) In former times, Control-<c> was the code of the key, mapped to the low range of ASCII (^C=3, ^G=bell, ^H=backspace, ...) – 
			//    chksr
			//    Commented Dec 2, 2016 at 16:37
			//    https://stackoverflow.com/questions/10127054/select-all-text-in-edit-contol-by-clicking-ctrla

            SendMessageW(hWnd, EM_SETSEL, 0, -1);
    		return 0;
		}
	}
	break;
	}
	return CallWindowProcW(OldWndProc, hWnd, message, wParam, lParam);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CTLCOLORSTATIC: {
		// Provides background and text colors for static (text) controls.
		auto found = controlColorMap.find((HWND)lParam);
		if (found != controlColorMap.end()) {
			HDC hdcStatic = (HDC) wParam;
			SetTextColor(hdcStatic, found->second);
		}
		if (std::find(whiteBgLabels.begin(), whiteBgLabels.end(), (HWND)lParam) != whiteBgLabels.end()) {
			return (INT_PTR)hbrBkgnd;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;
    case WM_COMMAND:
		// A button click
        {
			if ((HWND)lParam == hWndButton) {
				// The "Patch GuiltyGear" button
				handlePatchButton();
				break;
			}
			if ((HWND)lParam == hWndBackupCheckbox) {
				handleBackupCheckbox();
				break;
			}
			static bool antiInfiniteLoop = false;
			if ((HWND)lParam == hWndT4UpperLimitInclusive) {
				if (antiInfiniteLoop) break;
				currentPingValues[4] = 0;
				if (parsePingValue((HWND)lParam, pingFieldName[4], &currentPingValues[4], hWnd, true)) {
					antiInfiniteLoop = true;
					updateOtherPingFields(4);
					antiInfiniteLoop = false;
        		}
				break;
			}
			int i;
			for (i = 0; i < 3; ++i) {
				if ((HWND)lParam == hWndTNUpperLimitInclusive[i]) {
					if (antiInfiniteLoop) break;
					currentPingValues[i + 1] = 0;
					if (parsePingValue((HWND)lParam, pingFieldName[i + 1], &currentPingValues[i + 1], hWnd, true)) {
						antiInfiniteLoop = true;
						updateOtherPingFields(i + 1);
						antiInfiniteLoop = false;
        			}
					break;
				}
			}
			if (i != 3) break;
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // Not drawing anything here yet
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE: {
    	if (initializationDone) {
        	onSize(hWnd, lParam);
    	}
        break;
    }
    case WM_VSCROLL: {
        onVScroll(hWnd, wParam);
        break;
    }
    case WM_MOUSEWHEEL: {
        onMouseWheel(hWnd, wParam);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL CALLBACK setFontOnChildWindow(HWND hwnd, LPARAM lParam) {
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
	return TRUE;
}

// Finds if GuiltyGearXrd.exe is currently open and returns a handle to its process.
// -foundButFailedToOpen- may be used to learn that the process was found, but a handle
// to it could not be retrieved.
HANDLE findOpenGgProcess(bool* foundButFailedToOpen) {
    if (foundButFailedToOpen) *foundButFailedToOpen = false;
    // this method was chosen because it's much faster than enumerating all windows or all processes and checking their names
    const HWND foundGgWindow = FindWindowW(L"LaunchUnrealUWindowsClient", L"Guilty Gear Xrd -REVELATOR-");
    if (!foundGgWindow) return NULL;
    DWORD windsProcId = 0;
    GetWindowThreadProcessId(foundGgWindow, &windsProcId);
    HANDLE windsProcHandle = OpenProcess(
		// because GetModuleFileNameExW MSDN page says just this right is enough for Windows10+
		IsWindows10OrGreater() ? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE, windsProcId);
    if (!windsProcHandle && foundButFailedToOpen) {
        *foundButFailedToOpen = true;
    }
    return windsProcHandle;
}

// Uses Steam config files or other methods to discover the directory where Guilty Gear Xrd is installed.
// Returns true if a directory was found. The directory still needs to be validated.
bool findGgInstallPath(std::wstring& path) {
	
	if (!ggProcessModulePath.empty()) {
		std::wstring tmpPath = ggProcessModulePath;
		for (int i = 0; i < 3; ++i) {
			int pos = findCharRevW(tmpPath.c_str(), L'\\');
			if (pos < 1) {
				break;
			}
			if (pos > 0 && (tmpPath[pos - 1] == L'\\' || tmpPath[pos - 1] == L':')) {
				break;
			}
			tmpPath.resize(pos);
		}
		if (!tmpPath.empty()) {
			path = tmpPath;
			return true;
		}
		return false;
	}

	class CloseStuffOnReturn {
	public:
		HKEY hKey = NULL;
		~CloseStuffOnReturn() {
			if (hKey) RegCloseKey(hKey);
		}
	} closeStuffOnReturn;

	HKEY steamReg = NULL;
	LSTATUS lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", NULL, KEY_READ, &steamReg);
	if (lStatus != ERROR_SUCCESS) {
		WinError err;
		OutputDebugStringW(L"RegOpenKeyExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		return false;
	}
	closeStuffOnReturn.hKey = steamReg;

	wchar_t gimmePath[MAX_PATH] { L'0' };
	DWORD gimmePathSize = sizeof gimmePath;
	DWORD dataType = 0;
	lStatus = RegQueryValueExW(steamReg, L"InstallPath", NULL, &dataType, (BYTE*)gimmePath, &gimmePathSize);
	if (lStatus != ERROR_SUCCESS) {
		WinError err;
		OutputDebugStringW(L"RegQueryValueExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		return false;
	}
	RegCloseKey(steamReg);
	closeStuffOnReturn.hKey = NULL;
	steamReg = NULL;

	if (dataType != REG_SZ) {
		OutputDebugStringW(L"RegQueryValueExW returned not a string\n");
		return false;
	}
	if (gimmePathSize == MAX_PATH && gimmePath[MAX_PATH - 1] != L'\0' || gimmePathSize >= MAX_PATH) {
		OutputDebugStringW(L"RegQueryValueExW returned a string that's too big probably\n");
		return false;
	}
	if (gimmePathSize == 0) {
		OutputDebugStringW(L"RegQueryValueExW returned an empty string without even a null character\n");
		return false;
	}
	gimmePath[gimmePathSize - 1] = L'\0';
	if (gimmePathSize == 1) {
		OutputDebugStringW(L"RegQueryValueExW returned an empty string\n");
		return false;
	}
	std::wstring steamPath = gimmePath;
	if (steamPath.empty() || steamPath[steamPath.size() - 1] != L'\\') {
		steamPath += L'\\';
	}
	steamPath += L"config\\libraryfolders.vdf";
	VdfReader reader(steamPath);
	int i = 0;
	bool containsI;
	do {
		std::wstring iAsString = std::to_wstring(i);
		containsI = reader.hasKey(L"libraryfolders", iAsString);
		if (containsI) {
			std::wstring appId = L"520440";
			if (reader.hasKey(L"libraryfolders/" + iAsString + L"/apps", appId)) {
				reader.getValue(L"libraryfolders/" + iAsString + L"/path", steamLibraryPath);
				path = steamLibraryPath;
				if (path.empty() || path[path.size() - 1] != L'\\') {
					path += L'\\';
				}
				path += L"steamapps\\common\\GUILTY GEAR Xrd -REVELATOR-";
				return true;
			}
		}
		++i;
	} while (containsI);
	return false;
}

// Asks the user to select a directory through a dialog.
bool selectFolder(std::wstring& path) {
    wchar_t pszBuf[MAX_PATH] = { '\0' };
    CComPtr<IFileDialog> fileDialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_IFileDialog, (LPVOID*)&fileDialog))) return false;
    if (!fileDialog) return false;
    DWORD dwFlags;
    if (FAILED(fileDialog->GetOptions(&dwFlags))) return false;
    if (FAILED(fileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS))) return false;
    if (!path.empty()) {
        CComPtr<IShellItem> shellItem;
        if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), NULL, IID_IShellItem, (void**)&shellItem))) {
            fileDialog->SetFolder(shellItem);
        }
    }
    if (fileDialog->Show(mainWindow) != S_OK) return false;
    IShellItem* psiResult = nullptr;
    if (FAILED(fileDialog->GetResult(&psiResult))) return false;
    SFGAOF attribs{ 0 };
    if (FAILED(psiResult->GetAttributes(SFGAO_FILESYSTEM, &attribs))) return false;
    if ((attribs & SFGAO_FILESYSTEM) == 0) {
        MessageBoxW(mainWindow, L"The selected file is not a file system file.", ERROR_STR, MB_OK);
        return false;
    }
    LPWSTR pszName = nullptr;
    if (FAILED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszName))) return false;
    path = pszName;
    CoTaskMemFree(pszName);
    return true;
}

// Obtains the path of the GuiltyGearXrd.exe executable via its process and stores it into -ggProcessModulePath-.
void fillGgProcessModulePath(HANDLE ggProcess) {
	// sneaky or convenient? User is completely unaware of this feature, UI is not communicating it in any way
	ggProcessModulePath.resize(MAX_PATH - 1, L'\0');
	if (GetModuleFileNameExW(ggProcess, NULL, &ggProcessModulePath.front(), MAX_PATH) == 0) {
		WinError err;
		OutputDebugStringW(L"GetModuleFileNameExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		ggProcessModulePath.clear();
		return;
	}
	if (ggProcessModulePath.empty() || ggProcessModulePath[ggProcessModulePath.size() - 1] != L'\0') {
		OutputDebugStringW(L"fillGgProcessModulePath: ggProcessModulePath not null-terminated\n");
		ggProcessModulePath.clear();
		return;
	}
	ggProcessModulePath.resize(wcslen(ggProcessModulePath.c_str()));
}

// Checks if the provided installation path is valid.
bool validateInstallPath(std::wstring& path) {
	std::wstring pathFixed = path;
	if (pathFixed.empty() || pathFixed[pathFixed.size() - 1] != L'\\') {
		pathFixed += L'\\';
	}
	std::wstring subPath;

	subPath = pathFixed + L"Binaries\\Win32\\GuiltyGearXrd.exe";
	if (!fileExists(subPath.c_str())) {
		return false;
	}

	return true;

}

// Calls this once before calling addTextRow one or more times
void beginInsertRows() {
	hdc = GetDC(mainWindow);
	oldObj = SelectObject(hdc, (HGDIOBJ)font);
}

// Calls this once after calling addTextRow one or more times
void finishInsertRows() {
	SelectObject(hdc, oldObj);
	ReleaseDC(mainWindow, hdc);
	hdc = NULL;
	oldObj = NULL;
}

// One or more calls to this method must be wrapped by a single call to beginInsertRows()
// and a single calls to finishInsertRows().
// Adds a row of text at -nextTextRowY-.
HWND addTextRow(const wchar_t* txt) {
	SIZE textSz{0};
	GetTextExtentPoint32W(hdc, stringAndItsLength(txt), &textSz);
	HWND newWindow = CreateWindowW(WC_STATICW, txt,
			WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
			5, nextTextRowY, textSz.cx, textSz.cy, mainWindow, NULL, hInst,
			NULL);
	whiteBgLabels.push_back(newWindow);
	nextTextRowY += textSz.cy + 5;
	wholeWindowConentsHeight = nextTextRowY;
	if (initializationDone) setScrollNMaxNPage(false);
	return newWindow;
}

// One or more calls to this method must be wrapped by a single call to beginInsertRows()
// and a single calls to finishInsertRows().
// Adds a row of text at -nextTextRowY-.
HWND addTextInCurrentRow(const wchar_t* txt, int x, SIZE* textSz) {
	HWND newWindow = CreateWindowW(WC_STATICW, txt,
			WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
			x, nextTextRowY, textSz->cx, textSz->cy, mainWindow, NULL, hInst,
			NULL);
	whiteBgLabels.push_back(newWindow);
	return newWindow;
}

// Breaks up a string into multiple by -c- char. The new strings do not include the delimiter.
std::vector<std::wstring> split(const std::wstring& str, wchar_t c) {
	std::vector<std::wstring> result;
	if (str.empty()) return result;
	const wchar_t* strStart = &str.front();
	const wchar_t* strEnd = strStart + str.size();
	const wchar_t* prevPtr = strStart;
	const wchar_t* ptr = strStart;
	while (*ptr != L'\0') {
		if (*ptr == c) {
			if (ptr > prevPtr) {
				result.emplace_back(prevPtr, ptr - prevPtr);
			}
			prevPtr = ptr + 1;
		}
		++ptr;
	}
	if (prevPtr < strEnd) {
		result.emplace_back(prevPtr, strEnd - prevPtr);
	}
	return result;
}

// Creates one or more text rows for each element in -remarks-.
// It depends on whether the text row's character count is below some fixed predetermined count.
// If it's greater the row gets split into multiple.
// All rows are colored blue.
void printRemarks(std::vector<std::wstring>& remarks) {
	for (std::wstring& line : remarks) {
		beginInsertRows();
		while (!line.empty()) {
			std::wstring subline = line;
			const int charsMax = 150;
			if (line.size() > charsMax) {
				subline.resize(charsMax);
				line = line.substr(charsMax);
			} else {
				line.clear();
			}
			HWND newWindow = addTextRow(subline.c_str());
			rowsToRemove.push_back(newWindow);
			setFontOnChildWindow(newWindow, NULL);
			controlColorMap.insert({newWindow, RGB(0, 0, 255)});
			UpdateWindow(newWindow);
		}
		finishInsertRows();
	}
	if (!remarks.empty()) InvalidateRect(mainWindow, NULL, TRUE);
}

void handleBackupCheckbox() {
	bool isChecked = SendMessageW(hWndBackupCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
	isChecked = !isChecked;
	SendMessageW(hWndBackupCheckbox, BM_SETCHECK, isChecked ? BST_CHECKED : BST_UNCHECKED, 0);
	ShowWindow(backupWillbeCreatedMsg1, isChecked ? SW_HIDE : SW_SHOWNA);
	ShowWindow(backupWillbeCreatedMsg2, isChecked ? SW_HIDE : SW_SHOWNA);
	ShowWindow(backupWillbeCreatedMsg3, isChecked ? SW_HIDE : SW_SHOWNA);
}

void fillInInitialValues() {
	deletePreviousMessages();
	
	HANDLE ggProcess = findOpenGgProcess(nullptr);
	if (ggProcess) {
		// We can use the path of GuiltyGearXrd.exe as a backup mechanism for determining the installation directory of the game.
		fillGgProcessModulePath(ggProcess);
		CloseHandle(ggProcess);
		ggProcess = NULL;
	}
	
	// Find where the game is installed.
	std::wstring installPath;
	if (!(findGgInstallPath(installPath) && !installPath.empty() && validateInstallPath(installPath))) {
		installPath.clear();
	}
	if (installPath.empty()) {
		int response = MessageBoxW(mainWindow, L"Please select the Guilty Gear Xrd installation directory.", L"Message", MB_OKCANCEL);
		if (response != IDOK) {
			return;
		}
		// Ask the user to provide the installation directory manually if we failed to find it.
		if (!selectFolder(installPath)) {
			installPath.clear();
		}
		if (!validateInstallPath(installPath)) {
			MessageBoxW(mainWindow, L"Could not locate Binaries\\Win32\\GuiltyGearXrd.exe in the"
				" selected folder. Please select another folder and try again.", ERROR_STR, MB_OK);
			installPath.clear();
		}
	}
	if (installPath.empty()) return;
	if (installPath[installPath.size() - 1] != L'\\') {
		installPath += L'\\';
	}

	std::wstring remarks;
	PingValues values;
	if (!obtainValuesFromExe(installPath + L"Binaries\\Win32\\GuiltyGearXrd.exe", &values, remarks, mainWindow)) {
		return;
	}
	std::vector<std::wstring> remarksLines = split(remarks, L'\n');
	printRemarks(remarksLines);
	remarksLines.clear();
	
	for (int i = 1; i < 5; ++i) {
		currentPingValues[i] = values[i].max;
	}
	
	updateOtherPingFields(-1);
}

void updateOtherPingFields(int exceptThis) {
	char itoabuf[4] = { '\0' };
	if (exceptThis != 0) {
		_itoa_s(currentPingValues[1], itoabuf, 10);
		SetWindowTextA(hwndT0LowerLimitNonInclusive, itoabuf);
	}
	
	if (exceptThis != 4) {
		memset(itoabuf, 0, sizeof itoabuf);
		_itoa_s(currentPingValues[4], itoabuf, 10);
		SetWindowTextA(hWndT4UpperLimitInclusive, itoabuf);
	}
	
	for (int i = 1; i < 4; ++i) {
		if (exceptThis == i) continue;
		
		memset(itoabuf, 0, sizeof itoabuf);
		_itoa_s(currentPingValues[i + 1] + 1, itoabuf, 10);
		SetWindowTextA(hWndTNLowerLimitInclusive[i - 1], itoabuf);
		
		memset(itoabuf, 0, sizeof itoabuf);
		_itoa_s(currentPingValues[i], itoabuf, 10);
		SetWindowTextA(hWndTNUpperLimitInclusive[i - 1], itoabuf);
	}
	
}

void deletePreviousMessages() {
	// Delete messages from the previous run of the handlePatchButton()
	for (HWND hwnd : rowsToRemove) {
		DestroyWindow(hwnd);
		controlColorMap.erase(hwnd);
		for (auto it = whiteBgLabels.begin(); it != whiteBgLabels.end(); ++it) {
			if (*it == hwnd) {
				whiteBgLabels.erase(it);
				break;
			}
		}
	}
	rowsToRemove.clear();
	nextTextRowY = nextTextRowYOriginal;
	wholeWindowConentsHeight = nextTextRowY;
	setScrollNMaxNPage(false);
}

bool parsePingValue(HWND hwnd, const wchar_t* name, int* outPing, HWND mainWindow, bool silenceErrors) {
	std::wstring errorStr;
	int txtLen = Edit_GetTextLength(hwnd);
	if (txtLen > 3) {
		if (!silenceErrors) {
			errorStr = name;
			errorStr += L" contains more than 3 characters, which is not allowed.";
			MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
		}
		return false;
	}
	wchar_t chars[4];
	Edit_GetText(hwnd, chars, 4);
	chars[_countof(chars) - 1] = L'\0';
	for (wchar_t* c = chars; *c != L'\0'; ++c) {
		if (!(*c >= L'0' && *c <= L'9')) {
			if (!silenceErrors) {
				errorStr = name;
				errorStr += L" contains non-digit characters, which is not allowed.";
				MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
			}
			return false;
		}
	}
	int parsedInt;
	int elemsParsed = swscanf_s(chars, L"%d", &parsedInt);
	if (elemsParsed != 1) {
		if (!silenceErrors) {
			errorStr = L"In the ping value for ";
			errorStr += name;
			errorStr += L" failed to parse the number.";
			MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
		}
		return false;
	}
	if (parsedInt == 999) {
		if (!silenceErrors) {
			errorStr = L"In the ping value for ";
			errorStr += name;
			errorStr += L" is 999, which is not allowed.";
			MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
		}
		return false;
	}
	if (outPing) *outPing = parsedInt;
	return true;
}

// The "Patch GuiltyGear" button handler.
void handlePatchButton() {
	deletePreviousMessages();
	
	if (!parsePingValue(hWndT4UpperLimitInclusive, pingFieldName[4], &currentPingValues[4], mainWindow, false)) {
		return;
	}
	for (int i = 1; i < 4; ++i) {
		if (!parsePingValue(hWndTNUpperLimitInclusive[i - 1], pingFieldName[i], &currentPingValues[i], mainWindow, false)) {
			return;
		}
	}
	for (int i = 1; i < 4; ++i) {
		if (currentPingValues[i] < currentPingValues[i + 1]) {
			std::wstring errorStr = L"The values must go in order. The value in the field '";
			errorStr += pingFieldName[i];
			errorStr += L"' (" + std::to_wstring(currentPingValues[i]) + L") cannot be < than the value in the field '";
			errorStr += pingFieldName[i + 1];
			errorStr += L"' (" + std::to_wstring(currentPingValues[i + 1]) + L").";
			MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
			return;
		}
	}
	

	// GuiltyGearXrd.exe must not be running during the patching process.
	bool foundButFailedToOpen = false;
	HANDLE ggProcess = findOpenGgProcess(&foundButFailedToOpen);
	if (ggProcess || foundButFailedToOpen) {
		if (ggProcess) {
			// We can use the path of GuiltyGearXrd.exe as a backup mechanism for determining the installation directory of the game.
			fillGgProcessModulePath(ggProcess);
			CloseHandle(ggProcess);
			ggProcess = NULL;
		}
		MessageBoxW(mainWindow, L"Guilty Gear Xrd is currently open. Please close it and try again.", ERROR_STR, MB_OK);
		return;
	}
	if (ggProcess) {
		CloseHandle(ggProcess);
		ggProcess = NULL;
	}
	
	// Find where the game is installed.
	std::wstring installPath;
	if (!(findGgInstallPath(installPath) && !installPath.empty() && validateInstallPath(installPath))) {
		installPath.clear();
	}
	if (installPath.empty()) {
		int response = MessageBoxW(mainWindow, L"Please select the Guilty Gear Xrd installation directory.", L"Message", MB_OKCANCEL);
		if (response != IDOK) {
			return;
		}
		// Ask the user to provide the installation directory manually if we failed to find it.
		if (!selectFolder(installPath)) {
			installPath.clear();
		}
		if (!validateInstallPath(installPath)) {
			MessageBoxW(mainWindow, L"Could not locate Binaries\\Win32\\GuiltyGearXrd.exe in the"
				" selected folder. Please select another folder and try again.", ERROR_STR, MB_OK);
			installPath.clear();
		}
	}
	if (installPath.empty()) return;
	if (installPath[installPath.size() - 1] != L'\\') {
		installPath += L'\\';
	}

	// Patch GuiltyGearXrd.exe.
	std::wstring remarks;
	if (!performExePatching(installPath + L"Binaries\\Win32\\GuiltyGearXrd.exe", remarks, mainWindow,
			SendMessageW(hWndBackupCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED ? false : true,
			currentPingValues)) {
		return;
	}
	std::vector<std::wstring> remarksLines = split(remarks, L'\n');
	printRemarks(remarksLines);
	remarksLines.clear();
	
	MessageBoxW(mainWindow,
		somethingWentWrongWhenPatching
		? L"Patching is complete with errors. You may now close this window."
		: L"Patching is complete. You may now close this window.",
		L"Message", MB_OK);	

}
void onVScroll(HWND hWnd, WPARAM wParam) {
    const WORD scrollingRequest = LOWORD(wParam);
    const WORD scrollPosition = HIWORD(wParam);

    const int oldScrollPos = scrollInfo.nPos;
    scrollInfo.fMask = SIF_POS;

    switch (scrollingRequest) {
    case SB_BOTTOM:
        scrollInfo.nPos = scrollNMax - (scrollNPage - 1);
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_TOP:
        scrollInfo.nPos = 0;
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_LINEDOWN:
        scrollInfo.nPos += 21;
        if (scrollInfo.nPos > scrollNMax - (scrollNPage - 1)) {
            scrollInfo.nPos = scrollNMax - (scrollNPage - 1);
        }
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_LINEUP:
        if (scrollInfo.nPos <= 21) {
            scrollInfo.nPos = 0;
        } else {
            scrollInfo.nPos -= 21;
        }
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_PAGEDOWN:
        scrollInfo.nPos += scrollInfo.nPage;
        if (scrollInfo.nPos > scrollNMax - (scrollNPage - 1)) {
            scrollInfo.nPos = scrollNMax - (scrollNPage - 1);
        }
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_PAGEUP:
        if (scrollInfo.nPos <= scrollNPage) {
            scrollInfo.nPos = 0;
        } else {
            scrollInfo.nPos -= scrollNPage;
        }
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;
    case SB_THUMBTRACK:
        scrollInfo.nPos = scrollPosition;
        SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
        break;

    }

    const int scrollDifference = scrollInfo.nPos - oldScrollPos;

    if (scrollDifference != 0) {
    	ScrollWindow(hWnd, 0, -scrollDifference, NULL, NULL);
    }
}

void setScrollNMaxNPage(bool isBeingInitialized) {
	setScrollNMaxNPage(mainWindow, isBeingInitialized, windowClientRectHeight, wholeWindowConentsHeight);
}

void setScrollNMaxNPage(HWND hWnd, bool isBeingInitialized, int pageSize, int wholeSize) {
	bool needSetScrollInfo = false;
    scrollInfo.cbSize = sizeof(SCROLLINFO);
    scrollInfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;

    int oldNPos = scrollInfo.nPos;
	
	int oldWholeSize = scrollInfo.nMax;
	int oldPageSize = scrollInfo.nPage;
	
    scrollInfo.nMin = 0;
    scrollInfo.nMax = wholeSize - 1;
    scrollInfo.nPage = pageSize;

    if (scrollInfo.nPage - 1 >= (UINT)scrollInfo.nMax) {
        scrollInfo.nPos = 0;
    } else {
        const int actualScrollMax = scrollInfo.nMax - ((int)scrollInfo.nPage - 1);
        if (scrollInfo.nPos > actualScrollMax) {
            scrollInfo.nPos = actualScrollMax;
        }
    }
    scrollNMax = scrollInfo.nMax;
    scrollNPage = scrollInfo.nPage;

    needSetScrollInfo = isBeingInitialized
        || oldNPos != scrollInfo.nPos
        || oldWholeSize != scrollInfo.nMax
        || oldPageSize != scrollInfo.nPage;

    if (!isBeingInitialized && needSetScrollInfo && scrollInfo.nPos != oldNPos) {
    	ScrollWindow(hWnd, 0, oldNPos - scrollInfo.nPos, NULL, NULL);
    }
    if (needSetScrollInfo) {
    	SetScrollInfo(hWnd, SB_VERT, &scrollInfo, TRUE);
    }
}

void onMouseWheel(HWND hWnd, WPARAM wParam) {
    scrollInfo.fMask = SIF_POS;

    const int oldScrollPos = scrollInfo.nPos;

    if (scrollNPage >= scrollNMax + 1) return;
    
    const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam) / 40;

    scrollInfo.nPos -= 21 * wheelDelta;
    if (scrollInfo.nPos > scrollNMax - (scrollNPage - 1)) {
        scrollInfo.nPos = scrollNMax - (scrollNPage - 1);
    }
    if (scrollInfo.nPos < 0) {
        scrollInfo.nPos = 0;
    }
    SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);

    const int scrollDifference = scrollInfo.nPos - oldScrollPos;

    if (scrollDifference != 0) {
    	ScrollWindow(hWnd, 0, -scrollDifference, NULL, NULL);
    }
}

void onSize(HWND hWnd, LPARAM lParam) {
    const unsigned short newWidth = LOWORD(lParam);
    const unsigned short newHeight = HIWORD(lParam);

    windowClientRectHeight = (int)newHeight;
    setScrollNMaxNPage(false);

    InvalidateRect(hWnd, NULL, TRUE);
}