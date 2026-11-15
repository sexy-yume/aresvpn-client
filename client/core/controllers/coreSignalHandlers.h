#ifndef CORESIGNALHANDLERS_H
#define CORESIGNALHANDLERS_H

#include <QElapsedTimer>
#include <QObject>
#include "core/controllers/coreController.h"

class CoreSignalHandlers : public QObject
{
    Q_OBJECT

public:
    explicit CoreSignalHandlers(CoreController* coreController, QObject* parent = nullptr);

    void initAllHandlers();

private:
    void initErrorMessagesHandler();
    void initSettingsSplitTunnelingHandler();
    void initInstallControllerHandler();
    void initExportControllerHandler();
    void initImportControllerHandler();
    void initApiCountryModelUpdateHandler();
    void initSubscriptionRefreshHandler();
    void initAdminConfigRevokedHandler();
    void initPassphraseRequestHandler();
    void initTranslationsUpdatedHandler();
    void initLanguageHandler();
    void initAutoConnectHandler();
    void initAmneziaDnsToggledHandler();
    void initServersModelUpdateHandler();
    void initClientManagementModelUpdateHandler();
    void initSitesModelUpdateHandler();
    void initAllowedDnsModelUpdateHandler();
    void initAppSplitTunnelingModelUpdateHandler();
    void initPrepareConfigHandler();
    void initUnsupportedConnectDrawerHandler();
    void initStrictKillSwitchHandler();
    void initAndroidSettingsHandler();
    void initAndroidConnectionHandler();
    void initIosImportHandler();
    void initIosSettingsHandler();
    void initNotificationHandler();
    void initUpdateFoundHandler();
    void initAresSessionTunnelHandler();  // AresVPN Client (plan 24, plan 28)

    // AresVPN Client (plan 28): THE ONE ROUTINE by which the session re-points the tunnel - close it
    // if it is up, set the default server to the session's, reopen it if the session still has a
    // server the console has not refused (aresSession::afterTunnelClosed). `refreshWhileDown` also
    // refreshes the session in between, while no tunnel carries it.
    enum class AresPending { None, Reopen, RefreshThenReopen };
    void aresRepointTunnel(bool refreshWhileDown);
    void aresAfterTunnelClosed(AresPending pending);  // the routine's second half, once Disconnected
    bool aresAdoptSessionServer();  // the default server becomes the session's; true when it moved
    bool aresTunnelUp() const;      // Connected, Connecting or Reconnecting - not Disconnecting

    CoreController* m_coreController;
    AresPending m_aresPending = AresPending::None;  // what to do once the tunnel reports Disconnected
    Vpn::ConnectionState m_aresTunnelState = Vpn::ConnectionState::Disconnected;
    QElapsedTimer m_aresLastStallCycle;             // the last close -> refresh -> reopen for a stall
    bool m_aresReopenOwed = false;                  // the tunnel stays closed only until the console
                                                    // accepts the session's profile again (H3-V2)
};

#endif // CORESIGNALHANDLERS_H

