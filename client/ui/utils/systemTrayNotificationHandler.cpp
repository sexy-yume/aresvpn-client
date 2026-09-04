/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QDebug>
#include "systemTrayNotificationHandler.h"


#ifdef Q_OS_MAC
#  include "platforms/macos/macosstatusicon.h"
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QIcon>
#include <QWindow>

#include "version.h"

SystemTrayNotificationHandler::SystemTrayNotificationHandler(QObject* parent) :
    NotificationHandler(parent)
#ifndef Q_OS_MAC
    , m_systemTrayIcon(parent)
#endif
{
    m_trayActionShow =  m_menu.addAction(tr("Show") + " " + APPLICATION_NAME, this, [this](){
        emit raiseRequested();
    });
    m_menu.addSeparator();
    m_trayActionConnect = m_menu.addAction(tr("Connect"), this, [this](){ emit connectRequested(); });
    m_trayActionDisconnect = m_menu.addAction(tr("Disconnect"), this, [this](){ emit disconnectRequested(); });

    m_menu.addSeparator();

    m_trayActionVisitWebSite = m_menu.addAction(tr("Visit Website"), [&](){
        QDesktopServices::openUrl(QUrl(websiteUrl));
    });

    // Quit action: disconnect VPN first on macOS NE, else quit directly
    // The three menu images upstream names - tray/application.png, tray/link.png,
    // tray/cancel.png - are in neither the resource file nor the tree. A QIcon built from a
    // missing resource is silently empty, so the actions lost their pictures and nothing said so.
    // Naming a file that is not there reads as intent; asking for no icon is honest.
    m_trayActionQuit = m_menu.addAction(tr("Quit") + " " + APPLICATION_NAME,
                                       this,
                                       [&](){ qApp->quit(); });

#ifdef Q_OS_MAC
    // QSystemTrayIcon::setContextMenu crashes on macOS 14+: its menu-tracking
    // observer reads -[NSEvent clickCount] off a non-mouse event. Own the
    // NSStatusItem and attach the native NSMenu instead.
    m_statusIcon = new MacOSStatusIcon(this);
    m_statusIcon->setMenu(&m_menu);
#else
    connect(&m_systemTrayIcon, &QSystemTrayIcon::activated, this,
            &SystemTrayNotificationHandler::onTrayActivated);
    m_systemTrayIcon.setContextMenu(&m_menu);
#endif
    // THE ICON IS SET BEFORE THE TRAY ITEM IS SHOWN. It used to be the other way round, and Qt
    // says so on every launch: `QSystemTrayIcon::setVisible: No Icon set`. That line was in every
    // harness capture for two sessions and I read past it as noise, until the operator hit the
    // consequence - *따로 GUI켜져있는게 보이는게 없었는데 프로세스에는 남아있었네*. Closing the
    // window leaves the app running in a tray it never drew, so there is no window to raise, no
    // menu to quit from, and nothing on screen to say the program is still there.
    setTrayState(Vpn::ConnectionState::Disconnected);
#ifndef Q_OS_MAC
    m_systemTrayIcon.show();
#endif
}

SystemTrayNotificationHandler::~SystemTrayNotificationHandler() {
#ifdef Q_OS_MAC
    delete m_statusIcon;  // before m_menu: the status item references its NSMenu
    m_statusIcon = nullptr;
#endif
}

void SystemTrayNotificationHandler::setConnectionState(Vpn::ConnectionState state)
{
    setTrayState(state);
    NotificationHandler::setConnectionState(state);
}

void SystemTrayNotificationHandler::onTranslationsUpdated()
{
    m_trayActionShow->setText(tr("Show") + " " + APPLICATION_NAME);
    m_trayActionConnect->setText(tr("Connect"));
    m_trayActionDisconnect->setText(tr("Disconnect"));
    m_trayActionVisitWebSite->setText(tr("Visit Website"));
    m_trayActionQuit->setText(tr("Quit")+ " " + APPLICATION_NAME);
}

void SystemTrayNotificationHandler::updateWebsiteUrl(const QString &newWebsiteUrl) {
    qDebug() << "Updated website URL:" << newWebsiteUrl;
    websiteUrl = newWebsiteUrl;
}

void SystemTrayNotificationHandler::setTrayIcon(const QString &iconPath)
{
#ifdef Q_OS_MAC
    m_statusIcon->setIcon(iconPath);
#else
    // AND NOT AS A MASK. `QIcon::setIsMask` is macOS's template-image idea - the system recolours
    // a silhouette to match the menu bar - and upstream applies it in the NOT-macOS branch, where
    // it does the opposite of the intent: Windows renders the icon as a flat silhouette of its
    // alpha channel, which for this mark is a shape that disappears against half the taskbar
    // themes. The image is used as it is.
    m_systemTrayIcon.setIcon(QIcon(iconPath));
#endif
}

void SystemTrayNotificationHandler::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        emit raiseRequested();
    }
}

void SystemTrayNotificationHandler::setTrayState(Vpn::ConnectionState state)
{
    QString resourcesPath = ":/images/tray/%1";

    switch (state) {
    case Vpn::ConnectionState::Disconnected:
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
        m_trayActionConnect->setEnabled(true);
        m_trayActionDisconnect->setEnabled(false);
        break;
    case Vpn::ConnectionState::Preparing:
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        break;
    case Vpn::ConnectionState::Connecting:
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        break;
    case Vpn::ConnectionState::Connected:
        setTrayIcon(QString(resourcesPath).arg(ConnectedTrayIconName));
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        break;
    case Vpn::ConnectionState::Disconnecting:
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        break;
    case Vpn::ConnectionState::Reconnecting:
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        break;
    case Vpn::ConnectionState::Error:
        setTrayIcon(QString(resourcesPath).arg(ErrorTrayIconName));
        m_trayActionConnect->setEnabled(true);
        m_trayActionDisconnect->setEnabled(false);
        break;
    case Vpn::ConnectionState::Unknown:
    default:
        m_trayActionConnect->setEnabled(false);
        m_trayActionDisconnect->setEnabled(true);
        setTrayIcon(QString(resourcesPath).arg(DisconnectedTrayIconName));
    }

    //#ifdef Q_OS_MAC
    //    // Get theme from current user (note, this app can be launched as root application and in this case this theme can be different from theme of real current user )
    //    bool darkTaskBar = MacOSFunctions::instance().isMenuBarUseDarkTheme();
    //    darkTaskBar = forceUseBrightIcons ? true : darkTaskBar;
    //    resourcesPath = ":/images_mac/tray_icon/%1";
    //    useIconName = useIconName.replace(".png", darkTaskBar ? "@2x.png" : " dark@2x.png");
    //#endif
}


void SystemTrayNotificationHandler::notify(NotificationHandler::Message type,
                                           const QString& title,
                                           const QString& message,
                                           int timerMsec) {
  Q_UNUSED(type);

#ifdef Q_OS_MAC
  Q_UNUSED(timerMsec);
  m_statusIcon->showMessage(title, message);
#else
  QIcon icon(ConnectedTrayIconName);
  m_systemTrayIcon.showMessage(title, message, icon, timerMsec);
#endif
}

void SystemTrayNotificationHandler::showHideWindow() {
//  QmlEngineHolder* engine = QmlEngineHolder::instance();
//  if (engine->window()->isVisible()) {
//    engine->hideWindow();
//#ifdef MVPN_MACOS
//    MacOSUtils::hideDockIcon();
//#endif
//  } else {
//    engine->showWindow();
//#ifdef MVPN_MACOS
//    MacOSUtils::showDockIcon();
//#endif
//  }
}

