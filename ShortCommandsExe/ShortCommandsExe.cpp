#pragma region Includes
#pragma warning ( push, 0 )

#include <Windows.h>

#include <codecvt>
#include <iostream>

#include <nlohmann/json.hpp>

#include <fstream>
#pragma comment( lib, "Winmm.lib" )

#include <locale>
#include <psapi.h>
#include <regex>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
#include <SDL_ttf.h>

#include <shlobj.h>
#include <sstream>
#include <tchar.h>

using namespace std;
using namespace nlohmann;

#pragma warning ( pop )
#pragma endregion

#pragma region Vars

#define MUTEX_NAME TEXT("ShortCommandsEXE_Running")
#define CLEAR_TEXT_EVENT_NAME TEXT("ShortCommandsEXE_ClearText")
#define QUIT_EVENT_NAME TEXT("ShortCommandsEXE_Quit")

#pragma endregion

#pragma region Definitions

void ConfigureWindow(SDL_Window* window);

bool InitSDL();

void ShutdownSDL();

bool CreateSDLWindow();

void PreRender();

void PostRender();

void DestroySDLWindow();

void AllowWindowDestroy();

void BlockWindowDestroy();

void LoseWindowFocus();

void Initialize();

void Shutdown();

void LoadFont();

void ClearText();

void AsyncClearText(UINT timerId, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2);

void TimedText(const char* text, UINT timeout); void Type(const char* text);

void IgnoreHook(bool ignore);

void Type(const char* text);

void Type(WORD key);

INPUT* CreateInput(WORD vKey);

void LoadState();

void SaveState();

#pragma endregion

#pragma region SDL MAIN

//Credit: https://stackoverflow.com/questions/23048993/sdl-fullscreen-translucent-background
void ConfigureWindow(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_GetVersion(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hWnd = wmInfo.info.win.window;
    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    //UpdateLayeredWindow(hWnd, NULL, NULL, NULL, NULL, NULL, NULL, new BLENDFUNCTION{AC_SRC_OVER, NULL, 255, AC_SRC_ALPHA}, ULW_ALPHA);
    //SetLayeredWindowAttributes(hWnd, NULL, 180, LWA_ALPHA);
    SetLayeredWindowAttributes(hWnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    SetWindowPos(hWnd, (HWND)-2, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void error(const char* error) {
    cout << error << " (" << SDL_GetError() << ")" << endl;
}

bool InitSDL() {
    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        error("Error Initializing SDL!");
        return false;
    }
    if (TTF_Init() == -1) {
        error("Error Initializing TTF!");
        return false;
    }
    return true;
}

void ShutdownSDL() {
    TTF_Quit();
    SDL_Quit();
}

#pragma endregion

#pragma region SDL WINDOW

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
bool blockWindowDestroy = false;

bool CreateSDLWindow() {
    window = SDL_CreateWindow("ShortCommands Overlay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_OPENGL);
    ConfigureWindow(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        error("Error Creating Renderer!");
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    PreRender();
    PostRender();
    return true;
}

void PreRender() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);
}

void PostRender() {
    SDL_RenderPresent(renderer);
}

void DestroySDLWindow() {
    if (blockWindowDestroy) {
        return;
    }
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;
}

void AllowWindowDestroy() {
    blockWindowDestroy = false;
}

void BlockWindowDestroy() {
    blockWindowDestroy = true;
}

void LoseWindowFocusEventProc(HWINEVENTHOOK hook, DWORD evnt, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime) {
    UnhookWinEvent(hook);
    DestroySDLWindow();
    IgnoreHook(false);
}

void LoseWindowFocus() {
    CreateSDLWindow();
    IgnoreHook(true);
    SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, NULL, (WINEVENTPROC)LoseWindowFocusEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
}

TTF_Font* font;

void LoadFont() {
    font = TTF_OpenFont("Minecraft.ttf", 30);
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
}

UINT timerId;
bool hasTimer = false;

void RenderText(const char* text);

void ClearText() {
    AllowWindowDestroy();
    DestroySDLWindow();
}

void AsyncClearText(UINT timerId, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2) {
    HANDLE clearTextEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, CLEAR_TEXT_EVENT_NAME);
    if (clearTextEvent == NULL || clearTextEvent == INVALID_HANDLE_VALUE) {
        return;
    }
    SetEvent(clearTextEvent);
    CloseHandle(clearTextEvent);
}

void TimedText(const char* text, UINT timeout) {
    RenderText(text);
    hasTimer = true;
    timerId = timeSetEvent(timeout, 100, (LPTIMECALLBACK)AsyncClearText, NULL, TIME_ONESHOT | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
}

#pragma endregion

#pragma region Hook

bool shouldCancelEvent = false;
bool ignoreHook = false;
bool lshift = false;
bool rshift = false;
bool lctrl = false;
bool rctrl = false;
#define ToKeyState(state) ((state) ? 0x8000 : 0x00)

SHORT GetAsyncKeyStateUpdated(int vKey) {
    switch (vKey) {
    case VK_SHIFT:
        return ToKeyState(lshift || rshift);
    case VK_CONTROL:
        return ToKeyState(lctrl || rctrl);
    case VK_LSHIFT:
        return ToKeyState(lshift);
    case VK_RSHIFT:
        return ToKeyState(rshift);
    case VK_LCONTROL:
        return ToKeyState(lctrl);
    case VK_RCONTROL:
        return ToKeyState(rctrl);
    default:
        return GetAsyncKeyState(vKey);
    }
}

// Credit: https://www.gamedev.net/forums/topic/43463-how-to-use-getkeyb/
BOOL ReadKeyboard(BYTE* keys) { for (int x = 0; x < 256; x++) { keys[x] = (char)(GetAsyncKeyStateUpdated(x) >> 8); } return TRUE; }

void KeyboardHook(DWORD keyCode, char keyChar);

void UpdateKeyboard(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)lParam;
        switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if (key->vkCode == VK_LSHIFT) {
                lshift = true;
            }
            else if (key->vkCode == VK_RSHIFT) {
                rshift = true;
            }
            else if (key->vkCode == VK_LCONTROL) {
                lctrl = true;
            }
            else if (key->vkCode == VK_RCONTROL) {
                rctrl = true;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            if (key->vkCode == VK_LSHIFT) {
                lshift = false;
            }
            else if (key->vkCode == VK_RSHIFT) {
                rshift = false;
            }
            else if (key->vkCode == VK_LCONTROL) {
                lctrl = false;
            }
            else if (key->vkCode == VK_RCONTROL) {
                rctrl = false;
            }
            break;
        }
        default:
            break;
        }
    }
}

