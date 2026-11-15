#include "coreSignalHandlers.h"

#include "amneziaApplication.h"

#include <QTimer>
#include <QtConcurrent>

#include "core/utils/selfhosted/sshSession.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/controllers/coreController.h"
#include "core/repositories/secureServersRepository.h"
#include "core/utils/serverConfigUtils.h"
#include "core/repositories/secureAppSettingsRepository.h"
#include "vpnConnection.h"
#include "ui/controllers/qml/pageController.h"
#include "ui/controllers/connectionUiController.h"
#include "ui/controllers/settingsUiController.h"
#include "ui/controllers/serversUiController.h"
#include "ui/controllers/ipSplitTunnelingUiController.h"
#include "ui/controllers/allowedDnsUiController.h"
#include "ui/controllers/appSplitTunnelingUiController.h"
#include "ui/controllers/languageUiController.h"
#include "ui/controllers/selfhosted/installUiController.h"
#include "ui/controllers/importUiController.h"
#include "ui/controllers/api/subscriptionUiController.h"
#include "ui/controllers/updateUiController.h"
#include "ui/models/serversModel.h"
#include "core/controllers/serversController.h"
#include "core/controllers/ipSplitTunnelingController.h"
#include "core/controllers/appSplitTunnelingController.h"
#include "core/controllers/selfhosted/usersController.h"
#include "core/controllers/settingsController.h"
#include "core/controllers/selfhosted/installController.h"
#include "core/controllers/selfhosted/exportController.h"
#include "core/controllers/connectionController.h"
#include "ui/models/clientManagementModel.h"
#include "ui/controllers/api/apiNewsUiController.h"
#include "ui/models/containersModel.h"
#include "core/utils/containerEnum.h"

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    #include "ui/utils/notificationHandler.h"
    #include "ui/utils/systemTrayNotificationHandler.h"
#endif

#ifdef Q_OS_ANDROID
    #include "platforms/android/android_controller.h"
#endif

#ifdef Q_OS_IOS
    #include "platforms/ios/ios_controller.h"
    #include "core/utils/swiftBridge.h"
#endif

namespace
{
    // AresVPN Client (plan 28). A refresh made while no tunnel carries it - at start, and inside the
    // stall cycle - waits this long before the stored profile is used as it is.
    constexpr int kAresDirectRefreshMsecs = 10000;
    // A stall cycle drops the tunnel for a few seconds. When the console is what is down, every poll
    // would look like a stall, so the cycle runs at most this often.
    constexpr qint64 kAresStallCycleMsecs = 3600 * 1000;
}

CoreSignalHandlers::CoreSignalHandlers(CoreController* coreController, QObject* parent)
    : QObject(parent),
      m_coreController(coreController)
{
}

void CoreSignalHandlers::initAllHandlers()
{
    initErrorMessagesHandler();
    initSettingsSplitTunnelingHandler();
    initInstallControllerHandler();
    initExportControllerHandler();
    initImportControllerHandler();
    initApiCountryModelUpdateHandler();
    initSubscriptionRefreshHandler();
    initAdminConfigRevokedHandler();
    initPassphraseRequestHandler();
    initTranslationsUpdatedHandler();
    initLanguageHandler();
    initAutoConnectHandler();
    initAmneziaDnsToggledHandler();
    initServersModelUpdateHandler();
    initClientManagementModelUpdateHandler();
    initSitesModelUpdateHandler();
    initAllowedDnsModelUpdateHandler();
    initAppSplitTunnelingModelUpdateHandler();
    initPrepareConfigHandler();
    initUnsupportedConnectDrawerHandler();
    initStrictKillSwitchHandler();
    initAndroidSettingsHandler();
    initAndroidConnectionHandler();
    initIosImportHandler();
    initIosSettingsHandler();
    initNotificationHandler();
    initUpdateFoundHandler();
    initAresSessionTunnelHandler();
}

