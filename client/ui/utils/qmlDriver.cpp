// AresVPN Client - see qmlDriver.h. AresProject ROADMAP 18-3h, #L053.
#include "qmlDriver.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <functional>
#include <QTimer>
#include <QQuickItem>
#include <QQuickWindow>
#include <qpa/qwindowsysteminterface.h>
#include <QTextStream>

namespace {

QQuickItem *findRecursive(QQuickItem *item, const QString &name)
{
    if (!item) {
        return nullptr;
    }
    if (item->objectName() == name) {
        // A control on a page that has been popped is still in the object tree; only a visible
        // one is a control a customer could press. Without this the driver happily "clicks"
        // something nobody can see, which is a green step over a dead screen.
        if (item->isVisible() && item->width() > 0 && item->height() > 0) {
            return item;
        }
    }
    const QList<QQuickItem *> kids = item->childItems();
    for (QQuickItem *kid : kids) {
        if (QQuickItem *hit = findRecursive(kid, name)) {
            return hit;
        }
    }
    return nullptr;
}

void collectPages(QQuickItem *item, QStringList &out)
{
    if (!item) {
        return;
    }
    // The page's own `objectName: "page:PageHome"` does NOT survive: upstream's StackView push
    // overwrites it with the source URL, measured - the tree carries
    // "qrc:/ui/qml/Pages2/PageHome.qml". So the marker is read from what the app actually does
    // rather than from what we asked it to do (#L009: read the artefact, not the intention).
    const QString name = item->objectName();
    if (item->isVisible() && name.startsWith(QStringLiteral("qrc:/ui/qml/Pages2/"))
        && name.endsWith(QStringLiteral(".qml"))) {
        out.append(QStringLiteral("page:")
                   + name.mid(name.lastIndexOf(QLatin1Char('/')) + 1).chopped(4));
    }
    const QList<QQuickItem *> kids = item->childItems();
    for (QQuickItem *kid : kids) {
        collectPages(kid, out);
    }
}

} // namespace

QmlDriver::QmlDriver(QQuickWindow *window, const QString &shotDir)
    : m_window(window), m_shotDir(shotDir)
{
    QDir().mkpath(m_shotDir);
}

void QmlDriver::settle(int rounds)
{
    // A REAL EVENT LOOP, not processEvents. This is the fix for the one thing that was actually
    // wrong, and it took naming the mechanism to see it: under -platform offscreen the StackView's
    // cross-fade never finished, so the OUTGOING page stayed live and swallowed every press meant
    // for the incoming one. Measured, not guessed - with the rent list on top, `home.connect` from
    // the page underneath was still present and clickable.
    //
    // processEvents drains the queue; it does not run the loop the animation driver is stepped
    // from, so a transition can sit half-done for ever. A nested QEventLoop with a timer runs the
    // real thing, which is what a person's machine does between one click and the next.
    QEventLoop loop;
    QTimer::singleShot(rounds * 25, &loop, &QEventLoop::quit);
    loop.exec();
}

QQuickItem *QmlDriver::find(const QString &objectName) const
{
    return m_window ? findRecursive(m_window->contentItem(), objectName) : nullptr;
}

QString QmlDriver::currentPage() const
{
    QStringList pages;
    if (m_window) {
        collectPages(m_window->contentItem(), pages);
    }
    // the deepest visible page is the one on top of the stack
    if (!pages.isEmpty()) {
        return pages.last();
    }
    // A bare "(none)" says the walk found nothing and cannot say why. List a few objectNames it
    // DID see, so the next reader is told whether the page markers are missing or the tree is.
    QStringList seen;
    std::function<void(QQuickItem *)> sample = [&](QQuickItem *i) {
        if (!i || seen.size() >= 8) {
            return;
        }
        if (!i->objectName().isEmpty() && i->isVisible()) {
            seen.append(i->objectName());
        }
        const QList<QQuickItem *> kids = i->childItems();
        for (QQuickItem *k : kids) {
            sample(k);
        }
    };
    if (m_window) {
        sample(m_window->contentItem());
    }
    return QStringLiteral("(no page: marker; visible objectNames seen: %1)")
        .arg(seen.isEmpty() ? QStringLiteral("none at all") : seen.join(QStringLiteral(", ")));
}