LRESULT LowLevelHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    UpdateKeyboard(nCode, wParam, lParam);
    if (!ignoreHook && nCode == HC_ACTION) {
        switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)lParam;
            BYTE keyboardState[256] = { 0 };
            GetKeyState(0);
            if (!ReadKeyboard(keyboardState)) {
                cout << GetLastError() << endl;
            }
            WORD asciiText[2];
            int asciiResult = ToAscii(key->vkCode, key->scanCode, keyboardState, asciiText, 0);
            asciiText[0] &= 0xFF;
            if (asciiText[0] < 0x20 || asciiText[0] > 0x7E) {
                asciiResult = 0;
            }
            AllowWindowDestroy();
            KeyboardHook(key->vkCode, asciiResult == 1 ? (char)asciiText[0] : NULL);
            if (shouldCancelEvent) {
                shouldCancelEvent = false;
                return 1;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            break;
        default:
            break;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void cancelEvent() { shouldCancelEvent = true; }

void IgnoreHook(bool ignore) {
    ignoreHook = ignore;
}

void Type(const char* text) {
    BYTE keyboardState[256] = { 0 };
    GetKeyState(0);
    if (!ReadKeyboard(keyboardState)) {
        cout << GetLastError() << endl;
    }
    vector<INPUT> inputs;
    vector<INPUT> reset;
#define SetAndReset(key) if(keyboardState[key]) { INPUT* kInputs = CreateInput(key); inputs.push_back(kInputs[1]); reset.push_back(kInputs[0]); }
    SetAndReset(VK_LSHIFT);
    SetAndReset(VK_RSHIFT);
    SetAndReset(VK_LCONTROL);
    SetAndReset(VK_RCONTROL);
#undef SetAndReset
    for (size_t i = 0; i < strlen(text); i++) {
        vector<INPUT> tempReset;
        SHORT translatedKey = VkKeyScanA(text[i]);
        WORD vKey = translatedKey & 0xFF;
        WORD flags = (translatedKey >> 8) & 0xFF;
#define TempShift(flag, vKeyM) if((flags & flag) == flag) { INPUT* tempShift = CreateInput(vKeyM); inputs.push_back(tempShift[0]); tempReset.push_back(tempShift[1]); }
        TempShift(1, VK_LSHIFT);
        TempShift(2, VK_LCONTROL);
#undef TempShift
        INPUT* keyInputs = CreateInput(vKey);
        inputs.push_back(keyInputs[0]);
        inputs.push_back(keyInputs[1]);
        for (size_t i = 0; i < tempReset.size(); i++) {
            inputs.push_back(tempReset[i]);
        }
    }
    for (size_t i = 0; i < reset.size(); i++) {
        inputs.push_back(reset[i]);
    }
    IgnoreHook(true);
    SendInput((UINT)inputs.size(), &inputs[0], sizeof(INPUT));
    IgnoreHook(false);
}

void Type(WORD key) {
    IgnoreHook(true);
    SendInput(2, CreateInput(key), sizeof(INPUT));
    IgnoreHook(false);
}

INPUT* CreateInput(WORD vKey) {
    static INPUT inputs[2] = {};
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vKey;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vKey;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return inputs;
}

#pragma endregion

#pragma region JSON

json state;

void LoadState() {
    ifstream inStream("data.json");
    if (!inStream.good()) {
        state = {
            { "border", 10 },
            { "posX", 10 },
            { "posY", 100 },
            { "trigger", VK_OEM_2 },
            { "triggerFlags", 0 },
            { "TextColor", { 255, 255, 255, 255 } },
            { "BackgroundColor", { 30, 30, 30 } },
            { "copyOpenChar", true },
            { "commands", {} }
        };
        SaveState();
        LoadState();
        return;
    }
    inStream >> state;
}

void SaveState() {
    ofstream outputStream("data.json");
    outputStream << state;
}

#define sbool(name) state[name].get<bool>()
#define sdword(name) state[name].get<DWORD>()
#define sint(name) state[name].get<int>()
#define scolor(color, pos) state[color][pos].get<UINT8>()

#pragma endregion

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    HANDLE runningMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);
    if (runningMutex == NULL || runningMutex == INVALID_HANDLE_VALUE || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (runningMutex != NULL) {
            CloseHandle(runningMutex);
        }
        cerr << "Could not aquire running mutex! Is the program already running?" << endl;
        return 1;
    }

