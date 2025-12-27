#pragma once

#include "clipboard_watcher.h"
#include "trim_core.h"

#include <QDialog>
#include <QTabWidget>

class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QCheckBox;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(ClipboardWatcher *watcher, TrimCore *core, QWidget *parent = nullptr);

private slots:
    void refreshFromWatcher();
    void updateAggressivenessPreview();

private:
    void buildGeneralTab(QTabWidget *tabs);
    void buildAggressivenessTab(QTabWidget *tabs);
    void buildShortcutsTab(QTabWidget *tabs);
    void buildAboutTab(QTabWidget *tabs);

    QString sampleForAggressiveness(const QString &level) const;
    QString trimmedPreviewFor(const QString &level) const;

    ClipboardWatcher *m_watcher = nullptr;
    TrimCore *m_core = nullptr;

    QCheckBox *m_autoTrim = nullptr;
    QCheckBox *m_keepBlank = nullptr;
    QCheckBox *m_stripBox = nullptr;
    QCheckBox *m_trimPrompts = nullptr;

    QRadioButton *m_low = nullptr;
    QRadioButton *m_normal = nullptr;
    QRadioButton *m_high = nullptr;
    QLabel *m_blurb = nullptr;
    QPlainTextEdit *m_previewBefore = nullptr;
    QPlainTextEdit *m_previewAfter = nullptr;
};
