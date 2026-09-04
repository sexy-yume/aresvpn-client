// AresVPN Client - the QML face of the login (exposed as `AresProfileController`).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileUiController.h"

#include <QDateTime>

#include "core/utils/errorStrings.h"

AresProfileUiController::AresProfileUiController(AresProfileController *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
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
    const AresProfileController::Result r = m_controller->fetchAndImport(id, password, idx);
    if (r.errorCode != amnezia::ErrorCode::NoError) {
        m_lastError = r.message.isEmpty() ? errorString(r.errorCode) : r.message;
        emit lastErrorChanged();
        return false;
    }
    m_lastServerId = r.serverId;
    emit loginFinished(m_lastServerId);
    return true;
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
