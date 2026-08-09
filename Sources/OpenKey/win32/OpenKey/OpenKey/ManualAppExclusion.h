/*----------------------------------------------------------
NiceKey - The Cross platform Open source Vietnamese Keyboard application.

This file is part of NiceKey and is released under the GPL license.
-----------------------------------------------------------*/
#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct ManualExcludedAppInfo {
	std::wstring executableName;
	std::wstring displayName;
};

namespace ManualAppExclusion {
	void initialize();
	bool contains(const std::wstring& executableName);
	bool containsUtf8(const std::string& executableName);
	bool isForegroundExcluded();
	bool isWindowExcluded(HWND window);

	bool add(const std::wstring& executableName);
	bool remove(const std::wstring& executableName);
	std::vector<std::wstring> selectedApplications();
	std::vector<ManualExcludedAppInfo> runningApplications();

	bool runSelfTest();
}
