#pragma once

#include <windows.h>  

class Game;  

class DisplayWin32 {
private:
    Game* game;
    LONG ClientHeight, ClientWidth;
    WNDCLASSEX Wc;

public:
    HWND Window;

public:
    DisplayWin32(Game* game, LONG clientWidth, LONG clientHeight, HINSTANCE instance, LPCWSTR applicationName);
    void createMessageBox(LPCWSTR text, LPCWSTR caption, UINT type);
    void setWindowText(LPCWSTR text);

private:
    LRESULT HandleMessage(UINT umessage, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);
};