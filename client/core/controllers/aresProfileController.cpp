// AresVPN Client - the login: id + password + rent alias -> POST /api/profile -> imported server.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileController.h"

#include <QTimer>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>
#include <QSysInfo>
#include <QUuid>

#include "amneziaApplication.h"
#include "core/controllers/selfhosted/importController.h"
#include "core/repositories/secureAppSettingsRepository.h"
#include "core/repositories/secureServersRepository.h"
#include "core/utils/appUiConfig.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/networkUtilities.h"
#include "core/utils/serialization/serialization.h"
#include "core/utils/serverConfigUtils.h"
#include "core/utils/utilities.h"
#include "version.h"

#ifdef AMNEZIA_DESKTOP
    #include "core/utils/ipcClient.h"
#endif

#ifdef Q_OS_ANDROID
    #include <QJniObject>
    #include "platforms/android/android_utils.h"
#endif

using namespace amnezia;

namespace
{
    constexpr int kRequestTimeoutMsecs = 30000;
    constexpr char kProfilePath[] = "/api/profile";
    // Plan 28: the device-token refresh and the update manifest. Both are served by the console
    // from plan 28's server release; until then the refresh path is never taken (no token is ever
    // issued) and the manifest answers 404, which leaves the check inert.
    constexpr char kRefreshPath[] = "/api/profile/refresh";
    constexpr char kManifestPath[] = "/api/client/manifest";
    constexpr int kPollSeconds = 300;
    constexpr int kUpdateCheckSeconds = 6 * 3600;

    // The manifest's key for this build's platform; empty where no build is shipped.
    QString manifestPlatform()
    {
#if defined(Q_OS_ANDROID)
        return QStringLiteral("android");
#elif defined(Q_OS_WINDOWS)
        return QStringLiteral("windows");
#else
        return QString();
#endif
    }
}

AresProfileController::AresProfileController(ImportController *importController, SecureServersRepository *serversRepository,
                                             SecureAppSettingsRepository *appSettingsRepository, QObject *parent)
    : QObject(parent),
      m_importController(importController),
      m_serversRepository(serversRepository),
      m_appSettingsRepository(appSettingsRepository)
{
}

QString AresProfileController::endpoint() const
{
    QString base = m_appSettingsRepository ? m_appSettingsRepository->getAresEndpoint() : QString();
    if (base.isEmpty()) {
        base = QString(APP_ARES_ENDPOINT);
    }
    while (base.endsWith('/')) {
        base.chop(1);
    }
    return base;
}

QString AresProfileController::serverIdForRent(int rentId)
{
    return QStringLiteral("ares-rent-%1").arg(rentId);
}

QString AresProfileController::deviceName()
{
    QString model;
#ifdef Q_OS_ANDROID
    // android.os.Build.MODEL ("SM-S918N", "Pixel 8"): what tells two phones on one account apart.
    model = QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL").toString();
#endif
    return aresSession::chooseDeviceName(QSysInfo::machineHostName(), model, QSysInfo::prettyProductName());
}

