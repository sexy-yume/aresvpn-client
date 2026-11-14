#include "amneziaApplication.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickStyle>
#include <QResource>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTimer>
#include <QTranslator>
#include <QEvent>
#include <QDir>
#include <QSettings>
#include <QtQuick/QQuickWindow>  
#include <QWindow>     

#include "core/controllers/updateController.h"
#include "core/protocols/qmlRegisterProtocols.h"
#include "logger.h"
#include "ui/controllers/qml/pageController.h"
#include "ui/models/installedAppsModel.h"
#include "ui/utils/mtProxyPublicHostInput.h"
#include "version.h"
#include "core/utils/appUiConfig.h"

#include "platforms/ios/QRCodeReaderBase.h"
#ifdef Q_OS_IOS
    #include "platforms/ios/ioscontextmenu.h"
#endif

#ifdef Q_OS_ANDROID
#include "platforms/android/android_controller.h"
#endif
         

bool AmneziaApplication::m_forceQuit = false;

// See the header. One list, read by main.cpp and by the auto-connect handler, so the two can never
// disagree about what a harness run is.
bool AmneziaApplication::isHarnessMode()
{
#ifdef ARES_HARNESS
    const QStringList args = QCoreApplication::arguments();
    return args.contains(QStringLiteral("--qml-smoke"))
            || args.contains(QStringLiteral("--qml-shot"))
            || args.contains(QStringLiteral("--qml-drive"))
            || args.contains(QStringLiteral("--font-report"));
#else
    // The harness is not compiled in (ARES_HARNESS OFF, every release build): no flag makes one.
    return false;
#endif
}

