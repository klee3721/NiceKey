//
//  AboutViewController.m
//  OpenKey
//
//  Created by Tuyen on 2/15/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "AboutViewController.h"
#import "OpenKeyManager.h"

@interface AboutViewController ()

@end

@implementation AboutViewController

- (void)openURLString:(NSString *)urlString {
    NSURL *url = [NSURL URLWithString:urlString];
    if (url) {
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.
    
    self.VersionInfo.stringValue = [NSString stringWithFormat:@"Phiên bản %@ (build %@) - Ngày cập nhật %@",
                                    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleShortVersionString"],
                                    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"],
                                    [OpenKeyManager getBuildDate]] ;
    
    NSInteger dontCheckUpdate = [[NSUserDefaults standardUserDefaults] integerForKey:@"DontCheckUpdate"];
    self.CheckUpdateOnStatus.state = dontCheckUpdate ? NSControlStateValueOff :NSControlStateValueOn;
}

- (IBAction)onHomePage:(id)sender {
    [self openURLString:@"https://github.com/klee3721/NiceKey"];
}

- (IBAction)onFanPage:(id)sender {
    [self openURLString:@"https://t.me/kienvu37"];
}

- (IBAction)onLatestReleaseVersion:(id)sender {
    [self openURLString:@"https://github.com/klee3721/NiceKey/releases"];
}

- (IBAction)onCheckUpdateOnStartup:(NSButton *)sender {
    NSInteger val = sender.state == NSControlStateValueOn ? 0 : 1;
    [[NSUserDefaults standardUserDefaults] setInteger:val forKey:@"DontCheckUpdate"];
}

- (IBAction)onCheckNewVersion:(id)sender {
    
    self.CheckNewVersionButton.title = @"Đang kiểm tra...";
    self.CheckNewVersionButton.enabled = false;
    
    [OpenKeyManager checkNewVersion: self.view.window callbackFunc:^{
        self.CheckNewVersionButton.enabled = true;
        self.CheckNewVersionButton.title = @"Kiểm tra bản mới...";
    }];
}

@end