void CoreSignalHandlers::initErrorMessagesHandler()
{
    connect(m_coreController->m_connectionUiController, &ConnectionUiController::connectionErrorOccurred, this, [this](ErrorCode errorCode) {
        emit m_coreController->m_pageController->showErrorMessage(errorCode);
        m_coreController->m_connectionController->setConnectionState(Vpn::ConnectionState::Disconnected);
    });

    connect(m_coreController->m_subscriptionUiController, &SubscriptionUiController::errorOccurred, m_coreController->m_pageController,
            qOverload<ErrorCode>(&PageController::showErrorMessage));

    connect(m_coreController->m_settingsUiController, &SettingsUiController::errorOccurred, m_coreController->m_pageController,
            qOverload<ErrorCode>(&PageController::showErrorMessage));
}

void CoreSignalHandlers::initSettingsSplitTunnelingHandler()
{
    connect(m_coreController->m_settingsController, &SettingsController::siteSplitTunnelingRouteModeChanged, this, [this](RouteMode mode) {
        m_coreController->m_ipSplitTunnelingController->setRouteMode(mode);
    });
    connect(m_coreController->m_settingsController, &SettingsController::siteSplitTunnelingToggled, this, [this](bool enabled) {
        m_coreController->m_ipSplitTunnelingController->toggleSplitTunneling(enabled);
    });
    connect(m_coreController->m_settingsController, &SettingsController::appSplitTunnelingRouteModeChanged, this, [this](AppsRouteMode mode) {
        m_coreController->m_appSplitTunnelingController->setRouteMode(mode);
    });
    connect(m_coreController->m_settingsController, &SettingsController::appSplitTunnelingToggled, this, [this](bool enabled) {
        m_coreController->m_appSplitTunnelingController->toggleSplitTunneling(enabled);
    });
    connect(m_coreController->m_settingsController, &SettingsController::appSplitTunnelingClearAppsList, this, [this]() {
        m_coreController->m_appSplitTunnelingController->clearAppsList();
    });
}

void CoreSignalHandlers::initInstallControllerHandler()
{
    connect(m_coreController->m_installController, &InstallController::serverIsBusy, m_coreController->m_installUiController, &InstallUiController::serverIsBusy);
    connect(m_coreController->m_installUiController, &InstallUiController::cancelInstallation, m_coreController->m_installController, &InstallController::cancelInstallation);
    connect(m_coreController->m_serversUiController, &ServersUiController::processedServerIdChanged,
        m_coreController->m_installUiController, [this](const QString &serverId) {
        if (!serverId.isEmpty()) {
            m_coreController->m_installUiController->clearProcessedServerCredentials();
        }
    });
}

void CoreSignalHandlers::initExportControllerHandler()
{
    connect(m_coreController->m_exportController, &ExportController::appendClientRequested, this,
            [this](const QString &serverId, const QString &clientId, const QString &clientName, DockerContainer container) {
                m_coreController->m_usersController->appendClient(serverId, clientId, clientName, container);
            });
    connect(m_coreController->m_exportController, &ExportController::updateClientsRequested, this,
            [this](const QString &serverId, DockerContainer container) {
                m_coreController->m_usersController->updateClients(serverId, container);
            });
    connect(m_coreController->m_exportController, &ExportController::revokeClientRequested, this,
            [this](const QString &serverId, int row, DockerContainer container) {
                QtConcurrent::run([this, serverId, row, container]() {
                    m_coreController->m_usersController->revokeClient(serverId, row, container);
                });
            });
    connect(m_coreController->m_exportController, &ExportController::renameClientRequested, this,
            [this](const QString &serverId, int row, const QString &clientName, DockerContainer container) {
                m_coreController->m_usersController->renameClient(serverId, row, clientName, container);
            });
}

void CoreSignalHandlers::initImportControllerHandler()
{
    connect(m_coreController->m_importCoreController, &ImportController::importFinished, this, [this]() {
        if (m_coreController->m_connectionUiController->isConnected()) {
            return;
        }

        const int newServerIndex = m_coreController->m_serversController->getServersCount() - 1;
        const QString serverId = m_coreController->m_serversController->getServerId(newServerIndex);
        // AresVPN Client (H-8): while signed in, Connect connects the session's rent (D187 rule 5).
        // An imported file is stored, but it no longer takes the default from the session - that
        // put Connect on the file, possibly another device's key, until the next session event.
        if (!serverId.isEmpty() && !m_coreController->m_aresProfileController->hasSession()) {
            m_coreController->m_serversController->setDefaultServer(serverId);
        }
        if (m_coreController->m_serversUiController) {
            m_coreController->m_serversUiController->setProcessedServerId(serverId);
        }
    });
}

