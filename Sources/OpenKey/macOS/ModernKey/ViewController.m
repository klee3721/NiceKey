//
//  ViewController.m
//  ModernKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "ViewController.h"
#import "OpenKeyManager.h"
#import "AppDelegate.h"
#import "MyTextField.h"

extern AppDelegate* appDelegate;
extern void OnSpellCheckingChanged(void);
extern void RefreshManualAppExclusionState(void);

ViewController* viewController;
extern int vFreeMark;
extern int vCheckSpelling;
extern int vUseModernOrthography;
extern int vSwitchKeyStatus;
extern int vQuickTelex;
extern int vRestoreIfWrongSpelling;
extern int vFixRecommendBrowser;
extern int vUseMacro;
extern int vUseMacroInEnglishMode;
extern int vSendKeyStepByStep;
extern int vUseSmartSwitchKey;
extern int vUseManualAppExclusion;
extern int vUpperCaseFirstChar;
extern int vTempOffSpelling;
extern int vAllowConsonantZFWJ;
extern int vQuickStartConsonant;
extern int vQuickEndConsonant;
extern int vRememberCode;
extern int vOtherLanguage;
extern int vTempOffOpenKey;
extern int vShowIconOnDock;
extern int vAutoCapsMacro;
extern int vFixChromiumBrowser;
extern int vPerformLayoutCompat;

@implementation ViewController {
    __weak IBOutlet NSButton *CustomSwitchCommand;
    __weak IBOutlet NSButton *CustomSwitchOption;
    __weak IBOutlet NSButton *CustomSwitchControl;
    __weak IBOutlet NSButton *CustomSwitchShift;
    __weak IBOutlet MyTextField *CustomSwitchKey;
    __weak IBOutlet NSButton *CustomBeepSound;
    NSArray* tabviews, *tabbuttons;
    NSRect tabViewRect;
    NSView* tabButtonBackground;
    NSButton *manualAppExclusionButton;
    NSButton *tabbuttonExclusions;
    NSBox *tabviewExclusions;
    NSTableView *excludedAppsTable;
    NSTableView *runningAppsTable;
    NSButton *addExcludedAppButton;
    NSButton *removeExcludedAppButton;
    NSButton *refreshRunningAppsButton;
    NSArray<NSDictionary<NSString*, NSString*>*> *excludedAppItems;
    NSArray<NSDictionary<NSString*, NSString*>*> *runningAppItems;
}

static NSString * const ManualAppBundleIdentifierKey = @"bundleIdentifier";
static NSString * const ManualAppDisplayNameKey = @"displayName";

static void SetViewOriginY(NSView *view, CGFloat originY) {
    NSRect frame = view.frame;
    frame.origin.y = originY;
    view.frame = frame;
}

