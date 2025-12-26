#pragma once

#include <QString>

struct Settings {
    bool autoTrimEnabled = true;
    bool keepBlankLines = false;
    bool stripBoxChars = true;
    bool trimPrompts = true;
    int maxLines = 10;
    QString aggressiveness = QStringLiteral("normal");
    int graceDelayMs = 80;
    int pasteRestoreDelayMs = 1200;
};
