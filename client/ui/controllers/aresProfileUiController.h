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

public:
    explicit AresProfileUiController(AresProfileController *controller, QObject *parent = nullptr);

public slots:
    // Synchronous - wrap it in PageController.showBusyIndicator(true/false) from QML, the way the
    // credentials page wraps checkSshConnection(). true = the rent is stored (and its id is
    // lastServerId); false = lastError says why, in one sentence.
    bool login(const QString &id, const QString &password, const QString &idx);
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

signals:
    void loginFinished(const QString &serverId);
    void lastErrorChanged();
    void endpointChanged();

private:
    AresProfileController *m_controller;
    QString m_lastError;
    QString m_lastServerId;
};

#endif // ARESPROFILEUICONTROLLER_H
