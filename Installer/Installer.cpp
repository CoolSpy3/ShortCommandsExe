#pragma region Includes

#include <Windows.h>

#include <iostream>
#include <objbase.h>
#include <ObjIdl.h>
#include "resource.h"
#include <ShlGuid.h>
#include <shlobj.h>
#include <Shlwapi.h>
#pragma comment (lib, "shlwapi.lib")
#include <ShObjIdl.h>
#include <sstream>


using namespace std;

#pragma endregion

void ExtractResource(PWCHAR resourceName, wstring filename, const WCHAR* basePath) {
    HRSRC resource = FindResourceW(NULL, resourceName, RT_RCDATA);
    HGLOBAL resourceHandle = LoadResource(NULL, resource);
    LPVOID resourceData = LockResource(resourceHandle);
    WCHAR loc[MAX_PATH];
    wcscpy_s(loc, MAX_PATH, basePath);
    wcscpy_s(loc, MAX_PATH, filename.c_str());
    HANDLE fileHandle = CreateFileW(loc, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD numBytesWritten;
    WriteFile(fileHandle, resourceData, SizeofResource(NULL, resource), &numBytesWritten, NULL);
    CloseHandle(fileHandle);
}

int main()
{
    PWSTR path = NULL;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &path);
    wstringstream ss;
    ss << path << TEXT("\\shortcommands");
    wstring wspath = ss.str();
    const wchar_t* wpath = wspath.c_str();
    bool exists = CreateDirectoryW(wpath, NULL) == ERROR_ALREADY_EXISTS;
    SetCurrentDirectoryW(wpath);
    CoTaskMemFree(static_cast<void*>(path));
#define Extract(resource, filename) ExtractResource(MAKEINTRESOURCEW(resource), L ## filename, wpath)
    Extract(FONT, "Minecraft.ttf");
    Extract(EXE, "ShortCommands.exe");
    Extract(LIBFREETYPE, "libfrretype-6.dll");
    Extract(LIBJPEG, "libjpeg-9.dll");
    Extract(LIBPNG, "libpng16-16.dll");
    Extract(LIBTIFF, "libtiff-5.dll");
    Extract(LIBWEBP, "libwebp-7.dll");
    Extract(SDL2, "SDL2.dll");
    Extract(SDL2Image, "SDL2_image.dll");
    Extract(SDL2TTF, "SDL2_ttf.dll");
    Extract(ZLIB, "zlib1.dll");

    PWSTR shortcutPathT = NULL;
    SHGetKnownFolderPath(FOLDERID_Desktop, 0, 0, &shortcutPathT);
    wstringstream sss;
    sss << shortcutPathT << TEXT("\\ShortCommands.lnk");
    CoTaskMemFree(static_cast<void*>(shortcutPathT));
    wstring shortcutPathS = sss.str();
    const wchar_t* shortcutPath = shortcutPathS.c_str();
    if (!PathFileExistsW(shortcutPath)) {
        ss << TEXT("\\ShortCommands.exe");
        CoInitialize(NULL);
        IShellLinkW* psl;
        CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
        psl->SetPath(ss.str().c_str());
        IPersistFile* ppf;
        psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        ppf->Save(shortcutPath, TRUE);
        ppf->Release();
        psl->Release();
        CoUninitialize();
    }
}
