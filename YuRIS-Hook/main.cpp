#define no_init_all deprecated
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include <iostream>

#include "MinHook.h"

#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

/*
  81 40 | (space) 、 。 ， ． ・ ： ； ？ ！
*/
const char Special[] = {
	' ', '?', '.', ',', '.', '?', ':', ';', '?', '!'
};

/*
		  0 1 2 3 4 5 6 7 8 9 A B C D E F
  84 4_ | А Б В Г Д Е Ё Ж З И Й К Л М Н О
  84 5_ | П Р С Т У Ф Х Ц Ч Ш Щ Ъ Ы Ь Э Ю
  84 6_ | Я
  84 7_ | а б в г д е ё ж з и й к л м н ?
  84 8_ | о п р с т у ф х ц ч ш щ ъ ы ь э
  84 9_ | ю я
*/

const char Russia[] = {
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, '?', 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
	0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
	0xDF, '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, '?', 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, '?',
	0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD,
	0xFE, 0xFF
};

typedef int (WINAPI *TEXTOUTA)(HDC, int, int, LPCSTR, int);
TEXTOUTA fpTextOutA = NULL;

BOOL WINAPI _TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c)
{
	if (c == 2) {
		unsigned char a = lpString[0], b = lpString[1];
		if (a == 0x84 && b >= 0x40 && b <= 0x40 + sizeof(Russia))
		{
			return fpTextOutA(hdc, x, y, Russia + b - 0x40, 1);
		}
		else if (a == 0x81 && b >= 0x40 && b <= 0x40 + sizeof(Special))
		{
			return fpTextOutA(hdc, x, y, Special + b - 0x40, 1);
		}
	}
	return fpTextOutA(hdc, x, y, lpString, c);
}

typedef HFONT(WINAPI* CREATEFONTINDIRECTA)(LOGFONTA*);
CREATEFONTINDIRECTA fpCreateFontIndirectA = NULL;

HFONT WINAPI _CreateFontIndirectA(LOGFONTA* lplf)
{
	lplf->lfCharSet = RUSSIAN_CHARSET;
	strcpy_s(lplf->lfFaceName, "Arial");
	return fpCreateFontIndirectA(lplf);
}

typedef HFONT(WINAPI* CREATEFONTA)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
CREATEFONTA fpCreateFontA = NULL;

HFONT WINAPI _CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
	DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
	DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
	return fpCreateFontA(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic,
		bUnderline, bStrikeOut, RUSSIAN_CHARSET, iOutPrecision, iClipPrecision,
		iQuality, iPitchAndFamily, "Arial");
}

typedef int(WINAPI* MESSAGEBOXA)(HWND, LPCSTR, LPCSTR, UINT);
MESSAGEBOXA fpMessageBoxA = NULL;

int WINAPI _MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	// May replace the messagebox content
	return fpMessageBoxA(hWnd, lpText, lpCaption, uType);
}

BOOLEAN WINAPI DllMain(IN HINSTANCE hDllHandle, IN DWORD nReason, IN LPVOID Reserved)
{
	switch (nReason)
	{
	case DLL_PROCESS_ATTACH:
		// AllocConsole();
		// freopen("CONOUT$", "w", stdout);

		if (MH_Initialize() != MH_OK)
		{
			std::cout << "Failed to initialize MH";
			break;
		}

		if (MH_CreateHook(&TextOutA, &_TextOutA, reinterpret_cast<LPVOID*>(&fpTextOutA)) != MH_OK)
		{
			std::cout << "Failed to hook TextOutA";
			break;
		}
		if (MH_EnableHook(&TextOutA) != MH_OK)
		{
			std::cout << "Failed to enable TextOutA";
			break;
		}

		if (MH_CreateHook(&CreateFontIndirectA, &_CreateFontIndirectA, reinterpret_cast<LPVOID*>(&fpCreateFontIndirectA)) != MH_OK)
		{
			std::cout << "Failed to hook CreateFontIndirectA";
			break;
		}
		if (MH_EnableHook(&CreateFontIndirectA) != MH_OK)
		{
			std::cout << "Failed to enable CreateFontIndirectA";
			break;
		}

		std::cout << "Hook complete\n";

		return TRUE;

	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		return TRUE;
	}
	return FALSE;
}
