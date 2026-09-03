// AresVPN Client - the QML face of the login (exposed as `AresProfileController`).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresProfileUiController.h"

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
