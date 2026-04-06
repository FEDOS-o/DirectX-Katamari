#include "DisplayWin32.h"
#include "Game.h"  // Только здесь включаем Game.h!
#include <iostream>

DisplayWin32::DisplayWin32(Game* game, LONG clientWidth, LONG clientHeight, HINSTANCE instance, LPCWSTR applicationName) :
    game(game),
    ClientHeight(clientHeight),
    ClientWidth(clientWidth)
{
    Wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    Wc.lpfnWndProc = StaticWndProc;
    Wc.cbClsExtra = 0;
    Wc.cbWndExtra = 0;
    Wc.hInstance = instance;
    Wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    Wc.hIconSm = Wc.hIcon;
    Wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    Wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    Wc.lpszMenuName = nullptr;
    Wc.lpszClassName = applicationName;
    Wc.cbSize = sizeof(WNDCLASSEX);

    RegisterClassEx(&Wc);

    RECT windowRect = { 0, 0, static_cast<LONG>(ClientWidth), static_cast<LONG>(ClientHeight) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    auto dwStyle = WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME;

    auto posX = (GetSystemMetrics(SM_CXSCREEN) - ClientWidth) / 2;
    auto posY = (GetSystemMetrics(SM_CYSCREEN) - ClientHeight) / 2;

    Window = CreateWindowEx(
        WS_EX_APPWINDOW,
        applicationName,
        applicationName,
        dwStyle,
        posX, posY,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        this);  // Передаем указатель на DisplayWin32

    ShowWindow(Window, SW_SHOW);
    SetForegroundWindow(Window);
    SetFocus(Window);

    ShowCursor(true);
}

void DisplayWin32::createMessageBox(LPCWSTR text, LPCWSTR caption, UINT type) {
    MessageBox(Window, text, caption, type);
}

void DisplayWin32::setWindowText(LPCWSTR text) {
    SetWindowText(Window, text);
}

LRESULT DisplayWin32::HandleMessage(UINT umessage, WPARAM wparam, LPARAM lparam) {
    switch (umessage)
    {
    case WM_INPUT:
    {
        UINT dwSize = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        LPBYTE lpb = new BYTE[dwSize];
        if (lpb == nullptr) {
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
            OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb);

        if (raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            if (game && game->Input) {  // Теперь Game полностью определен!
                game->Input->OnKeyDown({
                    raw->data.keyboard.MakeCode,
                    raw->data.keyboard.Flags,
                    raw->data.keyboard.VKey,
                    raw->data.keyboard.Message
                    });
            }
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE)
        {
            if (game && game->Input) {
                game->Input->OnMouseMove({
                    raw->data.mouse.usFlags,
                    raw->data.mouse.usButtonFlags,
                    static_cast<int>(raw->data.mouse.ulExtraInformation),
                    static_cast<int>(raw->data.mouse.ulRawButtons),
                    static_cast<short>(raw->data.mouse.usButtonData),
                    raw->data.mouse.lLastX,
                    raw->data.mouse.lLastY
                    });
            }
        }

        delete[] lpb;
        return 0;
    }
    }
    return DefWindowProc(Window, umessage, wparam, lparam);
}

LRESULT CALLBACK DisplayWin32::StaticWndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    DisplayWin32* display = nullptr;

    if (umessage == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        display = static_cast<DisplayWin32*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(display));
    }
    else {
        display = reinterpret_cast<DisplayWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (display) {
        return display->HandleMessage(umessage, wparam, lparam);
    }

    return DefWindowProc(hwnd, umessage, wparam, lparam);
}