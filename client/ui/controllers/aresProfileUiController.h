// AresVPN Client - the QML face of the login (exposed as `AresProfileController`).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#ifndef ARESPROFILEUICONTROLLER_H
#define ARESPROFILEUICONTROLLER_H

#include <QObject>
#include <QString>

#include "core/controllers/aresProfileController.h"

class AresProfileUiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastServerId READ lastServerId NOTIFY loginFinished)
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY endpointChanged)

    // THE SESSION (AresProject #D187). `hasSession` is what the first screen branches on - not
    // "zero servers stored", which is a different question that only coincides by accident.
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionAccountId READ sessionAccountId NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionIdx READ sessionIdx NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionServerId READ sessionServerId NOTIFY sessionChanged)

public:
    explicit AresProfileUiController(AresProfileController *controller, QObject *parent = nullptr);

public slots:
    // Synchronous - wrap it in PageController.showBusyIndicator(true/false) from QML, the way the
    // credentials page wraps checkSshConnection(). true = the rent is stored (and its id is
    // lastServerId); false = lastError says why, in one sentence.
    // Sign in, or SWITCH. It replaces the session and takes the previous session's rent with it -
    // there is nothing to add to (#D187 supersedes #D182 rule 2, after the operator saw what
    // "add a rent" had produced). A failed sign-in leaves the previous session untouched.
    bool login(const QString &id, const QString &password, const QString &idx);

    bool hasSession() const;
    QString sessionAccountId() const;
    QString sessionIdx() const;
    QString sessionServerId() const;

    // Ask the console what this session's idx points at NOW. Returns true when the stored rent was
    // REPLACED, which the UI treats as a reason to reconnect. Never logs anybody out: a 404 means
    // the operator is mid-swap, not that the customer is signed out (#D187's update).
    bool refreshNow();

    // Forget the session and remove the rent it owns.
    void logout();
    QString lastError() const { return m_lastError; }
    QString lastServerId() const { return m_lastServerId; }
    QString endpoint() const;

    // A rent expires - the one fact about our product upstream's server model has no room for.
    // -1 means "never told" (an imported .conf rather than a login), which the UI shows as nothing
    // rather than as an expiry of zero.
    int daysLeftForServer(const QString &serverId) const;
    QString expiryTextForServer(const QString &serverId) const;

    // Called from the rent list immediately before the rent itself is removed, so the stored
    // expiry does not outlive the rent it describes. It lives here rather than in
    // ServersController because that file is upstream's and #D178 keeps its shape.
    void forgetRentExpiry(const QString &serverId);

    // The two things a rent screen shows and upstream had no accessor for. Found by RENDERING the
    // screens offscreen and looking at them: the home dial read "WireGuard | 112.168.124..." -
    // the address, which is the thing the customer bought, elided behind a protocol name that is
    // already on the line below - and every rent row read "· 3", the container ENUM printed as an
    // integer. Both are formatting, so both live in the UI controller.
    QString addressOnly(const QString &collapsedDescription) const;
    QString protocolLabel(int container) const;

signals:
    void loginFinished(const QString &serverId);
    void lastErrorChanged();
    void endpointChanged();
    void sessionChanged();

    // The rent behind this session's idx was replaced under it - a re-allocated address, or an
    // entirely different rent the operator hung the same idx on. The UI reconnects on this.
    void sessionRentChanged(const QString &serverId);
    void sessionRefreshFailed(const QString &message);

private:
    AresProfileController *m_controller;
    QString m_lastError;
    QString m_lastServerId;
};

#endif // ARESPROFILEUICONTROLLER_H
