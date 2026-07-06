//
//  NiceKeyClipboardBridge.swift
//  NiceKey
//
//  Objective-C bridge for the clipboard history feature copied from mkey.
//

import AppKit
import SwiftUI

enum MKBridge {
    static func setEngineSuspended(_ suspended: Bool) {
        NiceKeySetEngineSuspended(suspended)
    }
}

enum AppState {
    static func hotkeyDescription(_ status: Int32) -> String {
        let value = UInt32(bitPattern: status)
        var parts: [String] = []
        if value & 0x100 != 0 { parts.append("⌃") }
        if value & 0x200 != 0 { parts.append("⌥") }
        if value & 0x400 != 0 { parts.append("⌘") }
        if value & 0x800 != 0 { parts.append("⇧") }
        let char = UInt8((value >> 24) & 0xFF)
        if char != 0xFE {
            if char == 49 || Character(UnicodeScalar(char)) == " " {
                parts.append("Space")
            } else {
                parts.append(String(UnicodeScalar(char)).uppercased())
            }
        }
        return parts.isEmpty ? "—" : parts.joined()
    }
}

@MainActor
@objc(NiceKeyClipboardBridge)
final class NiceKeyClipboardBridge: NSObject {
    private static var settingsWindow: NSWindow?

    @objc static func startIfEnabled() {
        ClipboardManager.shared.startIfEnabled()
    }

    @objc static func stop() {
        ClipboardManager.shared.stopForNiceKey()
    }

    @objc static func togglePicker() {
        ClipboardManager.shared.togglePicker()
    }

    @objc static func showSettingsWindow() {
        if let settingsWindow {
            settingsWindow.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        let controller = NSHostingController(rootView: ClipboardPage())
        let window = NSWindow(contentViewController: controller)
        window.title = "Lịch sử Clipboard"
        window.styleMask = [.titled, .closable, .resizable]
        window.setContentSize(NSSize(width: 520, height: 420))
        window.center()
        window.isReleasedWhenClosed = false
        window.delegate = ClipboardSettingsWindowDelegate.shared
        settingsWindow = window
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc static func isEnabled() -> Bool {
        ClipboardManager.shared.enabled
    }

    @objc static func hotKeyDescription() -> String {
        AppState.hotkeyDescription(ClipboardManager.shared.hotKey)
    }

    fileprivate static func clearSettingsWindow(_ window: NSWindow) {
        if settingsWindow === window {
            settingsWindow = nil
        }
    }
}

private final class ClipboardSettingsWindowDelegate: NSObject, NSWindowDelegate {
    static let shared = ClipboardSettingsWindowDelegate()

    func windowWillClose(_ notification: Notification) {
        if let window = notification.object as? NSWindow {
            Task { @MainActor in
                NiceKeyClipboardBridge.clearSettingsWindow(window)
            }
        }
    }
}
