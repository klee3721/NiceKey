/*----------------------------------------------------------
OpenKey - The Cross platform Open source Vietnamese Keyboard application.

Copyright (C) 2019 Mai Vu Tuyen

This file is belong to the OpenKey project, Win32 version
which is released under GPL license.
You can fork, modify, improve this program. If you
redistribute your new version, it MUST be open source.
-----------------------------------------------------------*/
#include "OpenKeyManager.h"
#include <shlobj.h>

static vector<LPCTSTR> _inputType = {
	_T("Telex"),
	_T("VNI"),
	_T("Simple Telex"),
};

static vector<LPCTSTR> _tableCode = {
	_T("Unicode"),
	_T("TCVN3 (ABC)"),
	_T("VNI Windows"),
	_T("Unicode Tổ hợp"),
	_T("Vietnamese Locale CP 1258")
};

/*-----------------------------------------------------------------------*/

extern void OpenKeyInit();
extern void OpenKeyFree();

unsigned short  OpenKeyManager::_lastKeyCode = 0;

vector<LPCTSTR>& OpenKeyManager::getInputType() {
	return _inputType;
}

vector<LPCTSTR>& OpenKeyManager::getTableCode() {
	return _tableCode;
}

void OpenKeyManager::initEngine() {
	OpenKeyInit();
}

void OpenKeyManager::freeEngine() {
	OpenKeyFree();
}

bool OpenKeyManager::checkUpdate(string& newVersion) {
	newVersion.clear();
	wstring content = OpenKeyHelper::getContentOfUrl(
		_T("https://raw.githubusercontent.com/klee3721/NiceKey/master/version.json"));
	if (content.empty()) return false;
	size_t section = content.find(L"\"latestWinVersion\"");
	if (section == wstring::npos) return false;
	size_t codeKey = content.find(L"\"versionCode\"", section);
	size_t codeColon = codeKey == wstring::npos ? wstring::npos : content.find(L':', codeKey);
	if (codeColon == wstring::npos) return false;
	wchar_t* end = nullptr;
	long versionCode = wcstol(content.c_str() + codeColon + 1, &end, 10);
	if (end == content.c_str() + codeColon + 1) return false;

	size_t nameKey = content.find(L"\"versionName\"", section);
	size_t nameColon = nameKey == wstring::npos ? wstring::npos : content.find(L':', nameKey);
	size_t nameStart = nameColon == wstring::npos ? wstring::npos : content.find(L'\"', nameColon + 1);
	size_t nameEnd = nameStart == wstring::npos ? wstring::npos : content.find(L'\"', nameStart + 1);
	if (nameStart != wstring::npos && nameEnd != wstring::npos) {
		newVersion = wideStringToUtf8(content.substr(nameStart + 1, nameEnd - nameStart - 1));
	}
	return versionCode > static_cast<long>(OpenKeyHelper::getVersionNumber());
}

void OpenKeyManager::openReleasePage() {
	ShellExecuteW(nullptr, L"open", L"https://github.com/klee3721/NiceKey/releases", nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenKeyManager::createDesktopShortcut() {
	CoInitialize(NULL);
	IShellLink* pShellLink = NULL;
	HRESULT hres;
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_ALL,
							IID_IShellLink, (void**)&pShellLink);
	if (SUCCEEDED(hres)) {
		wstring path = OpenKeyHelper::getFullPath();
		pShellLink->SetPath(path.c_str());
		pShellLink->SetDescription(_T("NiceKey - Bộ gõ Tiếng Việt"));
		pShellLink->SetIconLocation(path.c_str(), 0);

		IPersistFile* pPersistFile;
		hres = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);

		if (SUCCEEDED(hres)) {
			wchar_t desktopPath[MAX_PATH + 1];
			wchar_t savePath[MAX_PATH + 10];
			SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath);
			wsprintf(savePath, _T("%s\\NiceKey.lnk"), desktopPath);
			hres = pPersistFile->Save(savePath, TRUE);
			pPersistFile->Release();
			pShellLink->Release();
		}
	}
}