#ifndef _DEBUG
    PWSTR path = NULL;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &path);
    wstringstream ss;
    ss << path << TEXT("\\shortcommands");
    wstring wspath = ss.str();
    const wchar_t* wpath = wspath.c_str();
    SetCurrentDirectoryW(wpath);
    CoTaskMemFree(static_cast<void*>(path));
#endif

    if (!InitSDL()) {
        return 1;
    }

    Initialize();
    LoadFont();

    LoseWindowFocus();

    HANDLE clearTextEvent = CreateEvent(NULL, TRUE, FALSE, CLEAR_TEXT_EVENT_NAME);
    HANDLE quitEvent = CreateEvent(NULL, TRUE, FALSE, QUIT_EVENT_NAME);
    
    lshift = GetAsyncKeyState(VK_LSHIFT);
    rshift = GetAsyncKeyState(VK_RSHIFT);
    lctrl = GetAsyncKeyState(VK_LCONTROL);
    rctrl = GetAsyncKeyState(VK_RCONTROL);
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)LowLevelHookProc, NULL, NULL);

    HANDLE events[2] = { clearTextEvent, quitEvent };

    MSG msg;
    while (true) {
        MsgWaitForMultipleObjects(2, events, FALSE, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE);
        if (WaitForSingleObject(clearTextEvent, 0) == WAIT_OBJECT_0) {
            hasTimer = false;
            ResetEvent(clearTextEvent);
            ClearText();
        }
        if (WaitForSingleObject(quitEvent, 0) == WAIT_OBJECT_0) {
            break;
        }
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    CloseHandle(quitEvent);
    quitEvent = nullptr;
    CloseHandle(clearTextEvent);
    clearTextEvent = nullptr;

    UnhookWindowsHookEx(hook);
    hook = nullptr;

    Shutdown();

    if (window != nullptr) {
        DestroySDLWindow();
    }

    ShutdownSDL();

    CloseHandle(runningMutex);
    runningMutex = nullptr;
    return 0;
}

