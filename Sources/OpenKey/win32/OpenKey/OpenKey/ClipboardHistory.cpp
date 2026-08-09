/*----------------------------------------------------------
NiceKey clipboard history for Windows.

This module is part of the NiceKey fork and is released under
the GPL-3.0 license together with the rest of the application.
-----------------------------------------------------------*/
#include "stdafx.h"
#include "ClipboardHistory.h"

#include <algorithm>
#include <commctrl.h>
#include <cwctype>
#include <fstream>
#include <shlobj.h>
#include <sstream>
#include <windowsx.h>

#pragma comment(lib, "Comctl32.lib")

extern void NiceKeySetEngineSuspended(bool suspended);

namespace {
	const wchar_t* kMessageWindowClass = L"NiceKeyClipboardMessageWindow";
	const wchar_t* kPickerWindowClass = L"NiceKeyClipboardPickerWindow";
	const wchar_t* kInternalClipboardFormat = L"NiceKey Internal Clipboard";
	const wchar_t* kExcludeMonitorFormat = L"ExcludeClipboardContentFromMonitorProcessing";
	const wchar_t* kIncludeInHistoryFormat = L"CanIncludeInClipboardHistory";

	const UINT kHotKeyId = 0x4E4B;
	const UINT_PTR kCaptureRetryTimer = 1;
	const UINT_PTR kPasteTimer = 2;
	const int kSearchEditId = 5100;
	const size_t kMaxImageBytes = 12 * 1024 * 1024;
	const size_t kMaxTextCharacters = 4 * 1024 * 1024;
	const uint32_t kHistoryVersion = 1;
	const char kHistoryMagic[4] = { 'N', 'K', 'C', 'H' };

	struct HistoryHeader {
		char magic[4];
		uint32_t version;
		uint32_t itemCount;
	};

	bool pointInRect(const RECT& rect, int x, int y) {
		POINT point = { x, y };
		return PtInRect(&rect, point) != FALSE;
	}

	std::wstring joinPath(const std::wstring& left, const std::wstring& right) {
		if (left.empty()) return right;
		if (left.back() == L'\\') return left + right;
		return left + L"\\" + right;
	}

	bool fileExists(const std::wstring& path) {
		DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool isSafeImageFileName(const std::wstring& name) {
		return name.size() > 9 && name.size() < 96 && name.rfind(L"clip-", 0) == 0 &&
			name.compare(name.size() - 4, 4, L".dib") == 0 &&
			name.find_first_of(L"\\/") == std::wstring::npos &&
			name.find(L"..") == std::wstring::npos;
	}

	bool writeString(std::ofstream& stream, const std::wstring& value) {
		uint32_t length = static_cast<uint32_t>(value.size());
		stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
		if (length > 0) {
			stream.write(reinterpret_cast<const char*>(value.data()), length * sizeof(wchar_t));
		}
		return stream.good();
	}

	bool readString(std::ifstream& stream, std::wstring& value) {
		uint32_t length = 0;
		stream.read(reinterpret_cast<char*>(&length), sizeof(length));
		if (!stream.good() || length > kMaxTextCharacters) return false;
		value.resize(length);
		if (length > 0) {
			stream.read(reinterpret_cast<char*>(&value[0]), length * sizeof(wchar_t));
		}
		return stream.good();
	}

	bool readBinaryFile(const std::wstring& path, std::vector<BYTE>& data) {
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream.good()) return false;
		std::streamoff size = stream.tellg();
		if (size <= 0 || static_cast<uint64_t>(size) > kMaxImageBytes) return false;
		data.resize(static_cast<size_t>(size));
		stream.seekg(0, std::ios::beg);
		stream.read(reinterpret_cast<char*>(data.data()), size);
		return stream.good();
	}

	std::wstring lowercase(const std::wstring& value) {
		std::wstring result = value;
		std::transform(result.begin(), result.end(), result.begin(), towlower);
		return result;
	}

	bool isCombiningMark(wchar_t character) {
		return (character >= 0x0300 && character <= 0x036F) ||
			(character >= 0x1AB0 && character <= 0x1AFF) ||
			(character >= 0x1DC0 && character <= 0x1DFF);
	}

	COLORREF blendColor(COLORREF foreground, COLORREF background, int foregroundPercent) {
		int inverse = 100 - foregroundPercent;
		return RGB(
			(GetRValue(foreground) * foregroundPercent + GetRValue(background) * inverse) / 100,
			(GetGValue(foreground) * foregroundPercent + GetGValue(background) * inverse) / 100,
			(GetBValue(foreground) * foregroundPercent + GetBValue(background) * inverse) / 100);
	}

	void fillSolidRect(HDC dc, const RECT& rect, COLORREF color) {
		HBRUSH brush = CreateSolidBrush(color);
		FillRect(dc, &rect, brush);
		DeleteObject(brush);
	}

	void drawRoundRect(HDC dc, const RECT& rect, int radius, COLORREF color) {
		HBRUSH brush = CreateSolidBrush(color);
		HPEN pen = CreatePen(PS_NULL, 0, color);
		HGDIOBJ oldBrush = SelectObject(dc, brush);
		HGDIOBJ oldPen = SelectObject(dc, pen);
		RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
		SelectObject(dc, oldPen);
		SelectObject(dc, oldBrush);
		DeleteObject(pen);
		DeleteObject(brush);
	}

	bool hasZeroDwordClipboardValue(UINT format) {
		if (!IsClipboardFormatAvailable(format)) return false;
		HANDLE handle = GetClipboardData(format);
		if (!handle || GlobalSize(handle) < sizeof(DWORD)) return false;
		const DWORD* value = static_cast<const DWORD*>(GlobalLock(handle));
		if (!value) return false;
		bool isZero = *value == 0;
		GlobalUnlock(handle);
		return isZero;
	}

	size_t dibPixelOffset(const BITMAPINFOHEADER* header, size_t totalSize) {
		if (!header || header->biSize < sizeof(BITMAPINFOHEADER) || header->biSize >= totalSize) return 0;
		size_t offset = header->biSize;
		if (header->biSize == sizeof(BITMAPINFOHEADER) &&
			(header->biCompression == BI_BITFIELDS || header->biCompression == 6)) {
			offset += header->biCompression == 6 ? sizeof(DWORD) * 4 : sizeof(DWORD) * 3;
		}
		if (header->biBitCount <= 8) {
			DWORD colors = header->biClrUsed ? header->biClrUsed : (1u << header->biBitCount);
			offset += static_cast<size_t>(colors) * sizeof(RGBQUAD);
		}
		return offset < totalSize ? offset : 0;
	}
}

ClipboardHistory& ClipboardHistory::shared() {
	static ClipboardHistory history;
	return history;
}

ClipboardHistory::ClipboardHistory()
	: instance_(nullptr), messageWindow_(nullptr), pickerWindow_(nullptr), searchEdit_(nullptr),
	settingsWindow_(nullptr), previousForegroundWindow_(nullptr), pendingPasteWindow_(nullptr),
	titleFont_(nullptr), bodyFont_(nullptr), bodyBoldFont_(nullptr), captionFont_(nullptr), iconFont_(nullptr),
	selectedIndex_(0), hoveredIndex_(-1), scrollRow_(0), rowHeight_(68), listTop_(0), listBottom_(0),
	hotKey_(kDefaultHotKey), captureRetryCount_(0), initialized_(false), enabled_(true), pinOnTop_(true),
	autoHide_(true), hotKeyRegistered_(false), ignoreNextClipboardChange_(false), pasting_(false),
	suppressGeometrySave_(false), internalClipboardFormat_(0), excludeMonitorFormat_(0), includeInHistoryFormat_(0) {
	SetRectEmpty(&closeButtonRect_);
	SetRectEmpty(&clearButtonRect_);
	SetRectEmpty(&pinButtonRect_);
	SetRectEmpty(&autoHideButtonRect_);
}

bool ClipboardHistory::initialize(HINSTANCE instance) {
	if (initialized_) return true;
	instance_ = instance;
	loadSettings();
	loadItems();

	internalClipboardFormat_ = RegisterClipboardFormatW(kInternalClipboardFormat);
	excludeMonitorFormat_ = RegisterClipboardFormatW(kExcludeMonitorFormat);
	includeInHistoryFormat_ = RegisterClipboardFormatW(kIncludeInHistoryFormat);

	if (!registerWindowClasses()) return false;
	messageWindow_ = CreateWindowExW(0, kMessageWindowClass, L"", 0, 0, 0, 0, 0,
		HWND_MESSAGE, nullptr, instance_, this);
	if (!messageWindow_) return false;

	AddClipboardFormatListener(messageWindow_);
	initialized_ = true;
	updateHotKeyRegistration();
	return true;
}