- (void)openURLString:(NSString *)urlString {
    NSURL *url = [NSURL URLWithString:urlString];
    if (url) {
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

- (void)viewDidLoad {
    [super viewDidLoad];
    viewController = self;
    CustomSwitchKey.Parent = self;
    
    self.appOK.hidden = YES;
    self.permissionWarning.hidden = YES;
    self.retryButton.enabled = NO;
 
    NSRect parentRect = self.viewParent.frame;
    parentRect.size.height = 490;
    self.viewParent.frame = parentRect;

    [self configureManualAppExclusionCheckbox];
    tabViewRect = self.tabviewPrimary.frame;
    [self configureAppExclusionTab];
    [self configureTabButtons];

    //set correct tabgroup
    tabviews = @[self.tabviewPrimary, self.tabviewMacro, self.tabviewSystem, tabviewExclusions, self.tabviewInfo];
    tabbuttons = @[self.tabbuttonPrimary, self.tabbuttonMacro, self.tabbuttonSystem, tabbuttonExclusions, self.tabbuttonInfo];
    NSButton* firstTabButton = [tabbuttons objectAtIndex:0];
    NSRect tabButtonBackgroundRect = firstTabButton.frame;
    for (NSButton* button in tabbuttons) {
        tabButtonBackgroundRect = NSUnionRect(tabButtonBackgroundRect, button.frame);
    }
    tabButtonBackgroundRect = NSInsetRect(tabButtonBackgroundRect, -2, -2);
    tabButtonBackground = [[NSView alloc] initWithFrame:tabButtonBackgroundRect];
    [tabButtonBackground setWantsLayer:YES];
    tabButtonBackground.layer.backgroundColor = [[NSColor windowBackgroundColor] CGColor];
    [self.view addSubview:tabButtonBackground];
    for (NSBox* b in tabviews) {
        b.frame = tabViewRect;
    }
    
    [self showTab:0];
    
    NSArray* inputTypeData = [[NSArray alloc] initWithObjects:@"Telex", @"VNI", @"Simple Telex 1", @"Simple Telex 2", nil];
    NSArray* codeData = [OpenKeyManager getTableCodes];
    
    //preset data
    [_popupInputType removeAllItems];
    [_popupInputType addItemsWithTitles:inputTypeData];
    
    [self.popupCode removeAllItems];
    [self.popupCode addItemsWithTitles:codeData];
    
    [self initKey];
    
    [self fillData];
    
    // set version info
    self.VersionInfo.stringValue = [NSString stringWithFormat:@"Phiên bản %@ (build %@) - Ngày cập nhật %@",
    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleShortVersionString"],
    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"],
    [OpenKeyManager getBuildDate]] ;
}

- (void)configureManualAppExclusionCheckbox {
    SetViewOriginY(self.UseModernOrthography, 202);
    SetViewOriginY(self.FixRecommendBrowser, 171);
    SetViewOriginY(self.UpperCaseFirstChar, 140);
    SetViewOriginY(self.AutoRememberSwitchKey, 109);
    SetViewOriginY(self.RememberTableCode, 47);

    SetViewOriginY(self.CheckSpellingButton, 202);
    SetViewOriginY(self.RestoreIfInvalidWord, 171);
    SetViewOriginY(self.AllowZWJF, 140);
    SetViewOriginY(self.TempOffSpellChecking, 109);
    SetViewOriginY(self.TempOffOpenKey, 78);

    manualAppExclusionButton = [[NSButton alloc] initWithFrame:NSMakeRect(18, 78, 230, 18)];
    manualAppExclusionButton.buttonType = NSButtonTypeSwitch;
    manualAppExclusionButton.title = @"Loại trừ ứng dụng thủ công";
    manualAppExclusionButton.font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    manualAppExclusionButton.toolTip = @"Ứng dụng được chọn trong tab Loại trừ sẽ luôn nhận phím gõ tiếng Anh";
    manualAppExclusionButton.target = self;
    manualAppExclusionButton.action = @selector(onManualAppExclusionChanged:);
    [self.tabviewPrimary.contentView addSubview:manualAppExclusionButton];
}

- (NSTextField *)sectionLabelWithTitle:(NSString *)title frame:(NSRect)frame {
    NSTextField *label = [NSTextField labelWithString:title];
    label.frame = frame;
    label.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    return label;
}

- (NSTableView *)applicationTableWithIdentifier:(NSString *)identifier frame:(NSRect)frame
                                      inContent:(NSView *)contentView {
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:frame];
    scrollView.borderType = NSBezelBorder;
    scrollView.hasVerticalScroller = YES;
    scrollView.autohidesScrollers = YES;

    NSTableView *tableView = [[NSTableView alloc] initWithFrame:scrollView.contentView.bounds];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:identifier];
    column.width = NSWidth(scrollView.contentView.bounds);
    column.resizingMask = NSTableColumnAutoresizingMask;
    [tableView addTableColumn:column];
    tableView.headerView = nil;
    tableView.rowHeight = 22;
    tableView.usesAlternatingRowBackgroundColors = YES;
    tableView.allowsMultipleSelection = NO;
    tableView.delegate = self;
    tableView.dataSource = self;
    tableView.target = self;
    tableView.doubleAction = @selector(onApplicationTableDoubleClick:);
    scrollView.documentView = tableView;
    [contentView addSubview:scrollView];
    return tableView;
}