AmneziaApplication::AmneziaApplication(int &argc, char *argv[]) : AMNEZIA_BASE_CLASS(argc, argv),
      m_optAutostart({QStringLiteral("a"), QStringLiteral("autostart")}, QStringLiteral("System autostart")),
      m_optCleanup  ({QStringLiteral("c"), QStringLiteral("cleanup")}, QStringLiteral("Cleanup logs")),
      m_optConnect  ({QStringLiteral("connect")}, QStringLiteral("Connect to server by index on startup"), QStringLiteral("index")),
      m_optImport   ({QStringLiteral("import")}, QStringLiteral("Import configuration from data string"), QStringLiteral("data"))
{
    setDesktopFileName(QStringLiteral(APPLICATION_NAME));
    setQuitOnLastWindowClosed(false);

    // Fix config file permissions
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    {
        QSettings s(ORGANIZATION_NAME, APPLICATION_NAME);
        s.setValue("permFixed", true);
    }

    QString configLoc1 = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first() + "/" + ORGANIZATION_NAME + "/"
            + APPLICATION_NAME + ".conf";
    QFile::setPermissions(configLoc1, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    QString configLoc2 = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first() + "/" + ORGANIZATION_NAME + "/"
            + APPLICATION_NAME + "/" + APPLICATION_NAME + ".conf";
    QFile::setPermissions(configLoc2, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif

    m_settings = new SecureQSettings(ORGANIZATION_NAME, APPLICATION_NAME, this);
    m_nam = new QNetworkAccessManager(this);
}

AmneziaApplication::~AmneziaApplication()
{
#ifdef AMNEZIA_DESKTOP
    if (m_vpnConnection && m_vpnConnectionThread.isRunning()) {
        QMetaObject::invokeMethod(m_vpnConnection.get(), "disconnectSlots", Qt::BlockingQueuedConnection);
        
        QMetaObject::invokeMethod(m_vpnConnection.get(), "disconnectFromVpn", Qt::BlockingQueuedConnection);
    }
#endif

    m_vpnConnectionThread.requestInterruption();
    m_vpnConnectionThread.quit();

    if (!m_vpnConnectionThread.wait(3000)) {
        m_vpnConnectionThread.terminate();
        m_vpnConnectionThread.wait(500);
    }

    if (m_engine) {
        delete m_engine;
    }
}

#ifdef Q_OS_ANDROID
namespace {
    static void clearQtCaches()
    {
        const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!cacheRoot.isEmpty()) {
            QDir(cacheRoot + "/QtShaderCache").removeRecursively();
            QDir(cacheRoot + "/qmlcache").removeRecursively();
        }
    }
}
#endif

void AmneziaApplication::init()
{
    m_engine = new QQmlApplicationEngine;

    const QUrl url(QStringLiteral(APP_QML_ENTRYPOINT));
    QObject::connect(
        m_engine, &QQmlApplicationEngine::objectCreated, this,
        [this, url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
                return;
            }
            // install filter on main window
            if (auto win = qobject_cast<QQuickWindow*>(obj)) {
                win->installEventFilter(this);
#ifdef Q_OS_ANDROID
                QObject::connect(win, &QQuickWindow::sceneGraphError,
                    [](QQuickWindow::SceneGraphError, const QString &msg) {
                        qWarning() << "Scene graph error (suppressed):" << msg;
                    });
                // Keep graphics context alive across hide/show cycles to avoid
                // eglSwapBuffers/makeCurrent being called on a context Android has reclaimed.
                win->setPersistentSceneGraph(true);
                win->setPersistentGraphics(true);
#endif
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
                win->show();
#else
                if (!m_coreController || !m_coreController->pageController()->shouldStartMinimized()) {
                    win->show();
                }
#endif
            }
        },
        Qt::QueuedConnection);

    m_engine->rootContext()->setContextProperty("Debug", &Logger::Instance());

#ifdef MACOS_NE
    m_engine->rootContext()->setContextProperty("IsMacOsNeBuild", true);
#else
    m_engine->rootContext()->setContextProperty("IsMacOsNeBuild", false);
#endif

#ifdef Q_OS_IOS
    m_engine->rootContext()->setContextProperty("IosContextMenu", new IosContextMenu(this));
#endif

#ifdef Q_OS_ANDROID
    m_engine->rootContext()->setContextProperty("IsPlayBuild", AndroidController::instance()->isPlay());
#else
    m_engine->rootContext()->setContextProperty("IsPlayBuild", false);
#endif

    m_vpnConnection.reset(new VpnConnection(nullptr, nullptr));
    m_vpnConnection->moveToThread(&m_vpnConnectionThread);
    m_vpnConnectionThread.start();

    m_coreController.reset(new CoreController(m_vpnConnection, m_settings, m_engine));

    m_engine->addImportPath(QStringLiteral(APP_QML_IMPORT_PATH));

    if (m_parser.isSet(m_optImport)) {
        const QString data = m_parser.value(m_optImport);
        if (!data.isEmpty()) {
            if (m_coreController) {
                m_coreController->importConfigFromData(data);
            }
        }
    }

#ifdef ARES_HARNESS
    runHarnessBeforeLoad();  // AresVPN Client: --ares-login; aresHarness.cpp
#endif

    m_engine->load(url);

    m_coreController->setQmlRoot();

#ifdef ARES_HARNESS
    runHarnessAfterLoad();  // AresVPN Client: --qml-smoke/-shot/-drive, --font-report; aresHarness.cpp
#endif

    m_coreController->checkForAppUpdates();

#ifdef Q_OS_WIN //TODO
    if (m_parser.isSet(m_optAutostart))
        m_coreController->pageController()->showOnStartup();
    else
        emit m_coreController->pageController()->raiseMainWindow();
#else
    m_coreController->pageController()->showOnStartup();
#endif

// Android TextArea clipboard workaround
// Text from TextArea always has "text/html" mime-type:
// /qt/6.6.1/Src/qtdeclarative/src/quick/items/qquicktextcontrol.cpp:1865
// Next, html is created for this mime-type:
// /qt/6.6.1/Src/qtdeclarative/src/quick/items/qquicktextcontrol.cpp:1885
// And this html goes to the Androids clipboard, i.e. text from TextArea is always copied as richText:
// /qt/6.6.1/Src/qtbase/src/plugins/platforms/android/androidjniclipboard.cpp:46
// So we catch all the copies to the clipboard and clear them from "text/html"
#ifdef Q_OS_ANDROID
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, []() {
        auto clipboard = QGuiApplication::clipboard();
        if (clipboard->mimeData()->hasHtml()) {
            clipboard->setText(clipboard->text());
        }
    });
#endif

    if (m_parser.isSet(m_optConnect)) {
        bool ok = false;
        int idx = m_parser.value(m_optConnect).toInt(&ok);
        if (ok) {
            QTimer::singleShot(0, this, [this, idx]() {
                if (m_coreController) {
                    m_coreController->openConnectionByIndex(idx);
                }
            });
        }
    }
}

void AmneziaApplication::registerTypes()
{
    qRegisterMetaType<ServerCredentials>("ServerCredentials");

    qRegisterMetaType<DockerContainer>("DockerContainer");
    using namespace amnezia::ProtocolEnumNS;
    qRegisterMetaType<TransportProto>("TransportProto");
    qRegisterMetaType<Proto>("Proto");
    qRegisterMetaType<ServiceType>("ServiceType");

    qmlRegisterType<QRCodeReader>("QRCodeReader", 1, 0, "QRCodeReader");

    m_containerProps.reset(new ContainerProps());
    qmlRegisterSingletonInstance("ContainerProps", 1, 0, "ContainerProps", m_containerProps.get());

    m_protocolProps.reset(new ProtocolProps());
    qmlRegisterSingletonInstance("ProtocolProps", 1, 0, "ProtocolProps", m_protocolProps.get());

    qmlRegisterSingletonType(QUrl("qrc:/ui/qml/Filters/ContainersModelFilters.qml"), "ContainersModelFilters", 1, 0,
                             "ContainersModelFilters");

    qmlRegisterType<InstalledAppsModel>("InstalledAppsModel", 1, 0, "InstalledAppsModel");

    qmlRegisterType<PublicHostInputValidator>("MtProxyConfig", 1, 0, "PublicHostInputValidator");
    qmlRegisterType<PublicHostInputValidator>("TelemtConfig", 1, 0, "PublicHostInputValidator");

    amnezia::declareQmlProtocolEnum();
    Vpn::declareQmlVpnConnectionStateEnum();
    PageLoader::declareQmlPageEnum();
    UpdateState::declareQmlUpdateStateEnum();
}

