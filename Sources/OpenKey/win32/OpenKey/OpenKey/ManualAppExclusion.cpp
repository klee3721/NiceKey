/*----------------------------------------------------------
NiceKey - The Cross platform Open source Vietnamese Keyboard application.

This file is part of NiceKey and is released under the GPL license.
-----------------------------------------------------------*/
#include "stdafx.h"
#include "ManualAppExclusion.h"

#include <algorithm>
#include <cwctype>
#include <map>
#include <set>

namespace {
	const wchar_t* kRegistryValue = L"manualExcludedApps";
	std::set<std::wstring> selectedApps;
	DWORD cachedForegroundProcessId = 0;
	std::wstring cachedForegroundExecutable;

	std::wstring normalizeExecutableName(const std::wstring& value) {
		size_t separator = value.find_last_of(L"\\/");
		std::wstring result = separator == std::wstring::npos ? value : value.substr(separator + 1);
		std::transform(result.begin(), result.end(), result.begin(), [](wchar_t character) {
			return static_cast<wchar_t>(std::towlower(character));
		});
		return result;
	}

	std::vector<BYTE> serialize(const std::set<std::wstring>& apps) {
		std::vector<wchar_t> data;
		for (const std::wstring& app : apps) {
			data.insert(data.end(), app.begin(), app.end());
			data.push_back(L'\0');
		}
		data.push_back(L'\0');
		if (apps.empty()) data.push_back(L'\0');

		const BYTE* begin = reinterpret_cast<const BYTE*>(data.data());
		return std::vector<BYTE>(begin, begin + data.size() * sizeof(wchar_t));
	}

	std::set<std::wstring> deserialize(const BYTE* bytes, size_t size) {
		std::set<std::wstring> result;
		if (!bytes || size < sizeof(wchar_t) || size % sizeof(wchar_t) != 0) return result;

		const wchar_t* data = reinterpret_cast<const wchar_t*>(bytes);
		const size_t characterCount = size / sizeof(wchar_t);
		size_t cursor = 0;
		while (cursor < characterCount && data[cursor] != L'\0') {
			size_t end = cursor;
			while (end < characterCount && data[end] != L'\0') ++end;
			if (end == characterCount) break;
			std::wstring app = normalizeExecutableName(std::wstring(data + cursor, end - cursor));
			if (!app.empty()) result.insert(app);
			cursor = end + 1;
		}
		return result;
	}

	void save() {
		std::vector<BYTE> data = serialize(selectedApps);
		OpenKeyHelper::setRegBinary(kRegistryValue, data.data(), static_cast<int>(data.size()));
	}

	bool executableForWindow(HWND window, std::wstring& executableName, std::wstring* fullPath = nullptr) {
		executableName.clear();
		DWORD processId = 0;
		GetWindowThreadProcessId(window, &processId);
		if (processId == 0) return false;

		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (!process) return false;

		std::vector<wchar_t> path(32768, L'\0');
		DWORD pathLength = static_cast<DWORD>(path.size());
		bool success = QueryFullProcessImageNameW(process, 0, path.data(), &pathLength) != FALSE;
		CloseHandle(process);
		if (!success || pathLength == 0) return false;

		std::wstring pathValue(path.data(), pathLength);
		executableName = normalizeExecutableName(pathValue);
		if (fullPath) *fullPath = pathValue;
		return !executableName.empty();
	}

	std::wstring fallbackDisplayName(const std::wstring& executableName) {
		std::wstring displayName = executableName;
		if (displayName.size() > 4 && displayName.compare(displayName.size() - 4, 4, L".exe") == 0) {
			displayName.resize(displayName.size() - 4);
		}
		if (!displayName.empty()) displayName[0] = static_cast<wchar_t>(std::towupper(displayName[0]));
		return displayName;
	}

	std::wstring fileDescription(const std::wstring& fullPath, const std::wstring& executableName) {
		DWORD ignored = 0;
		DWORD versionSize = GetFileVersionInfoSizeW(fullPath.c_str(), &ignored);
		if (versionSize == 0) return fallbackDisplayName(executableName);

		std::vector<BYTE> versionData(versionSize);
		if (!GetFileVersionInfoW(fullPath.c_str(), 0, versionSize, versionData.data())) {
			return fallbackDisplayName(executableName);
		}

		struct Translation {
			WORD language;
			WORD codePage;
		};
		Translation* translations = nullptr;
		UINT translationSize = 0;
		if (VerQueryValueW(versionData.data(), L"\\VarFileInfo\\Translation",
			reinterpret_cast<void**>(&translations), &translationSize) &&
			translations && translationSize >= sizeof(Translation)) {
			wchar_t query[64] = { 0 };
			wsprintfW(query, L"\\StringFileInfo\\%04x%04x\\FileDescription",
				translations[0].language, translations[0].codePage);
			wchar_t* description = nullptr;
			UINT descriptionLength = 0;
			if (VerQueryValueW(versionData.data(), query, reinterpret_cast<void**>(&description),
				&descriptionLength) && description && descriptionLength > 1) {
				return description;
			}
		}
		return fallbackDisplayName(executableName);
	}

