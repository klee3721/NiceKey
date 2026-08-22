//
//  OpenKeyManager.m
//  OpenKey
//
//  Created by Tuyen on 1/27/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "OpenKeyManager.h"

extern void OpenKeyInit(void);

extern CGEventRef OpenKeyCallback(CGEventTapProxy proxy,
                                  CGEventType type,
                                  CGEventRef event,
                                  void *refcon);

extern NSString* ConvertUtil(NSString* str);

@interface OpenKeyManager ()

@end

@implementation OpenKeyManager {

}
static BOOL _isInited = NO;
static BOOL _engineInited = NO;

static CFMachPortRef      eventTap;
static CGEventMask        eventMask;
static CFRunLoopSourceRef runLoopSource;

static NSString * const ManualExcludedAppsDefaultsKey = @"ManualExcludedApps";
static NSString * const ManualAppBundleIdentifierKey = @"bundleIdentifier";
static NSString * const ManualAppDisplayNameKey = @"displayName";
static NSMutableSet<NSString*> *manualExcludedBundleIdentifiers;

static void EnsureManualExcludedApplicationsLoaded(void) {
    if (manualExcludedBundleIdentifiers) {
        return;
    }

    NSArray *storedIdentifiers = [[NSUserDefaults standardUserDefaults] stringArrayForKey:ManualExcludedAppsDefaultsKey];
    manualExcludedBundleIdentifiers = [[NSMutableSet alloc] init];
    for (NSString *bundleIdentifier in storedIdentifiers) {
        if ([bundleIdentifier isKindOfClass:[NSString class]] && bundleIdentifier.length > 0) {
            [manualExcludedBundleIdentifiers addObject:bundleIdentifier];
        }
    }
}

static void SaveManualExcludedApplications(void) {
    NSArray *sortedIdentifiers = [[manualExcludedBundleIdentifiers allObjects]
                                  sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    [[NSUserDefaults standardUserDefaults] setObject:sortedIdentifiers forKey:ManualExcludedAppsDefaultsKey];
}

static NSString *DisplayNameForBundleIdentifier(NSString *bundleIdentifier) {
    NSArray<NSRunningApplication*> *runningApplications =
        [NSRunningApplication runningApplicationsWithBundleIdentifier:bundleIdentifier];
    NSString *displayName = runningApplications.firstObject.localizedName;
    if (displayName.length > 0) {
        return displayName;
    }

    NSURL *applicationURL = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleIdentifier];
    NSBundle *applicationBundle = applicationURL ? [NSBundle bundleWithURL:applicationURL] : nil;
    NSDictionary *localizedInfo = applicationBundle.localizedInfoDictionary;
    NSDictionary *info = applicationBundle.infoDictionary;
    displayName = localizedInfo[@"CFBundleDisplayName"] ?: localizedInfo[@"CFBundleName"] ?:
                  info[@"CFBundleDisplayName"] ?: info[@"CFBundleName"];
    return displayName.length > 0 ? displayName : bundleIdentifier;
}

static NSDictionary<NSString*, NSString*> *ManualApplicationInfo(NSString *bundleIdentifier,
                                                                  NSString *displayName) {
    return @{ ManualAppBundleIdentifierKey: bundleIdentifier,
              ManualAppDisplayNameKey: displayName.length > 0 ? displayName : bundleIdentifier };
}

static NSComparisonResult CompareManualApplicationInfo(NSDictionary<NSString*, NSString*> *left,
                                                        NSDictionary<NSString*, NSString*> *right) {
    NSComparisonResult displayNameResult = [left[ManualAppDisplayNameKey]
                                            localizedCaseInsensitiveCompare:right[ManualAppDisplayNameKey]];
    if (displayNameResult != NSOrderedSame) {
        return displayNameResult;
    }
    return [left[ManualAppBundleIdentifierKey]
            localizedCaseInsensitiveCompare:right[ManualAppBundleIdentifierKey]];
}

+(BOOL)isInited {
    return _isInited;
}