void AmneziaApplication::loadFonts()
{
    QQuickStyle::setStyle("Basic");

    QFontDatabase::addApplicationFont(QStringLiteral(APP_UI_FONT_RESOURCE));

    // AresVPN Client (AresProject ROADMAP 18-3h). PT Root UI and PT Mono carry NO HANGUL, and
    // this product's first market is Korea: rendered offscreen and LOOKED AT, every translated
    // string on every screen was a row of tofu boxes while the English ones were perfect. No
    // compiler, linter or name check can see that - it is why the screens are rendered at all.
    //
    // Registered here rather than in QML because `font.families` is not assignable on Text in
    // this Qt ("Cannot assign to non-existent property families" - it took the whole UI down for
    // one build, and --qml-smoke is what said so). QFont::insertSubstitutions is global, so every
    // `font.family: 'PT Root UI'` in the tree inherits the fallback from this one place.
    //
    // The font is NOT modified and NOT renamed, so PT Root UI's OFL duties are untouched
    // (#D180 rule 4): a substitution list is a matching hint, not a derivative work.
    const QStringList uiFallback = {QStringLiteral("Malgun Gothic"), QStringLiteral("Noto Sans CJK KR"),
                                    QStringLiteral("Microsoft YaHei"), QStringLiteral("Segoe UI")};
    const QStringList monoFallback = {QStringLiteral("Consolas"), QStringLiteral("D2Coding"),
                                      QStringLiteral("Malgun Gothic"), QStringLiteral("Noto Sans Mono CJK KR")};
    // Registered for the names the screens ACTUALLY ask for. These were "PT Root UI" and
    // "PT Mono" - neither of which is a family in this build, so the substitution was firing on
    // every draw and the bundled face never rendered. The exact family is "PT Root UI VF"
    // (upstream's own text types say so), and the mono request is Qt's generic `monospace`.
    QFont::insertSubstitutions(QStringLiteral("PT Root UI VF"), uiFallback);
    QFont::insertSubstitutions(QStringLiteral("monospace"), monoFallback);
}

bool AmneziaApplication::parseCommands()
{
    m_parser.setApplicationDescription(APPLICATION_NAME);
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(m_optAutostart);
    m_parser.addOption(m_optCleanup);
    m_parser.addOption(m_optConnect);
    m_parser.addOption(m_optImport);
#ifdef ARES_HARNESS
    addHarnessOptions();  // AresVPN Client: aresHarness.cpp
#endif
    
    m_parser.process(*this);

    if (m_parser.isSet(m_optCleanup)) {
        Logger::cleanUp();
        QTimer::singleShot(100, this, [this] { quit(); });
        exec();
        return false;
    }
    return true;
}

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
void AmneziaApplication::startLocalServer() {
    const QString serverName(APP_INSTANCE_NAME);
    QLocalServer::removeServer(serverName);

    QLocalServer *server = new QLocalServer(this);
    server->listen(serverName);

    QObject::connect(server, &QLocalServer::newConnection, this, [server, this]() {
        if (server) {
            QLocalSocket *clientConnection = server->nextPendingConnection();
            clientConnection->deleteLater();
        }
        emit m_coreController->pageController()->raiseMainWindow(); //TODO
    });
}
#endif

bool AmneziaApplication::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        quit();
#else
        if (m_forceQuit) {
            quit();
        } else {
            if (m_coreController && m_coreController->pageController()) {
                m_coreController->pageController()->hideMainWindow();
            }
        }
#endif
        return true; // eat the close
    }
    // call base QObject::eventFilter
    return QObject::eventFilter(watched, event);
}

void AmneziaApplication::forceQuit()
{
    m_forceQuit = true;
    quit();
}

QQmlApplicationEngine *AmneziaApplication::qmlEngine() const
{
    return m_engine;
}

QNetworkAccessManager *AmneziaApplication::networkManager()
{
    return m_nam;
}

QClipboard *AmneziaApplication::getClipboard()
{
    return this->clipboard();
}