void CoreSignalHandlers::initApiCountryModelUpdateHandler()
{
    connect(m_coreController->m_serversUiController, &ServersUiController::updateApiCountryModel, this, [this]() {
        const QString processedServerId = m_coreController->m_serversUiController->getProcessedServerId();
        if (processedServerId.isEmpty()) {
            return;
        }

        const auto apiV2 = m_coreController->m_serversRepository->apiV2Config(processedServerId);
        if (!apiV2.has_value()) {
            return;
        }

        m_coreController->m_apiCountryModel->updateModel(apiV2->apiConfig.availableCountries,
                                                           apiV2->apiConfig.serverCountryCode);
    });
}

void CoreSignalHandlers::initSubscriptionRefreshHandler()
{
    connect(m_coreController->m_subscriptionUiController, &SubscriptionUiController::subscriptionRefreshNeeded, this, [this]() {
        const QString defaultServerId = m_coreController->m_serversController->getDefaultServerId();
        if (!defaultServerId.isEmpty()) {
            m_coreController->m_subscriptionUiController->getAccountInfo(defaultServerId, false);
        }
    });
}

void CoreSignalHandlers::initAdminConfigRevokedHandler()
{
    connect(m_coreController->m_installController, &InstallController::clientRevocationRequested, this,
            [this](const QString &serverId, const ContainerConfig &containerConfig, DockerContainer container) {
                m_coreController->m_usersController->revokeClient(serverId, containerConfig, container);
            }, Qt::DirectConnection);

    connect(m_coreController->m_installController, &InstallController::clientAppendRequested, this,
            [this](const QString &serverId, const QString &clientId, const QString &clientName, DockerContainer container) {
                m_coreController->m_usersController->appendClient(serverId, clientId, clientName, container);
            }, Qt::DirectConnection);

    connect(m_coreController->m_usersController, &UsersController::adminConfigRevoked, m_coreController->m_installController,
            &InstallController::clearCachedProfile);
}

void CoreSignalHandlers::initPassphraseRequestHandler()
{
    connect(m_coreController->m_installUiController, &InstallUiController::passphraseRequestStarted, m_coreController->m_pageController,
            &PageController::showPassphraseRequestDrawer);
    connect(m_coreController->m_pageController, &PageController::passphraseRequestDrawerClosed, m_coreController->m_installUiController,
            &InstallUiController::setEncryptedPassphrase);
}

void CoreSignalHandlers::initTranslationsUpdatedHandler()
{
    connect(m_coreController->m_languageUiController, &LanguageUiController::updateTranslations, m_coreController, &CoreController::updateTranslator);
    connect(m_coreController, &CoreController::translationsUpdated, m_coreController->m_languageUiController, &LanguageUiController::translationsUpdated);
    connect(m_coreController, &CoreController::translationsUpdated, m_coreController->m_connectionUiController, &ConnectionUiController::onTranslationsUpdated);
}

void CoreSignalHandlers::initLanguageHandler()
{
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::appLanguageChanged, m_coreController->m_languageUiController, &LanguageUiController::onAppLanguageChanged);
    connect(m_coreController->m_settingsUiController, &SettingsUiController::resetLanguageToSystem, m_coreController->m_languageUiController, [this]() {
        m_coreController->m_languageUiController->changeLanguage(m_coreController->m_languageUiController->getSystemLanguageEnum());
    });
    connect(m_coreController->m_settingsUiController, &SettingsUiController::appLanguageChanged, m_coreController->m_languageUiController, [this]() {
        m_coreController->m_languageUiController->onAppLanguageChanged(m_coreController->m_settingsController->getAppLanguage());
    });
}