void ClipboardHistory::shutdown() {
	if (!initialized_) return;
	closePicker(false);
	if (settingsWindow_) {
		DestroyWindow(settingsWindow_);
		settingsWindow_ = nullptr;
	}
	if (hotKeyRegistered_) {
		UnregisterHotKey(messageWindow_, kHotKeyId);
		hotKeyRegistered_ = false;
	}
	if (messageWindow_) {
		RemoveClipboardFormatListener(messageWindow_);
		DestroyWindow(messageWindow_);
		messageWindow_ = nullptr;
	}
	initialized_ = false;
}

bool ClipboardHistory::registerWindowClasses() {
	WNDCLASSEXW messageClass = {};
	messageClass.cbSize = sizeof(messageClass);
	messageClass.lpfnWndProc = MessageWindowProc;
	messageClass.hInstance = instance_;
	messageClass.lpszClassName = kMessageWindowClass;
	if (!RegisterClassExW(&messageClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

	WNDCLASSEXW pickerClass = {};
	pickerClass.cbSize = sizeof(pickerClass);
	pickerClass.style = CS_DBLCLKS;
	pickerClass.lpfnWndProc = PickerWindowProc;
	pickerClass.hInstance = instance_;
	pickerClass.hIcon = LoadIcon(instance_, MAKEINTRESOURCE(IDI_APP_ICON));
	pickerClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	pickerClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	pickerClass.lpszClassName = kPickerWindowClass;
	pickerClass.hIconSm = pickerClass.hIcon;
	return RegisterClassExW(&pickerClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void ClipboardHistory::loadSettings() {
	enabled_ = OpenKeyHelper::getRegInt(L"clipboardHistoryEnabled", 1) != 0;
	pinOnTop_ = OpenKeyHelper::getRegInt(L"clipboardPickerPinOnTop", 1) != 0;
	autoHide_ = OpenKeyHelper::getRegInt(L"clipboardPickerAutoHide", 1) != 0;
	hotKey_ = OpenKeyHelper::getRegInt(L"clipboardHotKey", kDefaultHotKey);
	if ((hotKey_ & 0xFF) == 0) hotKey_ = kDefaultHotKey;
}

bool ClipboardHistory::isEnabled() const { return enabled_; }
bool ClipboardHistory::isPickerOpen() const { return pickerWindow_ != nullptr; }
bool ClipboardHistory::isHotKeyRegistered() const { return hotKeyRegistered_; }
int ClipboardHistory::itemCount() const { return static_cast<int>(items_.size()); }
int ClipboardHistory::hotKey() const { return hotKey_; }

void ClipboardHistory::setEnabled(bool enabled) {
	if (enabled_ == enabled) return;
	enabled_ = enabled;
	OpenKeyHelper::setRegInt(L"clipboardHistoryEnabled", enabled ? 1 : 0);
	if (!enabled_) closePicker();
	updateHotKeyRegistration();
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::setPinOnTop(bool pinOnTop) {
	if (pinOnTop_ == pinOnTop) return;
	pinOnTop_ = pinOnTop;
	OpenKeyHelper::setRegInt(L"clipboardPickerPinOnTop", pinOnTop ? 1 : 0);
	if (pickerWindow_) {
		SetWindowPos(pickerWindow_, pinOnTop_ ? HWND_TOPMOST : HWND_NOTOPMOST,
			0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		InvalidateRect(pickerWindow_, &pinButtonRect_, TRUE);
	}
	updateSettingsDialog();
}

void ClipboardHistory::setAutoHide(bool autoHide) {
	if (autoHide_ == autoHide) return;
	autoHide_ = autoHide;
	OpenKeyHelper::setRegInt(L"clipboardPickerAutoHide", autoHide ? 1 : 0);
	if (pickerWindow_) InvalidateRect(pickerWindow_, &autoHideButtonRect_, TRUE);
	updateSettingsDialog();
}

void ClipboardHistory::setHotKey(int hotKey) {
	if (hotKey_ == hotKey) return;
	hotKey_ = hotKey;
	OpenKeyHelper::setRegInt(L"clipboardHotKey", hotKey_);
	updateHotKeyRegistration();
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::resetSettings() {
	enabled_ = true;
	pinOnTop_ = true;
	autoHide_ = true;
	hotKey_ = kDefaultHotKey;
	OpenKeyHelper::setRegInt(L"clipboardHistoryEnabled", 1);
	OpenKeyHelper::setRegInt(L"clipboardPickerPinOnTop", 1);
	OpenKeyHelper::setRegInt(L"clipboardPickerAutoHide", 1);
	OpenKeyHelper::setRegInt(L"clipboardHotKey", hotKey_);
	resetPickerLayout();
	updateHotKeyRegistration();
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::updateHotKeyRegistration() {
	if (!messageWindow_) return;
	if (hotKeyRegistered_) {
		UnregisterHotKey(messageWindow_, kHotKeyId);
		hotKeyRegistered_ = false;
	}
	if (!enabled_) return;

	UINT key = static_cast<UINT>(hotKey_ & 0xFF);
	UINT modifiers = MOD_NOREPEAT;
	if (hotKey_ & 0x100) modifiers |= MOD_CONTROL;
	if (hotKey_ & 0x200) modifiers |= MOD_ALT;
	if (hotKey_ & 0x400) modifiers |= MOD_WIN;
	if (hotKey_ & 0x800) modifiers |= MOD_SHIFT;
	if (key == 0 || key == 0xFE || modifiers == MOD_NOREPEAT) return;
	hotKeyRegistered_ = RegisterHotKey(messageWindow_, kHotKeyId, modifiers, key) != FALSE;
}

std::wstring ClipboardHistory::keyDescription(UINT virtualKey) {
	if (virtualKey == 0 || virtualKey == 0xFE) return L"Chưa đặt";
	if (virtualKey == VK_SPACE) return L"Space";
	if ((virtualKey >= L'A' && virtualKey <= L'Z') || (virtualKey >= L'0' && virtualKey <= L'9')) {
		return std::wstring(1, static_cast<wchar_t>(virtualKey));
	}
	UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC) << 16;
	wchar_t name[64] = {};
	if (GetKeyNameTextW(static_cast<LONG>(scanCode), name, ARRAYSIZE(name)) > 0) return name;
	wchar_t fallback[16] = {};
	wsprintfW(fallback, L"VK %u", virtualKey);
	return fallback;
}

std::wstring ClipboardHistory::hotKeyDescription() const {
	UINT key = static_cast<UINT>(hotKey_ & 0xFF);
	if (key == 0 || key == 0xFE) return L"Chưa đặt";
	std::wstring result;
	auto append = [&result](const wchar_t* value) {
		if (!result.empty()) result += L" + ";
		result += value;
	};
	if (hotKey_ & 0x100) append(L"Ctrl");
	if (hotKey_ & 0x200) append(L"Alt");
	if (hotKey_ & 0x400) append(L"Win");
	if (hotKey_ & 0x800) append(L"Shift");
	std::wstring description = keyDescription(key);
	append(description.c_str());
	return result.empty() ? L"Chưa đặt" : result;
}

bool ClipboardHistory::shouldBypassEngineForHotKey(UINT virtualKey, bool control, bool alt, bool win, bool shift) const {
	if (!enabled_ || !hotKeyRegistered_ || virtualKey != static_cast<UINT>(hotKey_ & 0xFF)) return false;
	return control == ((hotKey_ & 0x100) != 0) &&
		alt == ((hotKey_ & 0x200) != 0) &&
		win == ((hotKey_ & 0x400) != 0) &&
		shift == ((hotKey_ & 0x800) != 0);
}

LRESULT CALLBACK ClipboardHistory::MessageWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	ClipboardHistory* self = reinterpret_cast<ClipboardHistory*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_NCCREATE) {
		CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		self = static_cast<ClipboardHistory*>(create->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	return self ? self->handleMessageWindow(window, message, wParam, lParam)
		: DefWindowProcW(window, message, wParam, lParam);
}

LRESULT ClipboardHistory::handleMessageWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CLIPBOARDUPDATE:
		if (!captureClipboard() && captureRetryCount_ < 5) {
			++captureRetryCount_;
			SetTimer(window, kCaptureRetryTimer, 100, nullptr);
		} else {
			captureRetryCount_ = 0;
		}
		return 0;
	case WM_HOTKEY:
		if (wParam == kHotKeyId) togglePicker();
		return 0;
	case WM_TIMER:
		if (wParam == kCaptureRetryTimer) {
			KillTimer(window, kCaptureRetryTimer);
			if (!captureClipboard() && captureRetryCount_ < 5) {
				++captureRetryCount_;
				SetTimer(window, kCaptureRetryTimer, 100, nullptr);
			} else {
				captureRetryCount_ = 0;
			}
			return 0;
		}
		if (wParam == kPasteTimer) {
			KillTimer(window, kPasteTimer);
			sendPasteShortcut();
			return 0;
		}
		break;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

bool ClipboardHistory::isSensitiveClipboard() const {
	if (internalClipboardFormat_ && IsClipboardFormatAvailable(internalClipboardFormat_)) return true;
	if (excludeMonitorFormat_ && IsClipboardFormatAvailable(excludeMonitorFormat_)) return true;
	if (includeInHistoryFormat_ && hasZeroDwordClipboardValue(includeInHistoryFormat_)) return true;

	UINT format = 0;
	while ((format = EnumClipboardFormats(format)) != 0) {
		wchar_t name[256] = {};
		if (GetClipboardFormatNameW(format, name, ARRAYSIZE(name)) <= 0) continue;
		std::wstring lowered = lowercase(name);
		if (lowered.find(L"1password") != std::wstring::npos ||
			lowered.find(L"keepass") != std::wstring::npos ||
			lowered.find(L"bitwarden") != std::wstring::npos ||
			lowered.find(L"passwordsafe") != std::wstring::npos ||
			lowered.find(L"concealed") != std::wstring::npos ||
			lowered.find(L"sensitive") != std::wstring::npos) {
			return true;
		}
	}
	return false;
}

bool ClipboardHistory::captureClipboard() {
	if (!enabled_) return true;
	if (!OpenClipboard(messageWindow_)) return false;

	if (ignoreNextClipboardChange_) {
		ignoreNextClipboardChange_ = false;
		CloseClipboard();
		return true;
	}
	if (isSensitiveClipboard()) {
		CloseClipboard();
		return true;
	}

	std::wstring source = sourceApplicationName();
	HANDLE textHandle = GetClipboardData(CF_UNICODETEXT);
	if (textHandle) {
		SIZE_T byteSize = GlobalSize(textHandle);
		const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(textHandle));
		if (text && byteSize >= sizeof(wchar_t)) {
			size_t capacity = byteSize / sizeof(wchar_t);
			size_t length = 0;
			while (length < capacity && text[length] != L'\0') ++length;
			bool valid = length < capacity && length <= kMaxTextCharacters;
			std::wstring value;
			if (valid) value.assign(text, length);
			GlobalUnlock(textHandle);
			if (!valid) {
				CloseClipboard();
				return true;
			}
			CloseClipboard();
			if (!value.empty()) addText(value, source);
			return true;
		}
		if (text) GlobalUnlock(textHandle);
	}

	UINT imageFormat = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 :
		(IsClipboardFormatAvailable(CF_DIB) ? CF_DIB : 0);
	if (imageFormat) {
		HANDLE imageHandle = GetClipboardData(imageFormat);
		SIZE_T size = imageHandle ? GlobalSize(imageHandle) : 0;
		const BYTE* bytes = imageHandle && size > 0 && size <= kMaxImageBytes
			? static_cast<const BYTE*>(GlobalLock(imageHandle)) : nullptr;
		if (bytes) {
			std::vector<BYTE> copy(bytes, bytes + size);
			GlobalUnlock(imageHandle);
			CloseClipboard();
			addImage(imageFormat, copy.data(), copy.size(), source);
			return true;
		}
	}

	CloseClipboard();
	return true;
}

void ClipboardHistory::addText(const std::wstring& text, const std::wstring& sourceApp) {
	for (auto it = items_.begin(); it != items_.end();) {
		if (it->type == NiceKeyClipItem::Type::Text && it->text == text) it = items_.erase(it);
		else ++it;
	}
	NiceKeyClipItem item = {};
	item.id = currentFileTime() ^ static_cast<uint64_t>(GetTickCount64());
	item.type = NiceKeyClipItem::Type::Text;
	item.capturedAt = currentFileTime();
	item.text = text;
	item.sourceApp = sourceApp;
	item.clipboardFormat = CF_UNICODETEXT;
	items_.insert(items_.begin(), item);
	trimItems();
	persistItems();
	rebuildFilter();
	if (pickerWindow_) {
		layoutPicker();
		InvalidateRect(pickerWindow_, nullptr, TRUE);
	}
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::addImage(UINT format, const BYTE* data, size_t size, const std::wstring& sourceApp) {
	if (!data || size < sizeof(BITMAPINFOHEADER) || size > kMaxImageBytes) return;
	const BITMAPINFOHEADER* header = reinterpret_cast<const BITMAPINFOHEADER*>(data);
	if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biWidth == 0 || header->biHeight == 0) return;

	NiceKeyClipItem item = {};
	item.id = currentFileTime() ^ static_cast<uint64_t>(GetTickCount64());
	item.type = NiceKeyClipItem::Type::Image;
	item.capturedAt = currentFileTime();
	item.sourceApp = sourceApp;
	item.clipboardFormat = format;
	item.imageWidth = abs(header->biWidth);
	item.imageHeight = abs(header->biHeight);
	wchar_t label[128] = {};
	wsprintfW(label, L"Hình ảnh %d×%d", item.imageWidth, item.imageHeight);
	item.text = label;
	wchar_t filename[96] = {};
	wsprintfW(filename, L"clip-%I64u.dib", item.id);
	item.imageFile = filename;

	std::ofstream stream(imagePath(item), std::ios::binary | std::ios::trunc);
	if (!stream.good()) return;
	stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
	stream.close();
	if (!stream.good()) {
		DeleteFileW(imagePath(item).c_str());
		return;
	}

	items_.insert(items_.begin(), item);
	trimItems();
	persistItems();
	rebuildFilter();
	if (pickerWindow_) {
		layoutPicker();
		InvalidateRect(pickerWindow_, nullptr, TRUE);
	}
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::trimItems() {
	while (items_.size() > kMaxItems) {
		deleteImageFile(items_.back());
		items_.pop_back();
	}
}

void ClipboardHistory::promoteItem(size_t index) {
	if (index >= items_.size() || index == 0) return;
	NiceKeyClipItem item = items_[index];
	items_.erase(items_.begin() + index);
	items_.insert(items_.begin(), item);
	persistItems();
}

void ClipboardHistory::removeItem(size_t index) {
	if (index >= items_.size()) return;
	deleteImageFile(items_[index]);
	items_.erase(items_.begin() + index);
	persistItems();
	rebuildFilter();
	selectedIndex_ = std::min(selectedIndex_, std::max(0, static_cast<int>(filteredItems_.size()) - 1));
	ensureSelectionVisible();
	if (pickerWindow_) {
		layoutPicker();
		InvalidateRect(pickerWindow_, nullptr, TRUE);
	}
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

void ClipboardHistory::clear() {
	for (const NiceKeyClipItem& item : items_) deleteImageFile(item);
	items_.clear();
	filteredItems_.clear();
	selectedIndex_ = 0;
	scrollRow_ = 0;
	persistItems();
	if (searchEdit_) {
		SetWindowTextW(searchEdit_, L"");
		ShowWindow(searchEdit_, SW_HIDE);
	}
	if (pickerWindow_) {
		layoutPicker();
		InvalidateRect(pickerWindow_, nullptr, TRUE);
	}
	updateSettingsDialog();
	SystemTrayHelper::updateData();
}

std::wstring ClipboardHistory::dataDirectory() const {
	wchar_t localAppData[MAX_PATH] = {};
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData))) return L"";
	std::wstring niceKeyDirectory = joinPath(localAppData, L"NiceKey");
	CreateDirectoryW(niceKeyDirectory.c_str(), nullptr);
	std::wstring clipboardDirectory = joinPath(niceKeyDirectory, L"Clipboard");
	CreateDirectoryW(clipboardDirectory.c_str(), nullptr);
	return clipboardDirectory;
}

std::wstring ClipboardHistory::historyFilePath() const {
	std::wstring directory = dataDirectory();
	return directory.empty() ? L"" : joinPath(directory, L"history.dat");
}

std::wstring ClipboardHistory::imagePath(const NiceKeyClipItem& item) const {
	std::wstring directory = dataDirectory();
	return directory.empty() ? L"" : joinPath(directory, item.imageFile);
}

void ClipboardHistory::deleteImageFile(const NiceKeyClipItem& item) const {
	if (item.type == NiceKeyClipItem::Type::Image && !item.imageFile.empty()) {
		DeleteFileW(imagePath(item).c_str());
	}
}

void ClipboardHistory::persistItems() const {
	std::wstring target = historyFilePath();
	if (target.empty()) return;
	std::wstring temporary = target + L".tmp";
	std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
	if (!stream.good()) return;
	HistoryHeader header = {};
	memcpy(header.magic, kHistoryMagic, sizeof(header.magic));
	header.version = kHistoryVersion;
	header.itemCount = static_cast<uint32_t>(items_.size());
	stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
	for (const NiceKeyClipItem& item : items_) {
		uint32_t type = static_cast<uint32_t>(item.type);
		stream.write(reinterpret_cast<const char*>(&item.id), sizeof(item.id));
		stream.write(reinterpret_cast<const char*>(&type), sizeof(type));
		stream.write(reinterpret_cast<const char*>(&item.capturedAt), sizeof(item.capturedAt));
		stream.write(reinterpret_cast<const char*>(&item.clipboardFormat), sizeof(item.clipboardFormat));
		stream.write(reinterpret_cast<const char*>(&item.imageWidth), sizeof(item.imageWidth));
		stream.write(reinterpret_cast<const char*>(&item.imageHeight), sizeof(item.imageHeight));
		if (!writeString(stream, item.text) || !writeString(stream, item.sourceApp) || !writeString(stream, item.imageFile)) break;
	}
	stream.close();
	if (stream.good()) MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	else DeleteFileW(temporary.c_str());
}

void ClipboardHistory::loadItems() {
	items_.clear();
	std::ifstream stream(historyFilePath(), std::ios::binary);
	if (!stream.good()) return;
	HistoryHeader header = {};
	stream.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!stream.good() || memcmp(header.magic, kHistoryMagic, sizeof(header.magic)) != 0 ||
		header.version != kHistoryVersion || header.itemCount > 1000) return;
	for (uint32_t index = 0; index < header.itemCount && items_.size() < kMaxItems; ++index) {
		NiceKeyClipItem item = {};
		uint32_t type = 0;
		stream.read(reinterpret_cast<char*>(&item.id), sizeof(item.id));
		stream.read(reinterpret_cast<char*>(&type), sizeof(type));
		stream.read(reinterpret_cast<char*>(&item.capturedAt), sizeof(item.capturedAt));
		stream.read(reinterpret_cast<char*>(&item.clipboardFormat), sizeof(item.clipboardFormat));
		stream.read(reinterpret_cast<char*>(&item.imageWidth), sizeof(item.imageWidth));
		stream.read(reinterpret_cast<char*>(&item.imageHeight), sizeof(item.imageHeight));
		if (!stream.good() || type > static_cast<uint32_t>(NiceKeyClipItem::Type::Image) ||
			!readString(stream, item.text) || !readString(stream, item.sourceApp) || !readString(stream, item.imageFile)) break;
		item.type = static_cast<NiceKeyClipItem::Type>(type);
		if (item.type == NiceKeyClipItem::Type::Image &&
			(!isSafeImageFileName(item.imageFile) || !fileExists(imagePath(item)))) continue;
		items_.push_back(item);
	}
	rebuildFilter();
}

void ClipboardHistory::togglePicker() {
	if (!enabled_) return;
	if (pickerWindow_) closePicker();
	else showPicker();
}

void ClipboardHistory::showPicker() {
	if (!enabled_ || pickerWindow_) return;
	previousForegroundWindow_ = GetForegroundWindow();
	query_.clear();
	selectedIndex_ = 0;
	hoveredIndex_ = -1;
	scrollRow_ = 0;
	rebuildFilter();
	createPicker();
}

void ClipboardHistory::createPicker() {
	int defaultWidth = scaled(nullptr, 480);
	int defaultHeight = scaled(nullptr, 640);
	int width = OpenKeyHelper::getRegInt(L"clipboardPickerWidth", defaultWidth);
	int height = OpenKeyHelper::getRegInt(L"clipboardPickerHeight", defaultHeight);
	width = std::max(width, scaled(nullptr, 380));
	height = std::max(height, scaled(nullptr, 300));
	int x = OpenKeyHelper::getRegInt(L"clipboardPickerX", CW_USEDEFAULT);
	int y = OpenKeyHelper::getRegInt(L"clipboardPickerY", CW_USEDEFAULT);

	if (x == CW_USEDEFAULT || y == CW_USEDEFAULT) {
		RECT workArea = {};
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
		x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
		y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
	} else {
		RECT proposed = { x, y, x + width, y + height };
		if (!MonitorFromRect(&proposed, MONITOR_DEFAULTTONULL)) {
			RECT workArea = {};
			SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
			x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
			y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
		}
	}

	DWORD exStyle = WS_EX_TOOLWINDOW | (pinOnTop_ ? WS_EX_TOPMOST : 0);
	pickerWindow_ = CreateWindowExW(exStyle, kPickerWindowClass, L"Lịch sử Clipboard",
		WS_POPUP | WS_THICKFRAME, x, y, width, height, nullptr, nullptr, instance_, this);
	if (!pickerWindow_) return;

	// Windows 11 rounds this borderless tool window; older versions ignore it.
	HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
	if (dwm) {
		typedef HRESULT(WINAPI* DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
		auto setAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
		if (setAttribute) {
			DWORD preference = 2;
			setAttribute(pickerWindow_, 33, &preference, sizeof(preference));
		}
		FreeLibrary(dwm);
	}

	createPickerFonts();
	layoutPicker();
	NiceKeySetEngineSuspended(true);
	ShowWindow(pickerWindow_, SW_SHOWNORMAL);
	SetForegroundWindow(pickerWindow_);
	if (searchEdit_ && !items_.empty()) SetFocus(searchEdit_);
}

void ClipboardHistory::closePicker(bool restorePreviousWindow) {
	if (!pickerWindow_) return;
	HWND previous = previousForegroundWindow_;
	savePickerLayout();
	DestroyWindow(pickerWindow_);
	if (restorePreviousWindow && !pasting_ && previous && IsWindow(previous)) {
		SetForegroundWindow(previous);
	}
	previousForegroundWindow_ = nullptr;
}

LRESULT CALLBACK ClipboardHistory::PickerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	ClipboardHistory* self = reinterpret_cast<ClipboardHistory*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_NCCREATE) {
		CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		self = static_cast<ClipboardHistory*>(create->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	return self ? self->handlePickerWindow(window, message, wParam, lParam)
		: DefWindowProcW(window, message, wParam, lParam);
}

LRESULT ClipboardHistory::handlePickerWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		searchEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
			window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSearchEditId)), instance_, nullptr);
		SetWindowSubclass(searchEdit_, SearchEditProc, 1, reinterpret_cast<DWORD_PTR>(this));
		SendMessageW(searchEdit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Tìm trong lịch sử..."));
		SendMessageW(searchEdit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
		return 0;
	case WM_SIZE:
		layoutPicker();
		InvalidateRect(window, nullptr, TRUE);
		return 0;
	case WM_GETMINMAXINFO: {
		MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
		info->ptMinTrackSize.x = scaled(window, 380);
		info->ptMinTrackSize.y = scaled(window, 300);
		return 0;
	}
	case WM_DPICHANGED: {
		RECT* proposed = reinterpret_cast<RECT*>(lParam);
		SetWindowPos(window, nullptr, proposed->left, proposed->top,
			proposed->right - proposed->left, proposed->bottom - proposed->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		createPickerFonts();
		return 0;
	}
	case WM_EXITSIZEMOVE:
		if (!suppressGeometrySave_) savePickerLayout();
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT paint = {};
		HDC dc = BeginPaint(window, &paint);
		paintPicker(dc);
		EndPaint(window, &paint);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_CTLCOLOREDIT:
		SetBkColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 255));
		SetTextColor(reinterpret_cast<HDC>(wParam), RGB(32, 32, 32));
		return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
	case WM_COMMAND:
		if (LOWORD(wParam) == kSearchEditId && HIWORD(wParam) == EN_CHANGE) {
			wchar_t buffer[2048] = {};
			GetWindowTextW(searchEdit_, buffer, ARRAYSIZE(buffer));
			query_ = buffer;
			selectedIndex_ = 0;
			scrollRow_ = 0;
			rebuildFilter();
			InvalidateRect(window, nullptr, TRUE);
			return 0;
		}
		break;
	case WM_MOUSEMOVE: {
		TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, window, 0 };
		TrackMouseEvent(&track);
		int hit = hitTestRow(GET_Y_LPARAM(lParam));
		if (hit != hoveredIndex_) {
			hoveredIndex_ = hit;
			InvalidateRect(window, nullptr, FALSE);
		}
		return 0;
	}
	case WM_MOUSELEAVE:
		hoveredIndex_ = -1;
		InvalidateRect(window, nullptr, FALSE);
		return 0;
	case WM_MOUSEWHEEL: {
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		int visibleRows = std::max(1, (listBottom_ - listTop_) / std::max(1, rowHeight_));
		int maxScroll = std::max(0, static_cast<int>(filteredItems_.size()) - visibleRows);
		scrollRow_ = std::max(0, std::min(maxScroll, scrollRow_ - (delta / WHEEL_DELTA) * 3));
		InvalidateRect(window, nullptr, FALSE);
		return 0;
	}
	case WM_LBUTTONDOWN: {
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);
		if (pointInRect(closeButtonRect_, x, y)) {
			closePicker();
			return 0;
		}
		if (pointInRect(clearButtonRect_, x, y)) {
			pasting_ = true;
			int answer = MessageBoxW(window, L"Xóa toàn bộ lịch sử Clipboard?", L"NiceKey", MB_ICONQUESTION | MB_YESNO);
			pasting_ = false;
			if (answer == IDYES) clear();
			return 0;
		}
		if (pointInRect(pinButtonRect_, x, y)) {
			togglePinOnTop();
			return 0;
		}
		if (pointInRect(autoHideButtonRect_, x, y)) {
			toggleAutoHide();
			return 0;
		}
		int filteredIndex = hitTestRow(y);
		if (filteredIndex >= 0 && filteredIndex < static_cast<int>(filteredItems_.size())) {
			RECT client = {};
			GetClientRect(window, &client);
			if (x >= client.right - scaled(window, 44)) removeItem(filteredItems_[filteredIndex]);
			else pickFilteredItem(filteredIndex);
			return 0;
		}
		if (y < scaled(window, 52)) {
			ReleaseCapture();
			SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		}
		return 0;
	}
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE && autoHide_ && !pasting_) closePicker(false);
		return 0;
	case WM_CLOSE:
		closePicker();
		return 0;
	case WM_DESTROY:
		if (searchEdit_) RemoveWindowSubclass(searchEdit_, SearchEditProc, 1);
		searchEdit_ = nullptr;
		pickerWindow_ = nullptr;
		NiceKeySetEngineSuspended(false);
		destroyPickerFonts();
		return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK ClipboardHistory::SearchEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR referenceData) {
	UNREFERENCED_PARAMETER(subclassId);
	UNREFERENCED_PARAMETER(referenceData);
	if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
	return DefSubclassProc(window, message, wParam, lParam);
}

