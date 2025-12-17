#pragma once

#include "trimmeh_js_core.h"

#include <QString>

class KSharedConfig;
class KConfigGroup;

class TrimmehSettings final {
public:
    TrimmehSettings();

    QString aggressiveness() const;
    void setAggressiveness(const QString& value);

    bool enableAutoTrim() const;
    void setEnableAutoTrim(bool enabled);

    bool keepBlankLines() const;
    void setKeepBlankLines(bool enabled);

    bool stripBoxChars() const;
    void setStripBoxChars(bool enabled);

    bool trimPrompts() const;
    void setTrimPrompts(bool enabled);

    int maxLines() const;
    void setMaxLines(int lines);

    TrimmehTrimOptions trimOptions() const;

    void sync();

private:
    KConfigGroup group() const;

    QString m_configFileName;
};

