#pragma once

#include <QJSEngine>
#include <QJSValue>
#include <QString>

struct TrimOptions {
    bool keepBlankLines = false;
    bool stripBoxChars = true;
    bool trimPrompts = true;
    int maxLines = 10;
};

struct TrimResult {
    QString output;
    bool changed = false;
    QString reason;
};

class TrimCore {
public:
    bool load(const QString &jsPath, QString *errorMessage = nullptr);
    bool isReady() const { return m_ready; }
    TrimResult trim(const QString &input,
                    const QString &aggressiveness,
                    const TrimOptions &options,
                    QString *errorMessage = nullptr);

private:
    QJSEngine m_engine;
    QJSValue m_trimFunc;
    bool m_ready = false;
};
