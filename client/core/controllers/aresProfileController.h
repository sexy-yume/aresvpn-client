// AresVPN Client - the login: id + password + rent alias -> POST /api/profile -> imported server.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// This is the ONE thing this fork adds to the reference client's core (AresProject #D177,
// #D180): the boundary between the two products is the AresVPN console's `/api/profile`, and
// this class is the client end of it. It does not use GatewayController - that one wraps every
// body in an RSA+AES envelope for a plain-HTTP gateway; ours is HTTPS (#D177's update).
#ifndef ARESPROFILECONTROLLER_H
#define ARESPROFILECONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class QTimer;

#include "core/utils/errorCodes.h"

class ImportController;
class SecureServersRepository;
class SecureAppSettingsRepository;

class AresProfileController : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        amnezia::ErrorCode errorCode = amnezia::ErrorCode::NoError;
        QString message;       // the server's one sentence, or ours; empty on success
        QString serverId;      // the stored server's id on success (deterministic per rent)
        QString protocol;      // the rent's protocol as the server names it
        int rentId = 0;
    };

    explicit AresProfileController(ImportController *importController,
                                   SecureServersRepository *serversRepository,
                                   SecureAppSettingsRepository *appSettingsRepository,
                                   QObject *parent = nullptr);

    // Synchronous: POSTs, waits with a QEventLoop (the shape every QML "busy indicator" slot in
    // this tree already uses), imports, returns. Never throws; the Result says what happened.
    Result fetchAndImport(const QString &id, const QString &password, const QString &idx);

    // The console's base URL, e.g. https://console.ares-vpn.org:8080 - from settings, or the
    // compiled default (CLIENT_ARES_ENDPOINT -> APP_ARES_ENDPOINT).
    QString endpoint() const;

    // The deterministic server id for a rent, so a repeated login REPLACES rather than
    // duplicates (#D180: add-or-replace by rent_id).
    static QString serverIdForRent(int rentId);

    // What /api/profile said this rent's expiry was, verbatim (ISO-8601), or empty when we were
    // never told - a config pasted in by hand rather than fetched by a login.
    QString rentExpiry(const QString &serverId) const;

    // Removing a rent removes its expiry with it. Without this the map grows by one entry per
    // rent the customer ever held and never shrinks - the harness lesson (#L023) applied to the
    // product's own storage.
    void forgetRentExpiry(const QString &serverId);

    // Builds the Xray JSON the importer stores for a socks5 rent from its socks5h:// URI. Public
    // so the shape can be checked without a network.
    static QString xrayConfigForSocks5(const QString &uri, QString *errorOut);

    // ---- THE LOGIN SESSION (AresProject #D187) ------------------------------------------------
    //
    // The client holds ONE session - id + pw + idx - and is bound to the **idx**, not to a rent.
    // Whatever rent carries that idx at this moment is the rent this device uses, so an operator
    // can retire it, sell another and hang the same idx on the new one, and the client moves across
    // on its own. That is why `refreshSession()` compares what the console returns for the idx
    // against what is stored rather than asking whether one rent's address changed.
    bool hasSession() const;
    QString sessionAccountId() const;
    QString sessionIdx() const;
    QString sessionServerId() const;

    // Sign in, or switch. REPLACES the session and takes the previous session's stored server and
    // its expiry with it - there is nothing to add to (#D187 supersedes #D182 rule 2).
    Result loginSession(const QString &id, const QString &password, const QString &idx);

    // Ask the console what the session's idx points at NOW and adopt it if it has changed.
    // `changed` is set when the stored server was replaced, which is the signal to hot-reload.
    // A 404 (the rent behind the idx is gone and nothing has replaced it yet) is NOT a logout:
    // the session stands, the caller is told, and the next poll picks up the replacement.
    Result refreshSession(bool *changed = nullptr);

    // Forget the session and remove the server it owns.
    void logoutSession();

    // Start/stop the periodic refresh. The interval is deliberately not a constant a caller has to
    // remember: `start` with 0 uses the default.
    void startSessionPolling(int intervalSeconds = 0);
    void stopSessionPolling();

signals:
    void imported(const QString &serverId);

    // The session's rent was replaced under it - a new address, or an entirely different rent on
    // the same idx. The UI reconnects/hot-reloads on this.
    void sessionRentChanged(const QString &serverId, const QString &previousServerId);

    // A refresh could not be made. Carries the console's own sentence; never a reason to log out.
    void sessionRefreshFailed(const QString &message);

    void sessionChanged();

private:
    struct Reply
    {
        int httpStatus = 0;
        QByteArray body;
        QString transportError;   // non-empty when the request never got an HTTP answer
        bool sslError = false;
    };
    Reply post(const QString &url, const QByteArray &jsonBody);

    // Replace the server the session owns, removing the previous one and its expiry with it
    // (#L023's residue rule, applied to the product's own storage).
    void adoptServer(const QString &newServerId, const QString &previousServerId);

    ImportController *m_importController;
    SecureServersRepository *m_serversRepository;
    SecureAppSettingsRepository *m_appSettingsRepository;
    QTimer *m_pollTimer = nullptr;
    bool m_refreshing = false;
};

#endif // ARESPROFILECONTROLLER_H
