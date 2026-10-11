#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

class DesktopIntegration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool canStart READ canStart WRITE setCanStart NOTIFY stateChanged)
public:
    explicit DesktopIntegration(QObject* parent = nullptr);
    bool trayAvailable() const { return m_available; }
    bool canStart() const { return m_canStart; }
    QMenu* menu() { return &m_menu; }
    void setCanStart(bool enabled);
    Q_INVOKABLE void setCapturing(bool capturing);
    void retranslate();
signals:
    void showRequested();
    void toggleCaptureRequested();
    void exitRequested();
    void stateChanged();
private:
    bool m_available;
    bool m_capturing = false;
    bool m_canStart = false;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
    QAction* m_show;
    QAction* m_capture;
    QAction* m_exit;
};
