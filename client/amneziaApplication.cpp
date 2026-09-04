#include "amneziaApplication.h"

#include <QClipboard>
#include <QTextStream>  // AresVPN Client: --ares-login
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMimeData>
#include <QImage>
#include <QSet>
#include <functional>
#include <QQmlComponent>
#include <QMetaEnum>
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
#include "ui/utils/qmlDriver.h"
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
    const QStringList args = QCoreApplication::arguments();
    return args.contains(QStringLiteral("--qml-smoke"))
            || args.contains(QStringLiteral("--qml-shot"))
            || args.contains(QStringLiteral("--qml-drive"))
            || args.contains(QStringLiteral("--font-report"));
}

AmneziaApplication::AmneziaApplication(int &argc, char *argv[]) : AMNEZIA_BASE_CLASS(argc, argv),
      m_optAutostart({QStringLiteral("a"), QStringLiteral("autostart")}, QStringLiteral("System autostart")),
      m_optCleanup  ({QStringLiteral("c"), QStringLiteral("cleanup")}, QStringLiteral("Cleanup logs")),
      m_optConnect  ({QStringLiteral("connect")}, QStringLiteral("Connect to server by index on startup"), QStringLiteral("index")),
      m_optImport   ({QStringLiteral("import")}, QStringLiteral("Import configuration from data string"), QStringLiteral("data")),
      m_optAresLogin({QStringLiteral("ares-login")}, QStringLiteral("AresVPN Client: read id, password and idx as three lines from stdin, store the rent, exit")),
      m_optQmlSmoke ({QStringLiteral("qml-smoke")}, QStringLiteral("AresVPN Client: compile every page in PageEnum, print any QML error, exit 4 if any failed")),
      m_optQmlShot  ({QStringLiteral("qml-shot")}, QStringLiteral("AresVPN Client: navigate to each named page and write a PNG of the window into <dir>, then exit"), QStringLiteral("dir")),
      m_optQmlDrive ({QStringLiteral("qml-drive")}, QStringLiteral("AresVPN Client: click through the #D182 walkthrough, writing a PNG per step into <dir>, then exit"), QStringLiteral("dir")),
      m_optFontReport({QStringLiteral("font-report")}, QStringLiteral("AresVPN Client: list the font families this process can see and whether they carry Hangul, then exit"))
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

    if (m_parser.isSet(m_optAresLogin) && m_coreController) {
        // AresVPN Client: headless login, the test vector for /api/profile (AresProject ROADMAP 18-3c)
        QTextStream in(stdin);
        const QString id = in.readLine();
        const QString pw = in.readLine();
        const QString idx = in.readLine();
        const AresProfileController::Result r = m_coreController->aresProfileController()->fetchAndImport(id, pw, idx);
        QTextStream out(stdout);
        if (r.errorCode == amnezia::ErrorCode::NoError) {
            out << "ARES-LOGIN OK rent=" << r.rentId << " protocol=" << r.protocol << " server=" << r.serverId << Qt::endl;
            ::exit(0);
        }
        out << "ARES-LOGIN REFUSED code=" << static_cast<int>(r.errorCode) << " message=" << r.message << Qt::endl;
        ::exit(3);
    }

    m_engine->load(url);

    m_coreController->setQmlRoot();

    if (m_parser.isSet(m_optQmlSmoke)) {
        // AresVPN Client (AresProject ROADMAP 18-3h, #L054). Every page except the start tree is
        // loaded ON DEMAND, so starting the app - even offscreen - compiles three files and says
        // nothing about the other sixty-eight. `onAccent` was a COMPILE error in a singleton and
        // it made the whole UI fail to load; qmllint, a name cross-check and three RC=0 builds all
        // said green. This walks PageEnum and asks the engine to compile each page, here, where
        // the context properties the pages bind to already exist.
        //
        // COMPILE, not create: instantiating a page would run its bindings and call controllers,
        // which is a side effect a check must not have (#L023). The class this catches is exactly
        // the class that bit - unresolved types, illegal property names, bad attached properties.
        QTextStream out(stdout);
        const QMetaEnum pages = QMetaEnum::fromType<PageLoader::PageEnum>();
        int compiled = 0;
        int failed = 0;
        QStringList noFile;
        for (int i = 0; i < pages.keyCount(); ++i) {
            const QString name = QString::fromLatin1(pages.key(i));
            const QString path = QStringLiteral(APP_QML_PAGES_PREFIX) + name + QStringLiteral(".qml");
            QQmlComponent component(m_engine, QUrl(path));
            if (component.isError()) {
                // TWO DIFFERENT THINGS, and lumping them would make this check unusable.
                // "No such file" means upstream has a PageEnum key with no page behind it -
                // measured 2026-09-04: PageAbout, PageProtocolIKev2Settings and
                // PageSettingsLanguage, and NO QML in this tree names any of the three, so
                // nothing can navigate to them. Reported, not failed.
                // Anything else is a QML error in a page that exists, which is the class this
                // check was written for (#L054).
                bool missing = false;
                const QList<QQmlError> errors = component.errors();
                for (const QQmlError &e : errors) {
                    if (e.toString().contains(QStringLiteral("No such file or directory"))) {
                        missing = true;
                    }
                }
                if (missing) {
                    noFile.append(name);
                    continue;
                }
                ++failed;
                out << "QML-SMOKE FAIL " << name << Qt::endl;
                for (const QQmlError &e : errors) {
                    out << "    " << e.toString() << Qt::endl;
                }
            } else {
                ++compiled;
            }
        }
        if (!noFile.isEmpty()) {
            out << "QML-SMOKE " << noFile.size() << " PageEnum key(s) with no page file (upstream's, "
                << "unreferenced by any QML, so unreachable): " << noFile.join(QStringLiteral(", "))
                << Qt::endl;
        }
        // A floor, because a walk that found no pages would print "0 failed" and look clean
        // (#L041). PageEnum has dozens of entries; anything under ten is a broken walk.
        if (pages.keyCount() < 10) {
            out << "QML-SMOKE BROKEN: PageEnum yielded " << pages.keyCount()
                << " keys - that is a broken walk, not a clean tree" << Qt::endl;
            ::exit(3);
        }
        out << "QML-SMOKE " << compiled << " page(s) compiled, " << failed << " failed" << Qt::endl;
        ::exit(failed ? 4 : 0);
    }

    if (m_parser.isSet(m_optQmlShot)) {
        // AresVPN Client (AresProject ROADMAP 18-3h). "It compiles" is not "a customer can use
        // it": a page whose text is the same colour as its background, whose rows overlap or
        // whose button is off the bottom edge compiles perfectly. This navigates the REAL shell
        // to each screen and writes what the renderer produced, so the screens can be LOOKED at
        // without a window ever appearing on anybody's desktop (#L053).
        //
        // Needs -platform offscreen and QT_QUICK_BACKEND=software: the offscreen platform has no
        // GPU surface, and the software renderer is what makes grabWindow() return pixels rather
        // than an empty image. The harness sets both; if it did not, the images would be blank
        // and blank would look like a rendered screen (#L009).
        QTextStream out(stdout);
        const QString dir = m_parser.value(m_optQmlShot);
        QDir().mkpath(dir);

        QQuickWindow *window = nullptr;
        const QList<QObject *> roots = m_engine->rootObjects();
        for (QObject *o : roots) {
            if (auto *w = qobject_cast<QQuickWindow *>(o)) {
                window = w;
                break;
            }
        }
        if (!window) {
            out << "QML-SHOT BROKEN: no QQuickWindow among " << roots.size() << " root object(s)" << Qt::endl;
            ::exit(3);
        }
        window->resize(420, 780);

        // ON A REAL PLATFORM PLUGIN, THE WINDOW GOES OFF-SCREEN AND TAKES NO FOCUS.
        //
        // Everything about this harness is built for `-platform offscreen`, where no window exists
        // to bother anybody (#L053). But the offscreen platform sees exactly ONE font family - the
        // bundled PT Root UI VF - so every Korean string renders as tofu whatever the code does,
        // and the render checks are structurally unable to say anything about the market this
        // product ships to first. That was measured from both sides (--font-report, and loading
        // Malgun Gothic by hand) and written down as a limit.
        //
        // The `windows` plugin has the system font database AND per-glyph fallback, so running
        // this same walk on it is the only thing that answers the question. What must NOT happen
        // is a window appearing on the operator's desktop and taking their focus while they work -
        // #L053 is about exactly that class of harm, and its remedy is "prefer a channel with no
        // shared resource". So on any platform other than offscreen the window is moved far
        // outside every monitor and told not to accept focus: it renders with the real platform's
        // fonts, grabWindow() still returns its pixels, and the desktop the operator is using is
        // never touched.
        if (QGuiApplication::platformName() != QLatin1String("offscreen")) {
            window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
            window->setPosition(-32000, -32000);
            out << "QML-SHOT platform=" << QGuiApplication::platformName()
                << " - the window is placed off every monitor and refuses focus" << Qt::endl;
        }
        window->show();

        const QMetaEnum pages = QMetaEnum::fromType<PageLoader::PageEnum>();
        const QStringList wanted = {
            QStringLiteral("PageHome"),
            QStringLiteral("PageAresSession"),
            QStringLiteral("PageSetupWizardAresLogin"),
            QStringLiteral("PageSettings"),
            QStringLiteral("PageSettingsConnection"),
            QStringLiteral("PageSettingsApplication"),
            QStringLiteral("PageSettingsAbout"),
            QStringLiteral("PageSettingsLicenses"),
        };
        int written = 0;
        int blank = 0;
        int licenceFailures = 0;
        for (const QString &name : wanted) {
            const int value = pages.keyToValue(name.toLatin1().constData());
            if (value < 0) {
                out << "QML-SHOT SKIP " << name << " - not a PageEnum key" << Qt::endl;
                continue;
            }
            m_coreController->pageController()->goToPage(static_cast<PageLoader::PageEnum>(value), false);
            // let the push, the bindings and one frame happen
            for (int i = 0; i < 12; ++i) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 60);
            }
            const QImage shot = window->grabWindow();
            const QString path = dir + QStringLiteral("/") + name + QStringLiteral(".png");
            if (shot.isNull() || !shot.save(path)) {
                out << "QML-SHOT FAIL " << name << " - grab or save failed" << Qt::endl;
                continue;
            }
            // A uniform image is a screen that drew nothing, and it must not read as a success.
            // Counting distinct colours over a coarse grid is enough to tell "a screen" from
            // "one flat rectangle" without pretending to judge the design.
            QSet<QRgb> colours;
            for (int y = 0; y < shot.height(); y += 7) {
                for (int x = 0; x < shot.width(); x += 7) {
                    colours.insert(shot.pixel(x, y));
                }
            }
            if (colours.size() < 4) {
                ++blank;
                out << "QML-SHOT BLANK " << name << " - " << colours.size()
                    << " distinct colour(s); it rendered nothing" << Qt::endl;
            } else {
                ++written;
                out << "QML-SHOT " << name << " -> " << path << "  " << shot.width() << "x"
                    << shot.height() << ", " << colours.size() << " distinct colours" << Qt::endl;
            }

            // THE LICENCE READER, opened WITHOUT a press (AresProject 18-3d).
            //
            // GPL-3 section 5 is only satisfied if a user can actually READ the licence, and the
            // list screen above proves only that four rows drew. The text is loaded from a Qt
            // resource with XMLHttpRequest, which either works or silently yields an empty string,
            // and an empty reader looks exactly like a licence with no text. So this opens the
            // first document by setting the page's `selected` property - a property write, not a
            // synthetic click, because the walkthrough's presses are flaky and may not be a gate
            // (#L056) - and then ASSERTS on the character count that came back.
            //
            // The page is found by the objectName StackView actually leaves on it, which is the
            // source URL; the `objectName: "page:..."` a page sets for itself does not survive the
            // push (measured - see qmlDriver.cpp::collectPages).
            if (name == QStringLiteral("PageSettingsLicenses")) {
                QQuickItem *page = nullptr;
                std::function<void(QQuickItem *)> findPage = [&](QQuickItem *item) {
                    if (!item || page) {
                        return;
                    }
                    if (item->isVisible()
                        && item->objectName() == QStringLiteral("qrc:/ui/qml/Pages2/PageSettingsLicenses.qml")) {
                        page = item;
                        return;
                    }
                    for (QQuickItem *kid : item->childItems()) {
                        findPage(kid);
                    }
                };
                findPage(window->contentItem());

                if (!page) {
                    ++licenceFailures;
                    out << "QML-SHOT LICENCE FAIL - the licences page is not in the visible tree"
                        << Qt::endl;
                } else if (!page->setProperty("selected", 0)) {
                    ++licenceFailures;
                    out << "QML-SHOT LICENCE FAIL - the page has no `selected` property to set"
                        << Qt::endl;
                } else {
                    for (int i = 0; i < 12; ++i) {
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 60);
                    }

                    QQuickItem *textItem = nullptr;
                    std::function<void(QQuickItem *)> findText = [&](QQuickItem *item) {
                        if (!item || textItem) {
                            return;
                        }
                        if (item->objectName() == QStringLiteral("licenses.text")) {
                            textItem = item;
                            return;
                        }
                        for (QQuickItem *kid : item->childItems()) {
                            findText(kid);
                        }
                    };
                    findText(page);

                    const QString loaded = textItem ? textItem->property("text").toString() : QString();
                    // The GPL-3 is 35 823 bytes on disk. The floor is deliberately far below that
                    // and far ABOVE the ~250-character "could not be read" message the page falls
                    // back to, so the two outcomes cannot be confused - and it is the fallback,
                    // not an empty string, that a broken resource produces.
                    const int floorChars = 10000;
                    if (loaded.size() < floorChars) {
                        ++licenceFailures;
                        out << "QML-SHOT LICENCE FAIL - the GPL-3 reader holds " << loaded.size()
                            << " character(s), under the " << floorChars << " floor. First 120: "
                            << loaded.left(120).replace(QLatin1Char('\n'), QLatin1Char(' ')) << Qt::endl;
                    } else {
                        const QImage reading = window->grabWindow();
                        const QString rpath = dir + QStringLiteral("/PageSettingsLicenses-reading.png");
                        reading.save(rpath);
                        out << "QML-SHOT LICENCE ok - the GPL-3 reader holds " << loaded.size()
                            << " characters, opens with \""
                            << loaded.left(38).replace(QLatin1Char('\n'), QLatin1Char(' ')).trimmed()
                            << "\" -> " << rpath << Qt::endl;
                    }
                }
            }
        }
        out << "QML-SHOT " << written << " screen(s) rendered, " << blank << " blank, "
            << licenceFailures << " licence-reader failure(s)" << Qt::endl;
        ::exit((blank || licenceFailures) ? 5 : 0);
    }

    // HARNESS ONLY, and only under the flags that look at pixels. The offscreen platform loads no
    // system fonts - measured, one family in the whole process - so every Korean string renders as
    // tofu and the render checks can say nothing about the market this product ships to first.
    // Loading a Hangul face from the system directory here lets them say something. It is NOT a
    // product change: a shipped run never takes this path, and the real fallback is the
    // substitution list in loadFonts().
    if (m_parser.isSet(m_optQmlShot) || m_parser.isSet(m_optQmlDrive) || m_parser.isSet(m_optFontReport)) {
        for (const QString &face : {QStringLiteral("C:/Windows/Fonts/malgun.ttf"),
                                    QStringLiteral("/usr/share/fonts/truetype/nanum/NanumGothic.ttf")}) {
            if (QFile::exists(face)) {
                QFontDatabase::addApplicationFont(face);
            }
        }
    }

    if (m_parser.isSet(m_optFontReport)) {
        // MEASURE, do not infer. The screens render every Hangul string as tofu, and I concluded
        // from ONE probe - setting the family straight to 'Malgun Gothic' changed nothing - that
        // the offscreen platform has no system font database. That is an INFERENCE from a single
        // observation, which is exactly what #L004 says not to act on. This asks the font database
        // directly: what families are there, and can the resolved font actually draw Hangul?
        QTextStream out(stdout);
        const QStringList families = QFontDatabase::families();
        out << "FONT-REPORT " << families.size() << " family(ies) in the database" << Qt::endl;

        const QStringList wanted = {QStringLiteral("PT Root UI VF"), QStringLiteral("PT Root UI"),
                                    QStringLiteral("Malgun Gothic"), QStringLiteral("Segoe UI"),
                                    QStringLiteral("Noto Sans CJK KR"), QStringLiteral("Gulim"),
                                    QStringLiteral("Batang"), QStringLiteral("Dotum")};
        for (const QString &name : wanted) {
            out << "  " << (families.contains(name) ? "present " : "ABSENT  ") << name << Qt::endl;
        }

        // The question that actually matters: given the family the screens ask for, does the font
        // Qt resolves have a glyph for a Hangul syllable? QFontMetrics::inFont answers it.
        const QChar hangul(0xC5F0);  // 연, the first character of "연결" (Connect)
        for (const QString &name : {QStringLiteral("PT Root UI VF"), QStringLiteral("PT Root UI"),
                                    QStringLiteral("monospace"), QStringLiteral("Malgun Gothic")}) {
            QFont font(name);
            const QFontMetrics metrics(font);
            const QFontInfo info(font);
            out << "  " << name << " -> resolved to \"" << info.family() << "\", Hangul glyph: "
                << (metrics.inFont(hangul) ? "YES" : "no") << Qt::endl;
        }
        out << "FONT-REPORT done" << Qt::endl;
        ::exit(0);
    }

    if (m_parser.isSet(m_optQmlDrive)) {
        // AresVPN Client (AresProject ROADMAP 18-3h). Rendering a screen says it DRAWS; it does
        // not say a customer can get anywhere. This presses the controls, in the real shell, and
        // asserts where each press lands - #D182's two rules walked rather than read.
        //
        // Events are POSTED to our own window inside our own offscreen process. That is the
        // distinction #L053 turns on: host-global injection carries no window handle and lands
        // wherever focus is, which is how the operator's terminal received my typing. Nothing
        // here is shared with anybody.
        QTextStream out(stdout);
        QQuickWindow *window = nullptr;
        const QList<QObject *> roots = m_engine->rootObjects();
        for (QObject *o : roots) {
            if (auto *w = qobject_cast<QQuickWindow *>(o)) {
                window = w;
                break;
            }
        }
        if (!window) {
            out << "QML-DRIVE BROKEN: no QQuickWindow" << Qt::endl;
            ::exit(3);
        }
        window->resize(420, 780);
        window->show();
        // ACTIVATE IT. QWindowSystemInterface routes a synthetic press the way the platform
        // routes a real one, and the platform delivers to the ACTIVE window - show() does not make
        // a window active on the offscreen plugin. Every red run failed from the first press
        // onwards, and an activation that had not happened yet is the shape that produces exactly
        // that: sometimes it has settled by the time the first click goes, sometimes it has not.
        window->requestActivate();
        // AND WAIT FOR IT ON A REAL EVENT LOOP, AND SAY WHETHER IT HAPPENED.
        //
        // The first version of this wait spun `processEvents` - which is the exact mistake
        // `QmlDriver::settle()` exists to correct: it drains the queue but does not run the loop
        // the platform's own delivery and the animation driver are stepped from, so activation
        // could sit pending for the whole wait. Worse, the loop was SILENT when it gave up: a run
        // that begins with an inactive window is a run whose first press may go nowhere, and that
        // is precisely the residual flakiness this harness has (#L056's update: five runs gave
        // green, red, green, red, green). A harness may not hide the condition that explains its
        // own variance (#L030: a check that fails for an environmental reason must NAME it).
        {
            bool active = window->isActive();
            for (int i = 0; i < 40 && !active; ++i) {
                QEventLoop wait;
                QTimer::singleShot(25, &wait, &QEventLoop::quit);
                wait.exec();
                active = window->isActive();
            }
            out << "QML-DRIVE window active=" << (active ? "yes" : "NO - presses may not land")
                << ", exposed=" << (window->isExposed() ? "yes" : "no") << Qt::endl;
        }

        QmlDriver drive(window, m_parser.value(m_optQmlDrive));
        // WARM UP PROPERLY. Measured over three consecutive runs: run 1 after a build failed ten
        // steps and runs 2 and 3 were clean. The first run is cold - the QML disk cache is being
        // written, models are filling, translations loading - and a press sent into that goes
        // nowhere. A check that fails one run in three teaches a reader to re-run it (#L033), so
        // the warm-up waits for the app to be quiet rather than for a fixed 500 ms.
        drive.settle(40);
        for (int i = 0; i < 20 && drive.currentPage().startsWith(QStringLiteral("(")); ++i) {
            drive.settle(20);
        }
        drive.settle(40);

        // Read the controllers through the QML CONTEXT rather than through CoreController's
        // accessors: those are protected, and widening an upstream header for a test hook is a
        // merge cost for nothing (#D177 rule 3). The context property is the same object the QML
        // binds to, which is also the object a customer's click reaches.
        auto contextObject = [this](const char *name) -> QObject * {
            return qvariant_cast<QObject *>(m_engine->rootContext()->contextProperty(QString::fromLatin1(name)));
        };
        QObject *serversUi = contextObject("ServersUiController");
        QObject *settingsUi = contextObject("SettingsController");
        if (!serversUi || !settingsUi) {
            out << "QML-DRIVE BROKEN: a context controller is missing" << Qt::endl;
            ::exit(3);
        }
        auto autoConnectEnabled = [settingsUi]() -> bool {
            bool value = false;
            QMetaObject::invokeMethod(settingsUi, "isAutoConnectEnabled", Q_RETURN_ARG(bool, value));
            return value;
        };
        const bool hasRent = !serversUi->property("defaultServerId").toString().isEmpty();
        out << "QML-DRIVE starting on " << drive.currentPage()
            << (hasRent ? " (a rent is stored)" : " (no rent stored)") << Qt::endl;
        drive.shot(QStringLiteral("start"));

        if (hasRent) {
            // #D182 rule 2: a rent stored means the home screen, and Rents is a SECOND screen the
            // customer visits when they want to - never a step on the way to connecting.
            drive.expectPage(QStringLiteral("page:PageHome"));
            drive.expectExists(QStringLiteral("home.connect"));
            drive.reportTruncated(QStringLiteral("the home screen"));

            // FIRST ACTION, deliberately: home.settings is an ImageButtonType, the same control
            // type as the trash button that would not act. Pressing it BEFORE any navigation
            // separates the two models that survived - "this control type never receives a
            // synthetic press" and "nothing acts after a page change" - and one run decides it.
            drive.clickTo(QStringLiteral("home.settings"), QStringLiteral("page:PageSettings"));
            m_coreController->pageController()->goToPageHome();
            drive.settle(20);
            drive.expectPage(QStringLiteral("page:PageHome"));

            drive.clickTo(QStringLiteral("home.rents"), QStringLiteral("page:PageAresSession"));
            drive.reportTruncated(QStringLiteral("the rent list"));
            drive.shot(QStringLiteral("rents"));

            // THE LAST DISCRIMINATOR. Navigation FROM the startup page works every time and
            // clicks ON a pushed page never do. If PageHome is still VISIBLE while the rent list
            // is on top, the push has not finished and both pages are live - which is a harness
            // condition (a transition that no event loop is driving to completion), not a defect
            // a customer would meet. If PageHome is gone, the push did finish and the dead clicks
            // are the product's, which is a very different answer.
            drive.expectExists(QStringLiteral("home.connect"), false);

            // The destructive path, as far as the confirmation and no further. The dialog is
            // MODAL, so the run must dismiss it before anything else - the first version did not,
            // and every later step failed against a screen the customer could not have left
            // either. That the failures were real is the point: the dialog does block.
            if (drive.click(QStringLiteral("rents.trash1"))) {
                drive.shot(QStringLiteral("remove-confirm"));
                drive.expectExists(QStringLiteral("rents.removeConfirm"));
                drive.click(QStringLiteral("rents.removeKeep"));
                drive.expectExists(QStringLiteral("rents.removeConfirm"), false);
                drive.expectExists(QStringLiteral("rents.trash1"));
            }

            drive.clickTo(QStringLiteral("rents.add"), QStringLiteral("page:PageSetupWizardAresLogin"));
            drive.reportTruncated(QStringLiteral("the login"));
            drive.type(QStringLiteral("login.id"), QStringLiteral("driver"));
            drive.type(QStringLiteral("login.idx"), QStringLiteral("odin_1"));
            drive.shot(QStringLiteral("login-typed"));
        } else {
            // #D182 rule 1: with no rent stored the LOGIN is the first screen.
            drive.expectPage(QStringLiteral("page:PageSetupWizardAresLogin"));
            drive.shot(QStringLiteral("first-run-login"));
        }

        // Settings, and each of the three groups it offers
        m_coreController->pageController()->goToPage(PageLoader::PageEnum::PageSettings, false);
        drive.settle(16);
        drive.expectPage(QStringLiteral("page:PageSettings"));
        drive.reportTruncated(QStringLiteral("Settings"));
        drive.shot(QStringLiteral("settings"));
        drive.clickTo(QStringLiteral("settings.group1"), QStringLiteral("page:PageSettingsConnection"));
        drive.reportTruncated(QStringLiteral("Settings > Connection"));
        drive.shot(QStringLiteral("settings-connection"));

        m_coreController->pageController()->goToPage(PageLoader::PageEnum::PageSettingsApplication, false);
        drive.settle(16);
        drive.expectPage(QStringLiteral("page:PageSettingsApplication"));
        drive.reportTruncated(QStringLiteral("Settings > Application"));
        // flip a switch and put it back, so the run leaves the settings as it found them (#L023)
        const bool before = autoConnectEnabled();
        drive.click(QStringLiteral("app.autoconnect.area"));
        const bool after = autoConnectEnabled();
        if (after == before) {
            out << "  FAIL toggling app.autoconnect changed nothing (still "
                << (before ? "on" : "off") << ")" << Qt::endl;
        } else {
            out << "  ok   app.autoconnect toggled " << (before ? "on->off" : "off->on") << Qt::endl;
        }
        drive.click(QStringLiteral("app.autoconnect.area"));
        const bool restored = autoConnectEnabled();
        out << (restored == before ? "  ok   app.autoconnect restored to what it was"
                                   : "  FAIL app.autoconnect was NOT restored") << Qt::endl;
        drive.shot(QStringLiteral("settings-application"));

        const int extra = (after == before ? 1 : 0) + (restored == before ? 0 : 1);
        out << "QML-DRIVE " << drive.steps() << " step(s), " << (drive.failures() + extra)
            << " failed" << Qt::endl;
        ::exit((drive.failures() + extra) ? 6 : 0);
    }

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
    m_parser.addOption(m_optAresLogin);
    m_parser.addOption(m_optQmlSmoke);
    m_parser.addOption(m_optQmlShot);
    m_parser.addOption(m_optQmlDrive);
    m_parser.addOption(m_optFontReport);
    
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
