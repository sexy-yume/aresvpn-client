#ifndef SECUREAPPSETTINGSREPOSITORY_H
#define SECUREAPPSETTINGSREPOSITORY_H

#include <QObject>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QByteArray>

#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"
#include "secureQSettings.h"

using namespace amnezia;

class SecureAppSettingsRepository : public QObject
{
    Q_OBJECT

public:
    explicit SecureAppSettingsRepository(SecureQSettings* settings, QObject *parent = nullptr);

    QLocale getAppLanguage() const;
    void setAppLanguage(QLocale locale);

    bool useAmneziaDns() const;
    void setUseAmneziaDns(bool enabled);
    QStringList getAllowedDnsServers() const;
    void setAllowedDnsServers(const QStringList &servers);
    QString primaryDns() const;
    void setPrimaryDns(const QString &dns);
    QString secondaryDns() const;
    void setSecondaryDns(const QString &dns);

    RouteMode routeMode() const;
    void setRouteMode(RouteMode mode);
    bool addVpnSite(RouteMode mode, const QString &site, const QStringList &ips = {});
    void addVpnSites(RouteMode mode, const QMap<QString, QStringList> &sites);
    void removeVpnSite(RouteMode mode, const QString &site);
    void removeAllVpnSites(RouteMode mode);
    QVariantMap vpnSites(RouteMode mode) const;

    // Normalizes a stored vpn site value into a list of IPs.
    // Supports both the legacy format (a single IP string) and the current one (a list of IPs).
    static QStringList siteIpList(const QVariant &value);
    bool isSitesSplitTunnelingEnabled() const;
    void setSitesSplitTunnelingEnabled(bool enabled);

    AppsRouteMode appsRouteMode() const;
    void setAppsRouteMode(AppsRouteMode mode);
    void setVpnApps(AppsRouteMode mode, const QVector<InstalledAppInfo> &apps);
    QVector<InstalledAppInfo> vpnApps(AppsRouteMode mode) const;
    bool isAppsSplitTunnelingEnabled() const;
    void setAppsSplitTunnelingEnabled(bool enabled);

    QString getGatewayEndpoint(bool isTestPurchase = false) const;
    void setGatewayEndpoint(const QString &endpoint);
    void resetGatewayEndpoint();
    void setDevGatewayEndpoint();
    bool isDevGatewayEnv(bool isTestPurchase = false) const;
    void toggleDevGatewayEnv(bool enabled);
    QByteArray readGatewayProxyUrls(const QString &cacheKey) const;

    // AresVPN Client: the console's base URL (Conf/aresEndpoint), empty = the compiled default
    QString getAresEndpoint() const;
    void setAresEndpoint(const QString &endpoint);
    void resetAresEndpoint();

    // AresVPN Client: a rent EXPIRES, which is the one thing about our product that upstream's
    // server model has no field for. Rather than thread a new column through ServerDescription and
    // the five buildServerDescription overloads - core shape #D178 says to leave alone - the
    // expiry is kept beside the servers, keyed by the deterministic server id (`ares-rent-<id>`).
    QString getRentExpiry(const QString &serverId) const;
    void setRentExpiry(const QString &serverId, const QString &expiresAtIso);
    void forgetRentExpiry(const QString &serverId);

    // AresVPN Client - THE LOGIN SESSION (AresProject #D187). One session, not a list: `id` + `pw`
    // + `idx`, plus the id of the one stored server that session currently owns.
    //
    // THE PASSWORD IS HERE ON PURPOSE, and it is the operator's decision rather than mine
    // (*비밀번호를 기기에 저장한다*, #L007). It is what lets the client re-ask `/api/profile` for its
    // `idx` without the customer, which is the whole of #D187 point 6: the server side already
    // re-assigns a rent when a slot's address is re-allocated, and downloading only on a manual
    // re-login leaves that finished feature half-working. This map lives in SecureQSettings, the
    // same encrypted store the configurations use; it never reaches a command line (#D030) or a log.
    //
    // Keys: "id", "pw", "idx", "serverId". An empty map means NOT LOGGED IN, which is the branch
    // the first screen reads - not "zero servers stored", which is what #D182 was implemented as
    // and is a different question that only coincides by accident.
    QVariantMap getAresSession() const;
    void setAresSession(const QString &id, const QString &password, const QString &idx,
                        const QString &serverId);
    void clearAresSession();
    void writeGatewayProxyUrls(const QString &cacheKey, const QByteArray &proxyUrlsEncrypted);

    bool isKillSwitchEnabled() const;
    void setKillSwitchEnabled(bool enabled);
    bool isStrictKillSwitchEnabled() const;
    void setStrictKillSwitchEnabled(bool enabled);
    
    bool isAutoConnect() const;
    void setAutoConnect(bool enabled);
    bool isStartMinimized() const;
    void setStartMinimized(bool enabled);
    bool isScreenshotsEnabled() const;
    void setScreenshotsEnabled(bool enabled);
    bool isNewsNotifications() const;
    void setNewsNotifications(bool enabled);

    bool isAutoUpdateCheckEnabled() const;
    void setAutoUpdateCheckEnabled(bool enabled);
    bool isSaveLogs() const;
    void setSaveLogs(bool enabled);
    QDateTime getLogEnableDate() const;
    void setLogEnableDate(const QDateTime &date);
    
    QString getInstallationUuid(bool createIfNotExists) const;
    QStringList getReadNewsIds() const;
    void setReadNewsIds(const QStringList &ids);

    bool isHomeAdLabelVisible() const;
    void disableHomeAdLabel();
    QByteArray backupAppConfig() const;
    bool restoreAppConfig(const QByteArray &cfg);
    void clearSettings();

    QByteArray xraySavedConfigs() const;
    void setXraySavedConfigs(const QByteArray &data);

signals:
    void appLanguageChanged(QLocale locale);
    void allowedDnsServersChanged(const QStringList &servers);
    void sitesChanged(RouteMode mode);
    void appsChanged(AppsRouteMode mode);
    void routeModeChanged(RouteMode mode);
    void appsRouteModeChanged(AppsRouteMode mode);
    void sitesSplitTunnelingEnabledChanged(bool enabled);
    void appsSplitTunnelingEnabledChanged(bool enabled);
    void useAmneziaDnsChanged(bool enabled);
    void saveLogsChanged(bool enabled);
    void screenshotsEnabledChanged(bool enabled);
    void settingsCleared();

private:
    void setVpnSites(RouteMode mode, const QVariantMap &sites);
    void setInstallationUuid(const QString &uuid);
    
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    SecureQSettings* m_settings;
    QString m_gatewayEndpoint;
};

#endif // SECUREAPPSETTINGSREPOSITORY_H