- (NSButton *)iconButtonWithImageName:(NSImageName)imageName
                              toolTip:(NSString *)toolTip
                               action:(SEL)action
                                frame:(NSRect)frame {
    NSButton *button = [NSButton buttonWithImage:[NSImage imageNamed:imageName]
                                         target:self
                                         action:action];
    button.frame = frame;
    button.bezelStyle = NSBezelStyleRounded;
    button.imagePosition = NSImageOnly;
    button.toolTip = toolTip;
    return button;
}

- (void)configureAppExclusionTab {
    tabviewExclusions = [[NSBox alloc] initWithFrame:tabViewRect];
    tabviewExclusions.boxType = NSBoxCustom;
    tabviewExclusions.titlePosition = NSNoTitle;
    tabviewExclusions.cornerRadius = 4;
    tabviewExclusions.borderColor = NSColor.unemphasizedSelectedContentBackgroundColor;
    tabviewExclusions.fillColor = NSColor.controlBackgroundColor;
    [self.view addSubview:tabviewExclusions];

    NSView *contentView = tabviewExclusions.contentView;
    [contentView addSubview:[self sectionLabelWithTitle:@"Ứng dụng đã loại trừ"
                                                  frame:NSMakeRect(18, 190, 260, 18)]];
    excludedAppsTable = [self applicationTableWithIdentifier:@"excludedApplications"
                                                       frame:NSMakeRect(18, 116, 400, 70)
                                                   inContent:contentView];
    removeExcludedAppButton = [self iconButtonWithImageName:NSImageNameRemoveTemplate
                                                      toolTip:@"Bỏ ứng dụng khỏi danh sách loại trừ"
                                                      action:@selector(onRemoveExcludedApp:)
                                                       frame:NSMakeRect(441, 137, 32, 28)];
    [contentView addSubview:removeExcludedAppButton];

    NSBox *separator = [[NSBox alloc] initWithFrame:NSMakeRect(18, 100, 482, 5)];
    separator.boxType = NSBoxSeparator;
    [contentView addSubview:separator];

    [contentView addSubview:[self sectionLabelWithTitle:@"Ứng dụng đang chạy"
                                                  frame:NSMakeRect(18, 77, 260, 18)]];
    runningAppsTable = [self applicationTableWithIdentifier:@"runningApplications"
                                                      frame:NSMakeRect(18, 5, 400, 68)
                                                  inContent:contentView];
    addExcludedAppButton = [self iconButtonWithImageName:NSImageNameAddTemplate
                                                   toolTip:@"Thêm ứng dụng vào danh sách loại trừ"
                                                   action:@selector(onAddExcludedApp:)
                                                    frame:NSMakeRect(441, 25, 32, 28)];
    [contentView addSubview:addExcludedAppButton];
    refreshRunningAppsButton = [self iconButtonWithImageName:NSImageNameRefreshTemplate
                                                      toolTip:@"Làm mới danh sách ứng dụng đang chạy"
                                                       action:@selector(onRefreshRunningApps:)
                                                        frame:NSMakeRect(478, 25, 32, 28)];
    [contentView addSubview:refreshRunningAppsButton];
}

- (void)configureTabButtons {
    tabbuttonExclusions = [[NSButton alloc] initWithFrame:self.tabbuttonInfo.frame];
    NSButtonCell *exclusionCell = [self.tabbuttonInfo.cell copy];
    exclusionCell.title = @"Loại trừ";
    tabbuttonExclusions.cell = exclusionCell;
    tabbuttonExclusions.tag = 3;
    tabbuttonExclusions.target = self;
    tabbuttonExclusions.action = @selector(onTabButton:);
    [self.view addSubview:tabbuttonExclusions];
    self.tabbuttonInfo.tag = 4;

    NSArray<NSButton*> *buttons = @[self.tabbuttonPrimary, self.tabbuttonMacro,
                                    self.tabbuttonSystem, tabbuttonExclusions, self.tabbuttonInfo];
    CGFloat originY = self.tabbuttonPrimary.frame.origin.y;
    for (NSInteger index = 0; index < buttons.count; index++) {
        buttons[index].frame = NSMakeRect(28 + index * 100, originY, 104, 32);
    }
}