bool ClipboardHistory::preTranslateMessage(MSG& message) {
	bool messageBelongsToPicker = pickerWindow_ &&
		(message.hwnd == pickerWindow_ || IsChild(pickerWindow_, message.hwnd));
	if (messageBelongsToPicker && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN)) {
		UINT key = static_cast<UINT>(message.wParam);
		if (key == VK_DOWN) { moveSelection(1); return true; }
		if (key == VK_UP) { moveSelection(-1); return true; }
		if (key == VK_RETURN || key == VK_TAB) {
			if (!filteredItems_.empty()) pickFilteredItem(selectedIndex_);
			return true;
		}
		if (key == VK_ESCAPE) {
			if (!query_.empty()) {
				SetWindowTextW(searchEdit_, L"");
			} else closePicker();
			return true;
		}
		if (query_.empty() && key >= L'1' && key <= L'9') {
			int quickIndex = static_cast<int>(key - L'1');
			if (quickIndex < static_cast<int>(filteredItems_.size())) pickFilteredItem(quickIndex);
			return true;
		}
	}

	if (settingsWindow_ && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
		GetFocus() == GetDlgItem(settingsWindow_, IDC_CLIPBOARD_HOTKEY_KEY)) {
		handleSettingsKey(static_cast<UINT>(message.wParam));
		return true;
	}

	if (settingsWindow_ && IsDialogMessageW(settingsWindow_, &message)) return true;
	return false;
}

