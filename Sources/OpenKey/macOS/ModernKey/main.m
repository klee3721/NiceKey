//
//  main.m
//  ModernKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <string.h>
#include <unistd.h>
#import "ViewController.h"
#import "OpenKeyManager.h"

static BOOL RunManualAppExclusionSelfTest(void) {
    NSString *bundleIdentifier = [NSString stringWithFormat:@"com.nicekey.self-test.%d", getpid()];
    if ([OpenKeyManager isManualExcludedBundleIdentifier:bundleIdentifier]) {
        return NO;
    }

    BOOL added = [OpenKeyManager addManualExcludedBundleIdentifier:bundleIdentifier];
    BOOL duplicateRejected = ![OpenKeyManager addManualExcludedBundleIdentifier:bundleIdentifier];
    BOOL found = [OpenKeyManager isManualExcludedBundleIdentifier:bundleIdentifier];
    BOOL listed = NO;
    for (NSDictionary<NSString*, NSString*> *application in
         [OpenKeyManager selectedManualExcludedApplications]) {
        if ([application[@"bundleIdentifier"] isEqualToString:bundleIdentifier]) {
            listed = YES;
            break;
        }
    }
    BOOL removed = [OpenKeyManager removeManualExcludedBundleIdentifier:bundleIdentifier];
    BOOL noLongerFound = ![OpenKeyManager isManualExcludedBundleIdentifier:bundleIdentifier];
    return added && duplicateRejected && found && listed && removed && noLongerFound;
}

int main(int argc, const char * argv[]) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        @autoreleasepool {
            return RunManualAppExclusionSelfTest() ? 0 : 2;
        }
    }
    return NSApplicationMain(argc, argv);
}
