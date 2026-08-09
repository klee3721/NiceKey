/*----------------------------------------------------------
OpenKey - The Cross platform Open source Vietnamese Keyboard application.

Copyright (C) 2019 Mai Vu Tuyen

This file is belong to the OpenKey project, Win32 version
which is released under GPL license.
You can fork, modify, improve this program. If you
redistribute your new version, it MUST be open source.
-----------------------------------------------------------*/
#pragma once
#include "BaseDialog.h"
#include "ManualAppExclusion.h"

class MainControlDialog : public BaseDialog {
private:
	HWND hTab, hTabPage1, hTabPage2, hTabPage3, hTabPage4, hTabPage5;
	HWND comboBoxInputType;
	HWND comboBoxTableCode;
	HWND checkCtrl, checkAlt, checkWin, checkShift, textSwitchKey, checkBeep;
	HWND checkVietnamese, checkEnglish;
	HWND checkModernOrthorgraphy, checkFixRecommendBrowser, checkShowOnStartup, checkRunWithWindows,
		checkSpelling, checkRestoreIfWrongSpelling, checkUseClipboard, checkModernIcon,
		checkAllowZWJF, checkTempOffSpelling, checkQuickStartConsonant, checkQuickEndConsonant;
	HWND checkSmartSwitchKey, checkManualAppExclusion, checkCapsFirstChar, checkQuickTelex, checkUseMacro, checkUseMacroInEnglish;
	HWND checkCreateDesktopShortcut, checkCheckNewVersion, checkRunAsAdmin, checkSupportMetroApp, checkMacroAutoCaps;
	HWND checkFixChromium, checkRememberTableCode, checkTempOffOpenKey, checkAllowOtherLanguages;
	HWND listExcludedApps, listRunningApps;
	HWND buttonAddExcludedApp, buttonRemoveExcludedApp, buttonRefreshRunningApps;
	HWND hUpdateButton;
	vector<wstring> selectedAppItems;
	vector<ManualExcludedAppInfo> allRunningAppItems;
	vector<ManualExcludedAppInfo> runningAppItems;
private:
	void initDialog();
	void onComboBoxSelected(const HWND& hCombobox, const int& comboboxId);
	void onCheckboxClicked(const HWND& hWnd);
	void onCharacter(const HWND& hWnd, const UINT16& keyCode);
	void setSwitchKeyText(const HWND& hWnd, const UINT16 & keyCode);
	void onTabIndexChanged();
	void refreshAppExclusionLists(bool reloadRunningApps);
	void updateAppExclusionButtons();
	void addSelectedRunningApp();
	void removeSelectedExcludedApp();
	void onUpdateButton();
	void requestRestartAsAdmin();
protected:
	INT_PTR eventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK tabPageEventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
	MainControlDialog(const HINSTANCE& hInstance, const int& resourceId);
	virtual ~MainControlDialog();
	virtual void fillData() override;
	void setSwitchKey(const unsigned short& code);

	friend INT_PTR CALLBACK tabPageEventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
