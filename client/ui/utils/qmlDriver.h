#ifndef QMLDRIVER_H
#define QMLDRIVER_H

// AresVPN Client - drive the UI the way a customer does, without taking anybody's mouse.
//
// AresProject ROADMAP 18-3h, and #L053 is the reason it is shaped like this. Automating a GUI by
// injecting into the HOST's global input hijacks the operator's machine: `mouse_event` and
// `keybd_event` carry no window handle, so they land wherever focus happens to be, and on
// 2026-09-04 they landed in the operator's terminal. That lesson also names the safe lever -
// "prefer a channel with no shared resource" - and this is it: events are POSTED to our own
// QQuickWindow inside our own offscreen process. Nothing is contended, nothing is taken, and
// there is no window on screen to steal focus from.
//
// It finds controls by objectName, which the screens set, so a step reads like the sentence a
// tester would say: click "home.rents", expect "page:PageAresRents".
//
// WHAT IT DELIBERATELY DOES NOT DO: press Connect. That would start a real tunnel from an
// unattended process, and a stray tunnel with a kill switch has already cost this project the
// operator's internet once (#L023's family). The connect path is measured on purpose, by a
// person, with the node in view.

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>

class QQuickWindow;
class QQuickItem;

class QmlDriver
{
public:
    QmlDriver(QQuickWindow *window, const QString &shotDir);

    // Every step reports itself; the run fails if any step does.
    bool click(const QString &objectName);
    bool type(const QString &objectName, const QString &text);
    bool expectPage(const QString &pageObjectName);
    bool expectExists(const QString &objectName, bool shouldExist = true);

    // Walk every visible Text on the current screen and report the ones the layout CUT. Qt sets
    // Text::truncated when elide or a fixed height dropped characters, so "this label does not
    // fit" is a question a machine can answer - and it is the legibility defect that actually
    // bites: an address ending in "..." is a rent nobody can read back to support.
    // Press a control and require the page to change, retrying the press ONCE.
    //
    // Measured over four consecutive runs of the plain click+expect form: green, green, red, red,
    // with an identical signature every time - the FIRST press of a run is sometimes lost, and
    // everything after it then fails against a screen that never moved. A press that is
    // occasionally dropped by an offscreen delivery agent is a harness condition, and a check that
    // is right half the time is worse than one that is red (#L033): it teaches a reader to re-run
    // it, which is how a real regression gets waved through. So the step states what it wants and
    // says how many presses it took.
    bool clickTo(const QString &objectName, const QString &expectedPage);

    int reportTruncated(const QString &where);
    void shot(const QString &tag);
    void settle(int rounds = 12);

    int failures() const { return m_failures; }
    int steps() const { return m_steps; }
    QString currentPage() const;

private:
    void sendClick(const QPointF &centre);
    QQuickItem *find(const QString &objectName) const;

    // Close anything on Qt Quick Controls' popup layer before pressing. See the mechanism in
    // qmlDriver.cpp beside dismissPopups(): a QQuickOverlay covers the WHOLE window while a Popup
    // or Drawer is open, the app raises an error notification of its own when no service is
    // running, and the scene's own hit test at the click point returned <QQuickOverlay> on every
    // failing run. That is the harness's residual flakiness, measured.
    // `target` is the objectName about to be pressed: a control that lives INSIDE a popup is
    // reached by leaving that popup open, so the sweep skips itself.
    void clearOverlay(const QString &target = QString());
    void pass(const QString &what);
    void fail(const QString &what, const QString &why);

    QQuickWindow *m_window;
    QString m_shotDir;
    int m_failures = 0;
    int m_steps = 0;
    int m_shotSeq = 0;
};

#endif
