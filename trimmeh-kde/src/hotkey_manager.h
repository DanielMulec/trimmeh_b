#pragma once

#include <QObject>

class QAction;
class ClipboardWatcher;

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(ClipboardWatcher *watcher, QObject *parent = nullptr);

public slots:
    void syncFromWatcher();

private:
    void applyAction(QAction *action, bool enabled, const QString &sequence);

    ClipboardWatcher *m_watcher = nullptr;
    QAction *m_pasteTrimmed = nullptr;
    QAction *m_pasteOriginal = nullptr;
    QAction *m_toggleAutoTrim = nullptr;
};
