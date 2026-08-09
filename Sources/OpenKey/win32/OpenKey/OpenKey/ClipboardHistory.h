/*----------------------------------------------------------
NiceKey clipboard history for Windows.

This module is part of the NiceKey fork and is released under
the GPL-3.0 license together with the rest of the application.
-----------------------------------------------------------*/
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

struct NiceKeyClipItem {
	enum class Type : uint32_t {
		Text = 0,
		Image = 1,
	};

	uint64_t id;
	Type type;
	uint64_t capturedAt;
	std::wstring text;
	std::wstring sourceApp;
	std::wstring imageFile;
	uint32_t clipboardFormat;
	int32_t imageWidth;
	int32_t imageHeight;
};

class ClipboardHistory {
public:
	static const int kMaxItems = 30;
	static const int kDefaultHotKey = 0x56000956; // Ctrl + Shift + V

	static ClipboardHistory& shared();

	bool initialize(HINSTANCE instance);
	void shutdown();
	bool preTranslateMessage(MSG& message);

	void togglePicker();
	void showPicker();
	void closePicker(bool restorePreviousWindow = true);
	void showSettings(HWND parent = nullptr);

	bool isEnabled() const;
	bool isPickerOpen() const;
	bool isHotKeyRegistered() const;
	int itemCount() const;
	int hotKey() const;
	std::wstring hotKeyDescription() const;

	void setEnabled(bool enabled);
	void setPinOnTop(bool pinOnTop);
	void setAutoHide(bool autoHide);
	void setHotKey(int hotKey);
	void resetSettings();
	void clear();

	bool shouldBypassEngineForHotKey(UINT virtualKey, bool control, bool alt, bool win, bool shift) const;
	static bool runSelfTest();

private:
	ClipboardHistory();
	ClipboardHistory(const ClipboardHistory&) = delete;
	ClipboardHistory& operator=(const ClipboardHistory&) = delete;

	static LRESULT CALLBACK MessageWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK PickerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK SearchEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR subclassId, DWORD_PTR referenceData);
	static LRESULT CALLBACK SettingsKeyEditProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR subclassId, DWORD_PTR referenceData);
	static INT_PTR CALLBACK SettingsDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT handleMessageWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT handlePickerWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR handleSettingsDialog(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	bool registerWindowClasses();
	void loadSettings();
	void updateHotKeyRegistration();
	void updateSettingsDialog();
	void updateSettingsHotKey();
	void handleSettingsKey(UINT virtualKey);

	bool captureClipboard();
	bool isSensitiveClipboard() const;
	void addText(const std::wstring& text, const std::wstring& sourceApp);
	void addImage(UINT format, const BYTE* data, size_t size, const std::wstring& sourceApp);
	void promoteItem(size_t index);
	void removeItem(size_t index);
	void trimItems();

	void loadItems();
	void persistItems() const;
	std::wstring dataDirectory() const;
	std::wstring historyFilePath() const;
	std::wstring imagePath(const NiceKeyClipItem& item) const;
	void deleteImageFile(const NiceKeyClipItem& item) const;

	void createPicker();
	void createPickerFonts();
	void destroyPickerFonts();
	void layoutPicker();
	void paintPicker(HDC dc);
	void paintRow(HDC dc, const RECT& rowRect, int filteredIndex, size_t itemIndex, bool selected, bool hovered);
	bool paintImageThumbnail(HDC dc, const RECT& destination, const NiceKeyClipItem& item) const;
	void rebuildFilter();
	void ensureSelectionVisible();
	int hitTestRow(int y) const;
	void moveSelection(int delta);
	void pickFilteredItem(int filteredIndex);
	void beginPaste(size_t itemIndex);
	bool placeItemOnClipboard(const NiceKeyClipItem& item);
	void sendPasteShortcut();
	void savePickerLayout() const;
	void resetPickerLayout();
	void togglePinOnTop();
	void toggleAutoHide();

	static std::wstring sourceApplicationName();
	static std::wstring relativeTime(uint64_t capturedAt);
	static std::wstring foldForSearch(const std::wstring& value);
	static std::wstring cleanDisplayText(const std::wstring& value);
	static uint64_t currentFileTime();
	static std::wstring keyDescription(UINT virtualKey);
	static int scaled(HWND window, int value);

	HINSTANCE instance_;
	HWND messageWindow_;
	HWND pickerWindow_;
	HWND searchEdit_;
	HWND settingsWindow_;
	HWND previousForegroundWindow_;
	HWND pendingPasteWindow_;

	HFONT titleFont_;
	HFONT bodyFont_;
	HFONT bodyBoldFont_;
	HFONT captionFont_;
	HFONT iconFont_;

	std::vector<NiceKeyClipItem> items_;
	std::vector<size_t> filteredItems_;
	std::wstring query_;

	int selectedIndex_;
	int hoveredIndex_;
	int scrollRow_;
	int rowHeight_;
	int listTop_;
	int listBottom_;
	int hotKey_;
	int captureRetryCount_;

	bool initialized_;
	bool enabled_;
	bool pinOnTop_;
	bool autoHide_;
	bool hotKeyRegistered_;
	bool ignoreNextClipboardChange_;
	bool pasting_;
	bool suppressGeometrySave_;

	UINT internalClipboardFormat_;
	UINT excludeMonitorFormat_;
	UINT includeInHistoryFormat_;

	RECT closeButtonRect_;
	RECT clearButtonRect_;
	RECT pinButtonRect_;
	RECT autoHideButtonRect_;
};

