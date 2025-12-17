#include "trimmeh_settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <algorithm>

namespace {
constexpr const char* kGroupName = "Trimmeh";

constexpr const char* kKeyAggressiveness = "aggressiveness";
constexpr const char* kKeyEnableAutoTrim = "enable_auto_trim";
constexpr const char* kKeyKeepBlankLines = "keep_blank_lines";
constexpr const char* kKeyStripBoxChars = "strip_box_chars";
constexpr const char* kKeyTrimPrompts = "trim_prompts";
constexpr const char* kKeyMaxLines = "max_lines";
} // namespace

TrimmehSettings::TrimmehSettings() : m_configFileName(QStringLiteral("trimmeh-kde")) {}

KConfigGroup TrimmehSettings::group() const {
    auto config = KSharedConfig::openConfig(m_configFileName);
    return KConfigGroup(config, QString::fromLatin1(kGroupName));
}

QString TrimmehSettings::aggressiveness() const {
    return group().readEntry(QString::fromLatin1(kKeyAggressiveness), QStringLiteral("normal"));
}

void TrimmehSettings::setAggressiveness(const QString& value) {
    if (value != QStringLiteral("low") && value != QStringLiteral("normal") && value != QStringLiteral("high")) {
        return;
    }
    group().writeEntry(QString::fromLatin1(kKeyAggressiveness), value);
}

bool TrimmehSettings::enableAutoTrim() const {
    return group().readEntry(QString::fromLatin1(kKeyEnableAutoTrim), true);
}

void TrimmehSettings::setEnableAutoTrim(bool enabled) {
    group().writeEntry(QString::fromLatin1(kKeyEnableAutoTrim), enabled);
}

bool TrimmehSettings::keepBlankLines() const {
    return group().readEntry(QString::fromLatin1(kKeyKeepBlankLines), false);
}

void TrimmehSettings::setKeepBlankLines(bool enabled) {
    group().writeEntry(QString::fromLatin1(kKeyKeepBlankLines), enabled);
}

bool TrimmehSettings::stripBoxChars() const {
    return group().readEntry(QString::fromLatin1(kKeyStripBoxChars), true);
}

void TrimmehSettings::setStripBoxChars(bool enabled) {
    group().writeEntry(QString::fromLatin1(kKeyStripBoxChars), enabled);
}

bool TrimmehSettings::trimPrompts() const {
    return group().readEntry(QString::fromLatin1(kKeyTrimPrompts), true);
}

void TrimmehSettings::setTrimPrompts(bool enabled) {
    group().writeEntry(QString::fromLatin1(kKeyTrimPrompts), enabled);
}

int TrimmehSettings::maxLines() const {
    const int value = group().readEntry(QString::fromLatin1(kKeyMaxLines), 10);
    return std::clamp(value, 1, 1000);
}

void TrimmehSettings::setMaxLines(int lines) {
    group().writeEntry(QString::fromLatin1(kKeyMaxLines), std::clamp(lines, 1, 1000));
}

TrimmehTrimOptions TrimmehSettings::trimOptions() const {
    return TrimmehTrimOptions{
        .keepBlankLines = keepBlankLines(),
        .stripBoxChars = stripBoxChars(),
        .trimPrompts = trimPrompts(),
        .maxLines = maxLines(),
    };
}

void TrimmehSettings::sync() {
    auto config = KSharedConfig::openConfig(m_configFileName);
    config->sync();
}

