// AresVPN Client - the QML face of the login (exposed as `AresProfileController`).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileUiController.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QTimer>
#include <QUrl>

#include "core/utils/containers/containerUtils.h"
#include "core/utils/errorStrings.h"

namespace
{
    constexpr int kExpiryTickMsecs = 3600 * 1000;
}

AresProfileUiController::AresProfileUiController(AresProfileController *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    if (!m_controller) {
        return;
    }
    // The core polls on its own timer (#D187 point 6) and re-points a running tunnel itself when the
    // session's server is set (CoreSignalHandlers, plan 28); QML only re-reads the session.
    connect(m_controller, &AresProfileController::sessionServerSet, this,
            [this](const QString &serverId, bool) {
                m_lastServerId = serverId;
                emit sessionChanged();
            });
    connect(m_controller, &AresProfileController::sessionChanged, this,
            &AresProfileUiController::sessionChanged);

    // #D187 says the client "refreshes when it can and says when it cannot". The saying was never
    // built: a background 401 or 404 went to a signal nothing listened to (plan 28, H-6). Only the
    // answers a customer can act on, or should know about, are shown; an unreachable console is
    // the normal state of a dead tunnel and is handled, not announced. A 409 is the console
    // refusing this device on the rent now behind the idx (a swap onto a rent at its device limit,
    // plan 30 T02): the stored profile is the retired rent's, and only the customer can make room.
    connect(m_controller, &AresProfileController::sessionRefreshFailed, this,
            [this](int httpStatus, bool, const QString &message) {
                if (httpStatus == 401) {
                    setSessionProblem(tr("This device has to sign in again: the console no longer accepts the sign-in stored on it."));
                } else if (httpStatus == 404) {
                    setSessionProblem(tr("No rent carries idx %1 right now. The app keeps checking and takes the new one as soon as it is assigned.")
                                              .arg(sessionIdx()));
                } else if (httpStatus == 409) {
                    setSessionProblem(message);
                }
            });
    connect(m_controller, &AresProfileController::sessionRefreshed, this, [this]() { setSessionProblem(QString()); });

    // The expiry line re-reads on a fetch that stored a new expiry, and once an hour so a countdown
    // left running crosses midnight and the expiry itself (plan 28, H-10).
    const auto bumpExpiry = [this]() {
        ++m_expiryRevision;
        emit expiryRevisionChanged();
    };
    connect(m_controller, &AresProfileController::imported, this, bumpExpiry);
    m_expiryTick = new QTimer(this);
    connect(m_expiryTick, &QTimer::timeout, this, bumpExpiry);
    m_expiryTick->start(kExpiryTickMsecs);

    connect(m_controller, &AresProfileController::updateInfoChanged, this,
            &AresProfileUiController::updateInfoChanged);

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
    // The customer pressed "Check now" and watches the busy indicator: input waits with them.
    const AresProfileController::Result r = m_controller->refreshSession(&changed, 0, true);
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

void AresProfileUiController::setSessionProblem(const QString &problem)
{
    if (problem == m_sessionProblem) {
        return;
    }
    m_sessionProblem = problem;
    emit sessionProblemChanged();
}

bool AresProfileUiController::signInNeeded() const
{
    return m_controller && m_controller->signInNeeded();
}

bool AresProfileUiController::updateAvailable() const
{
    return m_controller && m_controller->updateInfo().available;
}

bool AresProfileUiController::updateRequired() const
{
    return m_controller && m_controller->updateInfo().required;
}

QString AresProfileUiController::updateVersion() const
{
    return m_controller ? m_controller->updateInfo().latest : QString();
}

void AresProfileUiController::openUpdatePage() const
{
    const QString url = m_controller ? m_controller->updateInfo().url : QString();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
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
