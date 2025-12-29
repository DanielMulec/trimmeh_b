#pragma once

#include <QString>

namespace AppIdentity {
QString appId();
QString desktopFileName();
QString desktopFilePath();
QString desktopFileContents();
QString preauthCommand();

bool ensureDesktopFile(QString *errorMessage = nullptr);
bool registerWithPortal(QString *errorMessage = nullptr);
} // namespace AppIdentity
