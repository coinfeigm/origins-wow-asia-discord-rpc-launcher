#include "framework.h"
#include "OriginsWoWLauncher.h"
#include "DiscordIntegration.h"
#include "discord_rpc.h"
#include <commdlg.h>
#include <CommCtrl.h>
#include <shlwapi.h>
#include <string>
#include <fstream>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

wchar_t wow_path[MAX_PATH] = L"Wow.exe";
wchar_t wow_dir[MAX_PATH] = L"";

HWND hMainWnd = nullptr;
HWND hOptionsWnd = nullptr;
HWND hLaunchBtn = nullptr;
HWND hSaveBtnInOptions = nullptr;

#define ID_BUTTON_LAUNCH     101
#define ID_BUTTON_OPTIONS    102
#define ID_BUTTON_RESET      103
#define ID_BUTTON_LOCATE_WOW 104

#define ID_RESOLUTION        201
#define ID_WINDOW            202
#define ID_MUSIC             204
#define ID_SOUND             205
#define ID_MASTER            206
#define ID_SAVE              207

#define MAX_LOADSTRING 100

#include <windows.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

Gdiplus::Image* g_bgImage = nullptr;

bool discord_initialized = false;

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void ShowOptionsWindow(HWND hParent);
void UpdateWoWDirectoryAndUI();
void UpdateLaunchButtonState();
void EnsureDefaultConfigWTF();
void SaveWoWPath(const std::wstring& path);
bool LoadWoWPath(std::wstring& out_path);
void LoadConfigIntoControls(HWND hOptsWnd);
void WriteConfigFromControls(HWND hOptsWnd);
std::wstring GetConfigPath();
bool DeleteDirectoryRecursively(const wchar_t* path);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    g_bgImage = Gdiplus::Image::FromFile(L"YOUR_BG_PATH");

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ORIGINSWOWLAUNCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    std::wstring saved_path;
    if (LoadWoWPath(saved_path))
    {
        if (GetFileAttributesW(saved_path.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            wcscpy_s(wow_path, MAX_PATH, saved_path.c_str());
        }
    }

    if (!InitInstance(hInstance, nCmdShow)) return FALSE;

    HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_ORIGINSWOWLAUNCHER));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (discord_initialized)
            Discord_RunCallbacks();
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_ORIGINSWOWLAUNCHER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIconW(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void UpdateWoWDirectoryAndUI()
{
    wcscpy_s(wow_dir, MAX_PATH, wow_path);
    PathRemoveFileSpecW(wow_dir);

    BOOL wowExists = (GetFileAttributesW(wow_path) != INVALID_FILE_ATTRIBUTES);

    UpdateLaunchButtonState();

    if (hSaveBtnInOptions && IsWindow(hSaveBtnInOptions))
        EnableWindow(hSaveBtnInOptions, wowExists);

    HWND hOptionsBtn = GetDlgItem(hMainWnd, ID_BUTTON_OPTIONS);
    HWND hResetBtn = GetDlgItem(hMainWnd, ID_BUTTON_RESET);

    if (hOptionsBtn) EnableWindow(hOptionsBtn, wowExists);
    if (hResetBtn)   EnableWindow(hResetBtn, wowExists);
}

void UpdateLaunchButtonState()
{
    BOOL valid = (GetFileAttributesW(wow_path) != INVALID_FILE_ATTRIBUTES);
    EnableWindow(hLaunchBtn, valid);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    wcscpy_s(szTitle, L"Origins WoW Asia");

    hMainWnd = CreateWindowW(szWindowClass, szTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hMainWnd) return FALSE;

    HICON hIconLarge = (HICON)LoadImageW(
        nullptr,
        L"YOUR_ICON_PATH",
        IMAGE_ICON,
        32, 32,
        LR_LOADFROMFILE
    );

    HICON hIconSmall = (HICON)LoadImageW(
        nullptr,
        L"YOUR_ICON_PATH",
        IMAGE_ICON,
        16, 16,
        LR_LOADFROMFILE
    );

    SendMessageW(hMainWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
    SendMessageW(hMainWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, GetWindowLongW(hMainWnd, GWL_STYLE), FALSE);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    SetWindowPos(hMainWnd, nullptr, sx, sy, w, h, SWP_NOZORDER);

    const int btn_width = 130;
    const int btn_height = 50;
    const int locate_btn_w = 130;
    const int locate_btn_h = 35;
    const int spacing = 30;

    int total_main_width = 3 * btn_width + 2 * spacing;
    int start_x = (800 - total_main_width) / 2;

    int y_main = 480;
    int y_locate = y_main - locate_btn_h - 15;

    CreateWindowW(L"BUTTON", L"Locate WoW",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        start_x, y_locate, locate_btn_w, locate_btn_h,
        hMainWnd, (HMENU)ID_BUTTON_LOCATE_WOW, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Options",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        start_x, y_main, btn_width, btn_height,
        hMainWnd, (HMENU)ID_BUTTON_OPTIONS, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Reset",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        start_x + btn_width + spacing, y_main, btn_width, btn_height,
        hMainWnd, (HMENU)ID_BUTTON_RESET, hInstance, nullptr);

    hLaunchBtn = CreateWindowW(L"BUTTON", L"Launch",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        start_x + 2 * (btn_width + spacing), y_main, btn_width, btn_height,
        hMainWnd, (HMENU)ID_BUTTON_LAUNCH, hInstance, nullptr);

    HFONT hBtnFont = CreateFontW(19, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HWND buttons[] = {
        GetDlgItem(hMainWnd, ID_BUTTON_OPTIONS),
        GetDlgItem(hMainWnd, ID_BUTTON_RESET),
        GetDlgItem(hMainWnd, ID_BUTTON_LAUNCH),
        GetDlgItem(hMainWnd, ID_BUTTON_LOCATE_WOW)
    };

    for (auto btn : buttons)
        SendMessageW(btn, WM_SETFONT, (WPARAM)hBtnFont, TRUE);

    UpdateWoWDirectoryAndUI();

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    return TRUE;
}

bool DeleteDirectoryRecursively(const wchar_t* dirPath)
{
    if (GetFileAttributesW(dirPath) == INVALID_FILE_ATTRIBUTES)
        return true;

    wchar_t searchPath[MAX_PATH];
    wsprintfW(searchPath, L"%s\\*.*", dirPath);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;

            wchar_t fullPath[MAX_PATH];
            wsprintfW(fullPath, L"%s\\%s", dirPath, fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                DeleteDirectoryRecursively(fullPath);
            else
                DeleteFileW(fullPath);
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    return RemoveDirectoryW(dirPath);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        switch (id)
        {
        case ID_BUTTON_LAUNCH:
        {
            InitDiscord();
            UpdateDiscordPresence();

            wchar_t realmlistPath[MAX_PATH];
            wsprintfW(realmlistPath, L"%s\\realmlist.wtf", wow_dir);

            std::wofstream realmFile(realmlistPath, std::ios::trunc);
            if (realmFile.is_open())
            {
                realmFile << L"set realmlist play.originswow.asia\n";
                realmFile.close();
            }
            else
            {
                MessageBoxW(hWnd, L"Failed to write realmlist.wtf!", L"Error", MB_OK | MB_ICONERROR);
            }

            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            if (GetFileAttributesW(wow_path) == INVALID_FILE_ATTRIBUTES)
                return 0;

            EnsureDefaultConfigWTF();

            if (CreateProcessW(wow_path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, wow_dir, &si, &pi))
            {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            else
            {
                MessageBoxW(hWnd, L"Failed to launch Wow.exe.\nCheck the path and try again.", L"Launch Error", MB_OK | MB_ICONERROR);
            }
        } break;

        case ID_BUTTON_OPTIONS:
            ShowOptionsWindow(hWnd);
            break;

        case ID_BUTTON_RESET:
        {
            const wchar_t* folders[] = { L"Errors", L"Interface", L"Logs", L"WDB", L"WTF" };

            int result = MessageBoxW(hWnd,
                L"This will completely reset your World of Warcraft settings and addons.\n\n"
                L"The following folders will be deleted:\n"
                L"• Errors\n• Interface\n• Logs\n• WDB\n• WTF\n\n"
                L"Your Screenshots folder will NOT be deleted.\n\n"
                L"Are you sure you want to continue?",
                L"Confirm Reset", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

            if (result != IDYES)
                break;

            bool allSuccess = true;
            for (const wchar_t* folder : folders)
            {
                wchar_t fullPath[MAX_PATH];
                wsprintfW(fullPath, L"%s\\%s", wow_dir, folder);
                if (!DeleteDirectoryRecursively(fullPath))
                {
                    allSuccess = false;
                }
            }

            if (allSuccess)
            {
                MessageBoxW(hWnd, L"Reset completed successfully!\nAll settings and addons have been cleared.", L"Reset Complete", MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW(hWnd, L"Reset partially completed.\nSome folders could not be deleted (may be in use).", L"Reset Warning", MB_OK | MB_ICONWARNING);
            }

            if (hOptionsWnd)
                LoadConfigIntoControls(hOptionsWnd);
        } break;

        case ID_BUTTON_LOCATE_WOW:
        {
            OPENFILENAMEW ofn = { sizeof(ofn) };
            wchar_t fileName[MAX_PATH] = L"";
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"World of Warcraft\0Wow.exe\0All Files\0*.*\0";
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = L"exe";

            if (GetOpenFileNameW(&ofn))
            {
                wcscpy_s(wow_path, MAX_PATH, fileName);
                SaveWoWPath(wow_path);
                UpdateWoWDirectoryAndUI();
                if (hOptionsWnd) LoadConfigIntoControls(hOptionsWnd);
                MessageBoxW(hWnd, fileName, L"WoW.exe Located", MB_OK);
            }
        } break;
        }
    } break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (g_bgImage)
        {
            Gdiplus::Graphics graphics(hdc);
            graphics.DrawImage(g_bgImage, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom);
        }
        else
        {
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1)); // fallback color
        }

        EndPaint(hWnd, &ps);
    } break;


    case WM_DESTROY:
        if (discord_initialized)
        {
            ShutdownDiscord();
            discord_initialized = false;
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

std::wstring GetConfigPath()
{
    if (wcslen(wow_dir) == 0) return L"";
    return std::wstring(wow_dir) + L"\\WTF\\Config.wtf";
}

void ShowOptionsWindow(HWND hParent)
{
    EnsureDefaultConfigWTF();

    if (hOptionsWnd && IsWindow(hOptionsWnd))
    {
        SetForegroundWindow(hOptionsWnd);
        return;
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = OptionsWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"OriginsOptionsClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    hOptionsWnd = CreateWindowW(L"OriginsOptionsClass", L"Options",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        hParent, nullptr, hInst, nullptr);

    RECT rc;
    GetWindowRect(hOptionsWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    SetWindowPos(hOptionsWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hOptionsWnd, SW_SHOW);
    UpdateWindow(hOptionsWnd);
}

LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hResCombo, hWindowCombo, hMusicSlider, hSoundSlider, hMasterSlider;

    switch (message)
    {
    case WM_CREATE:
    {
        HFONT hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        const int left_margin = 20;
        const int label_width = 120;
        const int control_x = left_margin + label_width + 10;
        const int control_width = 150;
        const int row_height = 38;
        int y = 15;

        CreateWindowW(L"STATIC", L"Resolution:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
            left_margin, y + 5, label_width, 20, hWnd, nullptr, hInst, nullptr);
        hResCombo = CreateWindowW(L"COMBOBOX", nullptr,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            control_x, y, control_width, 150, hWnd, (HMENU)ID_RESOLUTION, hInst, nullptr);

        SendMessageW(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"800x600");
        SendMessageW(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1024x768");
        SendMessageW(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1280x720");
        SendMessageW(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1920x1080");
        SendMessageW(hResCombo, CB_SETCURSEL, 1, 0);

        y += row_height;

        CreateWindowW(L"STATIC", L"Window Mode:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
            left_margin, y + 5, label_width, 20, hWnd, nullptr, hInst, nullptr);
        hWindowCombo = CreateWindowW(L"COMBOBOX", nullptr,
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            control_x, y, control_width, 100, hWnd, (HMENU)ID_WINDOW, hInst, nullptr);

        SendMessageW(hWindowCombo, CB_ADDSTRING, 0, (LPARAM)L"Fullscreen");
        SendMessageW(hWindowCombo, CB_ADDSTRING, 0, (LPARAM)L"Windowed");
        SendMessageW(hWindowCombo, CB_SETCURSEL, 1, 0);

        y += row_height + 5;

        CreateWindowW(L"STATIC", L"Music Volume:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
            left_margin, y + 8, label_width, 20, hWnd, nullptr, hInst, nullptr);
        hMusicSlider = CreateWindowW(TRACKBAR_CLASS, nullptr,
            WS_VISIBLE | WS_CHILD | TBS_HORZ,
            control_x, y, control_width, 30, hWnd, (HMENU)ID_MUSIC, hInst, nullptr);
        SendMessageW(hMusicSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(hMusicSlider, TBM_SETPOS, TRUE, 70);

        y += row_height;

        CreateWindowW(L"STATIC", L"Sound Volume:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
            left_margin, y + 8, label_width, 20, hWnd, nullptr, hInst, nullptr);
        hSoundSlider = CreateWindowW(TRACKBAR_CLASS, nullptr,
            WS_VISIBLE | WS_CHILD | TBS_HORZ,
            control_x, y, control_width, 30, hWnd, (HMENU)ID_SOUND, hInst, nullptr);
        SendMessageW(hSoundSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(hSoundSlider, TBM_SETPOS, TRUE, 70);

        y += row_height;

        CreateWindowW(L"STATIC", L"Master Volume:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
            left_margin, y + 8, label_width, 20, hWnd, nullptr, hInst, nullptr);
        hMasterSlider = CreateWindowW(TRACKBAR_CLASS, nullptr,
            WS_VISIBLE | WS_CHILD | TBS_HORZ,
            control_x, y, control_width, 30, hWnd, (HMENU)ID_MASTER, hInst, nullptr);
        SendMessageW(hMasterSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(hMasterSlider, TBM_SETPOS, TRUE, 80);

        auto applyFont = [&](HWND ctrl) { SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFont, TRUE); };
        applyFont(hResCombo);
        applyFont(hWindowCombo);

        hSaveBtnInOptions = CreateWindowW(L"BUTTON", L"Save",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            140, 220, 120, 30, hWnd, (HMENU)ID_SAVE, hInst, nullptr);
        SendMessageW(hSaveBtnInOptions, WM_SETFONT, (WPARAM)hFont, TRUE);

        EnableWindow(hSaveBtnInOptions, GetFileAttributesW(wow_path) != INVALID_FILE_ATTRIBUTES);

        LoadConfigIntoControls(hWnd);
    } break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_SAVE)
        {
            WriteConfigFromControls(hWnd);
            DestroyWindow(hWnd);

            // Bring main window to front
            if (IsWindow(hMainWnd))
            {
                SetForegroundWindow(hMainWnd);
                SetActiveWindow(hMainWnd);
                SetFocus(hMainWnd);
            }
        }
        break;

    case WM_CLOSE:
        hOptionsWnd = nullptr;
        hSaveBtnInOptions = nullptr;
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        hOptionsWnd = nullptr;
        hSaveBtnInOptions = nullptr;
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void EnsureDefaultConfigWTF()
{
    std::wstring configPath = GetConfigPath();
    if (configPath.empty())
        return;

    if (GetFileAttributesW(configPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        return;

    std::wstring wtfDir = std::wstring(wow_dir) + L"\\WTF";
    CreateDirectoryW(wtfDir.c_str(), nullptr);

    std::wofstream file(configPath);
    if (!file.is_open())
        return;

    file << L"SET gxResolution \"1024x768\"\n";
    file << L"SET gxWindow \"1\"\n";
    file << L"SET MusicVolume \"70\"\n";
    file << L"SET SoundVolume \"70\"\n";
    file << L"SET MasterVolume \"80\"\n";
    file << L"SET movie \"0\"\n";
    file << L"SET readTOS \"1\"\n";
    file << L"SET readEULA \"1\"\n";
    file.close();
}

std::wstring GetAppDirectory()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    std::wstring exe_path(path);
    size_t pos = exe_path.find_last_of(L"\\/");
    return exe_path.substr(0, pos + 1);
}

void SaveWoWPath(const std::wstring& path)
{
    std::wstring file_path = GetAppDirectory() + L"wow_path.txt";

    std::wofstream file(file_path);
    if (file.is_open())
        file << path;
}

bool LoadWoWPath(std::wstring& out_path)
{
    std::wstring file_path = GetAppDirectory() + L"wow_path.txt";

    std::wifstream file(file_path);
    if (!file.is_open())
        return false;

    std::getline(file, out_path);
    return !out_path.empty();
}

void LoadConfigIntoControls(HWND hOptsWnd)
{
    HWND hRes = GetDlgItem(hOptsWnd, ID_RESOLUTION);
    HWND hWin = GetDlgItem(hOptsWnd, ID_WINDOW);
    HWND hMus = GetDlgItem(hOptsWnd, ID_MUSIC);
    HWND hSnd = GetDlgItem(hOptsWnd, ID_SOUND);
    HWND hMst = GetDlgItem(hOptsWnd, ID_MASTER);

    SendMessageW(hRes, CB_SETCURSEL, 1, 0);
    SendMessageW(hWin, CB_SETCURSEL, 1, 0);
    SendMessageW(hMus, TBM_SETPOS, TRUE, 70);
    SendMessageW(hSnd, TBM_SETPOS, TRUE, 70);
    SendMessageW(hMst, TBM_SETPOS, TRUE, 80);

    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) return;

    std::wifstream file(configPath);
    if (!file.is_open()) return;

    std::wstring line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == L';') continue;
        size_t pos = line.find(L"SET ");
        if (pos == std::wstring::npos) continue;
        line = line.substr(pos + 4);

        size_t q1 = line.find(L'"');
        if (q1 == std::wstring::npos) continue;
        size_t q2 = line.find(L'"', q1 + 1);
        if (q2 == std::wstring::npos) continue;

        std::wstring key = line.substr(0, q1);
        key.erase(std::remove_if(key.begin(), key.end(), ::iswspace), key.end());
        std::wstring value = line.substr(q1 + 1, q2 - q1 - 1);

        if (key == L"gxResolution")
        {
            for (int i = 0; i < 4; ++i)
            {
                wchar_t buf[32];
                SendMessageW(hRes, CB_GETLBTEXT, i, (LPARAM)buf);
                if (wcscmp(buf, value.c_str()) == 0)
                    SendMessageW(hRes, CB_SETCURSEL, i, 0);
            }
        }
        else if (key == L"gxWindow")
            SendMessageW(hWin, CB_SETCURSEL, (value == L"0" ? 0 : 1), 0);
        else if (key == L"MusicVolume")   SendMessageW(hMus, TBM_SETPOS, TRUE, _wtoi(value.c_str()));
        else if (key == L"SoundVolume")   SendMessageW(hSnd, TBM_SETPOS, TRUE, _wtoi(value.c_str()));
        else if (key == L"MasterVolume")  SendMessageW(hMst, TBM_SETPOS, TRUE, _wtoi(value.c_str()));
    }
}

void WriteConfigFromControls(HWND hOptsWnd)
{
    if (GetFileAttributesW(wow_path) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(hOptsWnd, L"WoW.exe path not located!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    HWND hRes = GetDlgItem(hOptsWnd, ID_RESOLUTION);
    HWND hWin = GetDlgItem(hOptsWnd, ID_WINDOW);
    HWND hMus = GetDlgItem(hOptsWnd, ID_MUSIC);
    HWND hSnd = GetDlgItem(hOptsWnd, ID_SOUND);
    HWND hMst = GetDlgItem(hOptsWnd, ID_MASTER);

    wchar_t res[32] = {};
    int idx = (int)SendMessageW(hRes, CB_GETCURSEL, 0, 0);
    if (idx != CB_ERR)
        SendMessageW(hRes, CB_GETLBTEXT, idx, (LPARAM)res);

    int windowed = (int)SendMessageW(hWin, CB_GETCURSEL, 0, 0);
    int music = (int)SendMessageW(hMus, TBM_GETPOS, 0, 0);
    int sound = (int)SendMessageW(hSnd, TBM_GETPOS, 0, 0);
    int master = (int)SendMessageW(hMst, TBM_GETPOS, 0, 0);

    std::wstring wtfDir = std::wstring(wow_dir) + L"\\WTF";
    CreateDirectoryW(wtfDir.c_str(), nullptr);

    std::wstring configPath = GetConfigPath();
    std::vector<std::wstring> lines;

    std::wifstream in(configPath);
    if (in.is_open())
    {
        std::wstring l;
        while (std::getline(in, l)) lines.push_back(l);
        in.close();
    }

    auto updateLine = [&lines](const wchar_t* key, const std::wstring& val)
        {
            std::wstring target = L"SET " + std::wstring(key);
            bool found = false;
            for (auto& line : lines)
            {
                if (line.find(target) != std::wstring::npos)
                {
                    line = L"SET " + std::wstring(key) + L" \"" + val + L"\"";
                    found = true;
                    break;
                }
            }
            if (!found)
                lines.push_back(L"SET " + std::wstring(key) + L" \"" + val + L"\"");
        };

    updateLine(L"gxResolution", res);
    updateLine(L"gxWindow", windowed == 0 ? L"0" : L"1");
    updateLine(L"MusicVolume", std::to_wstring(music));
    updateLine(L"SoundVolume", std::to_wstring(sound));
    updateLine(L"MasterVolume", std::to_wstring(master));
    updateLine(L"movie", L"0");
    updateLine(L"readTOS", L"1");
    updateLine(L"readEULA", L"1");

    std::wofstream out(configPath);
    if (out.is_open())
    {
        for (const auto& l : lines) out << l << L"\n";
        MessageBoxW(hOptsWnd, L"Settings saved successfully!", L"Success", MB_OK);
    }
    else
    {
        MessageBoxW(hOptsWnd, L"Failed to save settings.", L"Error", MB_OK | MB_ICONERROR);
    }
}