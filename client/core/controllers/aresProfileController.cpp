// AresVPN Client - the login: id + password + rent alias -> POST /api/profile -> imported server.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileController.h"

#include <QTimer>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>
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

#ifdef AMNEZIA_DESKTOP
    #include "core/utils/ipcClient.h"
#endif

using namespace amnezia;

namespace
{
    constexpr int kRequestTimeoutMsecs = 30000;
    constexpr char kProfilePath[] = "/api/profile";
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

AresProfileController::Reply AresProfileController::post(const QString &url, const QByteArray &jsonBody)
{
    Reply out;
    QNetworkRequest request;
    request.setTransferTimeout(kRequestTimeoutMsecs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QByteArrayLiteral("X-Client-Request-ID"),
                         QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
    request.setUrl(QUrl(url));

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

    QNetworkReply *reply = amnApp->networkManager()->post(request, jsonBody);
    QEventLoop wait;
    connect(reply, &QNetworkReply::finished, &wait, &QEventLoop::quit);
    // TLS errors are recorded and REFUSED; nothing here ever calls ignoreSslErrors() - the whole
    // reason /api/profile needs no envelope is that the transport is trusted (#D177's update).
    connect(reply, &QNetworkReply::sslErrors, [&out](const QList<QSslError> &errors) { out.sslError = !errors.isEmpty(); });
    wait.exec(QEventLoop::ExcludeUserInputEvents);

    out.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    out.body = reply->readAll();
    if (out.httpStatus == 0) {
        out.transportError = reply->errorString();
    }
    reply->deleteLater();
    return out;
}

AresProfileController::Result AresProfileController::fetchAndImport(const QString &id, const QString &password, const QString &idx)
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
    const Reply reply = post(endpoint() + QString::fromLatin1(kProfilePath), QJsonDocument(payload).toJson(QJsonDocument::Compact));

    if (reply.sslError) {
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console's certificate could not be verified.");
        return result;
    }
    if (reply.httpStatus == 0) {
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console did not answer: %1").arg(reply.transportError);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(reply.body, &parseError);
    const QJsonObject body = doc.isObject() ? doc.object() : QJsonObject();
    const QString serverSentence = body.value(QStringLiteral("error")).toString();

    switch (reply.httpStatus) {
    case 200:
        break;
    case 401:
        result.errorCode = ErrorCode::AresBadCredentials;
        result.message = serverSentence.isEmpty() ? tr("bad credentials") : serverSentence;
        return result;
    case 404:
        result.errorCode = ErrorCode::AresNoSuchRent;
        result.message = serverSentence.isEmpty() ? tr("no such rent") : serverSentence;
        return result;
    case 429:
    case 503:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("Too many attempts - wait a minute and try again.");
        return result;
    default:
        result.errorCode = ErrorCode::AresEndpointUnreachable;
        result.message = tr("The AresVPN console answered HTTP %1.").arg(reply.httpStatus);
        return result;
    }

    result.rentId = body.value(QStringLiteral("rent_id")).toInt();
    result.protocol = body.value(QStringLiteral("protocol")).toString();
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
    const QString serverId = serverIdForRent(result.rentId);
    const serverConfigUtils::ConfigType kind = serverConfigUtils::configTypeFromJson(config);
    if (m_serversRepository->orderedServerIds().contains(serverId)) {
        m_serversRepository->editServer(serverId, config, kind);
    } else {
        m_serversRepository->addServer(serverId, config, kind);
    }
    result.serverId = serverId;

    // A rent expires; a self-hosted server does not, so nothing upstream carries this. Keep what
    // the console told us, verbatim, so the UI can say how long is left (#D178's boundary keeps it
    // out of ServerDescription).
    if (m_appSettingsRepository) {
        m_appSettingsRepository->setRentExpiry(serverId, body.value(QStringLiteral("expires_at")).toString());
    }

    emit imported(serverId);
    return result;
}

// ---- THE LOGIN SESSION (AresProject #D187) --------------------------------------------------
//
// One session: id + pw + idx. The client is bound to the **idx**, not to a rent - whatever rent
// carries that idx right now is the rent this device uses. That is the whole model, and it is why
// there is no list here and nothing to add to.

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
    const QString previousServerId = sessionServerId();

    Result r = fetchAndImport(id, password, idx);
    if (r.errorCode != amnezia::ErrorCode::NoError) {
        // A failed sign-in leaves the previous session exactly as it was. Signing in with the wrong
        // password must not cost a customer the rent they already had.
        return r;
    }

    adoptServer(r.serverId, previousServerId);
    if (m_appSettingsRepository) {
        m_appSettingsRepository->setAresSession(id, password, idx, r.serverId);
    }
    emit sessionChanged();
    return r;
}

AresProfileController::Result AresProfileController::refreshSession(bool *changed)
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
    // One at a time. The poll timer and a manual refresh can both arrive, and fetchAndImport spins
    // a nested event loop - re-entering it would interleave two imports over one session.
    if (m_refreshing) {
        r.message = tr("A refresh is already running.");
        return r;
    }
    m_refreshing = true;

    const QVariantMap session = m_appSettingsRepository->getAresSession();
    const QString id = session.value(QStringLiteral("id")).toString();
    const QString pw = session.value(QStringLiteral("pw")).toString();
    const QString idx = session.value(QStringLiteral("idx")).toString();
    const QString previousServerId = session.value(QStringLiteral("serverId")).toString();

    r = fetchAndImport(id, pw, idx);
    m_refreshing = false;

    if (r.errorCode != amnezia::ErrorCode::NoError) {
        // NOT A LOGOUT, and this is the failure the model has to be designed for. A 404 here means
        // the rent behind this idx is gone and nothing has replaced it YET - which is precisely the
        // window an operator is inside while they retire one rent and hang the same idx on another
        // (#D187's update). Throwing the customer back to a login screen mid-swap would be worse
        // than never refreshing at all. The session stands; the next poll picks up the replacement.
        emit sessionRefreshFailed(r.message);
        return r;
    }

    if (r.serverId != previousServerId) {
        // A DIFFERENT RENT, not merely a new address: the idx now points somewhere else. Adopt it,
        // drop the old one, and tell the UI so it can reconnect onto what the customer actually has.
        adoptServer(r.serverId, previousServerId);
        m_appSettingsRepository->setAresSession(id, pw, idx, r.serverId);
        if (changed) {
            *changed = true;
        }
        emit sessionRentChanged(r.serverId, previousServerId);
        emit sessionChanged();
    }
    return r;
}

void AresProfileController::logoutSession()
{
    const QString serverId = sessionServerId();
    if (!serverId.isEmpty() && m_serversRepository
        && m_serversRepository->orderedServerIds().contains(serverId)) {
        m_serversRepository->removeServer(serverId);
    }
    if (m_appSettingsRepository) {
        m_appSettingsRepository->forgetRentExpiry(serverId);
        m_appSettingsRepository->clearAresSession();
    }
    stopSessionPolling();
    emit sessionChanged();
}

// The poll. Five minutes by default: the thing it is watching for is an operator re-allocating a
// slot or swapping a rent, which is a human-speed event, and a shorter interval would put a request
// on the console for every running client for no gain (#D061's login guard answers 429 to a client
// that asks too often, and that guard is not to be worked around).
void AresProfileController::startSessionPolling(int intervalSeconds)
{
    const int secs = intervalSeconds > 0 ? intervalSeconds : 300;
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, [this]() {
            if (!hasSession()) {
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