void CoreSignalHandlers::initAutoConnectHandler()
{
    // A HARNESS PROCESS DOES NOT BRING UP A TUNNEL. See AmneziaApplication::isHarnessMode().
    if (AmneziaApplication::isHarnessMode()) {
        qDebug() << "auto-connect: skipped, this is a harness run";
        return;
    }

    QTimer::singleShot(1000, this, [this]() {
        AresProfileController *ares = m_coreController->m_aresProfileController;

        // AresVPN Client (plan 28): REFRESH FIRST, while no tunnel is up. The session's first poll
        // used to come 300 s after start, and auto-connect a second after it - so a stale profile
        // (a re-allocated slot, a deleted or re-keyed device) brought up a dead tunnel, and every
        // refresh after that went into it and timed out. Asked now, the console answers outside any
        // tunnel; if it does not answer within kAresDirectRefreshMsecs, the stored profile is used.
        int refreshStatus = 0;
        if (ares->hasSession()) {
            qDebug() << "ares session: refreshing before auto-connect";
            const AresProfileController::Result r = ares->refreshSession(nullptr, kAresDirectRefreshMsecs);
            refreshStatus = r.superseded ? 0 : r.httpStatus;
            aresAdoptSessionServer();
        }
        ares->startUpdateChecks();

        const bool enabled = m_coreController->m_settingsUiController->isAutoConnectEnabled();
        const QString defaultServerId = m_coreController->m_serversController->getDefaultServerId();

        // AND IT SAYS WHAT IT DECIDED, both ways. The operator turned this on, launched, and nothing
        // connected - and there was no way to tell whether it had never fired, fired and been refused,
        // or fired and failed. A feature that silently does not happen is the shape this project keeps
        // paying for (#L012); one line removes the question.
        if (!enabled) {
            qDebug() << "auto-connect: off in settings";
            return;
        }
        if (defaultServerId.isEmpty()) {
            qDebug() << "auto-connect: on, but no rent is set up yet - nothing to connect to";
            return;
        }
        if (aresTunnelUp()) {
            // A tunnel the system kept (Android restores one); toggling would close it.
            qDebug() << "auto-connect: the tunnel is already up";
            return;
        }
        if (ares->signInNeeded() || aresSession::refusesStoredProfile(refreshStatus)) {
            // The refresh above refused the stored profile (401, 404, 409): it may be a removed
            // device's or a retired rent's, and opening it unasked would leave the customer in
            // Connecting. Home says why; a Connect they press still uses it (plan 30 T02, H3-3,
            // H3-V2). The profile the console accepts next is opened (m_aresReopenOwed).
            qDebug() << "auto-connect: refused, the console refused the stored profile (" << refreshStatus << ")";
            m_aresReopenOwed = true;
            return;
        }
        qDebug() << "auto-connect: firing for" << defaultServerId;
        m_coreController->m_connectionUiController->toggleConnection();
    });
}

void CoreSignalHandlers::initAmneziaDnsToggledHandler()
{
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::useAmneziaDnsChanged, m_coreController->m_serversUiController, &ServersUiController::updateModel);
}

void CoreSignalHandlers::initServersModelUpdateHandler()
{
    connect(m_coreController->m_serversRepository, &SecureServersRepository::serverAdded,
            m_coreController->m_serversUiController, &ServersUiController::updateModel);
    connect(m_coreController->m_serversRepository, &SecureServersRepository::serverEdited,
            m_coreController->m_serversUiController, &ServersUiController::updateModel);
    connect(m_coreController->m_serversRepository, &SecureServersRepository::serverRemoved,
            m_coreController->m_serversUiController, &ServersUiController::updateModel);
    connect(m_coreController->m_serversRepository, &SecureServersRepository::defaultServerChanged,
            m_coreController->m_serversUiController, &ServersUiController::onDefaultServerChanged);

    connect(m_coreController->m_serversRepository, &SecureServersRepository::serverAdded, this,
            [this](const QString &serverId) {
                if (m_coreController->m_serversRepository->apiV2Config(serverId).has_value()) {
                    m_coreController->m_apiNewsUiController->fetchNews(false);
                }
            });

    connect(m_coreController->m_settingsUiController, &SettingsUiController::restoreBackupFinished, this, [this]() {
        m_coreController->m_serversUiController->updateModel();
        if (m_coreController->m_serversUiController->hasServersFromGatewayApi()) {
            m_coreController->m_apiNewsUiController->fetchNews(false);
        }
    });
}

void CoreSignalHandlers::initClientManagementModelUpdateHandler()
{
    connect(m_coreController->m_usersController, &UsersController::clientsUpdated,
            m_coreController->m_clientManagementModel, &ClientManagementModel::updateModel);
    connect(m_coreController->m_usersController, &UsersController::clientRenamed,
            m_coreController->m_clientManagementModel, &ClientManagementModel::updateClientName);
    connect(m_coreController->m_usersController, &UsersController::revokeFinished,
            m_coreController->m_exportController, &ExportController::revokeFinished);
}

