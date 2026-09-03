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

    // Builds the Xray JSON the importer stores for a socks5 rent from its socks5h:// URI. Public
    // so the shape can be checked without a network.
    static QString xrayConfigForSocks5(const QString &uri, QString *errorOut);

signals:
    void imported(const QString &serverId);

private:
    struct Reply
    {
        int httpStatus = 0;
        QByteArray body;
        QString transportError;   // non-empty when the request never got an HTTP answer
        bool sslError = false;
    };
    Reply post(const QString &url, const QByteArray &jsonBody);

    ImportController *m_importController;
    SecureServersRepository *m_serversRepository;
    SecureAppSettingsRepository *m_appSettingsRepository;
};

#endif // ARESPROFILECONTROLLER_H