void ClipboardHistory::createPickerFonts() {
	destroyPickerFonts();
	int dpi = pickerWindow_ ? static_cast<int>(GetDpiForWindow(pickerWindow_)) : 96;
	auto fontHeight = [dpi](int points) { return -MulDiv(points, dpi, 72); };
	titleFont_ = CreateFontW(fontHeight(12), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
	bodyFont_ = CreateFontW(fontHeight(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
	bodyBoldFont_ = CreateFontW(fontHeight(11), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
	captionFont_ = CreateFontW(fontHeight(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
	iconFont_ = CreateFontW(fontHeight(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
	if (searchEdit_) SendMessageW(searchEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
}

void ClipboardHistory::destroyPickerFonts() {
	HFONT* fonts[] = { &titleFont_, &bodyFont_, &bodyBoldFont_, &captionFont_, &iconFont_ };
	for (HFONT* font : fonts) {
		if (*font) DeleteObject(*font);
		*font = nullptr;
	}
}

void ClipboardHistory::layoutPicker() {
	if (!pickerWindow_) return;
	RECT client = {};
	GetClientRect(pickerWindow_, &client);
	int headerHeight = scaled(pickerWindow_, 52);
	int searchHeight = items_.empty() ? 0 : scaled(pickerWindow_, 54);
	int footerHeight = scaled(pickerWindow_, 38);
	rowHeight_ = scaled(pickerWindow_, 68);
	listTop_ = headerHeight + searchHeight;
	listBottom_ = std::max(listTop_, static_cast<int>(client.bottom) - footerHeight);
	if (searchEdit_) {
		if (items_.empty()) ShowWindow(searchEdit_, SW_HIDE);
		else {
			SetWindowPos(searchEdit_, nullptr, scaled(pickerWindow_, 16), headerHeight + scaled(pickerWindow_, 10),
				std::max(80, static_cast<int>(client.right) - scaled(pickerWindow_, 32)), scaled(pickerWindow_, 32),
				SWP_NOZORDER | SWP_SHOWWINDOW);
		}
	}
	ensureSelectionVisible();
}

void ClipboardHistory::paintPicker(HDC dc) {
	RECT client = {};
	GetClientRect(pickerWindow_, &client);
	COLORREF windowColor = RGB(246, 247, 249);
	COLORREF headerColor = RGB(237, 239, 242);
	COLORREF separator = RGB(210, 213, 218);
	fillSolidRect(dc, client, windowColor);

	RECT header = { 0, 0, client.right, scaled(pickerWindow_, 52) };
	fillSolidRect(dc, header, headerColor);
	SetBkMode(dc, TRANSPARENT);

	RECT iconRect = { scaled(pickerWindow_, 16), scaled(pickerWindow_, 13), scaled(pickerWindow_, 44), scaled(pickerWindow_, 42) };
	SelectObject(dc, iconFont_);
	SetTextColor(dc, RGB(0, 120, 215));
	DrawTextW(dc, L"\xE8C8", -1, &iconRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	bool showCount = !items_.empty() && client.right >= scaled(pickerWindow_, 460);
	int controlsBoundary = static_cast<int>(client.right) - scaled(pickerWindow_, items_.empty() ? 120 : 202);
	int titleRight = controlsBoundary - (showCount ? scaled(pickerWindow_, 58) : 0);
	RECT titleRect = { scaled(pickerWindow_, 48), 0, std::max(scaled(pickerWindow_, 120), titleRight), header.bottom };
	SelectObject(dc, titleFont_);
	SetTextColor(dc, RGB(27, 27, 27));
	DrawTextW(dc, L"Lịch sử Clipboard", -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	int buttonSize = scaled(pickerWindow_, 32);
	int right = client.right - scaled(pickerWindow_, 8);
	closeButtonRect_ = { right - buttonSize, scaled(pickerWindow_, 10), right, scaled(pickerWindow_, 42) };
	right -= buttonSize + scaled(pickerWindow_, 4);
	autoHideButtonRect_ = { right - buttonSize, scaled(pickerWindow_, 10), right, scaled(pickerWindow_, 42) };
	right -= buttonSize + scaled(pickerWindow_, 4);
	pinButtonRect_ = { right - buttonSize, scaled(pickerWindow_, 10), right, scaled(pickerWindow_, 42) };
	right -= buttonSize + scaled(pickerWindow_, 8);

	if (!items_.empty()) {
		int clearWidth = scaled(pickerWindow_, 72);
		clearButtonRect_ = { right - clearWidth, scaled(pickerWindow_, 10), right, scaled(pickerWindow_, 42) };
		right -= clearWidth + scaled(pickerWindow_, 10);
		SelectObject(dc, captionFont_);
		SetTextColor(dc, RGB(100, 100, 104));
		DrawTextW(dc, L"Xóa tất cả", -1, &clearButtonRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	} else SetRectEmpty(&clearButtonRect_);

	SelectObject(dc, iconFont_);
	SetTextColor(dc, pinOnTop_ ? RGB(0, 120, 215) : RGB(100, 100, 104));
	DrawTextW(dc, L"\xE718", -1, &pinButtonRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	SetTextColor(dc, autoHide_ ? RGB(0, 120, 215) : RGB(100, 100, 104));
	DrawTextW(dc, autoHide_ ? L"\xED1A" : L"\xE890", -1, &autoHideButtonRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	SetTextColor(dc, RGB(95, 95, 99));
	DrawTextW(dc, L"\xE8BB", -1, &closeButtonRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (showCount) {
		wchar_t countText[32] = {};
		wsprintfW(countText, L"%d mục", static_cast<int>(items_.size()));
		RECT countRect = { titleRect.right + scaled(pickerWindow_, 4), 0, right, header.bottom };
		SelectObject(dc, captionFont_);
		SetTextColor(dc, RGB(105, 105, 108));
		DrawTextW(dc, countText, -1, &countRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	HPEN separatorPen = CreatePen(PS_SOLID, 1, separator);
	HGDIOBJ oldPen = SelectObject(dc, separatorPen);
	MoveToEx(dc, 0, header.bottom, nullptr);
	LineTo(dc, client.right, header.bottom);
	if (!items_.empty()) {
		MoveToEx(dc, 0, listTop_, nullptr);
		LineTo(dc, client.right, listTop_);
	}
	MoveToEx(dc, 0, listBottom_, nullptr);
	LineTo(dc, client.right, listBottom_);
	SelectObject(dc, oldPen);
	DeleteObject(separatorPen);

	if (items_.empty()) {
		RECT emptyTitle = { scaled(pickerWindow_, 30), listTop_ + scaled(pickerWindow_, 55), client.right - scaled(pickerWindow_, 30), listTop_ + scaled(pickerWindow_, 90) };
		SelectObject(dc, titleFont_);
		SetTextColor(dc, RGB(100, 100, 104));
		DrawTextW(dc, L"Chưa có gì trong Clipboard", -1, &emptyTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		RECT emptySubtitle = { emptyTitle.left, emptyTitle.bottom, emptyTitle.right, emptyTitle.bottom + scaled(pickerWindow_, 55) };
		SelectObject(dc, bodyFont_);
		SetTextColor(dc, RGB(135, 135, 139));
		DrawTextW(dc, L"Sao chép văn bản hoặc hình ảnh để bắt đầu lưu lịch sử.", -1, &emptySubtitle,
			DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
	} else if (filteredItems_.empty()) {
		RECT empty = { scaled(pickerWindow_, 30), listTop_ + scaled(pickerWindow_, 45), client.right - scaled(pickerWindow_, 30), listBottom_ };
		SelectObject(dc, titleFont_);
		SetTextColor(dc, RGB(100, 100, 104));
		DrawTextW(dc, L"Không tìm thấy mục phù hợp", -1, &empty, DT_CENTER | DT_TOP | DT_NOPREFIX);
	} else {
		int visibleRows = std::max(1, (listBottom_ - listTop_) / std::max(1, rowHeight_));
		int end = std::min(static_cast<int>(filteredItems_.size()), scrollRow_ + visibleRows + 1);
		for (int filteredIndex = scrollRow_; filteredIndex < end; ++filteredIndex) {
			int rowTop = listTop_ + (filteredIndex - scrollRow_) * rowHeight_ + scaled(pickerWindow_, 3);
			RECT row = { scaled(pickerWindow_, 8), rowTop, client.right - scaled(pickerWindow_, 8),
				std::min(listBottom_ - scaled(pickerWindow_, 3), rowTop + rowHeight_ - scaled(pickerWindow_, 4)) };
			paintRow(dc, row, filteredIndex, filteredItems_[filteredIndex], filteredIndex == selectedIndex_, filteredIndex == hoveredIndex_);
		}
	}

	RECT footer = { scaled(pickerWindow_, 14), listBottom_, client.right - scaled(pickerWindow_, 14), client.bottom };
	SelectObject(dc, captionFont_);
	SetTextColor(dc, RGB(105, 105, 108));
	DrawTextW(dc, query_.empty() ? L"↑↓  Chọn     Tab/Enter  Dán     1-9  Chọn nhanh     Esc  Đóng"
		: L"↑↓  Chọn     Tab/Enter  Dán     Esc  Xóa tìm kiếm / Đóng",
		-1, &footer, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void ClipboardHistory::paintRow(HDC dc, const RECT& rowRect, int filteredIndex, size_t itemIndex, bool selected, bool hovered) {
	if (itemIndex >= items_.size()) return;
	const NiceKeyClipItem& item = items_[itemIndex];
	COLORREF blue = RGB(0, 120, 215);
	if (selected) drawRoundRect(dc, rowRect, scaled(pickerWindow_, 7), blue);
	else if (hovered) drawRoundRect(dc, rowRect, scaled(pickerWindow_, 7), RGB(231, 233, 237));

	SetBkMode(dc, TRANSPARENT);
	COLORREF primary = selected ? RGB(255, 255, 255) : RGB(28, 28, 30);
	COLORREF secondary = selected ? RGB(220, 237, 255) : RGB(105, 105, 108);
	int x = rowRect.left + scaled(pickerWindow_, 8);
	RECT numberRect = { x, rowRect.top, x + scaled(pickerWindow_, 24), rowRect.bottom };
	wchar_t number[16] = {};
	wsprintfW(number, L"%d", filteredIndex + 1);
	SelectObject(dc, captionFont_);
	SetTextColor(dc, secondary);
	DrawTextW(dc, number, -1, &numberRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	x = numberRect.right + scaled(pickerWindow_, 10);

	if (item.type == NiceKeyClipItem::Type::Image) {
		RECT thumbnail = { x, rowRect.top + scaled(pickerWindow_, 8), x + scaled(pickerWindow_, 56), rowRect.bottom - scaled(pickerWindow_, 8) };
		paintImageThumbnail(dc, thumbnail, item);
		x = thumbnail.right + scaled(pickerWindow_, 10);
	}

	int rightPadding = hovered ? scaled(pickerWindow_, 42) : scaled(pickerWindow_, 14);
	RECT textRect = { x, rowRect.top + scaled(pickerWindow_, 7), rowRect.right - rightPadding, rowRect.top + scaled(pickerWindow_, 43) };
	SelectObject(dc, bodyFont_);
	SetTextColor(dc, primary);
	std::wstring display = cleanDisplayText(item.text);
	DrawTextW(dc, display.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);

	std::wstring subtitle = item.sourceApp.empty() ? L"Khác" : item.sourceApp;
	subtitle += L" • ";
	subtitle += relativeTime(item.capturedAt);
	RECT subtitleRect = { x, rowRect.bottom - scaled(pickerWindow_, 23), rowRect.right - rightPadding, rowRect.bottom - scaled(pickerWindow_, 5) };
	SelectObject(dc, captionFont_);
	SetTextColor(dc, secondary);
	DrawTextW(dc, subtitle.c_str(), -1, &subtitleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

	if (hovered) {
		RECT removeRect = { rowRect.right - scaled(pickerWindow_, 35), rowRect.top, rowRect.right - scaled(pickerWindow_, 5), rowRect.bottom };
		SelectObject(dc, iconFont_);
		SetTextColor(dc, selected ? RGB(255, 255, 255) : RGB(118, 118, 122));
		DrawTextW(dc, L"\xE711", -1, &removeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}
}

bool ClipboardHistory::paintImageThumbnail(HDC dc, const RECT& destination, const NiceKeyClipItem& item) const {
	std::vector<BYTE> data;
	if (!readBinaryFile(imagePath(item), data) || data.size() < sizeof(BITMAPINFOHEADER)) return false;
	const BITMAPINFOHEADER* header = reinterpret_cast<const BITMAPINFOHEADER*>(data.data());
	size_t pixelOffset = dibPixelOffset(header, data.size());
	if (pixelOffset == 0) return false;
	fillSolidRect(dc, destination, RGB(255, 255, 255));
	int sourceWidth = abs(header->biWidth);
	int sourceHeight = abs(header->biHeight);
	int availableWidth = destination.right - destination.left;
	int availableHeight = destination.bottom - destination.top;
	double ratio = std::min(static_cast<double>(availableWidth) / sourceWidth,
		static_cast<double>(availableHeight) / sourceHeight);
	int width = std::max(1, static_cast<int>(sourceWidth * ratio));
	int height = std::max(1, static_cast<int>(sourceHeight * ratio));
	int x = destination.left + (availableWidth - width) / 2;
	int y = destination.top + (availableHeight - height) / 2;
	SetStretchBltMode(dc, HALFTONE);
	return StretchDIBits(dc, x, y, width, height, 0, 0, sourceWidth, sourceHeight,
		data.data() + pixelOffset, reinterpret_cast<const BITMAPINFO*>(data.data()), DIB_RGB_COLORS, SRCCOPY) != GDI_ERROR;
}

void ClipboardHistory::rebuildFilter() {
	filteredItems_.clear();
	std::wstring foldedQuery = foldForSearch(query_);
	size_t first = foldedQuery.find_first_not_of(L" \t\r\n");
	size_t last = foldedQuery.find_last_not_of(L" \t\r\n");
	if (first == std::wstring::npos) foldedQuery.clear();
	else foldedQuery = foldedQuery.substr(first, last - first + 1);
	for (size_t index = 0; index < items_.size(); ++index) {
		if (foldedQuery.empty() || foldForSearch(items_[index].text).find(foldedQuery) != std::wstring::npos) {
			filteredItems_.push_back(index);
		}
	}
	selectedIndex_ = std::min(selectedIndex_, std::max(0, static_cast<int>(filteredItems_.size()) - 1));
	ensureSelectionVisible();
}

void ClipboardHistory::ensureSelectionVisible() {
	int visibleRows = std::max(1, (listBottom_ - listTop_) / std::max(1, rowHeight_));
	if (selectedIndex_ < scrollRow_) scrollRow_ = selectedIndex_;
	if (selectedIndex_ >= scrollRow_ + visibleRows) scrollRow_ = selectedIndex_ - visibleRows + 1;
	int maxScroll = std::max(0, static_cast<int>(filteredItems_.size()) - visibleRows);
	scrollRow_ = std::max(0, std::min(scrollRow_, maxScroll));
}

int ClipboardHistory::hitTestRow(int y) const {
	if (y < listTop_ || y >= listBottom_ || rowHeight_ <= 0) return -1;
	int index = scrollRow_ + (y - listTop_) / rowHeight_;
	return index < static_cast<int>(filteredItems_.size()) ? index : -1;
}

void ClipboardHistory::moveSelection(int delta) {
	if (filteredItems_.empty()) return;
	selectedIndex_ = std::max(0, std::min(static_cast<int>(filteredItems_.size()) - 1, selectedIndex_ + delta));
	ensureSelectionVisible();
	InvalidateRect(pickerWindow_, nullptr, FALSE);
}

void ClipboardHistory::pickFilteredItem(int filteredIndex) {
	if (filteredIndex < 0 || filteredIndex >= static_cast<int>(filteredItems_.size())) return;
	beginPaste(filteredItems_[filteredIndex]);
}

void ClipboardHistory::beginPaste(size_t itemIndex) {
	if (itemIndex >= items_.size()) return;
	NiceKeyClipItem item = items_[itemIndex];
	if (!placeItemOnClipboard(item)) {
		MessageBeep(MB_ICONERROR);
		return;
	}
	HWND target = previousForegroundWindow_;
	promoteItem(itemIndex);
	pasting_ = true;
	closePicker(false);
	pasting_ = false;
	pendingPasteWindow_ = target;
	if (target && IsWindow(target)) SetForegroundWindow(target);
	SetTimer(messageWindow_, kPasteTimer, 80, nullptr);
}

bool ClipboardHistory::placeItemOnClipboard(const NiceKeyClipItem& item) {
	if (!OpenClipboard(messageWindow_)) return false;
	EmptyClipboard();
	bool success = false;
	if (item.type == NiceKeyClipItem::Type::Text) {
		size_t bytes = (item.text.size() + 1) * sizeof(wchar_t);
		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (memory) {
			void* destination = GlobalLock(memory);
			if (destination) {
				memcpy(destination, item.text.c_str(), bytes);
				GlobalUnlock(memory);
				success = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
			}
			if (!success) GlobalFree(memory);
		}
	} else {
		std::vector<BYTE> data;
		if (readBinaryFile(imagePath(item), data)) {
			HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, data.size());
			if (memory) {
				void* destination = GlobalLock(memory);
				if (destination) {
					memcpy(destination, data.data(), data.size());
					GlobalUnlock(memory);
					success = SetClipboardData(item.clipboardFormat, memory) != nullptr;
				}
				if (!success) GlobalFree(memory);
			}
		}
	}
	if (success && internalClipboardFormat_) {
		HGLOBAL marker = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
		if (marker && !SetClipboardData(internalClipboardFormat_, marker)) GlobalFree(marker);
	}
	ignoreNextClipboardChange_ = success;
	CloseClipboard();
	return success;
}

void ClipboardHistory::sendPasteShortcut() {
	if (pendingPasteWindow_ && IsWindow(pendingPasteWindow_)) SetForegroundWindow(pendingPasteWindow_);
	INPUT input[4] = {};
	for (INPUT& event : input) {
		event.type = INPUT_KEYBOARD;
		event.ki.dwExtraInfo = 0x4E4B;
	}
	input[0].ki.wVk = VK_CONTROL;
	input[1].ki.wVk = L'V';
	input[2].ki.wVk = L'V';
	input[2].ki.dwFlags = KEYEVENTF_KEYUP;
	input[3].ki.wVk = VK_CONTROL;
	input[3].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(ARRAYSIZE(input), input, sizeof(INPUT));
	pendingPasteWindow_ = nullptr;
}

void ClipboardHistory::savePickerLayout() const {
	if (!pickerWindow_ || suppressGeometrySave_) return;
	RECT rect = {};
	if (!GetWindowRect(pickerWindow_, &rect)) return;
	OpenKeyHelper::setRegInt(L"clipboardPickerX", rect.left);
	OpenKeyHelper::setRegInt(L"clipboardPickerY", rect.top);
	OpenKeyHelper::setRegInt(L"clipboardPickerWidth", rect.right - rect.left);
	OpenKeyHelper::setRegInt(L"clipboardPickerHeight", rect.bottom - rect.top);
}

void ClipboardHistory::resetPickerLayout() {
	OpenKeyHelper::setRegInt(L"clipboardPickerX", CW_USEDEFAULT);
	OpenKeyHelper::setRegInt(L"clipboardPickerY", CW_USEDEFAULT);
	OpenKeyHelper::setRegInt(L"clipboardPickerWidth", scaled(nullptr, 480));
	OpenKeyHelper::setRegInt(L"clipboardPickerHeight", scaled(nullptr, 640));
	if (pickerWindow_) {
		RECT workArea = {};
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
		int width = scaled(pickerWindow_, 480);
		int height = scaled(pickerWindow_, 640);
		int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
		int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
		suppressGeometrySave_ = true;
		SetWindowPos(pickerWindow_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		suppressGeometrySave_ = false;
	}
}

void ClipboardHistory::togglePinOnTop() { setPinOnTop(!pinOnTop_); }
void ClipboardHistory::toggleAutoHide() { setAutoHide(!autoHide_); }

std::wstring ClipboardHistory::sourceApplicationName() {
	HWND owner = GetClipboardOwner();
	if (!owner) owner = GetForegroundWindow();
	DWORD processId = 0;
	GetWindowThreadProcessId(owner, &processId);
	if (!processId) return L"Khác";
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!process) return L"Khác";
	wchar_t path[1024] = {};
	DWORD length = ARRAYSIZE(path);
	bool ok = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
	CloseHandle(process);
	if (!ok) return L"Khác";
	const wchar_t* slash = wcsrchr(path, L'\\');
	return slash ? slash + 1 : path;
}

uint64_t ClipboardHistory::currentFileTime() {
	FILETIME fileTime = {};
	GetSystemTimeAsFileTime(&fileTime);
	ULARGE_INTEGER value = {};
	value.LowPart = fileTime.dwLowDateTime;
	value.HighPart = fileTime.dwHighDateTime;
	return value.QuadPart;
}

std::wstring ClipboardHistory::relativeTime(uint64_t capturedAt) {
	uint64_t now = currentFileTime();
	uint64_t seconds = now > capturedAt ? (now - capturedAt) / 10000000ULL : 0;
	wchar_t buffer[64] = {};
	if (seconds < 5) return L"Vừa xong";
	if (seconds < 60) { wsprintfW(buffer, L"%I64u giây trước", seconds); return buffer; }
	uint64_t minutes = seconds / 60;
	if (minutes < 60) { wsprintfW(buffer, L"%I64u phút trước", minutes); return buffer; }
	uint64_t hours = minutes / 60;
	if (hours < 24) { wsprintfW(buffer, L"%I64u giờ trước", hours); return buffer; }
	uint64_t days = hours / 24;
	if (days < 7) { wsprintfW(buffer, L"%I64u ngày trước", days); return buffer; }
	wsprintfW(buffer, L"%I64u tuần trước", days / 7);
	return buffer;
}

std::wstring ClipboardHistory::foldForSearch(const std::wstring& value) {
	if (value.empty()) return L"";
	int required = FoldStringW(MAP_COMPOSITE, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
	std::wstring decomposed;
	if (required > 0) {
		decomposed.resize(required);
		FoldStringW(MAP_COMPOSITE, value.c_str(), static_cast<int>(value.size()), &decomposed[0], required);
	} else decomposed = value;
	std::wstring result;
	result.reserve(decomposed.size());
	for (wchar_t character : decomposed) {
		if (isCombiningMark(character)) continue;
		if (character == L'đ' || character == L'Đ') character = L'd';
		result.push_back(static_cast<wchar_t>(towlower(character)));
	}
	return result;
}

std::wstring ClipboardHistory::cleanDisplayText(const std::wstring& value) {
	std::wstring result;
	result.reserve(value.size());
	bool previousSpace = false;
	for (wchar_t character : value) {
		bool whitespace = character == L'\r' || character == L'\n' || character == L'\t';
		if (whitespace) character = L' ';
		if (character == L' ') {
			if (previousSpace) continue;
			previousSpace = true;
		} else previousSpace = false;
		result.push_back(character);
	}
	size_t first = result.find_first_not_of(L' ');
	if (first == std::wstring::npos) return L"";
	size_t last = result.find_last_not_of(L' ');
	return result.substr(first, last - first + 1);
}

int ClipboardHistory::scaled(HWND window, int value) {
	UINT dpi = window ? GetDpiForWindow(window) : GetDpiForSystem();
	return MulDiv(value, dpi ? dpi : 96, 96);
}

void ClipboardHistory::showSettings(HWND parent) {
	if (settingsWindow_) {
		SetForegroundWindow(settingsWindow_);
		return;
	}
	settingsWindow_ = CreateDialogParamW(instance_, MAKEINTRESOURCEW(IDD_DIALOG_CLIPBOARD_SETTINGS), parent,
		SettingsDialogProc, reinterpret_cast<LPARAM>(this));
	if (settingsWindow_) ShowWindow(settingsWindow_, SW_SHOWNORMAL);
}

INT_PTR CALLBACK ClipboardHistory::SettingsDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	ClipboardHistory* self = reinterpret_cast<ClipboardHistory*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_INITDIALOG) {
		self = reinterpret_cast<ClipboardHistory*>(lParam);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	return self ? self->handleSettingsDialog(window, message, wParam, lParam) : FALSE;
}

INT_PTR ClipboardHistory::handleSettingsDialog(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	switch (message) {
	case WM_INITDIALOG: {
		settingsWindow_ = window;
		HICON icon = LoadIcon(instance_, MAKEINTRESOURCE(IDI_APP_ICON));
		SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
		SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
		HWND keyEdit = GetDlgItem(window, IDC_CLIPBOARD_HOTKEY_KEY);
		SetWindowSubclass(keyEdit, SettingsKeyEditProc, 1, reinterpret_cast<DWORD_PTR>(this));
		updateSettingsDialog();
		return TRUE;
	}
	case WM_COMMAND: {
		int id = LOWORD(wParam);
		if (id == IDOK || id == IDCANCEL) {
			DestroyWindow(window);
			return TRUE;
		}
		if (HIWORD(wParam) != BN_CLICKED) return FALSE;
		switch (id) {
		case IDC_CLIPBOARD_ENABLED:
			setEnabled(IsDlgButtonChecked(window, IDC_CLIPBOARD_ENABLED) == BST_CHECKED);
			return TRUE;
		case IDC_CLIPBOARD_PIN_ON_TOP:
			setPinOnTop(IsDlgButtonChecked(window, IDC_CLIPBOARD_PIN_ON_TOP) == BST_CHECKED);
			return TRUE;
		case IDC_CLIPBOARD_AUTO_HIDE:
			setAutoHide(IsDlgButtonChecked(window, IDC_CLIPBOARD_AUTO_HIDE) == BST_CHECKED);
			return TRUE;
		case IDC_CLIPBOARD_HOTKEY_CTRL:
		case IDC_CLIPBOARD_HOTKEY_ALT:
		case IDC_CLIPBOARD_HOTKEY_WIN:
		case IDC_CLIPBOARD_HOTKEY_SHIFT:
			updateSettingsHotKey();
			return TRUE;
		case IDC_CLIPBOARD_OPEN_PICKER:
			togglePicker();
			return TRUE;
		case IDC_CLIPBOARD_CLEAR:
			if (MessageBoxW(window, L"Xóa toàn bộ lịch sử Clipboard?", L"NiceKey", MB_ICONQUESTION | MB_YESNO) == IDYES) clear();
			return TRUE;
		case IDC_CLIPBOARD_RESET_LAYOUT:
			resetPickerLayout();
			return TRUE;
		case IDC_CLIPBOARD_RESET_HOTKEY:
			setHotKey(kDefaultHotKey);
			return TRUE;
		}
		break;
	}
	case WM_CLOSE:
		DestroyWindow(window);
		return TRUE;
	case WM_DESTROY:
		RemoveWindowSubclass(GetDlgItem(window, IDC_CLIPBOARD_HOTKEY_KEY), SettingsKeyEditProc, 1);
		settingsWindow_ = nullptr;
		return TRUE;
	}
	return FALSE;
}

LRESULT CALLBACK ClipboardHistory::SettingsKeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclassId, DWORD_PTR referenceData) {
	UNREFERENCED_PARAMETER(subclassId);
	UNREFERENCED_PARAMETER(referenceData);
	if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
	return DefSubclassProc(window, message, wParam, lParam);
}

void ClipboardHistory::updateSettingsDialog() {
	if (!settingsWindow_) return;
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_ENABLED, enabled_ ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_PIN_ON_TOP, pinOnTop_ ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_AUTO_HIDE, autoHide_ ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_HOTKEY_CTRL, hotKey_ & 0x100 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_HOTKEY_ALT, hotKey_ & 0x200 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_HOTKEY_WIN, hotKey_ & 0x400 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(settingsWindow_, IDC_CLIPBOARD_HOTKEY_SHIFT, hotKey_ & 0x800 ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemTextW(settingsWindow_, IDC_CLIPBOARD_HOTKEY_KEY, keyDescription(hotKey_ & 0xFF).c_str());
	std::wstring description = hotKeyDescription();
	UINT key = static_cast<UINT>(hotKey_ & 0xFF);
	bool hasKey = key != 0 && key != 0xFE;
	bool hasModifier = (hotKey_ & 0xF00) != 0;
	if (enabled_ && hasKey && !hasModifier) description += L" (cần phím bổ trợ)";
	else if (enabled_ && hasKey && hasModifier && !hotKeyRegistered_) description += L" (đang bị ứng dụng khác sử dụng)";
	SetDlgItemTextW(settingsWindow_, IDC_CLIPBOARD_HOTKEY_PREVIEW, description.c_str());
	wchar_t count[64] = {};
	wsprintfW(count, L"Đang lưu %d/%d mục", static_cast<int>(items_.size()), kMaxItems);
	SetDlgItemTextW(settingsWindow_, IDC_CLIPBOARD_ITEM_COUNT, count);
	EnableWindow(GetDlgItem(settingsWindow_, IDC_CLIPBOARD_CLEAR), !items_.empty());
	int controls[] = { IDC_CLIPBOARD_PIN_ON_TOP, IDC_CLIPBOARD_AUTO_HIDE, IDC_CLIPBOARD_HOTKEY_CTRL,
		IDC_CLIPBOARD_HOTKEY_ALT, IDC_CLIPBOARD_HOTKEY_WIN, IDC_CLIPBOARD_HOTKEY_SHIFT,
		IDC_CLIPBOARD_HOTKEY_KEY, IDC_CLIPBOARD_RESET_HOTKEY, IDC_CLIPBOARD_OPEN_PICKER };
	for (int control : controls) EnableWindow(GetDlgItem(settingsWindow_, control), enabled_);
}

void ClipboardHistory::updateSettingsHotKey() {
	if (!settingsWindow_) return;
	int value = hotKey_ & ~0xF00;
	if (IsDlgButtonChecked(settingsWindow_, IDC_CLIPBOARD_HOTKEY_CTRL) == BST_CHECKED) value |= 0x100;
	if (IsDlgButtonChecked(settingsWindow_, IDC_CLIPBOARD_HOTKEY_ALT) == BST_CHECKED) value |= 0x200;
	if (IsDlgButtonChecked(settingsWindow_, IDC_CLIPBOARD_HOTKEY_WIN) == BST_CHECKED) value |= 0x400;
	if (IsDlgButtonChecked(settingsWindow_, IDC_CLIPBOARD_HOTKEY_SHIFT) == BST_CHECKED) value |= 0x800;
	setHotKey(value);
}

void ClipboardHistory::handleSettingsKey(UINT virtualKey) {
	if (virtualKey == VK_SHIFT || virtualKey == VK_CONTROL || virtualKey == VK_MENU ||
		virtualKey == VK_LWIN || virtualKey == VK_RWIN) return;
	UINT key = virtualKey;
	if (key == VK_DELETE || key == VK_BACK) key = 0xFE;
	bool accepted = key == 0xFE || key == VK_SPACE ||
		(key >= L'A' && key <= L'Z') || (key >= L'0' && key <= L'9') ||
		(key >= VK_F1 && key <= VK_F24);
	if (!accepted) {
		MessageBeep(MB_ICONWARNING);
		return;
	}
	int value = (hotKey_ & ~0xFF) | key;
	value = (value & 0x00FFFFFF) | ((key & 0xFF) << 24);
	setHotKey(value);
}

bool ClipboardHistory::runSelfTest() {
	std::wstring folded = foldForSearch(L"Không Được");
	if (folded.find(L"khong duoc") == std::wstring::npos) return false;
	if (cleanDisplayText(L"  một\n\nđoạn\t văn bản  ") != L"một đoạn văn bản") return false;
	if (keyDescription(L'V') != L"V") return false;
	ClipboardHistory history;
	if (history.hotKeyDescription() != L"Ctrl + Shift + V") return false;
	return true;
}