void CoreSignalHandlers::initSitesModelUpdateHandler()
{
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::sitesChanged, m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::updateModel);
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::sitesSplitTunnelingEnabledChanged, m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::updateModel);
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::routeModeChanged, m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::updateModel);
}

void CoreSignalHandlers::initAllowedDnsModelUpdateHandler()
{
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::allowedDnsServersChanged, m_coreController->m_allowedDnsUiController, &AllowedDnsUiController::updateModel);
}

void CoreSignalHandlers::initAppSplitTunnelingModelUpdateHandler()
{
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::appsChanged, m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::updateModel);
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::appsSplitTunnelingEnabledChanged, m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::updateModel);
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::appsRouteModeChanged, m_coreController->m_appSplitTunnelingUiController, &AppSplitTunnelingUiController::updateModel);
}

void CoreSignalHandlers::initPrepareConfigHandler()
{
    connect(m_coreController->m_connectionUiController, &ConnectionUiController::prepareConfig, this, [this]() {
        m_coreController->m_connectionController->setConnectionState(Vpn::ConnectionState::Preparing);

        const QString serverId = m_coreController->m_serversController->getDefaultServerId();
        if (serverId.isEmpty()) {
            m_coreController->m_connectionController->setConnectionState(Vpn::ConnectionState::Disconnected);
            return;
        }

        const serverConfigUtils::ConfigType kind = m_coreController->m_serversRepository->serverKind(serverId);

        if (serverConfigUtils::isApiV2Subscription(kind) || serverConfigUtils::isLegacyApiSubscription(kind)) {
            m_coreController->m_subscriptionUiController->validateConfig();
        } else {
            m_coreController->m_installUiController->validateConfig();
        }
    });

    connect(m_coreController->m_subscriptionUiController, &SubscriptionUiController::configValidated, this, [this](bool isValid) {
        if (!isValid) {
            m_coreController->m_connectionController->setConnectionState(Vpn::ConnectionState::Disconnected);
            return;
        }

        m_coreController->m_connectionUiController->openConnection();
    });

    connect(m_coreController->m_installUiController, &InstallUiController::configValidated, this, [this](bool isValid) {
        if (!isValid) {
            m_coreController->m_connectionController->setConnectionState(Vpn::ConnectionState::Disconnected);
            return;
        }

        m_coreController->m_connectionUiController->openConnection();
    });
}

void CoreSignalHandlers::initUnsupportedConnectDrawerHandler()
{
    connect(m_coreController->m_subscriptionUiController, &SubscriptionUiController::unsupportedConnectDrawerRequested,
            m_coreController->m_pageController, &PageController::unsupportedConnectDrawerRequested);

    connect(m_coreController->m_connectionUiController, &ConnectionUiController::unsupportedConnectDrawerRequested,
            m_coreController->m_pageController, &PageController::unsupportedConnectDrawerRequested);
}

void CoreSignalHandlers::initStrictKillSwitchHandler()
{
    connect(m_coreController->m_settingsUiController, &SettingsUiController::strictKillSwitchEnabledChanged, m_coreController->m_connectionController,
            &ConnectionController::onKillSwitchModeChanged);
}