void QmlDriver::pass(const QString &what)
{
    ++m_steps;
    QTextStream(stdout) << "  ok   " << what << Qt::endl;
}

void QmlDriver::fail(const QString &what, const QString &why)
{
    ++m_steps;
    ++m_failures;
    QTextStream(stdout) << "  FAIL " << what << " - " << why << Qt::endl;
}

// WAIT FOR THE ITEM TO STOP MOVING, AND TO BE INSIDE THE WINDOW.
//
// This is the single fix for what looked like four unrelated failures. A StackView push is
// ANIMATED: the moment the new page is pushed, its root is already visible and already in the
// object tree - so `expectPage` passed and `find` returned controls - while the page itself is
// still sliding in from off-screen. Every click computed from `mapToScene` therefore landed
// OUTSIDE the window and did nothing, and every screenshot showed the previous page. The object
// tree was telling the truth about a frame the customer had not been shown yet.
static bool waitStable(QQuickWindow *window, QQuickItem *item, QPointF &centreOut)
{
    // NO grabWindow() HERE. Two earlier versions waited on the position and then on the rendered
    // FRAME, and neither changed the result - which by #L010 says the model was wrong, not the
    // details. The frame version also put a synchronous render inside the click path, which is
    // its own suspect: a grab on an offscreen window forces a render pass between the caller and
    // the delivery agent. Waiting is now plain event processing plus a position check, so the
    // click path does nothing but wait.
    QEventLoop loop;
    QTimer::singleShot(700, &loop, &QEventLoop::quit);
    loop.exec();
    const QPointF centre = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
    centreOut = centre;
    return centre.x() >= 0 && centre.y() >= 0
        && centre.x() <= window->width() && centre.y() <= window->height();
}

bool QmlDriver::click(const QString &objectName)
{
    QQuickItem *item = find(objectName);
    if (!item) {
        fail(QStringLiteral("click %1").arg(objectName), QStringLiteral("no visible item with that objectName"));
        return false;
    }
    // REPORT WHAT THE ITEM ACTUALLY IS. The experiment that got this far showed that clicks on
    // the STARTUP page work and clicks on any PUSHED page do not, whatever the control type -
    // home.settings, an ImageButtonType, navigates; rents.trash1, the same type on a pushed page,
    // does not. The remaining candidate is that a pushed page is visible but not ENABLED, which a
    // StackView does to anything that is not its currentItem, and which the never-completing
    // cross-fade would leave in place. So say it, rather than guess again.
    if (!item->isEnabled()) {
        fail(QStringLiteral("click %1").arg(objectName),
             QStringLiteral("the item is VISIBLE but not ENABLED - on a StackView that means the "
                            "page is not the current item, so no press can reach it"));
        return false;
    }
    QPointF centre;
    if (!waitStable(m_window, item, centre)) {
        fail(QStringLiteral("click %1").arg(objectName),
             QStringLiteral("it never came to rest inside the window (last centre %1,%2 in %3x%4) - "
                            "a transition that does not finish is a control a customer cannot press either")
                 .arg(centre.x()).arg(centre.y()).arg(m_window->width()).arg(m_window->height()));
        return false;
    }
    const QPointF global = m_window->mapToGlobal(centre);
    // The DEVICE matters. Built with the short constructor the event carries a null
    // QPointingDevice, and Qt Quick's delivery agent drops it: the first version of this driver
    // reported "ok click home.rents" and the screen never moved, which is a green step over a
    // press that never happened (#L020's cannot-fail half, in a harness). Naming the primary
    // pointing device makes it a real press as far as Quick is concerned.
    // DELIVERED AT THE PLATFORM LEVEL, not by sendEvent. Two earlier versions posted a
    // QMouseEvent straight at the window - first with the short constructor, then with a proper
    // QPointingDevice - and BOTH reported "ok click" while the screen never moved: Qt Quick's
    // delivery agent did not treat them as real presses. QWindowSystemInterface is the same door
    // the platform plugin uses and the same one QTest uses, and it is addressed to THIS WINDOW,
    // so it is still a channel with no shared resource (#L053).
    QWindowSystemInterface::handleMouseEvent(m_window, centre, global, Qt::LeftButton,
                                             Qt::LeftButton, QEvent::MouseButtonPress, Qt::NoModifier);
    QWindowSystemInterface::flushWindowSystemEvents();
    // No settle between press and release. A row inside a Flickable claims a press that is HELD
    // and turns it into a drag, so the click never arrives - measured on the Application page,
    // where the toggle was found, pressed and did not move.
    QWindowSystemInterface::handleMouseEvent(m_window, centre, global, Qt::NoButton,
                                             Qt::LeftButton, QEvent::MouseButtonRelease, Qt::NoModifier);
    QWindowSystemInterface::flushWindowSystemEvents();
    settle();
    pass(QStringLiteral("click %1").arg(objectName));
    return true;
}