- (void)viewDidAppear {
    [super viewDidAppear];
    NSString* str = @"NiceKey %@ - Bộ gõ Tiếng Việt";
    self.view.window.title = [NSString stringWithFormat:str, [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleShortVersionString"]];
}

- (void)viewWillAppear {
    [self initKey];
}

-(void)initKey {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (![OpenKeyManager initEventTap]) {
            //self.permissionWarning.hidden = NO;
            //self.retryButton.enabled = YES;
        } else {
            //self.appOK.hidden = NO;
        }
    });
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

-(void)showTab:(NSInteger)index {
    NSRect tempRect = tabViewRect;
    tempRect.origin.y = 1000;
    for (NSBox* b in tabviews) {
        [b setHidden:YES];
        b.frame = tempRect;
    }
    for (NSButton* b in tabbuttons) {
        [b setState:NSControlStateValueOff];
    }
    NSBox* b = [tabviews objectAtIndex:index];
    [b setHidden:NO];
    b.frame = tabViewRect;
    
    NSButton* button = [tabbuttons objectAtIndex:index];
    [button setState:NSControlStateValueOn];

    if (index == 3) {
        [self refreshAppExclusionLists:YES];
    }

    [self.view addSubview:tabButtonBackground positioned:NSWindowAbove relativeTo:b];
    for (NSButton* tabButton in tabbuttons) {
        [self.view addSubview:tabButton positioned:NSWindowAbove relativeTo:nil];
    }
}

- (IBAction)onTabButton:(NSButton *)sender {
    [self showTab:sender.tag];
}

- (NSString *)applicationLabel:(NSDictionary<NSString*, NSString*> *)application {
    NSString *bundleIdentifier = application[ManualAppBundleIdentifierKey];
    NSString *displayName = application[ManualAppDisplayNameKey];
    if (displayName.length == 0 || [displayName caseInsensitiveCompare:bundleIdentifier] == NSOrderedSame) {
        return bundleIdentifier ?: @"";
    }
    return [NSString stringWithFormat:@"%@ (%@)", displayName, bundleIdentifier];
}

- (void)refreshAppExclusionLists:(BOOL)reloadRunningApplications {
    excludedAppItems = [OpenKeyManager selectedManualExcludedApplications];
    if (reloadRunningApplications || !runningAppItems) {
        NSMutableArray<NSDictionary<NSString*, NSString*>*> *availableApplications = [[NSMutableArray alloc] init];
        for (NSDictionary<NSString*, NSString*> *application in
             [OpenKeyManager runningApplicationsForManualExclusion]) {
            if (![OpenKeyManager isManualExcludedBundleIdentifier:application[ManualAppBundleIdentifierKey]]) {
                [availableApplications addObject:application];
            }
        }
        runningAppItems = availableApplications;
    } else {
        NSMutableArray<NSDictionary<NSString*, NSString*>*> *availableApplications = [runningAppItems mutableCopy];
        NSIndexSet *selectedIndexes = [availableApplications indexesOfObjectsPassingTest:
            ^BOOL(NSDictionary<NSString*, NSString*> *application, NSUInteger index, BOOL *stop) {
                return [OpenKeyManager isManualExcludedBundleIdentifier:application[ManualAppBundleIdentifierKey]];
            }];
        [availableApplications removeObjectsAtIndexes:selectedIndexes];
        runningAppItems = availableApplications;
    }

    [excludedAppsTable reloadData];
    [runningAppsTable reloadData];
    [self updateAppExclusionButtons];
}

- (void)updateAppExclusionButtons {
    addExcludedAppButton.enabled = runningAppsTable.selectedRow >= 0;
    removeExcludedAppButton.enabled = excludedAppsTable.selectedRow >= 0;
}

- (IBAction)onManualAppExclusionChanged:(NSButton *)sender {
    vUseManualAppExclusion = sender.state == NSControlStateValueOn ? 1 : 0;
    [[NSUserDefaults standardUserDefaults] setInteger:vUseManualAppExclusion
                                               forKey:@"UseManualAppExclusion"];
    RefreshManualAppExclusionState();
}

- (IBAction)onAddExcludedApp:(id)sender {
    NSInteger selectedRow = runningAppsTable.selectedRow;
    if (selectedRow < 0 || selectedRow >= (NSInteger)runningAppItems.count) {
        return;
    }

    NSString *bundleIdentifier = runningAppItems[selectedRow][ManualAppBundleIdentifierKey];
    if ([OpenKeyManager addManualExcludedBundleIdentifier:bundleIdentifier]) {
        vUseManualAppExclusion = 1;
        [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"UseManualAppExclusion"];
        manualAppExclusionButton.state = NSControlStateValueOn;
        RefreshManualAppExclusionState();
        [self refreshAppExclusionLists:NO];
    }
}

- (IBAction)onRemoveExcludedApp:(id)sender {
    NSInteger selectedRow = excludedAppsTable.selectedRow;
    if (selectedRow < 0 || selectedRow >= (NSInteger)excludedAppItems.count) {
        return;
    }

    NSString *bundleIdentifier = excludedAppItems[selectedRow][ManualAppBundleIdentifierKey];
    if ([OpenKeyManager removeManualExcludedBundleIdentifier:bundleIdentifier]) {
        [self refreshAppExclusionLists:YES];
        RefreshManualAppExclusionState();
    }
}

- (IBAction)onRefreshRunningApps:(id)sender {
    [self refreshAppExclusionLists:YES];
}

- (void)onApplicationTableDoubleClick:(NSTableView *)tableView {
    if (tableView.clickedRow < 0) {
        return;
    }
    [tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)tableView.clickedRow]
           byExtendingSelection:NO];
    if (tableView == runningAppsTable) {
        [self onAddExcludedApp:tableView];
    } else if (tableView == excludedAppsTable) {
        [self onRemoveExcludedApp:tableView];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return tableView == excludedAppsTable ? excludedAppItems.count : runningAppItems.count;
}