#pragma region Logic

int timeout = 2000;

enum CurrentState {
    DEFAULT, CONFIRM_CLEAR, CONFIRM_IMPORT, CONFIRM_RESET, COPY_OPEN_CHAR, SET_OPEN_CHAR, QUIT
};

CurrentState currentState = CurrentState::DEFAULT;

void RenderText(const char* text) {
    BlockWindowDestroy();
    if (hasTimer) {
        timeKillEvent(timerId);
        hasTimer = false;
    }
    if (window == nullptr) {
        CreateSDLWindow();
    }
    if (strlen(text) == 0) {
        text = " ";
    }
    PreRender();
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, { scolor("TextColor", 0), scolor("TextColor", 1), scolor("TextColor", 2), scolor("TextColor", 3) });
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    int w, h;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);
    SDL_SetRenderDrawColor(renderer, scolor("BackgroundColor", 0), scolor("BackgroundColor", 1), scolor("BackgroundColor", 2), 255);
    SDL_Rect bgRect{ sint("posX"), sint("posY"), w + 2 * sint("border"), h + 2 * sint("border") };
    SDL_RenderFillRect(renderer, &bgRect);
    SDL_Rect textRect{ sint("posX") + sint("border"), sint("posY") + sint("border"), w, h };
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    PostRender();
}

void Initialize() {
    LoadState();
}

#pragma region MACROS
#define LSHIFT_CONST 1
#define RSHIFT_CONST 2
#define LCONTROL_CONST 4
#define RCONTROL_CONST 8
#pragma endregion

int GetState() {
    BYTE keyboardState[256] = { 0 };
    GetKeyState(VK_SHIFT);
    if (!ReadKeyboard(keyboardState)) {
        cout << GetLastError() << endl;
    }
    int flags = 0;
#define UpdateFlag2(vKey, kConst) if(keyboardState[vKey]) { flags |= kConst; }
#define UpdateFlag1(name) UpdateFlag2(VK_ ## name, name ## _CONST)
    UpdateFlag1(LSHIFT);
    UpdateFlag1(RSHIFT);
    UpdateFlag1(LCONTROL);
    UpdateFlag1(RCONTROL);
#undef UpdateFlag1
#undef UpdateFlag2
    return flags;
}

string currentCommand;

#ifdef _DEBUG

bool IsLunarRunning() { return true; }

#else

// Credit: https://docs.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
bool IsLunarRunning() {
    DWORD aProcesses[1024], cbNeeded;
    if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
        return false;
    }
    DWORD cProcesses = cbNeeded / sizeof(DWORD);
    for (unsigned int i = 0; i < cProcesses; i++) {
        DWORD pid = aProcesses[i];
        if (pid != NULL) {
            char processPath[MAX_PATH] = "<unknown>";
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess != NULL) {
                GetModuleFileNameExA(hProcess, NULL, processPath, MAX_PATH);
            }
            string path(processPath);
            bool found = path.find(".lunarclient/jre") != string::npos || path.find(".lunarclient\\jre") != string::npos;
            CloseHandle(hProcess);
            if (found) {
                return true;
            }
        }
    }

    return false;
}

#endif

// Credit: https://stackoverflow.com/questions/313970/how-to-convert-an-instance-of-stdstring-to-lower-case
string lower(string str) {
    string data(str);
    transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return tolower(c); });
    return data;
}

