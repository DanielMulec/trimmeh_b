#pragma once

#include <QString>

struct Settings {
    bool autoTrimEnabled = true;
    bool keepBlankLines = false;
    bool stripBoxChars = true;
    bool trimPrompts = true;
    bool useClipboardFallbacks = false;
    int maxLines = 10;
    QString aggressiveness = QStringLiteral("normal");
    int graceDelayMs = 80;
    int pasteRestoreDelayMs = 1200;
    int pasteInjectDelayMs = 120;
    bool startAtLogin = false;
    bool pasteTrimmedHotkeyEnabled = true;
    bool pasteOriginalHotkeyEnabled = false;
    bool toggleAutoTrimHotkeyEnabled = false;
    QString pasteTrimmedHotkey;
    QString pasteOriginalHotkey;
    QString toggleAutoTrimHotkey;
};
