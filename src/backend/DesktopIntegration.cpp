#include "DesktopIntegration.h"
#include <QAction>
#include <QIcon>
#include <QGuiApplication>

DesktopIntegration::DesktopIntegration(QObject* parent)
    : QObject(parent), m_available(QSystemTrayIcon::isSystemTrayAvailable())
{
    m_tray.setIcon(QIcon(":/icons/packet-capture.ico"));
    m_show = m_menu.addAction(QString());
    m_capture = m_menu.addAction(QString());
    m_menu.addSeparator();
    m_exit = m_menu.addAction(QString());
    connect(m_show, &QAction::triggered, this, &DesktopIntegration::showRequested);
    connect(m_capture, &QAction::triggered, this, &DesktopIntegration::toggleCaptureRequested);
    connect(m_exit, &QAction::triggered, this, &DesktopIntegration::exitRequested);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) emit showRequested();
    });
    m_tray.setContextMenu(&m_menu);
    retranslate();
    if (m_available) m_tray.show();
}

void DesktopIntegration::setCanStart(bool enabled)
{
    if (m_canStart == enabled) return;
    m_canStart = enabled;
    retranslate();
    emit stateChanged();
}

void DesktopIntegration::setCapturing(bool capturing)
{
    if (m_capturing == capturing) return;
    m_capturing = capturing;
    retranslate();
    emit stateChanged();
}

void DesktopIntegration::retranslate()
{
    m_menu.setFont(QGuiApplication::font());
    m_show->setText(tr("显示主窗口"));
    m_capture->setText(m_capturing ? tr("停止采集") : tr("开始采集"));
    m_capture->setEnabled(m_capturing || m_canStart);
    m_exit->setText(tr("退出软件"));
    m_tray.setToolTip(tr("报文采集与分析") + " — " + (m_capturing ? tr("正在采集") : tr("已停止")));
}