char GetOpenChar() {
    DWORD vk = sdword("trigger");
    int flags = sint("triggerFlags");
    BYTE keyboardState[256] = { 0 };
    keyboardState[vk] = 0x80;
#define SET_MODIF(modif) if ((flags & modif ## _CONST) == modif ## _CONST) { keyboardState[VK_ ## modif] = 0x80; }
    SET_MODIF(LSHIFT)
    SET_MODIF(RSHIFT)
    SET_MODIF(LCONTROL)
    SET_MODIF(RCONTROL)
#undef SET_MODIF
#define SET_COMB_MODIF(modif) if(((flags & L ## modif ## _CONST) == L ## modif ## _CONST) && ((flags & R ## modif ## _CONST) == R ## modif ## _CONST)) { keyboardState[VK_ ## modif] = 0x80; }
    SET_COMB_MODIF(SHIFT)
    SET_COMB_MODIF(CONTROL)
#undef SET_COMB_MODIF
    WORD asciiText[2];
    int asciiResult = ToAscii(vk, MapVirtualKeyA(vk, MAPVK_VK_TO_VSC), keyboardState, asciiText, 0);
    asciiText[0] &= 0xFF;
    if (asciiResult != 1 || asciiText[0] < 0x20 || asciiText[0] > 0x7E) {
        asciiText[0] = NULL;
    }
    return (char)asciiText[0];
}

#define VK_N 0x4E
#define VK_Y 0x59

