#pragma once

#include "clipboard_watcher.h"
#include "trim_core.h"

#include <QDialog>
#include <QTabWidget>

class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QCheckBox;
class QKeySequenceEdit;
class QPushButton;
class QGroupBox;
class PortalPasteInjector;
class QSpinBox;
class QProcess;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(ClipboardWatcher *watcher,
                      TrimCore *core,
                      PortalPasteInjector *injector = nullptr,
                      QWidget *parent = nullptr);

    void showAboutTab();

private slots:
    void refreshFromWatcher();
    void updateAggressivenessPreview();

private:
    void buildGeneralTab(QTabWidget *tabs);
    void buildAggressivenessTab(QTabWidget *tabs);
    void buildShortcutsTab(QTabWidget *tabs);
    void buildAboutTab(QTabWidget *tabs);
    void refreshPermission();
    void installCli();

    QString sampleForAggressiveness(const QString &level) const;
    QString trimmedPreviewFor(const QString &level) const;

    ClipboardWatcher *m_watcher = nullptr;
    TrimCore *m_core = nullptr;
    PortalPasteInjector *m_injector = nullptr;
    QTabWidget *m_tabs = nullptr;
    int m_aboutTabIndex = -1;

    QGroupBox *m_permissionGroup = nullptr;
    QLabel *m_permissionLabel = nullptr;
    QPushButton *m_permissionPermanentButton = nullptr;
    QPushButton *m_permissionSettingsButton = nullptr;
    QLabel *m_permissionStatus = nullptr;
    QSpinBox *m_restoreDelay = nullptr;

    QCheckBox *m_autoTrim = nullptr;
    QCheckBox *m_keepBlank = nullptr;
    QCheckBox *m_stripBox = nullptr;
    QCheckBox *m_trimPrompts = nullptr;
    QCheckBox *m_clipboardFallbacks = nullptr;
    QCheckBox *m_startAtLogin = nullptr;
    QCheckBox *m_pasteTrimmedHotkeyEnabled = nullptr;
    QCheckBox *m_pasteOriginalHotkeyEnabled = nullptr;
    QCheckBox *m_toggleAutoTrimHotkeyEnabled = nullptr;
    QKeySequenceEdit *m_pasteTrimmedHotkey = nullptr;
    QKeySequenceEdit *m_pasteOriginalHotkey = nullptr;
    QKeySequenceEdit *m_toggleAutoTrimHotkey = nullptr;

    QRadioButton *m_low = nullptr;
    QRadioButton *m_normal = nullptr;
    QRadioButton *m_high = nullptr;
    QLabel *m_blurb = nullptr;
    QPlainTextEdit *m_previewBefore = nullptr;
    QPlainTextEdit *m_previewAfter = nullptr;

    QPushButton *m_installCliButton = nullptr;
    QLabel *m_cliStatus = nullptr;
    QProcess *m_cliProcess = nullptr;
};