- (id)tableView:(NSTableView *)tableView
    objectValueForTableColumn:(NSTableColumn *)tableColumn
                         row:(NSInteger)row {
    NSArray<NSDictionary<NSString*, NSString*>*> *applications =
        tableView == excludedAppsTable ? excludedAppItems : runningAppItems;
    if (row < 0 || row >= (NSInteger)applications.count) {
        return @"";
    }
    return [self applicationLabel:applications[row]];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    [self updateAppExclusionButtons];
}

- (IBAction)onInputTypeChanged:(NSPopUpButton *)sender {
    [appDelegate onInputTypeSelectedIndex:(int)[self.popupInputType indexOfSelectedItem]];
}

- (IBAction)onCodeTableChanged:(NSPopUpButton *)sender {
    [appDelegate onCodeTableChanged:(int)[self.popupCode indexOfSelectedItem]];
}

- (IBAction)onLanguageChanged:(id)sender {
    [appDelegate onInputMethodSelected];
}

- (IBAction)onRestart:(id)sender {
    self.appOK.hidden = YES;
    self.permissionWarning.hidden = YES;
    self.retryButton.enabled = NO;
    
    [self initKey];
}

- (IBAction)onFreeMark:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"FreeMark"];
    vFreeMark = (int)val;
}

- (IBAction)onModernOrthography:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"ModernOrthography"];
    vUseModernOrthography = (int)val;
}

- (IBAction)onCheckSpelling:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"Spelling"];
    vCheckSpelling = (int)val;
    [self.RestoreIfInvalidWord setEnabled:val];
    [self.AllowZWJF setEnabled:val];
    [self.TempOffSpellChecking setEnabled:val];
    OnSpellCheckingChanged();
}

- (IBAction)onShowUIOnStartup:(NSButton *)sender {
    [self setCustomValue:sender keyToSet:@"ShowUIOnStartup"];
}

- (IBAction)onRunOnStartup:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"RunOnStartup"];
    [appDelegate setRunOnStartup:val];
}

- (IBAction)onGrayIcon:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"GrayIcon"];
    [appDelegate setGrayIcon:val];
}

- (IBAction)onQuickTelex:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"QuickTelex"];
    vQuickTelex = (int)val;
}

