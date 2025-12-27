#pragma once

#include "klipper_bridge.h"
#include "settings.h"
#include "trim_core.h"

#include <QObject>
#include <QTimer>

class SettingsStore;
class AutostartManager;
class PortalPasteInjector;

class ClipboardWatcher : public QObject {
    Q_OBJECT
public:
    ClipboardWatcher(KlipperBridge *bridge,
                     TrimCore *core,
                     const Settings &settings,
                     SettingsStore *store = nullptr,
                     AutostartManager *autostart = nullptr,
                     PortalPasteInjector *injector = nullptr,
                     QObject *parent = nullptr);

    bool autoTrimEnabled() const { return m_settings.autoTrimEnabled; }
    void setAutoTrimEnabled(bool enabled);

    bool keepBlankLines() const { return m_settings.keepBlankLines; }
    bool stripBoxChars() const { return m_settings.stripBoxChars; }
    bool trimPrompts() const { return m_settings.trimPrompts; }
    int maxLines() const { return m_settings.maxLines; }
    QString aggressiveness() const { return m_settings.aggressiveness; }
    bool startAtLogin() const { return m_settings.startAtLogin; }
    bool pasteTrimmedHotkeyEnabled() const { return m_settings.pasteTrimmedHotkeyEnabled; }
    bool pasteOriginalHotkeyEnabled() const { return m_settings.pasteOriginalHotkeyEnabled; }
    bool toggleAutoTrimHotkeyEnabled() const { return m_settings.toggleAutoTrimHotkeyEnabled; }
    QString pasteTrimmedHotkey() const { return m_settings.pasteTrimmedHotkey; }
    QString pasteOriginalHotkey() const { return m_settings.pasteOriginalHotkey; }
    QString toggleAutoTrimHotkey() const { return m_settings.toggleAutoTrimHotkey; }

    void setKeepBlankLines(bool enabled);
    void setStripBoxChars(bool enabled);
    void setTrimPrompts(bool enabled);
    void setMaxLines(int maxLines);
    void setAggressiveness(const QString &level);
    void setStartAtLogin(bool enabled);
    void setPasteTrimmedHotkeyEnabled(bool enabled);
    void setPasteOriginalHotkeyEnabled(bool enabled);
    void setToggleAutoTrimHotkeyEnabled(bool enabled);
    void setPasteTrimmedHotkey(const QString &sequence);
    void setPasteOriginalHotkey(const QString &sequence);
    void setToggleAutoTrimHotkey(const QString &sequence);

    QString lastSummary() const { return m_lastSummary; }
    QString lastOriginal() const { return m_lastOriginal; }
    QString lastTrimmed() const { return m_lastTrimmed; }
    bool hasLastOriginal() const { return !m_lastOriginal.isEmpty(); }

    bool pasteTrimmed();
    bool pasteOriginal();
    bool restoreLastCopy();

public slots:
    void onClipboardHistoryUpdated();

signals:
    void summaryChanged(const QString &summary);
    void stateChanged();

private slots:
    void onDebounceTimeout();

private:
    void process(quint64 genAtSchedule);
    void updateSummary(const QString &text);
    QString summarize(const QString &text) const;
    QString ellipsize(const QString &text, int limit) const;
    QString hashText(const QString &text) const;
    bool swapClipboardTemporarily(const QString &text, const QString &previous);
    void persistSettings();
    void setRestoreGuard(const QString &text, int durationMs);
    bool shouldIgnoreRestoreGuard(const QString &hash);

    KlipperBridge *m_bridge = nullptr;
    TrimCore *m_core = nullptr;
    Settings m_settings;
    SettingsStore *m_store = nullptr;
    AutostartManager *m_autostart = nullptr;
    PortalPasteInjector *m_injector = nullptr;
    QTimer m_debounce;
    quint64 m_gen = 0;
    quint64 m_pendingGen = 0;
    QString m_lastWrittenHash;
    QString m_restoreGuardHash;
    qint64 m_restoreGuardExpiresMs = 0;
    QString m_lastOriginal;
    QString m_lastTrimmed;
    QString m_lastSummary;
    bool m_enabled = true;
};