void CoreSignalHandlers::initAndroidSettingsHandler()
{
#ifdef Q_OS_ANDROID
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::saveLogsChanged, AndroidController::instance(), &AndroidController::setSaveLogs);
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::screenshotsEnabledChanged, AndroidController::instance(), &AndroidController::setScreenshotsEnabled);
    connect(m_coreController->m_serversRepository, &SecureServersRepository::serverRemoved, this,
            [](const QString &/*serverId*/, int removedIndex) {
                AndroidController::instance()->resetLastServer(removedIndex);
            });
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::settingsCleared, []() {
        AndroidController::instance()->resetLastServer(-1);
        AndroidController::instance()->forgetLastConfig();
    });
    // AresVPN Client (K2-1, H3-V1): the VPN service keeps the last tunnel config it started - for a
    // WireGuard rent, the device's private key - and starts it again from Always-on, the Quick
    // Settings tile and the notification's Connect, inside the service and without any of the app's
    // checks. So the service starts only a config the app would open unasked: it goes on sign-out
    // (the one empty `sessionServerSet`, or the next person on the phone gets the rent back), when
    // the console refuses the stored profile (401, 404, 409: D305), and when this build is below the
    // console's min_version. A Connect pressed in the app passes the config and saves it again.
    AresProfileController *ares = m_coreController->m_aresProfileController;
    connect(ares, &AresProfileController::sessionServerSet, this,
            [](const QString &serverId, bool) {
                if (serverId.isEmpty()) {
                    AndroidController::instance()->forgetLastConfig();
                }
            });
    connect(ares, &AresProfileController::sessionRefreshFailed, this, [](int httpStatus, bool, const QString &) {
        if (aresSession::refusesStoredProfile(httpStatus)) {
            AndroidController::instance()->forgetLastConfig();
        }
    });
    connect(ares, &AresProfileController::updateInfoChanged, this, [ares]() {
        if (ares->updateInfo().required) {
            AndroidController::instance()->forgetLastConfig();
        }
    });
#endif
}

void CoreSignalHandlers::initAndroidConnectionHandler()
{
#ifdef Q_OS_ANDROID
    connect(AndroidController::instance(), &AndroidController::initConnectionState, this, [this](Vpn::ConnectionState state) {
        m_coreController->m_connectionUiController->onConnectionStateChanged(state);
        m_coreController->m_connectionController->restoreConnection();
    });
    connect(AndroidController::instance(), &AndroidController::importConfigFromOutside, this, [this](QString data) {
        emit m_coreController->m_pageController->goToPageHome();
        m_coreController->m_importController->extractConfigFromData(data);
        data.clear();
        emit m_coreController->m_pageController->goToPageViewConfig();
    });
#endif
}

void CoreSignalHandlers::initIosImportHandler()
{
#ifdef Q_OS_IOS
    connect(IosController::Instance(), &IosController::importConfigFromOutside, this, [this](QString data) {
        emit m_coreController->m_pageController->goToPageHome();
        m_coreController->m_importController->extractConfigFromData(data);
        emit m_coreController->m_pageController->goToPageViewConfig();
    });
    connect(IosController::Instance(), &IosController::importBackupFromOutside, this, [this](QString filePath) {
        emit m_coreController->m_pageController->goToPageHome();
        m_coreController->m_pageController->goToPageSettingsBackup();
        emit m_coreController->m_settingsUiController->importBackupFromOutside(filePath);
    });
#endif
}

void CoreSignalHandlers::initIosSettingsHandler()
{
#ifdef Q_OS_IOS
    connect(m_coreController->m_appSettingsRepository, &SecureAppSettingsRepository::screenshotsEnabledChanged, [](bool enabled) { SWIFT_BRIDGE_NAMESPACE::toggleScreenshots(enabled); });
#endif
}

void CoreSignalHandlers::initNotificationHandler()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    m_coreController->m_notificationHandler = NotificationHandler::create(m_coreController);

    connect(m_coreController->m_connectionController, &ConnectionController::connectionStateChanged, m_coreController->m_notificationHandler,
            &NotificationHandler::setConnectionState);

    connect(m_coreController->m_notificationHandler, &NotificationHandler::raiseRequested, m_coreController->m_pageController, &PageController::raiseMainWindow);
    connect(m_coreController->m_notificationHandler, &NotificationHandler::connectRequested, m_coreController->m_connectionUiController,
            static_cast<void (ConnectionUiController::*)()>(&ConnectionUiController::openConnection));
    connect(m_coreController->m_notificationHandler, &NotificationHandler::disconnectRequested, m_coreController->m_connectionUiController,
            &ConnectionUiController::closeConnection);
    connect(m_coreController, &CoreController::translationsUpdated, m_coreController->m_notificationHandler, &NotificationHandler::onTranslationsUpdated);

    auto* trayHandler = qobject_cast<SystemTrayNotificationHandler*>(m_coreController->m_notificationHandler);
    connect(m_coreController, &CoreController::websiteUrlChanged, trayHandler, &SystemTrayNotificationHandler::updateWebsiteUrl);
#endif    
}