- (IBAction)onRestoreIfInvalidWord:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"RestoreIfInvalidWord"];
    vRestoreIfWrongSpelling = (int)val;
}

- (IBAction)omTempOffSpellChecking:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vTempOffSpelling"];
    vTempOffSpelling = (int)val;
}

- (IBAction)onAllowZFWJ:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vAllowConsonantZFWJ"];
    vAllowConsonantZFWJ = (int)val;
}

- (IBAction)onFixRecommendBrowser:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"FixRecommendBrowser"];
    vFixRecommendBrowser = (int)val;
    [self.FixChromiumBrowser setEnabled:val];
}

- (IBAction)onControlSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:nil];
    vSwitchKeyStatus &= (~0x100);
    vSwitchKeyStatus |= val << 8;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onOptionSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:nil];
    vSwitchKeyStatus &= (~0x200);
    vSwitchKeyStatus |= val << 9;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onCommandSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:nil];
    vSwitchKeyStatus &= (~0x400);
    vSwitchKeyStatus |= val << 10;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onShiftSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:nil];
    vSwitchKeyStatus &= (~0x800);
    vSwitchKeyStatus |= val << 11;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

-(void)onMyTextFieldKeyChange:(unsigned short)keyCode character:(unsigned short)character {
    vSwitchKeyStatus &= 0xFFFFFF00;
    vSwitchKeyStatus |= keyCode;
    vSwitchKeyStatus &= 0x00FFFFFF;
    vSwitchKeyStatus |= ((unsigned int)character<<24);
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onBeepSound:(NSButton *)sender {
    unsigned int val = (unsigned int)[self setCustomValue:sender keyToSet:nil];
    vSwitchKeyStatus &= (~0x8000);
    vSwitchKeyStatus |= val << 15;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onSendKeyStepByStep:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"SendKeyStepByStep"];
    vSendKeyStepByStep = (int)val;
}

- (IBAction)onPerformLayoutCompat:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vPerformLayoutCompat"];
    vPerformLayoutCompat = (int)val;
}

- (NSInteger)setCustomValue:(NSButton*)sender keyToSet:(NSString*) key {
    NSInteger val = 0;
    if (sender.state == NSControlStateValueOn) {
        val = 1;
    } else {
        val = 0;
    }
    if (key != nil)
        [[NSUserDefaults standardUserDefaults] setInteger:val forKey:key];
    return val;
}

- (IBAction)onMacroButton:(id)sender {
    [appDelegate onMacroSelected];
}

- (IBAction)onMacroChanged:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseMacro"];
    vUseMacro = (int)val;
}

- (IBAction)onUseMacroInEnglishModeChanged:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseMacroInEnglishMode"];
    vUseMacroInEnglishMode = (int)val;
}

- (IBAction)onAutoRememberSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseSmartSwitchKey"];
    vUseSmartSwitchKey = (int)val;
}

- (IBAction)onUpperCaseFirstChar:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UpperCaseFirstChar"];
    vUpperCaseFirstChar = (int)val;
}
- (IBAction)onQuickStartConsonant:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vQuickStartConsonant"];
    vQuickStartConsonant = (int)val;
}

- (IBAction)onQuickEndConsonant:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vQuickEndConsonant"];
    vQuickEndConsonant = (int)val;
}

- (IBAction)onTempOffOpenKeyByHotKey:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vTempOffOpenKey"];
    vTempOffOpenKey = (int)val;
}

- (IBAction)onRememberTableCode:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vRememberCode"];
    vRememberCode = (int)val;
}
- (IBAction)onOtherLanguage:(id)sender {
    
    NSInteger val = [self setCustomValue:sender keyToSet:@"vOtherLanguage"];
    vOtherLanguage = (int)val;
}


- (IBAction)onAutoCapsMacro:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vAutoCapsMacro"];
    vAutoCapsMacro = (int)val;
}

- (IBAction)onShowIconOnDock:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vShowIconOnDock"];
    vShowIconOnDock = (int)val;
    if (!vShowIconOnDock) {
        [self.view.window close];
    }
    [appDelegate showIconOnDock:vShowIconOnDock];
}

