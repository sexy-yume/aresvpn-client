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
        QString message;       // a translated sentence chosen by HTTP status; empty on success
        QString serverId;      // the stored server's id on success (deterministic per rent)
        QString protocol;      // the rent's protocol as the server names it
        int rentId = 0;
        QString deviceId;      // the console's device_id for this installation; empty for a proxy rent
        QString identity;      // SHA-256 of the importer's text minus `#` comment lines; see identityDigest
        int httpStatus = 0;    // the console's status; 0 when it never answered
        bool noAnswer = false; // no HTTP answer and no TLS failure: the request never got through
        bool notModified = false; // 304 to a token refresh: nothing changed, nothing was imported
        QString refreshToken;  // a device token, from a console that issues them (plan 28); else empty
        QString etag;          // the console's ETag for this profile, sent back as If-None-Match
    };

    // What the console's update manifest says about this build (plan 28). Empty while the console
    // serves no manifest, which is today: every field false or empty, nothing shown.
    struct UpdateInfo
    {
        QString latest;        // the newest version for this platform
        QString url;           // where to get it (https only)
        bool available = false; // latest is newer than this build
        bool required = false;  // this build is older than min_version: Connect is refused
    };

    explicit AresProfileController(ImportController *importController,
                                   SecureServersRepository *serversRepository,
                                   SecureAppSettingsRepository *appSettingsRepository,
                                   QObject *parent = nullptr);

    // Synchronous: POSTs, waits with a QEventLoop (the shape every QML "busy indicator" slot in
    // this tree already uses), imports, returns. Never throws; the Result says what happened.
    // `timeoutMsecs` 0 is the default transfer timeout.
    Result fetchAndImport(const QString &id, const QString &password, const QString &idx, int timeoutMsecs = 0);

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
    // The client holds ONE session - id + idx, and the credential that refreshes it - and is bound
    // to the **idx**, not to a rent. Whatever rent carries that idx at this moment is the rent this
    // device uses, so an operator can retire it, sell another and hang the same idx on the new one,
    // and the client moves across on its own. That is why `refreshSession()` compares what the
    // console returns for the idx against what is stored rather than asking whether one rent's
    // address changed.
    //
    // THE SESSION OWNS THE TUNNEL (plan 28). Every change of the session's server - sign-in, switch,
    // sign-out, a refresh that brought a new identity - is announced once, by `sessionServerSet`,
    // and `CoreSignalHandlers` re-points the tunnel through one routine: close, set the default,
    // reopen. Nothing here touches the connection directly.
    bool hasSession() const;
    QString sessionAccountId() const;
    QString sessionIdx() const;
    QString sessionServerId() const;

    // True after the console refused the stored credential (401): the poll stops, the stored
    // profile stays, and the customer is asked to sign in again. Cleared by a sign-in, or by a
    // refresh the console accepts again.
    bool signInNeeded() const { return m_signInNeeded; }

    // Sign in, or switch. REPLACES the session and takes the previous session's stored server and
    // its expiry with it - there is nothing to add to (#D187 supersedes #D182 rule 2).
    Result loginSession(const QString &id, const QString &password, const QString &idx);

    // Ask the console what the session's idx points at NOW and adopt it if it has changed.
    // `changed` is set when the stored server was replaced, which is the signal to hot-reload.
    // A 404 (the rent behind the idx is gone and nothing has replaced it yet) is NOT a logout:
    // the session stands, the caller is told, and the next poll picks up the replacement.
    Result refreshSession(bool *changed = nullptr, int timeoutMsecs = 0);

    // Forget the session and remove the server it owns.
    void logoutSession();

    // Start/stop the periodic refresh. The interval is deliberately not a constant a caller has to
    // remember: `start` with 0 uses the default.
    void startSessionPolling(int intervalSeconds = 0);
    void stopSessionPolling();

    // ---- THE UPDATE MANIFEST (plan 28) ---------------------------------------------------------
    // GET <endpoint>/api/client/manifest. A 404 means the console serves none, and the check is
    // inert: nothing is shown and nothing is refused.
    void checkForUpdate(int timeoutMsecs = 0);
    // Checks now and then on a long interval.
    void startUpdateChecks();
    UpdateInfo updateInfo() const { return m_updateInfo; }

signals:
    void imported(const QString &serverId);

    // The session's server was set: a sign-in, a switch, a sign-out (empty id), or a refresh that
    // brought a different rent or a new identity on the same rent. `identityChanged` is false only
    // for a sign-in that brought back exactly the profile already stored - the tunnel, if it runs
    // on that server, is then still right.
    void sessionServerSet(const QString &serverId, bool identityChanged);

    // A refresh could not be made. Never a reason to log out. `httpStatus` 0 with `noAnswer` means
    // the request never got through - through a dead tunnel, that is what it looks like.
    void sessionRefreshFailed(int httpStatus, bool noAnswer, const QString &message);

    // A refresh got an answer and the session is current.
    void sessionRefreshed();

    void sessionChanged();

    void updateInfoChanged();

private:
    struct Reply
    {
        int httpStatus = 0;
        QByteArray body;
        QByteArray etag;
        QString transportError;   // non-empty when the request never got an HTTP answer
        bool sslError = false;
    };
    // GET when `jsonBody` is null, POST otherwise.
    Reply request(const QString &url, const QByteArray &jsonBody, int timeoutMsecs, const QByteArray &ifNoneMatch = {});

    // The refresh with a device token instead of the password (plan 28): no scrypt on the console.
    Result refreshWithToken(const QString &token, const QString &idx, const QString &etag, int timeoutMsecs);

    // Status -> Result, and on 200 the import. `viaToken` changes only what a 401 means.
    Result importReply(const Reply &reply, const QString &alias, bool viaToken);

    // Replace the server the session owns, removing the previous one and its expiry with it
    // (#L023's residue rule, applied to the product's own storage).
    void adoptServer(const QString &newServerId, const QString &previousServerId);

    // `base` with what a successful answer changed: server, device, identity, ETag, and a device
    // token in place of the password when the console issued one.
    static QVariantMap sessionWith(QVariantMap base, const Result &r);

    // The label this device carries on the console's device page (plan 24): the host name, or the
    // product name where the host name says nothing (Android answers "localhost"). At most 64.
    static QString deviceName();

    // What identifies this device's tunnel in a profile: a digest of the text minus its `#` comment
    // lines. A renewal rewrites only the comment (`# ARES rent X device Y - expires ...`), so it is
    // no change. Persisted with the session, so refreshSession compares across restarts.
    static QString identityDigest(const QString &text);

    ImportController *m_importController;
    SecureServersRepository *m_serversRepository;
    SecureAppSettingsRepository *m_appSettingsRepository;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_updateTimer = nullptr;
    bool m_refreshing = false;
    bool m_signInNeeded = false;
    UpdateInfo m_updateInfo;
};

#endif // ARESPROFILECONTROLLER_H
