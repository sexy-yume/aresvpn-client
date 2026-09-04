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
    void shot(const QString &tag);
    void settle(int rounds = 12);

    int failures() const { return m_failures; }
    int steps() const { return m_steps; }
    QString currentPage() const;

private:
    QQuickItem *find(const QString &objectName) const;
    void pass(const QString &what);
    void fail(const QString &what, const QString &why);

    QQuickWindow *m_window;
    QString m_shotDir;
    int m_failures = 0;
    int m_steps = 0;
    int m_shotSeq = 0;
};

#endif