+(BOOL)initEventTap {
    if (_isInited) {
        if (eventTap && CFMachPortIsValid(eventTap)) {
            if (!CGEventTapIsEnabled(eventTap)) {
                CGEventTapEnable(eventTap, true);
            }
            if (CGEventTapIsEnabled(eventTap)) {
                return YES;
            }
        }
        [self stopEventTap];
    }
    
    //init modernKey
    if (!_engineInited) {
        OpenKeyInit();
        _engineInited = YES;
    }
    
    // Create an event tap. We are interested in key presses.
    eventMask = ((1 << kCGEventKeyDown) |
                 (1 << kCGEventKeyUp) |
                 (1 << kCGEventFlagsChanged) |
                 (1 << kCGEventLeftMouseDown) |
                 (1 << kCGEventRightMouseDown) |
                 (1 << kCGEventLeftMouseDragged) |
                 (1 << kCGEventRightMouseDragged));
    
    eventTap = CGEventTapCreate(kCGSessionEventTap,
                                kCGHeadInsertEventTap,
                                0,
                                eventMask,
                                OpenKeyCallback,
                                &eventTap);
    
    if (!eventTap) {
        
        fprintf(stderr, "failed to create event tap\n");
        return NO;
    }
    
    // Create a run loop source.
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    if (!runLoopSource) {
        CFMachPortInvalidate(eventTap);
        CFRelease(eventTap);
        eventTap = nil;
        return NO;
    }
    
    // Add to the current run loop.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    
    // Enable the event tap.
    CGEventTapEnable(eventTap, true);
    if (!CGEventTapIsEnabled(eventTap)) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
        CFRelease(runLoopSource);
        runLoopSource = nil;
        CFMachPortInvalidate(eventTap);
        CFRelease(eventTap);
        eventTap = nil;
        return NO;
    }

    _isInited = YES;
    
    return YES;
}

+(BOOL)stopEventTap {
    if (_isInited) { //release all object
        if (runLoopSource) {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
            CFRelease(runLoopSource);
            runLoopSource = nil;
        }
        
        if (eventTap) {
            CFMachPortInvalidate(eventTap);
            CFRelease(eventTap);
            eventTap = nil;
        }
        
        _isInited = false;
    }
    return YES;
}

+(NSArray*)getTableCodes {
    return [[NSArray alloc] initWithObjects:
            @"Unicode",
            @"TCVN3 (ABC)",
            @"VNI Windows",
            @"Unicode tổ hợp",
            @"Vietnamese Locale CP 1258", nil];
}

+(NSString*)getBuildDate {
    return [NSString stringWithUTF8String:__DATE__];
}

#pragma mark -Convert feature
+(BOOL)quickConvert {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *htmlString = [pasteboard stringForType:NSPasteboardTypeHTML];
    NSString *rawString = [pasteboard stringForType:NSPasteboardTypeString];
    bool converted = false;
    if (htmlString != nil) {
        htmlString = ConvertUtil(htmlString);
        converted = true;
    }
    if (rawString != nil) {
        rawString = ConvertUtil(rawString);
        converted = true;
    }
    if (converted) {
        [pasteboard clearContents];
        if (htmlString != nil)
            [pasteboard setString:htmlString forType:NSPasteboardTypeHTML];
        if (rawString != nil)
            [pasteboard setString:rawString forType:NSPasteboardTypeString];
        
        return YES;
    }
    return NO;
}

#pragma mark -Manual app exclusion

+(BOOL)isManualExcludedBundleIdentifier:(NSString*)bundleIdentifier {
    if (bundleIdentifier.length == 0) {
        return NO;
    }
    EnsureManualExcludedApplicationsLoaded();
    return [manualExcludedBundleIdentifiers containsObject:bundleIdentifier];
}