void CoreSignalHandlers::initUpdateFoundHandler()
{
    connect(m_coreController->m_updateUiController, &UpdateUiController::updateFound, this, [this]() {
        emit m_coreController->m_pageController->goToPage(PageLoader::PageEnum::PageUpdate);
    });
}

// AresVPN Client: THE SESSION OWNS THE TUNNEL (plan 24, D187 rule 6; plan 28).
//
// Every change of the session's server - a sign-in, a switch to another rent, a sign-out, a poll
// that brought a different rent or this device's new identity - re-points the tunnel through ONE
// routine: close it if it is up, make the session's server the default, reopen it if the session
// still has one. Storing a new config is not enough on its own - the running tunnel still holds the
// dead key, and WireGuard then stops handshaking with no error - and signing out without closing it
// left the next person at a shared PC on this customer's rent.
//
// AND THE REFRESH NEVER DEPENDS ON THE TUNNEL IT REPAIRS. A rent routes 0.0.0.0/0, so a poll made
// while the tunnel is up travels inside it, and a tunnel whose endpoint or key is dead swallows the
// poll that would have fixed it. When a poll gets no answer at all while the tunnel is up, the
// routine closes the tunnel, refreshes with nothing in the way, and reopens on whatever the console
// returned. Excluding the console from the tunnel was the alternative; it is not taken because the
// rent's resolvers are routed through the same tunnel (the console's name would not resolve either),
// because Android offers no per-socket exclusion for Qt's network stack, and because it is the
// standing exception #D181 defers to the owner, where #D187 says the refresh opens no hole.
void CoreSignalHandlers::initAresSessionTunnelHandler()
{
    AresProfileController *ares = m_coreController->m_aresProfileController;

    connect(ares, &AresProfileController::sessionServerSet, this, [this](const QString &serverId, bool identityChanged) {
        if (serverId.isEmpty()) {
            m_aresReopenOwed = false;  // signed out: nothing is owed to the next person on this device
        }
        // The running tunnel is stale when the session's identity moved, or when it runs on a
        // server that was not the session's (an imported file had become the default).
        const bool defaultMoved = aresAdoptSessionServer();
        if (!identityChanged && !defaultMoved) {
            return;
        }
        if (!aresTunnelUp()) {
            qDebug() << "ares session: server set to" << serverId << "- tunnel is down, nothing to re-point";
            return;
        }
        qDebug() << "ares session: server set to" << (serverId.isEmpty() ? QStringLiteral("none (signed out)") : serverId)
                 << "- re-pointing the tunnel";
        aresRepointTunnel(false);
    });

    connect(ares, &AresProfileController::sessionRefreshFailed, this, [this](int, bool noAnswer, const QString &) {
        if (!noAnswer || !aresTunnelUp()) {
            return;
        }
        if (m_aresLastStallCycle.isValid() && m_aresLastStallCycle.elapsed() < kAresStallCycleMsecs) {
            qDebug() << "ares session: no answer through the tunnel, but a stall cycle ran within the hour";
            return;
        }
        m_aresLastStallCycle.start();
        qDebug() << "ares session: no answer through the tunnel - closing it to refresh outside it";
        aresRepointTunnel(true);
    });

    // The tunnel the session left closed, or auto-connect did not open, because the console refused
    // the stored profile (aresAfterTunnelClosed, initAutoConnectHandler) comes back on the first
    // profile the console accepts again: a retire-first swap's new rent, a freed device, a sign-in
    // (H3-3, H3-V2). Queued, because a refresh adopts its server after it says it was accepted.
    connect(ares, &AresProfileController::sessionRefreshed, this, [this]() {
        if (!m_aresReopenOwed) {
            return;
        }
        QTimer::singleShot(0, this, [this]() {
            if (!m_aresReopenOwed || aresTunnelUp() || m_coreController->m_aresProfileController->sessionServerId().isEmpty()) {
                return;
            }
            m_aresReopenOwed = false;
            aresAdoptSessionServer();
            qDebug() << "ares session: the console accepts the session's profile again - opening the tunnel";
            m_coreController->m_connectionUiController->openConnection();
        });
    });

    connect(m_coreController->m_connectionController, &ConnectionController::connectionStateChanged, this,
            [this](Vpn::ConnectionState state) {
                m_aresTunnelState = state;
                if (state != Vpn::ConnectionState::Disconnected) {
                    m_aresReopenOwed = false;  // someone opened it meanwhile; a later close is theirs
                }
                if (m_aresPending == AresPending::None) {
                    return;
                }
                if (state == Vpn::ConnectionState::Error) {
                    // The close itself failed; leave the tunnel to the user rather than open over it.
                    m_aresPending = AresPending::None;
                    qWarning() << "ares session: re-point abandoned, closing the tunnel reported an error";
                    return;
                }
                if (state != Vpn::ConnectionState::Disconnected) {
                    return;
                }
                const AresPending pending = m_aresPending;
                m_aresPending = AresPending::None;
                // Queued: work from a clean stack, not from inside the state change that closed it.
                QTimer::singleShot(0, this, [this, pending]() { aresAfterTunnelClosed(pending); });
            });
}

