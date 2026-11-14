// AresVPN Client - see aresSessionLogic.h.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
#include "aresSessionLogic.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QVersionNumber>

namespace aresSession
{
    QString identityDigest(const QString &text)
    {
        QStringList kept;
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.startsWith(QLatin1Char('#'))) {
                kept.append(trimmed);
            }
        }
        return QString::fromLatin1(QCryptographicHash::hash(kept.join(QLatin1Char('\n')).trimmed().toUtf8(),
                                                            QCryptographicHash::Sha256).toHex());
    }

    QVariantMap withAnswer(QVariantMap base, const QString &serverId, const QString &deviceId,
                           const QString &identity, const QString &etag, const QString &refreshToken)
    {
        base.insert(QStringLiteral("serverId"), serverId);
        base.insert(QStringLiteral("deviceId"), deviceId);
        base.insert(QStringLiteral("identity"), identity);
        base.insert(QStringLiteral("etag"), etag);
        // The first answer that carries a token takes the password off the device; a console
        // without tokens never sends one, and the session keeps the password as before.
        if (!refreshToken.isEmpty()) {
            base.insert(QStringLiteral("token"), refreshToken);
            base.remove(QStringLiteral("pw"));
        }
        return base;
    }

    bool judgeManifest(const QByteArray &body, const QString &platform, const QString &currentVersion,
                       UpdateInfo *out)
    {
        UpdateInfo info;
        const QJsonObject entry = QJsonDocument::fromJson(body).object().value(platform).toObject();
        const QVersionNumber current = QVersionNumber::fromString(currentVersion);
        const QVersionNumber latest = QVersionNumber::fromString(entry.value(QStringLiteral("latest")).toString());
        const QVersionNumber minimum = QVersionNumber::fromString(entry.value(QStringLiteral("min_version")).toString());
        const QUrl url(entry.value(QStringLiteral("url")).toString());
        // An offer the customer cannot follow is no offer: the link must be https.
        const bool usable = !current.isNull() && !latest.isNull() && url.isValid()
                && url.scheme() == QLatin1String("https");
        if (usable) {
            info.latest = latest.toString();
            info.url = url.toString();
            info.available = QVersionNumber::compare(latest, current) > 0;
            info.required = !minimum.isNull() && QVersionNumber::compare(current, minimum) < 0;
        }
        if (out) {
            *out = info;
        }
        return usable;
    }

    QString chooseDeviceName(const QString &hostName, const QString &model, const QString &productName)
    {
        QString name = hostName.trimmed();
        if (name.isEmpty() || name.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0) {
            name = model.trimmed();
        }
        if (name.isEmpty()) {
            name = productName.trimmed();
        }
        return name.left(64);
    }

    QString deviceKeyFromAndroidId(const QString &androidId)
    {
        const QString id = androidId.trimmed();
        if (id.isEmpty()) {
            return QString();
        }
        // Hashed, so the raw ANDROID_ID - an identifier other apps of the same signer can read - is
        // never what the console stores. 32 hex digits, the length of the uuid it replaces.
        const QByteArray digest = QCryptographicHash::hash(
                QByteArrayLiteral("aresvpn-device:") + id.toUtf8(), QCryptographicHash::Sha256).toHex();
        return QStringLiteral("a-") + QString::fromLatin1(digest.left(32));
    }

    bool serverNeedsWrite(bool stored, const QString &sessionServerId, const QString &sessionIdentity,
                          const QString &serverId, const QString &identity)
    {
        return !stored || sessionServerId != serverId || sessionIdentity.isEmpty() || sessionIdentity != identity;
    }
}