bool QmlDriver::type(const QString &objectName, const QString &text)
{
    QQuickItem *item = find(objectName);
    if (!item) {
        fail(QStringLiteral("type into %1").arg(objectName), QStringLiteral("no visible item with that objectName"));
        return false;
    }
    // The named item is the wrapper; the thing that takes keys is whatever inside it accepts
    // focus. forceActiveFocus on the wrapper delegates down through Qt Quick's focus scopes.
    item->forceActiveFocus();
    settle(4);
    for (const QChar &c : text) {
        QKeyEvent down(QEvent::KeyPress, 0, Qt::NoModifier, QString(c));
        QKeyEvent up(QEvent::KeyRelease, 0, Qt::NoModifier, QString(c));
        QCoreApplication::sendEvent(m_window, &down);
        QCoreApplication::sendEvent(m_window, &up);
    }
    settle(6);
    pass(QStringLiteral("type \"%1\" into %2").arg(text, objectName));
    return true;
}

bool QmlDriver::expectPage(const QString &pageObjectName)
{
    const QString now = currentPage();
    if (now == pageObjectName) {
        pass(QStringLiteral("on %1").arg(pageObjectName));
        return true;
    }
    fail(QStringLiteral("expected %1").arg(pageObjectName), QStringLiteral("the top visible page is %1").arg(now));
    return false;
}

bool QmlDriver::expectExists(const QString &objectName, bool shouldExist)
{
    QQuickItem *hit = find(objectName);
    const bool there = hit != nullptr;
    if (there && !hit->isEnabled()) {
        QTextStream(stdout) << "       (note: " << objectName
                            << " is visible but NOT enabled)" << Qt::endl;
    }
    if (there == shouldExist) {
        pass(QStringLiteral("%1 is %2").arg(objectName, shouldExist ? QStringLiteral("there") : QStringLiteral("gone")));
        return true;
    }
    fail(QStringLiteral("%1 should be %2").arg(objectName, shouldExist ? QStringLiteral("there") : QStringLiteral("gone")),
         there ? QStringLiteral("it is present") : QStringLiteral("it is absent"));
    return false;
}

void QmlDriver::shot(const QString &tag)
{
    if (!m_window) {
        return;
    }
    // let any transition finish, or the picture is of the previous screen - same
    // frame-comparison rule the click path uses
    QImage previous;
    for (int i = 0; i < 120; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        const QImage frame = m_window->grabWindow();
        if (!previous.isNull() && frame == previous) {
            break;
        }
        previous = frame;
    }
    const QImage image = m_window->grabWindow();
    if (image.isNull()) {
        return;
    }
    const QString path = QStringLiteral("%1/%2-%3.png")
                             .arg(m_shotDir)
                             .arg(m_shotSeq++, 2, 10, QLatin1Char('0'))
                             .arg(tag);
    image.save(path);
}