- (IBAction)onCheckNewVersionOnStartup:(NSButton *)sender {
    NSInteger val = sender.state == NSControlStateValueOn ? 0 : 1;
    [[NSUserDefaults standardUserDefaults] setInteger:val forKey:@"DontCheckUpdate"];
}

- (IBAction)onFixChromiumBrowser:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vFixChromiumBrowser"];
    vFixChromiumBrowser = (int)val;
}

- (IBAction)onTerminateApp:(id)sender {
    [NSApp terminate:0];
}

-(void)fillData {
    NSInteger value;
    
    NSInteger intInputMethod = [[NSUserDefaults standardUserDefaults] integerForKey:@"InputMethod"];
    if (intInputMethod == 1) {
        self.VietButton.state = NSControlStateValueOn;
    } else if (intInputMethod == 0) {
        self.EngButton.state = NSControlStateValueOn;
    }
    
    NSInteger intInputType = [[NSUserDefaults standardUserDefaults] integerForKey:@"InputType"];
    [self.popupInputType selectItemAtIndex:intInputType];
    
    NSInteger intCodeTable = [[NSUserDefaults standardUserDefaults] integerForKey:@"CodeTable"];
    [self.popupCode selectItemAtIndex:intCodeTable];
    
    //option
    NSInteger showui = [[NSUserDefaults standardUserDefaults] integerForKey:@"ShowUIOnStartup"];
    self.ShowUIButton.state = showui ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger freeMark = [[NSUserDefaults standardUserDefaults] integerForKey:@"FreeMark"];
    self.FreeMarkButton.state = freeMark ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger useModernOrthography = [[NSUserDefaults standardUserDefaults] integerForKey:@"ModernOrthography"];
    self.UseModernOrthography.state = useModernOrthography ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger spelling = [[NSUserDefaults standardUserDefaults] integerForKey:@"Spelling"];
    self.CheckSpellingButton.state = spelling ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger runOnStartup = [[NSUserDefaults standardUserDefaults] integerForKey:@"RunOnStartup"];
    self.RunOnStartupButton.state = runOnStartup ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger useGrayIcon = [[NSUserDefaults standardUserDefaults] integerForKey:@"GrayIcon"];
    self.UseGrayIcon.state = useGrayIcon ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger quicTelex = [[NSUserDefaults standardUserDefaults] integerForKey:@"QuickTelex"];
    self.QuickTelex.state = quicTelex ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger restoreIfInvalidWord = [[NSUserDefaults standardUserDefaults] integerForKey:@"RestoreIfInvalidWord"];
    self.RestoreIfInvalidWord.state = restoreIfInvalidWord ? NSControlStateValueOn : NSControlStateValueOff;
    [self.RestoreIfInvalidWord setEnabled:spelling];
    
    NSInteger tempOffSpelling = [[NSUserDefaults standardUserDefaults] integerForKey:@"vTempOffSpelling"];
    self.TempOffSpellChecking.state = tempOffSpelling ? NSControlStateValueOn : NSControlStateValueOff;
    [self.TempOffSpellChecking setEnabled:spelling];
    
    NSInteger allowZFWJ = [[NSUserDefaults standardUserDefaults] integerForKey:@"vAllowConsonantZFWJ"];
    self.AllowZWJF.state = allowZFWJ ? NSControlStateValueOn : NSControlStateValueOff;
    [self.AllowZWJF setEnabled:spelling];
    
    NSInteger fixRecommendBrowser = [[NSUserDefaults standardUserDefaults] integerForKey:@"FixRecommendBrowser"];
    self.FixRecommendBrowser.state = fixRecommendBrowser ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger useMacro = [[NSUserDefaults standardUserDefaults] integerForKey:@"UseMacro"];
    self.UseMacro.state = useMacro ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger useMacroInEnglish = [[NSUserDefaults standardUserDefaults] integerForKey:@"UseMacroInEnglishMode"];
    self.UseMacroInEnglishMode.state = useMacroInEnglish ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger sendKeySbS = [[NSUserDefaults standardUserDefaults] integerForKey:@"SendKeyStepByStep"];
    self.SendKeyStepByStep.state = sendKeySbS ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger useSmartSwitchKey = [[NSUserDefaults standardUserDefaults] integerForKey:@"UseSmartSwitchKey"];
    self.AutoRememberSwitchKey.state = useSmartSwitchKey ? NSControlStateValueOn : NSControlStateValueOff;

    NSInteger useManualAppExclusion = [[NSUserDefaults standardUserDefaults] integerForKey:@"UseManualAppExclusion"];
    manualAppExclusionButton.state = useManualAppExclusion ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger upperCaseFirstChar = [[NSUserDefaults standardUserDefaults] integerForKey:@"UpperCaseFirstChar"];
    self.UpperCaseFirstChar.state = upperCaseFirstChar ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger quickStartConsonant = [[NSUserDefaults standardUserDefaults] integerForKey:@"vQuickStartConsonant"];
    self.QuickStartConsonant.state = quickStartConsonant ? NSControlStateValueOn : NSControlStateValueOff;
    
    NSInteger quickEndConsonant = [[NSUserDefaults standardUserDefaults] integerForKey:@"vQuickEndConsonant"];
    self.QuickEndConsonant.state = quickEndConsonant ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vRememberCode"];
    self.RememberTableCode.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vOtherLanguage"];
    self.OtherLanguage.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vTempOffOpenKey"];
    self.TempOffOpenKey.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vAutoCapsMacro"];
    self.AutoCapsMacro.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vShowIconOnDock"];
    self.ShowIconOnDock.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"DontCheckUpdate"];
    self.CheckNewVersionOnStartup.state = value ? NSControlStateValueOff :NSControlStateValueOn;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vFixChromiumBrowser"];
    self.FixChromiumBrowser.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    self.FixChromiumBrowser.enabled = fixRecommendBrowser ? YES : NO;
    
    value = [[NSUserDefaults standardUserDefaults] integerForKey:@"vPerformLayoutCompat"];
    self.PerformLayoutCompat.state = value ? NSControlStateValueOn : NSControlStateValueOff;
    
    CustomSwitchControl.state = (vSwitchKeyStatus & 0x100) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchOption.state = (vSwitchKeyStatus & 0x200) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchCommand.state = (vSwitchKeyStatus & 0x400) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchShift.state = (vSwitchKeyStatus & 0x800) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomBeepSound.state = (vSwitchKeyStatus & 0x8000) ? NSControlStateValueOn : NSControlStateValueOff;
    [CustomSwitchKey setTextByChar:((vSwitchKeyStatus>>24) & 0xFF)];
    
}