void CoreSignalHandlers::aresAfterTunnelClosed(AresPending pending)
{
    AresProfileController *ares = m_coreController->m_aresProfileController;
    bool refreshBusy = false;
    int refreshStatus = 0;
    if (pending == AresPending::RefreshThenReopen) {
        // No tunnel now: this request leaves by the ordinary route.
        const AresProfileController::Result r = ares->refreshSession(nullptr, kAresDirectRefreshMsecs);
        refreshBusy = r.busy;
        refreshStatus = r.superseded ? 0 : r.httpStatus;  // a dropped answer refuses nothing
    }
    aresAdoptSessionServer();
    switch (aresSession::afterTunnelClosed(refreshBusy, !ares->sessionServerId().isEmpty(), ares->signInNeeded(),
                                           refreshStatus, aresTunnelUp())) {
    case aresSession::AfterClose::AskAgain:
        // A poll that left through the closing tunnel is still on the wire (H2-V1); it ends within
        // its own timeout. Reopening now would put the stale profile back unrefreshed.
        qDebug() << "ares session: a refresh is still on the wire - asking again before reopening";
        QTimer::singleShot(1000, this, [this, pending]() { aresAfterTunnelClosed(pending); });
        return;
    case aresSession::AfterClose::Leave:
        if (ares->sessionServerId().isEmpty()) {
            qDebug() << "ares session: no session server - the tunnel stays closed";
        } else if (!aresTunnelUp()) {
            // The console refused the stored profile (401, 404, 409). Whatever it accepts next - a
            // sign-in after a 401, the direct poll's answer once the idx names a new rent or a device
            // is freed - reopens the tunnel the session closed (m_aresReopenOwed).
            qDebug() << "ares session: the console refused the stored profile (" << refreshStatus
                     << ") - the tunnel stays closed until it accepts one";
            m_aresReopenOwed = true;
        }
        return;
    case aresSession::AfterClose::Reopen:
        m_coreController->m_connectionUiController->openConnection();
        return;
    }
}

void CoreSignalHandlers::aresRepointTunnel(bool refreshWhileDown)
{
    const AresPending wanted = refreshWhileDown ? AresPending::RefreshThenReopen : AresPending::Reopen;
    if (m_aresPending != AresPending::None) {
        // A close is already under way; it carries the refresh too if one is now owed.
        if (wanted == AresPending::RefreshThenReopen) {
            m_aresPending = wanted;
        }
        return;
    }
    m_aresPending = wanted;
    m_coreController->m_connectionUiController->closeConnection();
}

bool CoreSignalHandlers::aresAdoptSessionServer()
{
    const QString serverId = m_coreController->m_aresProfileController->sessionServerId();
    ServersController *servers = m_coreController->m_serversController;
    if (serverId.isEmpty() || servers->indexOfServerId(serverId) < 0 || servers->getDefaultServerId() == serverId) {
        return false;
    }
    servers->setDefaultServer(serverId);
    return true;
}

// A tunnel being closed is not up: re-pointing it then would reopen what the customer is closing.
bool CoreSignalHandlers::aresTunnelUp() const
{
    return m_aresTunnelState == Vpn::ConnectionState::Connected
        || m_aresTunnelState == Vpn::ConnectionState::Connecting
        || m_aresTunnelState == Vpn::ConnectionState::Reconnecting;
}