QString AresProfileController::deviceKey() const
{
#ifdef Q_OS_ANDROID
    // Settings.Secure.ANDROID_ID survives an uninstall (it is per signing key and user since
    // Android 8), where QSettings' `files/settings` - excluded from backup on purpose - does not.
    const QJniObject context = AndroidUtils::getActivity();
    if (context.isValid()) {
        const QJniObject resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
        const QJniObject androidId = QJniObject::callStaticObjectMethod(
                "android/provider/Settings$Secure", "getString",
                "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                resolver.object(), QJniObject::fromString(QStringLiteral("android_id")).object<jstring>());
        const QString key = aresSession::deviceKeyFromAndroidId(androidId.toString());
        if (!key.isEmpty()) {
            return key;
        }
    }
#endif
    return m_appSettingsRepository ? m_appSettingsRepository->getInstallationUuid(true) : QString();
}

// A socks5 rent is a user name and a password against a listener on the rent's own public
// address (AresProject #D050); the console hands it over as socks5h://user:pass@addr:port. The
// reference client has no import branch for a socks5 upstream (survey 6.5), so the fork builds the
// same object its ss:// path builds - one Xray outbound of type "socks" plus the local inbound the
// tun2socks bridge expects - and stores it as an Xray container (#D180 rule 5).
QString AresProfileController::xrayConfigForSocks5(const QString &uri, QString *errorOut)
{
    const QUrl u = QUrl::fromUserInput(uri);
    if (!u.isValid() || u.host().isEmpty() || u.port() <= 0) {
        if (errorOut) *errorOut = QStringLiteral("socks5 uri is not host:port");
        return QString();
    }
    QJsonObject user;
    user[QStringLiteral("user")] = u.userName(QUrl::FullyDecoded);
    user[QStringLiteral("pass")] = u.password(QUrl::FullyDecoded);
    QJsonObject server;
    server[QStringLiteral("address")] = u.host();
    server[QStringLiteral("port")] = u.port();
    server[QStringLiteral("users")] = QJsonArray { user };
    QJsonObject settings;
    settings[QStringLiteral("servers")] = QJsonArray { server };

    QJsonObject root;
    root[QStringLiteral("outbounds")] = QJsonArray {
        serialization::outbounds::GenerateOutboundEntry(QStringLiteral("PROXY") /* ss.cpp's OUTBOUND_TAG_PROXY */, QStringLiteral("socks"), settings, {})
    };
    root[QStringLiteral("inbounds")] = QJsonArray { serialization::inbounds::GenerateInboundEntry() };
    return Utils::JsonToString(root, QJsonDocument::JsonFormat::Compact);
}

AresProfileController::Reply AresProfileController::request(const QString &url, const QByteArray &jsonBody,
                                                            int timeoutMsecs, const QByteArray &ifNoneMatch,
                                                            bool allowUserInput)
{
    Reply out;
    QNetworkRequest req;
    req.setTransferTimeout(timeoutMsecs > 0 ? timeoutMsecs : kRequestTimeoutMsecs);
    req.setRawHeader(QByteArrayLiteral("X-Client-Request-ID"),
                     QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
    if (!jsonBody.isNull()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
    if (!ifNoneMatch.isEmpty()) {
        req.setRawHeader(QByteArrayLiteral("If-None-Match"), ifNoneMatch);
    }
    req.setUrl(QUrl(url));

    // With the strict kill switch on, nothing leaves the machine that is not on the allow list -
    // the console must be, for as long as the login takes (the shape GatewayController uses).
#ifdef AMNEZIA_DESKTOP
    if (m_appSettingsRepository && m_appSettingsRepository->isStrictKillSwitchEnabled()) {
        const QString ip = NetworkUtilities::getIPAddress(QUrl(url).host());
        if (!ip.isEmpty()) {
            IpcClient::withInterface([&](QSharedPointer<IpcInterfaceReplica> iface) {
                QRemoteObjectPendingReply<bool> reply = iface->addKillSwitchAllowedRange(QStringList { ip });
                if (!reply.waitForFinished(1000) || !reply.returnValue())
                    qWarning() << "AresProfileController: addKillSwitchAllowedRange failed";
            });
        }
    }
#endif

    QNetworkAccessManager *nam = amnApp->networkManager();
    QNetworkReply *reply = jsonBody.isNull() ? nam->get(req) : nam->post(req, jsonBody);
    QEventLoop wait;
    connect(reply, &QNetworkReply::finished, &wait, &QEventLoop::quit);
    // TLS errors are recorded and REFUSED; nothing here ever calls ignoreSslErrors() - the whole
    // reason /api/profile needs no envelope is that the transport is trusted (#D177's update).
    connect(reply, &QNetworkReply::sslErrors, [&out](const QList<QSslError> &errors) { out.sslError = !errors.isEmpty(); });
    // A request somebody is watching holds back input, as every busy-indicator slot here does. One
    // nobody is watching - the poll, a refresh through a dead tunnel - must not freeze Disconnect
    // for the length of its timeout (H-13); its callers drop an answer the session outlived.
    wait.exec(allowUserInput ? QEventLoop::AllEvents : QEventLoop::ExcludeUserInputEvents);

    out.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    out.body = reply->readAll();
    out.etag = reply->rawHeader(QByteArrayLiteral("ETag"));
    if (out.httpStatus == 0) {
        out.transportError = reply->errorString();
    }
    reply->deleteLater();
    return out;
}

AresProfileController::Result AresProfileController::fetchAndImport(const QString &id, const QString &password,
                                                                    const QString &idx, int timeoutMsecs)
{
    return passwordFetch(id, password, idx, timeoutMsecs, nullptr, false);
}

AresProfileController::Result AresProfileController::passwordFetch(const QString &id, const QString &password,
                                                                   const QString &idx, int timeoutMsecs,
                                                                   const RefreshContext *refresh, bool allowUserInput)
{
    Result result;
    if (!m_importController || !m_serversRepository) {
        result.errorCode = ErrorCode::InternalError;
        return result;
    }
    const QString login = id.trimmed();
    const QString alias = idx.trimmed();
    if (login.isEmpty() || password.isEmpty() || alias.isEmpty()) {
        result.errorCode = ErrorCode::AresBadCredentials;
        result.message = tr("Enter your account id, your password and the rent's idx.");
        return result;
    }

    QJsonObject payload;
    payload[QStringLiteral("id")] = login;
    payload[QStringLiteral("pw")] = password;
    payload[QStringLiteral("idx")] = alias;
    // Plan 24: one rent, many devices. The installation's own key picks (or creates) this device's
    // peer on the console, so two devices sharing a rent no longer knock each other off.
    if (m_appSettingsRepository) {
        payload[QStringLiteral("device")] = deviceKey();
    }
    payload[QStringLiteral("device_name")] = deviceName();
    const Reply reply = request(endpoint() + QString::fromLatin1(kProfilePath),
                                QJsonDocument(payload).toJson(QJsonDocument::Compact), timeoutMsecs, {},
                                allowUserInput);
    return importReply(reply, alias, false, refresh);
}

// Plan 28: the refresh a device token pays for, in place of the password. The console looks the
// token up by its hash - no scrypt, outside the login gate - and answers 304 when the profile behind
// the ETag we hold has not moved. The token alone names the account, the rent's idx and this
// installation on the console; `device` is sent so a token copied to another machine is refused.
AresProfileController::Result AresProfileController::refreshWithToken(const QString &token, const QString &idx,
                                                                      const QString &etag, int timeoutMsecs,
                                                                      const RefreshContext *refresh, bool allowUserInput)
{
    Result result;
    if (!m_importController || !m_serversRepository || !m_appSettingsRepository) {
        result.errorCode = ErrorCode::InternalError;
        return result;
    }
    QJsonObject payload;
    payload[QStringLiteral("token")] = token;
    payload[QStringLiteral("device")] = deviceKey();
    payload[QStringLiteral("device_name")] = deviceName();
    Reply reply = request(endpoint() + QString::fromLatin1(kRefreshPath),
                          QJsonDocument(payload).toJson(QJsonDocument::Compact), timeoutMsecs, etag.toUtf8(),
                          allowUserInput);
    if (refresh && refresh->generation != m_sessionGeneration) {
        result.superseded = true;
        return result;
    }
    if (reply.httpStatus == 304) {
        result.httpStatus = 304;
        result.notModified = true;
        return result;
    }
    if (reply.httpStatus == 404 && !QJsonDocument::fromJson(reply.body).isObject()) {
        // The console's "no rent on this idx" is a JSON 404; a bare one means the route itself is
        // gone (a console rolled back past device tokens). The password is no longer stored, so a
        // sign-in is the only way on - which is what a 401 says.
        reply.httpStatus = 401;
    }
    return importReply(reply, idx.trimmed(), true, refresh);
}

AresProfileController::Result AresProfileController::importReply(const Reply &reply, const QString &alias, bool viaToken,
                                                                 const RefreshContext *refresh)
{
    Result result;
    result.httpStatus = reply.httpStatus;

    // The customer signed in or out while this refresh was on the wire (H-13 lets them). Its answer
    // describes a session that no longer exists; writing it would put the old rent back.
    if (refresh && refresh->generation != m_sessionGeneration) {
        result.superseded = true;
        return result;
    }

    if (reply.sslError) {
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console's certificate could not be verified.");
        return result;
    }
    if (reply.httpStatus == 0) {
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.noAnswer = true;
        result.message = tr("The AresVPN console did not answer: %1").arg(reply.transportError);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(reply.body, &parseError);
    const QJsonObject body = doc.isObject() ? doc.object() : QJsonObject();

    // The message is ours, chosen by status and translated. The console's own sentence is English
    // and was what a customer used to read ("bad credentials"), which made every translated text
    // here unreachable; it is kept for the log only.
    if (reply.httpStatus != 200) {
        qWarning() << "AresProfileController: the console answered" << reply.httpStatus
                   << body.value(QStringLiteral("error")).toString();
    }
    switch (reply.httpStatus) {
    case 200:
        break;
    case 400:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console refused this app's request. Updating the app should fix it.");
        return result;
    case 401:
        result.errorCode = ErrorCode::AresBadCredentials;
        result.message = viaToken
                ? tr("The console no longer accepts this device's sign-in. Sign in again.")
                : tr("The account id or the password is wrong.");
        return result;
    case 404:
        result.errorCode = ErrorCode::AresNoSuchRent;
        result.message = tr("No rent in this account carries the idx \"%1\" right now.").arg(alias);
        return result;
    case 409:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = body.value(QStringLiteral("error")).toString() == QLatin1String("device limit reached")
                ? tr("This rent is at its device limit. Remove a device it no longer uses on its devices page in the console, or contact support.")
                : tr("The console could not add this device to the rent. Try again later, or contact support.");
        return result;
    case 429:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("Too many attempts - wait a minute and try again.");
        return result;
    case 503:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console is busy - try again in a moment.");
        return result;
    default:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console answered HTTP %1.").arg(reply.httpStatus);
        return result;
    }

    result.rentId = body.value(QStringLiteral("rent_id")).toInt();
    result.protocol = body.value(QStringLiteral("protocol")).toString();
    result.refreshToken = body.value(QStringLiteral("refresh_token")).toString();
    result.etag = QString::fromUtf8(reply.etag);
    // A number for tunnel rents, null for proxies; kept as text so null stays distinguishable.
    const QJsonValue deviceId = body.value(QStringLiteral("device_id"));
    result.deviceId = deviceId.isDouble() ? QString::number(deviceId.toVariant().toLongLong()) : QString();
    const QJsonObject file = body.value(QStringLiteral("file")).toObject();
    const QString fileBody = file.value(QStringLiteral("body")).toString();
    const QString uri = body.value(QStringLiteral("uri")).toString();

    // Which text the importer gets, per protocol (survey 6.5; #D180 rule 5):
    //   wireguard, openvpn  - the file, as-is (both parse through checkConfigFormat)
    //   shadowsocks         - the ss:// line (the file's own shape has no import branch)
    //   socks5              - an Xray config with a socks outbound, built from the socks5h:// line
    //   http                - not a client product; say so instead of importing nothing
    QString text;
    if (result.protocol == QLatin1String("wireguard") || result.protocol == QLatin1String("openvpn")) {
        text = fileBody;
    } else if (result.protocol == QLatin1String("shadowsocks")) {
        text = uri;
    } else if (result.protocol == QLatin1String("socks5")) {
        QString err;
        text = xrayConfigForSocks5(uri, &err);
        if (text.isEmpty()) {
            result.errorCode = ErrorCode::AresRentNotImportable;
            result.message = tr("This socks5 rent could not be turned into a client configuration (%1).").arg(err);
            return result;
        }
    } else if (result.protocol == QLatin1String("http")) {
        result.errorCode = ErrorCode::AresRentNotImportable;
        result.message = tr("An HTTP proxy rent is used from a browser or curl, not from this app. "
                            "Its address and credentials are on your rent's page in the AresVPN console.");
        return result;
    } else {
        result.errorCode = ErrorCode::AresRentNotImportable;
        result.message = tr("This app does not know the protocol \"%1\" yet.").arg(result.protocol);
        return result;
    }
    if (text.isEmpty()) {
        result.errorCode = ErrorCode::AresRentNotImportable;
        result.message = tr("The console returned an empty configuration for this rent.");
        return result;
    }
    result.identity = aresSession::identityDigest(text);

    // (named `extracted`, not `imported` - that is the signal below)
    ImportController::ImportResult extracted = m_importController->extractConfigFromData(text, file.value(QStringLiteral("filename")).toString());
    if (extracted.errorCode != ErrorCode::NoError || extracted.config.isEmpty()) {
        result.errorCode = extracted.errorCode == ErrorCode::NoError ? ErrorCode::AresRentNotImportable : extracted.errorCode;
        result.message = tr("The rent's configuration could not be imported.");
        return result;
    }

    // The list shows the alias the user typed, not "Server N".
    QJsonObject config = extracted.config;
    config[configKey::description] = alias;

    // Add-or-replace by rent (#D180): the id is the rent's, so the second login for the same rent
    // - after a renewal, a reinstall, a second device syncing a backup - updates the entry instead of
    // stacking "Server 1", "Server 2" ... for one rent.
    //
    // And a refresh that brought back exactly the stored profile writes nothing (H-18): the list is
    // encrypted and every write resets the UI model, five-minutely, for no change. The expiry line
    // does not depend on that write; it re-reads on `imported` below (H-10).
    const QString serverId = serverIdForRent(result.rentId);
    const serverConfigUtils::ConfigType kind = serverConfigUtils::configTypeFromJson(config);
    const bool stored = m_serversRepository->orderedServerIds().contains(serverId);
    const bool write = !refresh
            || aresSession::serverNeedsWrite(stored, refresh->session.value(QStringLiteral("serverId")).toString(),
                                             refresh->session.value(QStringLiteral("identity")).toString(),
                                             serverId, result.identity);
    if (write && stored) {
        m_serversRepository->editServer(serverId, config, kind);
    } else if (write) {
        m_serversRepository->addServer(serverId, config, kind);
    }
    result.serverId = serverId;

    // A rent expires; a self-hosted server does not, so nothing upstream carries this. Keep what
    // the console told us, verbatim, so the UI can say how long is left (#D178's boundary keeps it
    // out of ServerDescription).
    if (m_appSettingsRepository) {
        const QString expiry = body.value(QStringLiteral("expires_at")).toString();
        if (m_appSettingsRepository->getRentExpiry(serverId) != expiry) {
            m_appSettingsRepository->setRentExpiry(serverId, expiry);
        }
    }

    emit imported(serverId);
    return result;
}

// ---- THE LOGIN SESSION (AresProject #D187) --------------------------------------------------
//
// One session: id + idx + the credential that refreshes it. The client is bound to the **idx**,
// not to a rent - whatever rent carries that idx right now is the rent this device uses. That is
// the whole model, and it is why there is no list here and nothing to add to.

bool AresProfileController::hasSession() const
{
    return !sessionIdx().isEmpty();
}

QString AresProfileController::sessionAccountId() const
{
    return m_appSettingsRepository
            ? m_appSettingsRepository->getAresSession().value(QStringLiteral("id")).toString()
            : QString();
}

QString AresProfileController::sessionIdx() const
{
    return m_appSettingsRepository
            ? m_appSettingsRepository->getAresSession().value(QStringLiteral("idx")).toString()
            : QString();
}

QString AresProfileController::sessionServerId() const
{
    return m_appSettingsRepository
            ? m_appSettingsRepository->getAresSession().value(QStringLiteral("serverId")).toString()
            : QString();
}

// The previous session's server goes when the new one arrives. Removing it is the difference
// between a session and a collection, and forgetting its expiry with it is #L023's residue rule
// applied to the product's own storage - without that the map grows by one entry per rent the
// customer ever held and never shrinks.
void AresProfileController::adoptServer(const QString &newServerId, const QString &previousServerId)
{
    if (previousServerId.isEmpty() || previousServerId == newServerId) {
        return;
    }
    if (m_serversRepository && m_serversRepository->orderedServerIds().contains(previousServerId)) {
        m_serversRepository->removeServer(previousServerId);
    }
    if (m_appSettingsRepository) {
        m_appSettingsRepository->forgetRentExpiry(previousServerId);
    }
}

AresProfileController::Result AresProfileController::loginSession(const QString &id,
                                                                  const QString &password,
                                                                  const QString &idx)
{
    const QVariantMap previous = m_appSettingsRepository ? m_appSettingsRepository->getAresSession() : QVariantMap();
    const QString previousServerId = previous.value(QStringLiteral("serverId")).toString();

    Result r = fetchAndImport(id, password, idx);
    if (r.errorCode != amnezia::ErrorCode::NoError) {
        // A failed sign-in leaves the previous session exactly as it was. Signing in with the wrong
        // password must not cost a customer the rent they already had.
        return r;
    }

    QVariantMap base;
    base.insert(QStringLiteral("id"), id);
    base.insert(QStringLiteral("pw"), password);
    base.insert(QStringLiteral("idx"), idx);
    if (!m_appSettingsRepository
        || !m_appSettingsRepository->setAresSession(
                aresSession::withAnswer(base, r.serverId, r.deviceId, r.identity, r.etag, r.refreshToken))) {
        // The encrypted store refused the write (no keychain). Nothing may be left half-done: the
        // server just imported goes again, and the previous session stands.
        if (r.serverId != previousServerId && m_serversRepository
            && m_serversRepository->orderedServerIds().contains(r.serverId)) {
            m_serversRepository->removeServer(r.serverId);
            forgetRentExpiry(r.serverId);
        }
        r.errorCode = amnezia::ErrorCode::InternalError;
        r.message = tr("This device could not store your sign-in securely: the system keychain refused. Nothing was changed.");
        return r;
    }

    ++m_sessionGeneration;
    adoptServer(r.serverId, previousServerId);
    m_signInNeeded = false;
    // A first sign-in has no poll running yet (the UI controller starts it only for a session that
    // already existed at launch, and a logout stops it), so start it here.
    startSessionPolling();
    emit sessionChanged();
    emit sessionRefreshed();
    const bool identityChanged = r.serverId != previousServerId
            || r.deviceId != previous.value(QStringLiteral("deviceId")).toString()
            || r.identity != previous.value(QStringLiteral("identity")).toString();
    emit sessionServerSet(r.serverId, identityChanged);
    return r;
}

AresProfileController::Result AresProfileController::refreshSession(bool *changed, int timeoutMsecs, bool userWaiting)
{
    if (changed) {
        *changed = false;
    }
    Result r;
    if (!m_appSettingsRepository || !hasSession()) {
        r.errorCode = amnezia::ErrorCode::AresBadCredentials;
        r.message = tr("Not signed in.");
        return r;
    }
    // One at a time. The poll timer and a manual refresh can both arrive, and the request spins a
    // nested event loop - re-entering it would interleave two imports over one session.
    if (m_refreshing) {
        r.message = tr("A refresh is already running.");
        return r;
    }
    m_refreshing = true;

    const QVariantMap session = m_appSettingsRepository->getAresSession();
    const QString idx = session.value(QStringLiteral("idx")).toString();
    const QString token = session.value(QStringLiteral("token")).toString();
    const QString previousServerId = session.value(QStringLiteral("serverId")).toString();
    const QString previousDeviceId = session.value(QStringLiteral("deviceId")).toString();
    const QString previousIdentity = session.value(QStringLiteral("identity")).toString();

    // A sign-in or sign-out that lands while this request waits (a background refresh lets input
    // through, H-13) bumps the generation, and the answer is then dropped unwritten.
    RefreshContext context;
    context.session = session;
    context.generation = m_sessionGeneration;
    const bool allowUserInput = !userWaiting;
    r = token.isEmpty()
            ? passwordFetch(session.value(QStringLiteral("id")).toString(),
                            session.value(QStringLiteral("pw")).toString(), idx, timeoutMsecs, &context, allowUserInput)
            : refreshWithToken(token, idx, session.value(QStringLiteral("etag")).toString(), timeoutMsecs,
                               &context, allowUserInput);
    m_refreshing = false;
    if (r.superseded) {
        qDebug() << "AresProfileController: a refresh answer arrived after the session changed - dropped";
        return r;
    }

    if (r.errorCode != amnezia::ErrorCode::NoError) {
        // NOT A LOGOUT, and this is the failure the model has to be designed for. A 404 here means
        // the rent behind this idx is gone and nothing has replaced it YET - which is precisely the
        // window an operator is inside while they retire one rent and hang the same idx on another
        // (#D187's update). Throwing the customer back to a login screen mid-swap would be worse
        // than never refreshing at all. The session stands; the next poll picks up the replacement.
        //
        // A 401 is different (plan 28): the stored credential is dead - the password was changed,
        // or the device and its token were deleted - and asking again every five minutes only fills
        // the console's log with refusals that look like credential stuffing. The poll stops, the
        // stored profile stays (it still works when only the password changed), and the customer is
        // asked to sign in again.
        if (r.httpStatus == 401) {
            m_signInNeeded = true;
            stopSessionPolling();
        }
        emit sessionRefreshFailed(r.httpStatus, r.noAnswer, r.message);
        return r;
    }
    if (m_signInNeeded) {
        // The credential works again (a manual check after a transient refusal): resume the poll.
        m_signInNeeded = false;
        startSessionPolling();
    }
    emit sessionRefreshed();
    if (r.notModified) {
        return r;
    }

    // The same rent can still hand this device a different identity (plan 24: its device was
    // removed and re-created, or its key re-minted). The tunnel must reconnect onto it too. A
    // renewal is NOT one: it rewrites only the file's comment line, which `identity` leaves out.
    // The baseline is persisted with the session, so this holds across a restart. A session with
    // no stored identity (written by a client before plan 24, which sent no `device`) counts as
    // changed once: its stored config is the rent's first device, not this installation's.
    const bool identityChanged = r.deviceId != previousDeviceId || r.identity != previousIdentity;
    const bool serverChanged = r.serverId != previousServerId || identityChanged;

    const QVariantMap next = aresSession::withAnswer(session, r.serverId, r.deviceId, r.identity, r.etag, r.refreshToken);
    if (next != session && !m_appSettingsRepository->setAresSession(next)) {
        // The keychain refused. Announcing a change that is not stored would reconnect on every
        // poll, so say nothing and keep the session as it was.
        r.errorCode = amnezia::ErrorCode::InternalError;
        r.message = tr("This device could not store your sign-in securely: the system keychain refused. Nothing was changed.");
        return r;
    }
    if (serverChanged) {
        // A DIFFERENT RENT, or this device's new identity on the same rent: adopt it (a no-op when
        // the server id is unchanged), and announce it so the tunnel is re-pointed onto it.
        adoptServer(r.serverId, previousServerId);
        if (changed) {
            *changed = true;
        }
        emit sessionChanged();
        emit sessionServerSet(r.serverId, true);
    }
    return r;
}

void AresProfileController::logoutSession()
{
    const QString serverId = sessionServerId();
    ++m_sessionGeneration;
    // Announced FIRST, so the tunnel is closed before the server it runs on is removed (plan 28).
    // Signing out on a shared PC must not leave the next person on this customer's rent.
    emit sessionServerSet(QString(), true);
    if (!serverId.isEmpty() && m_serversRepository
        && m_serversRepository->orderedServerIds().contains(serverId)) {
        m_serversRepository->removeServer(serverId);
    }
    if (m_appSettingsRepository) {
        m_appSettingsRepository->forgetRentExpiry(serverId);
        m_appSettingsRepository->clearAresSession();
    }
    m_signInNeeded = false;
    stopSessionPolling();
    emit sessionChanged();
    emit sessionRefreshed();
}

// The poll. Five minutes by default: the thing it is watching for is an operator re-allocating a
// slot or swapping a rent, which is a human-speed event, and a shorter interval would put a request
// on the console for every running client for no gain (#D061's login guard answers 429 to a client
// that asks too often, and that guard is not to be worked around).
void AresProfileController::startSessionPolling(int intervalSeconds)
{
    const int secs = intervalSeconds > 0 ? intervalSeconds : kPollSeconds;
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, [this]() {
            if (!hasSession() || m_signInNeeded) {
                return;
            }
            bool changed = false;
            refreshSession(&changed);
        });
    }
    m_pollTimer->start(secs * 1000);
}

void AresProfileController::stopSessionPolling()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}

// ---- THE UPDATE MANIFEST (plan 28) ------------------------------------------------------------
//
// The console serves one JSON object with a block per platform:
//   { "windows": { "latest": "5.0.3.0", "min_version": "5.0.0.0",
//                  "url": "https://...", "sha256": "<hex>" }, "android": { ... } }
// `latest` newer than this build shows an offer; this build older than `min_version` refuses to
// connect and says so. `sha256` is for the download page and a future in-app installer; nothing
// here downloads. Anything that does not parse is ignored, and a 404 - the console serves no
// manifest - clears what an earlier check found, so removing the route switches the banner off.
void AresProfileController::checkForUpdate(int timeoutMsecs)
{
    const QString platform = manifestPlatform();
    if (platform.isEmpty()) {
        return;
    }
    // Nobody waits on this check, so it does not hold back input (H-13).
    const Reply reply = request(endpoint() + QString::fromLatin1(kManifestPath), QByteArray(), timeoutMsecs, {}, true);
    UpdateInfo info;
    if (reply.httpStatus == 200 && !reply.sslError) {
        if (!aresSession::judgeManifest(reply.body, platform, QString::fromLatin1(APP_VERSION), &info)) {
            qWarning() << "AresProfileController: the update manifest has no usable entry for" << platform;
        }
    } else if (reply.httpStatus != 404) {
        // Unreachable, or an error: keep what the last good answer said.
        qDebug() << "AresProfileController: update check failed" << reply.httpStatus << reply.transportError;
        return;
    }
    if (info.latest != m_updateInfo.latest || info.url != m_updateInfo.url
        || info.available != m_updateInfo.available || info.required != m_updateInfo.required) {
        m_updateInfo = info;
        emit updateInfoChanged();
    }
}

void AresProfileController::startUpdateChecks()
{
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, [this]() { checkForUpdate(); });
    }
    m_updateTimer->start(kUpdateCheckSeconds * 1000);
    checkForUpdate(10000);
}

QString AresProfileController::rentExpiry(const QString &serverId) const
{
    return m_appSettingsRepository ? m_appSettingsRepository->getRentExpiry(serverId) : QString();
}

void AresProfileController::forgetRentExpiry(const QString &serverId)
{
    if (m_appSettingsRepository) {
        m_appSettingsRepository->forgetRentExpiry(serverId);
    }
}