- (IBAction)onOK:(id)sender {
    [self.view.window close];
}

- (IBAction)onDefaultConfig:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Bạn có chắc chắn muốn thiết lập lại cấu hình mặc định?"];
    [alert addButtonWithTitle:@"Có"];
    [alert addButtonWithTitle:@"Không"];
    [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == 1000) {
            [appDelegate loadDefaultConfig];
            [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"ShowUIOnStartup"];
            self.ShowUIButton.state = NSControlStateValueOff;
            
            [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"RunOnStartup"];
            self.RunOnStartupButton.state = NSControlStateValueOn;
        }
    }];
}

- (IBAction)onHomePageLink:(id)sender {
    [self openURLString:@"https://github.com/klee3721/NiceKey"];
}

- (IBAction)onFanpageLink:(id)sender {
    [self openURLString:@"https://t.me/kienvu37"];
}

- (IBAction)onEmailLink:(id)sender {
    [self openURLString:@"https://github.com/klee3721/NiceKey"];
}

- (IBAction)onSourceCode:(id)sender {
    [self openURLString:@"https://github.com/klee3721/NiceKey"];
}

- (IBAction)onCheckNewVersionButton:(id)sender {
    self.CheckNewVersionButton.title = @"Đang kiểm tra...";
    self.CheckNewVersionButton.enabled = false;
    
    [OpenKeyManager checkNewVersion:self.view.window callbackFunc:^{
        self.CheckNewVersionButton.enabled = true;
        self.CheckNewVersionButton.title = @"Kiểm tra bản mới...";
    }];
}

@end
