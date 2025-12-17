#pragma once

#include <QJSEngine>
#include <QString>

struct TrimmehTrimOptions {
    bool keepBlankLines = false;
    bool stripBoxChars = true;
    bool trimPrompts = true;
    int maxLines = 10;
};

struct TrimmehTrimResult {
    QString output;
    bool changed = false;
    QString reason;
};

class TrimmehJsCore final {
public:
    TrimmehJsCore();

    TrimmehTrimResult trim(const QString& input, const QString& aggressiveness, const TrimmehTrimOptions& options);

private:
    bool ensureLoaded();

    bool m_loaded = false;
    QJSEngine m_engine;
};

