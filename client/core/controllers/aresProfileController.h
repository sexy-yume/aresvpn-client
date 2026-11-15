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

#include "core/controllers/aresSessionLogic.h"
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
        bool superseded = false;  // the session was signed in or out while a background refresh
                                  // was on the wire; its answer was dropped, nothing was written
        bool busy = false;        // another refresh was on the wire, so this one never asked the
                                  // console; errorCode is not NoError, so no caller reads it as success
        QString refreshToken;  // a device token, from a console that issues them (plan 28); else empty
        QString etag;          // the console's ETag for this profile, sent back as If-None-Match
    };

    using UpdateInfo = aresSession::UpdateInfo;

    explicit AresProfileController(ImportController *importController,
                                   SecureServersRepository *serversRepository,
                                   SecureAppSettingsRepository *appSettingsRepository,
                                   QObject *parent = nullptr);

    // Synchronous: POSTs, waits with a QEventLoop that holds back user input (the shape every QML
    // "busy indicator" slot in this tree already uses), imports, returns. Never throws; the Result
    // says what happened. `timeoutMsecs` 0 is the default transfer timeout.
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

    // Builds the Xray JSON the importer stores for a socks5 rent from its socks5h:// URI. Static and
    // network-free, but NOT under client/tests/ares: it builds on upstream's Xray serialization,
    // which that QtCore-only target does not compile.
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
    // profile stays for a Connect the customer presses, nothing opens the tunnel on it unasked
    // (auto-connect, the stall cycle's reopen: plan 30 T02), and the customer is asked to sign in
    // again. Cleared by a sign-in, or by a refresh the console accepts again.
    bool signInNeeded() const { return m_signInNeeded; }

    // Sign in, or switch. REPLACES the session and takes the previous session's stored server and
    // its expiry with it - there is nothing to add to (#D187 supersedes #D182 rule 2).
    Result loginSession(const QString &id, const QString &password, const QString &idx);

    // Ask the console what the session's idx points at NOW and adopt it if it has changed.
    // `changed` is set when the stored server was replaced, which is the signal to hot-reload.
    // A 404 (the rent behind the idx is gone and nothing has replaced it yet) is NOT a logout:
    // the session stands, the caller is told, and the next poll picks up the replacement.
    //
    // `userWaiting` is true only where a customer pressed something and watches a busy indicator.
    // Every other refresh - the poll, the one before auto-connect, the one a stalled tunnel runs -
    // leaves user input flowing while it waits (H-13: a 30 s timeout through a dead tunnel used to
    // freeze every click, Disconnect included), and an answer that arrives after the customer
    // signed in or out meanwhile is dropped (`Result::superseded`) rather than written over them.
    Result refreshSession(bool *changed = nullptr, int timeoutMsecs = 0, bool userWaiting = false);

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
    // What a refresh carries into the import: the session as it was when the request left, and its
    // generation, so an answer that outlived a sign-in or sign-out is recognised and dropped.
    struct RefreshContext
    {
        QVariantMap session;
        quint64 generation = 0;
    };

    // GET when `jsonBody` is null, POST otherwise. `allowUserInput` leaves clicks flowing while it
    // waits - for requests nobody is watching (H-13).
    Reply request(const QString &url, const QByteArray &jsonBody, int timeoutMsecs,
                  const QByteArray &ifNoneMatch = {}, bool allowUserInput = false);

    // The password request behind fetchAndImport, and behind the refresh of a session stored before
    // the console issued tokens; `refresh` is null for a sign-in.
    Result passwordFetch(const QString &id, const QString &password, const QString &idx, int timeoutMsecs,
                         const RefreshContext *refresh, bool allowUserInput);

    // The refresh with a device token instead of the password (plan 28): no scrypt on the console.
    Result refreshWithToken(const QString &token, const QString &idx, const QString &etag, int timeoutMsecs,
                            const RefreshContext *refresh, bool allowUserInput);

    // Status -> Result, and on 200 the import. `viaToken` changes only what a 401 means; `refresh`
    // is null for a sign-in, and for a refresh it decides whether the answer is still wanted and
    // whether the server list needs writing at all (H-18).
    Result importReply(const Reply &reply, const QString &alias, bool viaToken, const RefreshContext *refresh);

    // The `device` this installation sends: on Android derived from ANDROID_ID, so a reinstall is
    // the same device (H-12); elsewhere, and where Android gives no id, the stored installation uuid.
    QString deviceKey() const;

    // Replace the server the session owns, removing the previous one and its expiry with it
    // (#L023's residue rule, applied to the product's own storage).
    void adoptServer(const QString &newServerId, const QString &previousServerId);

    // The label this device carries on the console's device page (plan 24): see
    // aresSession::chooseDeviceName, fed with the platform's host name, model and OS name.
    static QString deviceName();

    ImportController *m_importController;
    SecureServersRepository *m_serversRepository;
    SecureAppSettingsRepository *m_appSettingsRepository;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_updateTimer = nullptr;
    bool m_refreshing = false;
    // Bumped by every sign-in and sign-out; a background refresh compares it before it writes.
    quint64 m_sessionGeneration = 0;
    bool m_signInNeeded = false;
    UpdateInfo m_updateInfo;
};

#endif // ARESPROFILECONTROLLER_H