+(NSArray<NSDictionary<NSString*, NSString*>*>*)selectedManualExcludedApplications {
    EnsureManualExcludedApplicationsLoaded();
    NSMutableArray<NSDictionary<NSString*, NSString*>*> *applications = [[NSMutableArray alloc] init];
    for (NSString *bundleIdentifier in manualExcludedBundleIdentifiers) {
        [applications addObject:ManualApplicationInfo(bundleIdentifier,
                                                       DisplayNameForBundleIdentifier(bundleIdentifier))];
    }
    [applications sortUsingComparator:^NSComparisonResult(NSDictionary<NSString*, NSString*> *left,
                                                           NSDictionary<NSString*, NSString*> *right) {
        return CompareManualApplicationInfo(left, right);
    }];
    return applications;
}

+(NSArray<NSDictionary<NSString*, NSString*>*>*)runningApplicationsForManualExclusion {
    NSMutableDictionary<NSString*, NSDictionary<NSString*, NSString*>*> *applicationsByIdentifier =
        [[NSMutableDictionary alloc] init];
    for (NSRunningApplication *application in [[NSWorkspace sharedWorkspace] runningApplications]) {
        NSString *bundleIdentifier = application.bundleIdentifier;
        if (application.terminated || application.activationPolicy != NSApplicationActivationPolicyRegular ||
            bundleIdentifier.length == 0 || [bundleIdentifier isEqualToString:[[NSBundle mainBundle] bundleIdentifier]]) {
            continue;
        }
        applicationsByIdentifier[bundleIdentifier] = ManualApplicationInfo(bundleIdentifier,
                                                                             application.localizedName);
    }

    NSMutableArray<NSDictionary<NSString*, NSString*>*> *applications =
        [[applicationsByIdentifier allValues] mutableCopy];
    [applications sortUsingComparator:^NSComparisonResult(NSDictionary<NSString*, NSString*> *left,
                                                           NSDictionary<NSString*, NSString*> *right) {
        return CompareManualApplicationInfo(left, right);
    }];
    return applications;
}

+(BOOL)addManualExcludedBundleIdentifier:(NSString*)bundleIdentifier {
    if (bundleIdentifier.length == 0) {
        return NO;
    }
    EnsureManualExcludedApplicationsLoaded();
    if ([manualExcludedBundleIdentifiers containsObject:bundleIdentifier]) {
        return NO;
    }
    [manualExcludedBundleIdentifiers addObject:bundleIdentifier];
    SaveManualExcludedApplications();
    return YES;
}

+(BOOL)removeManualExcludedBundleIdentifier:(NSString*)bundleIdentifier {
    if (bundleIdentifier.length == 0) {
        return NO;
    }
    EnsureManualExcludedApplicationsLoaded();
    if (![manualExcludedBundleIdentifiers containsObject:bundleIdentifier]) {
        return NO;
    }
    [manualExcludedBundleIdentifiers removeObject:bundleIdentifier];
    SaveManualExcludedApplications();
    return YES;
}

+(void)showMessage:(NSWindow*)window message:(NSString*)msg subMsg:(NSString*)subMsg {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:msg];
    [alert setInformativeText:subMsg];
    [alert addButtonWithTitle:@"OK"];
    if (window) {
        [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse returnCode) {
        }];
    } else {
        [alert runModal];
    }
}

#pragma mark -AutoUpdate feature