void KeyboardHook(DWORD keyCode, char keyChar) {
    if (window != nullptr && !hasTimer) {
        if (currentState == CurrentState::CONFIRM_CLEAR) {
            if (keyCode == VK_N) {
                TimedText("Operation Canceled!", timeout);
                currentState = CurrentState::DEFAULT;
            }
            else if (keyCode == VK_Y) {
                state["commands"] = { };
                SaveState();
                TimedText("All Commands Cleared!", timeout);
                currentState = CurrentState::DEFAULT;
            }
        }
        else if (currentState == CurrentState::CONFIRM_IMPORT) {
            if (keyCode == VK_N) {
                TimedText("Operation Canceled!", timeout);
                currentState = CurrentState::DEFAULT;
            }
            else if (keyCode == VK_Y) {
                PWSTR path = NULL;
                SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &path);
                wstringstream ss;
                ss << path << TEXT("\\.minecraft\\shortcommands.cfg.json");
                CoTaskMemFree(static_cast<void*>(path));
                string pathStr = wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(ss.str());
                ifstream inStream(pathStr.c_str());
                if (inStream.good()) {
                    json importJson;
                    inStream >> importJson;
                    for (auto& el : importJson["shortcuts"].items()) {
                        state["commands"][el.key()] = el.value();
                    }
                    SaveState();
                    TimedText("Import Complete!", timeout);
                }
                else {
                    TimedText("Error: Cannot Find ShortCommands Config!", timeout);
                }
                currentState = CurrentState::DEFAULT;
            }
        }
        else if (currentState == CurrentState::CONFIRM_RESET) {
            if (keyCode == VK_N) {
                TimedText("Operation Canceled!", timeout);
                currentState = CurrentState::DEFAULT;
            }
            else if (keyCode == VK_Y) {
                map<string, string> commands;
                for (auto& el : state["commands"].items()) {
                    commands.emplace(el.key(), el.value());
                }
                remove("data.json");
                LoadState();
                for (auto& el : commands) {
                    state["commands"][el.first] = el.second;
                }
                SaveState();
                TimedText("All Settings Reset!", timeout);
                currentState = CurrentState::DEFAULT;
            }
        }
        else if (currentState == CurrentState::COPY_OPEN_CHAR) {
            if (keyCode == VK_N) {
                state["copyOpenChar"] = false;
                SaveState();
                TimedText("N", timeout);
                currentState = CurrentState::DEFAULT;
            }
            else if (keyCode == VK_Y) {
                state["copyOpenChar"] = true;
                SaveState();
                TimedText("Y", timeout);
                currentState = CurrentState::DEFAULT;
            }
        }
        else if (currentState == CurrentState::SET_OPEN_CHAR) {
            string displayText;
            int kbState = GetState();
#define UpdateDisplayText(modif) if((kbState & modif ## _CONST) == modif ## _CONST) { displayText += #modif; displayText += " + ";  }
            UpdateDisplayText(LSHIFT)
            UpdateDisplayText(RSHIFT)
            UpdateDisplayText(LCONTROL)
            UpdateDisplayText(RCONTROL)
#undef UpdateDisplayText
            if (keyChar == NULL) {
                if (displayText.empty()) {
                    RenderText("Enter New Open Key");
                }
                else {
                    RenderText(displayText.substr(0, displayText.length()-3).c_str());
                }
            }
            else {
                state["trigger"] = keyCode;
                state["triggerFlags"] = kbState;
                SaveState();
                displayText += GetOpenChar();
                TimedText(displayText.c_str(), timeout);
                currentState = CurrentState::DEFAULT;
            }
        }
        else if (currentState == CurrentState::QUIT) {
            HANDLE quitEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, QUIT_EVENT_NAME);
            if (quitEvent != NULL && quitEvent != INVALID_HANDLE_VALUE) {
                SetEvent(quitEvent);
            }
        }
        else if (currentState == CurrentState::DEFAULT) {
            if (keyChar == NULL) {
                if (keyCode == VK_RETURN) {
                    if (lower(currentCommand) == "/set") {
                        TimedText("Usage: /set [pos | bg | fg | border | reset | openchar | copyOpenChar]", timeout);
                    }
                    else if (lower(currentCommand).rfind("/set ", 0) == 0) {
                        string command = lower(currentCommand.substr(5));
                        if (command.rfind("pos", 0) == 0) {
                            smatch match;
                            if (regex_match(command, match, regex("pos (x|y) (set|move) (\\-?[0-9]{1,4})"))) {
                                bool move = match.str(2) == "move";
                                bool error = false;
                                int dist;
                                try {
                                    dist = stoi(match.str(3));
                                }
                                catch (...) {
                                    TimedText("Usage: /set pos [x | y] [set | move] <distance>", timeout);
                                    error = true;
                                }
                                if (!error) {
                                    if (match.str(1) == "x") {
                                        if (move) {
                                            state["posX"] = sint("posX") + dist;
                                        }
                                        else {
                                            state["posX"] = dist;
                                        }
                                    }
                                    else {
                                        if (move) {
                                            state["posY"] = sint("posY") + dist;
                                        }
                                        else {
                                            state["posY"] = dist;
                                        }
                                    }
                                    SaveState();
                                }
                            }
                            else {
                                TimedText("Usage: /set pos [x | y] [set | move] <distance>", timeout);
                            }
                        }
                        else if (command.rfind("bg", 0) == 0) {
                            smatch match;
                            if (regex_match(command, match, regex("bg #?([0-9a-f]{6})"))) {
                                stringstream hexStream;
                                hexStream << hex << match.str(1);
                                unsigned int rgb;
                                hexStream >> rgb;
                                int blue = rgb & 255;
                                int green = (rgb >> 8) & 255;
                                int red = (rgb >> 16) & 255;
                                state["BackgroundColor"][0] = red;
                                state["BackgroundColor"][1] = green;
                                state["BackgroundColor"][2] = blue;
                                SaveState();
                            }
                            else {
                                TimedText("Usage: /set bg <hex rgb color>", timeout);
                            }
                        }
                        else if (command.rfind("fg", 0) == 0) {
                            smatch match;
                            if (regex_match(command, match, regex("fg #?([0-9a-f]{8})"))) {
                                stringstream hexStream;
                                hexStream << hex << match.str(1);
                                unsigned int rgb;
                                hexStream >> rgb;
                                int blue = rgb & 255;
                                int green = (rgb >> 8) & 255;
                                int red = (rgb >> 16) & 255;
                                int alpha = (rgb >> 24) & 255;
                                state["TextColor"][0] = red;
                                state["TextColor"][1] = green;
                                state["TextColor"][2] = blue;
                                state["TextColor"][3] = alpha;
                                SaveState();
                            }
                            else {
                                TimedText("Usage: /set fg <hex rgb color>", timeout);
                            }
                        }
                        else if (command.rfind("border", 0) == 0) {
                            smatch match;
                            if (regex_match(command, match, regex("border ([0-9]{1,4})"))) {
                                bool error = false;
                                int border;
                                try {
                                    border = stoi(match.str(1));
                                }
                                catch (...) {
                                    TimedText("Usage: /set border <width>", timeout);
                                    error = true;
                                }
                                if (!error) {
                                    state["border"] = border;
                                    SaveState();
                                }
                            }
                            else {
                                TimedText("Usage: /set border <width>", timeout);
                            }
                        }
                        else if (command.rfind("reset", 0) == 0) {
                            currentState = CurrentState::CONFIRM_RESET;
                            RenderText("Confirm Reset? [Y/N]");
                        }
                        else if (command.rfind("openchar", 0) == 0) {
                            currentState = CurrentState::SET_OPEN_CHAR;
                            RenderText("Enter New Open Key");
                        }
                        else if (command.rfind("copyOpenChar", 0) == 0) {
                            currentState = CurrentState::COPY_OPEN_CHAR;
                            RenderText("Copy Open Char? [Y/N]");
                        }
                        else {
                            TimedText("Usage: /set [pos | bg | fg | border | reset | openchar | copyOpenChar]", timeout);
                        }
                    }
                    else if (lower(currentCommand) == "/command") {
                        TimedText("Usage: /set [add | remove | clear]", timeout);
                    }
                    else if (lower(currentCommand).rfind("/command ", 0) == 0) {
                        string command = lower(currentCommand.substr(9));
                        if (command.rfind("add", 0) == 0) {
                            smatch match;
                            string subStrCmd = currentCommand.substr(12);
                            if (regex_match(subStrCmd, match, regex(" ([^ ]+) (.+)"))) {
                                string shortcut = lower(match.str(1));
                                string command = match.str(2);
                                auto commands = state["commands"];
                                if (commands.contains(shortcut)) {
                                    commands.erase(shortcut);
                                }
                                commands[shortcut] = command;
                                SaveState();
                            }
                            else {
                                TimedText("Usage: /command add <shortcut> <command>", timeout);
                            }
                        }
                        else if (command.rfind("remove", 0) == 0) {
                            smatch match;
                            if (regex_match(command, match, regex("remove ([^ ]+)"))) {
                                string shortcut = lower(match.str(1));
                                string command = match.str(2);
                                auto commands = state["commands"];
                                if (commands.contains(shortcut)) {
                                    commands.erase(shortcut);
                                }
                                SaveState();
                            }
                            else {
                                TimedText("Usage: /command remove <shortcut>", timeout);
                            }
                        }
                        else if (command.rfind("clear", 0) == 0) {
                            currentState = CurrentState::CONFIRM_CLEAR;
                            RenderText("Confirm Clear? [Y/N]");
                        }
                    }
                    else if (lower(currentCommand).rfind("/import", 0) == 0 && lower(currentCommand).rfind("/import ", 0) != 0) {
                        currentState = CurrentState::CONFIRM_IMPORT;
                        RenderText("Confirm Import? [Y/N]");
                    }
                    else if (lower(currentCommand).rfind("/quit", 0) == 0 && lower(currentCommand).rfind("/quit ", 0) != 0) {
                        currentState = CurrentState::QUIT;
                        RenderText("Press Any Key To Quit...");
                    }
                    else {
                        string text = currentCommand;
                        for (auto& el : state["commands"].items()) {
                            if (el.key() == text) {
                                text = el.value();
                                break;
                            }
                        }
                        if (!text.empty()) {
                            Type(text.c_str());
                        }
                    }
                    DestroySDLWindow();
                }
                else if (keyCode == VK_BACK && currentCommand.length() > 0) {
                    currentCommand = currentCommand.substr(0, currentCommand.length() - 1);
                    RenderText(currentCommand.c_str());
                }
            }
            else {
                currentCommand += keyChar;
                RenderText(currentCommand.c_str());
            }
        }
        cancelEvent();
    }
    else if (keyCode == sdword("trigger") && GetState() == sint("triggerFlags") && IsLunarRunning()) {
        currentCommand.clear();
        if (sbool("copyOpenChar")) {
            char openChar = GetOpenChar();
            if (openChar != NULL) {
                currentCommand.insert(currentCommand.begin(), openChar);
            }
        }
        RenderText(currentCommand.c_str());
        cancelEvent();
    }
}

#pragma region UNDEFMACROS
#undef LSHIFT_CONST
#undef RSHIFT_CONST
#undef LCTRL_CONST
#undef RCTRL_CONST
#pragma endregion

void Shutdown() {
    SaveState();
}

#pragma endregion
