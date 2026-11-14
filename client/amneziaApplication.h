#ifndef AMNEZIA_APPLICATION_H
#define AMNEZIA_APPLICATION_H

#include <QCommandLineParser>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
  #include <QGuiApplication>
#else
  #include <QApplication>
#endif
#include <QClipboard>

#include "core/controllers/coreController.h"
#include "secureQSettings.h"
#include "vpnConnection.h"
#include "ui/models/containerProps.h"
#include "ui/models/protocolProps.h"

#define amnApp (static_cast<AmneziaApplication *>(QCoreApplication::instance()))

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
  #define AMNEZIA_BASE_CLASS QGuiApplication
#else
  #define AMNEZIA_BASE_CLASS QApplication
#endif

class AmneziaApplication : public AMNEZIA_BASE_CLASS
{
    Q_OBJECT
public:
    AmneziaApplication(int &argc, char *argv[]);
    virtual ~AmneziaApplication();

    void init();
    void registerTypes();
    void loadFonts();
    bool parseCommands();

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
    void startLocalServer();
#endif

    QQmlApplicationEngine *qmlEngine() const;
    QNetworkAccessManager *networkManager();
    QClipboard *getClipboard();

public slots:
    void forceQuit();

private:
    static bool m_forceQuit;

public:
    // AresVPN Client - IS THIS PROCESS A MEASUREMENT RATHER THAN THE PRODUCT?
    //
    // It decides two things that must agree, and until now nothing decided them together:
    //   * main.cpp skips the single-instance guard, so a measurement can run beside the client the
    //     operator is using (measured: it could not, and it died printing NOTHING to stdout);
    //   * CoreSignalHandlers::initAutoConnectHandler() does NOT arm the auto-connect timer.
    //
    // THE SECOND ONE IS A SAFETY FIX ON MY OWN HARNESS. `--qml-drive` says in its own header that
    // Connect is never pressed, because an unattended tunnel with a kill switch has cost this
    // project the operator's internet once - and all the while the auto-connect handler was
    // starting a connection a second after every harness process launched, underneath the UI the
    // driver was carefully not touching. It only ever failed because the daemon service is not
    // running on this workstation (ErrorCode 103, AmneziaServiceNotRunning), which is luck, not
    // design. A process whose job is to measure a build must not bring up a tunnel.
    static bool isHarnessMode();
    QQmlApplicationEngine *m_engine {};
    SecureQSettings* m_settings;

    QScopedPointer<CoreController> m_coreController;

    QSharedPointer<ContainerProps> m_containerProps;
    QSharedPointer<ProtocolProps> m_protocolProps;

    QCommandLineParser m_parser;

    QCommandLineOption m_optAutostart;
    QCommandLineOption m_optCleanup;
    QCommandLineOption m_optConnect;
    QCommandLineOption m_optImport;

#ifdef ARES_HARNESS
    // AresVPN Client: the measurement modes, compiled in only with -DARES_HARNESS=ON and never into
    // a release build (client/CMakeLists.txt refuses ARES_RELEASE with ARES_HARNESS; audit H-19).
    // Defined in aresHarness.cpp, so upstream's file carries three hook lines and nothing else.
    QCommandLineOption m_optAresLogin {QStringLiteral("ares-login"), QStringLiteral("AresVPN Client: read id, password and idx as three lines from stdin, store the rent, exit")};
    QCommandLineOption m_optQmlSmoke {QStringLiteral("qml-smoke"), QStringLiteral("AresVPN Client: compile every page in PageEnum, print any QML error, exit 4 if any failed")};
    QCommandLineOption m_optQmlShot {QStringLiteral("qml-shot"), QStringLiteral("AresVPN Client: navigate to each named page and write a PNG of the window into <dir>, then exit"), QStringLiteral("dir")};
    QCommandLineOption m_optQmlDrive {QStringLiteral("qml-drive"), QStringLiteral("AresVPN Client: click through the #D182 walkthrough, writing a PNG per step into <dir>, then exit"), QStringLiteral("dir")};
    QCommandLineOption m_optFontReport {QStringLiteral("font-report"), QStringLiteral("AresVPN Client: list the font families this process can see and whether they carry Hangul, then exit")};
    void addHarnessOptions();
    void runHarnessBeforeLoad();  // --ares-login: exits when set
    void runHarnessAfterLoad();   // --qml-smoke, --qml-shot, --font-report, --qml-drive: exit when set
#endif

    QSharedPointer<VpnConnection> m_vpnConnection;
    QThread m_vpnConnectionThread;

    QNetworkAccessManager *m_nam;
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // AMNEZIA_APPLICATION_H