+(void)checkNewVersion:(NSWindow*)parent callbackFunc:(CheckNewVersionCallback) callback {
    NSString *versionURLString = @"https://raw.githubusercontent.com/klee3721/NiceKey/master/version.json";
    if (versionURLString.length == 0) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (callback != nil) {
                callback();
                [self showMessage:parent
                           message:@"NiceKey chưa cấu hình nguồn cập nhật."
                            subMsg:@"Khi có repository phát hành riêng, hãy cập nhật URL version.json trong OpenKeyManager.m."];
            }
        });
        return;
    }

    //load new version config
    NSURLSession *aSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
    [[aSession dataTaskWithURL:[NSURL URLWithString:versionURLString] completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (((NSHTTPURLResponse *)response).statusCode == 200) {
            if (data) {
                if(NSClassFromString(@"NSJSONSerialization")) {
                    NSError *error = nil;
                    id object = [NSJSONSerialization
                                 JSONObjectWithData:data
                                 options:0
                                 error:&error];
                    
                    if(error) {  }
                    if([object isKindOfClass:[NSDictionary class]]) {
                        NSDictionary *results = object;
                        NSDictionary *ver = [results valueForKey:@"latestVersion"];
                        NSString* versionCodeString = [ver valueForKey:@"versionCode"];
                        int versionCode = (int)[versionCodeString integerValue];
                        int currentVersionCode = (int)[((NSString*)[[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"]) integerValue];
                        
                        dispatch_async(dispatch_get_main_queue(), ^{
                            if (callback != nil) {
                                callback();
                            }
                            if (versionCode > currentVersionCode || callback != nil) {
                                [self showUpdateMessage:parent needUpdating:versionCode > currentVersionCode newVersion:[ver valueForKey:@"versionName"]];
                            }
                        });
                    }
                    else {
                        //oh my god
                    }
                }
                else {
                    //can not parse json
                }
            }
        }
    }] resume];
}

+(void)showUpdateMessage:(NSWindow*)parent needUpdating:(BOOL)needUpdating newVersion:(NSString*)versionString {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:(needUpdating ? [NSString stringWithFormat:@"NiceKey có phiên bản mới (%@), bạn có muốn cập nhật không?", versionString] : @"Bạn đang dùng phiên bản mới nhất!")];
    [alert setInformativeText:(needUpdating ? @"Bấm 'Có' để cập nhật NiceKey." : @"")];
    
    if (!needUpdating) {
        [alert addButtonWithTitle:@"OK"];
    } else {
        [alert addButtonWithTitle:@"Có"];
        [alert addButtonWithTitle:@"Không"];
    }
    if (parent == nil) {
        [alert.window makeKeyAndOrderFront:nil];
        [alert.window setLevel:NSStatusWindowLevel];
        NSModalResponse res = [alert runModal];
        if (res == 1000 && needUpdating) {
            [self launchUpdateHelper];
        }
    } else {
        [alert beginSheetModalForWindow:parent completionHandler:^(NSModalResponse returnCode) {
            if (returnCode == 1000 && needUpdating) {
                [self launchUpdateHelper];
            }
        }];
    }
}

+(void)launchUpdateHelper {
    //check update app has exist or not
    NSError *copyError = nil;
    NSString* target = [NSString stringWithFormat:@"%@/NiceKeyUpdate.app", [self getApplicationSupportFolder]];
    [[NSFileManager defaultManager] removeItemAtPath:target error:&copyError];
    if (![[NSFileManager defaultManager] fileExistsAtPath:target]) {
        [[NSFileManager defaultManager] createDirectoryAtPath:[self getApplicationSupportFolder] withIntermediateDirectories:YES attributes:nil error:nil];
        
        if (![[NSFileManager defaultManager] copyItemAtPath:[self getUpdateBundlePath] toPath:target error:&copyError]) {
            NSLog(@"Error on copy");
        }
    }
    
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *url = [NSURL fileURLWithPath:[workspace fullPathForApplication:target]];
    NSError *error = nil;
    NSArray *arguments = [NSArray arrayWithObjects: @"yeah", nil];
    [workspace launchApplicationAtURL:url options:0 configuration:[NSDictionary dictionaryWithObject:arguments forKey:NSWorkspaceLaunchConfigurationArguments] error:&error];
    
    [NSApp terminate:0]; //exit main app
}

+(NSString*)getApplicationSupportFolder {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString *applicationSupportDirectory = [paths firstObject];
    return [NSString stringWithFormat:@"%@/NiceKey", applicationSupportDirectory];
}

+(NSString*)getUpdateBundlePath {
    NSString *currentpath = [[NSBundle mainBundle] bundlePath];
    return [NSString stringWithFormat:@"%@/Contents/Library/LoginItems/NiceKeyUpdate.app", currentpath];
}
@end
