#pragma once

#include <QString>

class AutostartManager {
public:
    bool isEnabled() const;
    bool setEnabled(bool enabled, QString *errorMessage = nullptr) const;

private:
    QString desktopFilePath() const;
    QString desktopFileContents() const;
    QString quotedExecPath() const;
    QString legacyDesktopFilePath() const;
};
