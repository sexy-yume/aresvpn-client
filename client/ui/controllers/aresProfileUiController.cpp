// AresVPN Client - the QML face of the login (exposed as `AresProfileController`).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileUiController.h"

#include <QDateTime>

#include "core/utils/containers/containerUtils.h"
#include "core/utils/errorStrings.h"

AresProfileUiController::AresProfileUiController(AresProfileController *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    if (!m_controller) {
        return;
    }
    // The core polls on its own timer (#D187 point 6); QML needs to hear about it, because a rent
    // swapped underneath a live tunnel has to be reconnected onto, not merely re-stored.
    connect(m_controller, &AresProfileController::sessionRentChanged, this,
            [this](const QString &serverId, const QString &) {
                m_lastServerId = serverId;
                emit sessionRentChanged(serverId);
                emit sessionChanged();
            });
    connect(m_controller, &AresProfileController::sessionRefreshFailed, this,
            &AresProfileUiController::sessionRefreshFailed);
    connect(m_controller, &AresProfileController::sessionChanged, this,
            &AresProfileUiController::sessionChanged);

    // Start the poll if this device is already signed in - the customer does not press anything to
    // get it, which is the whole point (#D187: a server-side re-allocation must reach the client
    // without a manual re-login, or that finished feature stays 반쪽짜리).
    if (m_controller->hasSession()) {
        m_controller->startSessionPolling();
    }
}

QString AresProfileUiController::endpoint() const
{
    return m_controller ? m_controller->endpoint() : QString();
}

bool AresProfileUiController::login(const QString &id, const QString &password, const QString &idx)
{
    m_lastError.clear();
    m_lastServerId.clear();
    if (!m_controller) {
        m_lastError = QStringLiteral("no controller");
        emit lastErrorChanged();
        return false;
    }
    // loginSession, not fetchAndImport: this REPLACES the session and takes the previous rent with
    // it. Calling fetchAndImport here is what made the product a collection of rents (#D187).
    const AresProfileController::Result r = m_controller->loginSession(id, password, idx);
    if (r.errorCode != amnezia::ErrorCode::NoError) {
        m_lastError = r.message.isEmpty() ? errorString(r.errorCode) : r.message;
        emit lastErrorChanged();
        return false;
    }
    m_lastServerId = r.serverId;
    emit loginFinished(m_lastServerId);
    emit sessionChanged();
    return true;
}

bool AresProfileUiController::hasSession() const
{
    return m_controller && m_controller->hasSession();
}

QString AresProfileUiController::sessionAccountId() const
{
    return m_controller ? m_controller->sessionAccountId() : QString();
}

QString AresProfileUiController::sessionIdx() const
{
    return m_controller ? m_controller->sessionIdx() : QString();
}

QString AresProfileUiController::sessionServerId() const
{
    return m_controller ? m_controller->sessionServerId() : QString();
}

bool AresProfileUiController::refreshNow()
{
    m_lastError.clear();
    if (!m_controller) {
        return false;
    }
    bool changed = false;
    const AresProfileController::Result r = m_controller->refreshSession(&changed);
    if (r.errorCode != amnezia::ErrorCode::NoError) {
        m_lastError = r.message.isEmpty() ? errorString(r.errorCode) : r.message;
        emit lastErrorChanged();
        return false;
    }
    if (changed) {
        m_lastServerId = r.serverId;
    }
    emit sessionChanged();
    return changed;
}

void AresProfileUiController::logout()
{
    if (m_controller) {
        m_controller->logoutSession();
    }
    m_lastServerId.clear();
    m_lastError.clear();
    emit sessionChanged();
}

// -1 means the console never told us - a config pasted in by hand rather than fetched by a login.
// A rent already past its date returns 0, which the UI renders as EXPIRED rather than as "0 days".
int AresProfileUiController::daysLeftForServer(const QString &serverId) const
{
    if (!m_controller || serverId.isEmpty()) {
        return -1;
    }
    const QString iso = m_controller->rentExpiry(serverId);
    if (iso.isEmpty()) {
        return -1;
    }
    const QDateTime expiry = QDateTime::fromString(iso, Qt::ISODate);
    if (!expiry.isValid()) {
        return -1;
    }
    const qint64 secs = QDateTime::currentDateTimeUtc().secsTo(expiry.toUTC());
    return secs <= 0 ? 0 : static_cast<int>(secs / 86400);
}

QString AresProfileUiController::expiryTextForServer(const QString &serverId) const
{
    const int days = daysLeftForServer(serverId);
    if (days < 0) {
        return QString();
    }
    if (days == 0) {
        return tr("EXPIRED");
    }
    return tr("%n day(s) left", "", days);
}

void AresProfileUiController::forgetRentExpiry(const QString &serverId)
{
    if (m_controller) {
        m_controller->forgetRentExpiry(serverId);
    }
}

// "WireGuard | 112.168.124.190" -> "112.168.124.190". Upstream builds the collapsed description
// as "<protocol> | <address>"; the home screen shows the protocol on its own line already, and
// at display size the address is what gets cut. If the separator is ever absent the whole string
// is returned unchanged, so a shape change upstream degrades to the old behaviour rather than to
// an empty screen.
QString AresProfileUiController::addressOnly(const QString &collapsedDescription) const
{
    const int bar = collapsedDescription.lastIndexOf(QStringLiteral(" | "));
    if (bar < 0) {
        return collapsedDescription;
    }
    return collapsedDescription.mid(bar + 3).trimmed();
}

// The ServersModel exposes `defaultContainer` as the DockerContainer enum's integer, and a rent
// row was rendering it as "3". This is the same table the rest of the app names containers from,
// so a row says WireGuard where the connect screen says WireGuard.
QString AresProfileUiController::protocolLabel(int container) const
{
    const QMap<DockerContainer, QString> names = ContainerUtils::containerHumanNames();
    const auto it = names.constFind(static_cast<DockerContainer>(container));
    return it == names.constEnd() ? QString() : it.value();
}