	struct EnumerationContext {
		std::map<std::wstring, ManualExcludedAppInfo> apps;
		DWORD ownProcessId;
	};

	BOOL CALLBACK enumerateWindow(HWND window, LPARAM parameter) {
		EnumerationContext* context = reinterpret_cast<EnumerationContext*>(parameter);
		if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr ||
			GetWindowTextLengthW(window) == 0) return TRUE;

		DWORD processId = 0;
		GetWindowThreadProcessId(window, &processId);
		if (processId == 0 || processId == context->ownProcessId) return TRUE;

		std::wstring executableName;
		std::wstring fullPath;
		if (!executableForWindow(window, executableName, &fullPath)) return TRUE;
		if (context->apps.find(executableName) != context->apps.end()) return TRUE;

		ManualExcludedAppInfo info;
		info.executableName = executableName;
		info.displayName = fileDescription(fullPath, executableName);
		context->apps[executableName] = info;
		return TRUE;
	}
}

void ManualAppExclusion::initialize() {
	DWORD size = 0;
	BYTE* data = OpenKeyHelper::getRegBinary(kRegistryValue, size);
	selectedApps = deserialize(data, size);
	cachedForegroundProcessId = 0;
	cachedForegroundExecutable.clear();
}

bool ManualAppExclusion::contains(const std::wstring& executableName) {
	return selectedApps.find(normalizeExecutableName(executableName)) != selectedApps.end();
}

bool ManualAppExclusion::containsUtf8(const std::string& executableName) {
	return contains(utf8ToWideString(executableName));
}

bool ManualAppExclusion::isWindowExcluded(HWND window) {
	std::wstring executableName;
	return executableForWindow(window, executableName) && contains(executableName);
}

bool ManualAppExclusion::isForegroundExcluded() {
	HWND foregroundWindow = GetForegroundWindow();
	DWORD processId = 0;
	GetWindowThreadProcessId(foregroundWindow, &processId);
	if (processId == 0) return false;

	if (processId != cachedForegroundProcessId) {
		cachedForegroundProcessId = processId;
		if (!executableForWindow(foregroundWindow, cachedForegroundExecutable)) {
			cachedForegroundExecutable.clear();
		}
	}
	return !cachedForegroundExecutable.empty() && contains(cachedForegroundExecutable);
}

bool ManualAppExclusion::add(const std::wstring& executableName) {
	std::wstring normalized = normalizeExecutableName(executableName);
	if (normalized.empty() || !selectedApps.insert(normalized).second) return false;
	save();
	return true;
}

bool ManualAppExclusion::remove(const std::wstring& executableName) {
	std::wstring normalized = normalizeExecutableName(executableName);
	if (selectedApps.erase(normalized) == 0) return false;
	save();
	return true;
}

std::vector<std::wstring> ManualAppExclusion::selectedApplications() {
	return std::vector<std::wstring>(selectedApps.begin(), selectedApps.end());
}

std::vector<ManualExcludedAppInfo> ManualAppExclusion::runningApplications() {
	EnumerationContext context = {};
	context.ownProcessId = GetCurrentProcessId();
	EnumWindows(enumerateWindow, reinterpret_cast<LPARAM>(&context));

	std::vector<ManualExcludedAppInfo> result;
	for (const auto& item : context.apps) result.push_back(item.second);
	std::sort(result.begin(), result.end(), [](const ManualExcludedAppInfo& left, const ManualExcludedAppInfo& right) {
		int displayComparison = CompareStringOrdinal(left.displayName.c_str(), -1,
			right.displayName.c_str(), -1, TRUE);
		if (displayComparison != CSTR_EQUAL) return displayComparison == CSTR_LESS_THAN;
		return CompareStringOrdinal(left.executableName.c_str(), -1,
			right.executableName.c_str(), -1, TRUE) == CSTR_LESS_THAN;
	});
	return result;
}

bool ManualAppExclusion::runSelfTest() {
	std::set<std::wstring> testApps = {
		normalizeExecutableName(L"C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\Brave.exe"),
		normalizeExecutableName(L"NOTEPAD.EXE")
	};
	std::vector<BYTE> data = serialize(testApps);
	std::set<std::wstring> restored = deserialize(data.data(), data.size());
	if (restored != testApps || restored.find(L"brave.exe") == restored.end() ||
		restored.find(L"notepad.exe") == restored.end()) return false;

	const BYTE malformed[] = { 'b', 0, 'a' };
	if (!deserialize(malformed, sizeof(malformed)).empty()) return false;
	return normalizeExecutableName(L"BRAVE.EXE") == L"brave.exe";
}
