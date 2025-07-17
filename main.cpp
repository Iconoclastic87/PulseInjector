#define UNICODE
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <commdlg.h>
#include <string>
#include <shlwapi.h>
#include <psapi.h>
#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comctl32.lib")  // Link common controls library

HWND hProcNameInput, hDllPathLabel;
std::wstring selectedDllPath;

DWORD GetProcIdByName(const std::wstring& procName) {
    PROCESSENTRY32W entry = { sizeof(entry) };
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    while (Process32NextW(snapshot, &entry)) {
        if (!_wcsicmp(entry.szExeFile, procName.c_str())) {
            pid = entry.th32ProcessID;
            break;
        }
    }
    CloseHandle(snapshot);
    return pid;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    void* pAlloc = VirtualAllocEx(hProc, nullptr, (dllPath.size() + 1) * sizeof(wchar_t),
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pAlloc) {
        CloseHandle(hProc);
        return false;
    }

    if (!WriteProcessMemory(hProc, pAlloc, dllPath.c_str(), dllPath.size() * sizeof(wchar_t), nullptr)) {
        VirtualFreeEx(hProc, pAlloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    FARPROC loadLib = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!loadLib) {
        VirtualFreeEx(hProc, pAlloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)loadLib,
                                        pAlloc, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProc, pAlloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hProc);
    return true;
}

void OpenFileDialog(HWND hwnd) {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        selectedDllPath = filename;
        SetWindowTextW(hDllPathLabel, filename);
    }
}

void OnInject(HWND hwnd) {
    wchar_t procName[260];
    GetWindowTextW(hProcNameInput, procName, 260);

    if (selectedDllPath.empty() || wcslen(procName) == 0) {
        MessageBoxW(hwnd, L"Select a DLL and enter process name.", L"Error", MB_ICONERROR);
        return;
    }

    DWORD pid = GetProcIdByName(procName);
    if (pid == 0) {
        MessageBoxW(hwnd, L"Process not found.", L"Error", MB_ICONERROR);
        return;
    }

    if (InjectDLL(pid, selectedDllPath)) {
        MessageBoxW(hwnd, L"Injection successful!", L"Success", MB_OK);
    } else {
        MessageBoxW(hwnd, L"Injection failed.", L"Error", MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icex);

        
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

       
        HBRUSH hBackground = CreateSolidBrush(RGB(45, 45, 48));
        SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBackground);

        
        HWND hProcLabel = CreateWindowW(L"STATIC", L"Process Name", WS_VISIBLE | WS_CHILD,
                                       20, 15, 150, 28, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hProcLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

      
        hProcNameInput = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                      20, 45, 480, 30, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hProcNameInput, WM_SETFONT, (WPARAM)hFont, TRUE);

      
        HWND hDllLabel = CreateWindowW(L"STATIC", L"DLL Path", WS_VISIBLE | WS_CHILD,
                                      20, 85, 150, 28, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hDllLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

      
        hDllPathLabel = CreateWindowW(L"STATIC", L"No DLL selected", WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
                                     20, 115, 480, 30, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(hDllPathLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        
        HWND hSelectDllBtn = CreateWindowW(L"BUTTON", L"Select DLL", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                           20, 155, 150, 40, hwnd, (HMENU)1, nullptr, nullptr);
        SendMessageW(hSelectDllBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

       
        HWND hInjectBtn = CreateWindowW(L"BUTTON", L"Inject DLL", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                                       350, 155, 150, 40, hwnd, (HMENU)2, nullptr, nullptr);
        SendMessageW(hInjectBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(220, 220, 220));
        SetBkMode(hdcStatic, TRANSPARENT);
        static HBRUSH hBrush = CreateSolidBrush(RGB(45, 45, 48));
        return (LRESULT)hBrush;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(230, 230, 230));
        SetBkColor(hdcEdit, RGB(60, 60, 65));
        static HBRUSH hBrushEdit = CreateSolidBrush(RGB(60, 60, 65));
        return (LRESULT)hBrushEdit;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: OpenFileDialog(hwnd); break;
        case 2: OnInject(hwnd); break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpszClassName = L"PulseInjector";
    wc.hInstance = hInstance;
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"PulseInjector", L"PulseInjector",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 530, 250, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
