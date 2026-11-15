// AresVPN Client - the decisions the login session makes, with no network, no storage and no Qt GUI.
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// AresProfileController does the I/O; every rule it applies to what came back lives here, so the
// rules run under `client/tests/ares` without a console, a keychain or an application object
// (audit 2026-11-14 H-17). Depends on QtCore only - keep it that way, or the test target stops
// being buildable on its own.
#ifndef ARESSESSIONLOGIC_H
#define ARESSESSIONLOGIC_H

#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace aresSession
{
    // What the console's update manifest says about this build (plan 28).
    struct UpdateInfo
    {
        QString latest;         // the newest version for this platform
        QString url;            // where to get it (https only)
        bool available = false; // latest is newer than this build
        bool required = false;  // this build is older than min_version: Connect is refused
    };

    // What identifies this device's tunnel in a profile: a SHA-256 of the text minus its `#`
    // comment lines. A renewal rewrites only the comment (`# ARES rent X device Y - expires ...`),
    // so it is no change. Never the text itself: the text holds the device's private key.
    QString identityDigest(const QString &text);

    // `base` with what a successful answer changed: server, device, identity, ETag, and a device
    // token in place of the password when the console issued one (D268).
    QVariantMap withAnswer(QVariantMap base, const QString &serverId, const QString &deviceId,
                           const QString &identity, const QString &etag, const QString &refreshToken);

    // The manifest `body` judged for `platform` against `currentVersion`. A usable entry needs a
    // parseable `latest` and an https `url`; anything else is the empty UpdateInfo and `false`.
    bool judgeManifest(const QByteArray &body, const QString &platform, const QString &currentVersion,
                       UpdateInfo *out);

    // The label a device carries on the console's device page (plan 24, D238): the host name, the
    // phone's model where the host name says nothing (Android answers "localhost"), the OS name as
    // the last resort - "Android 14" names every phone on that release alike (H-11). At most 64.
    QString chooseDeviceName(const QString &hostName, const QString &model, const QString &productName);

    // The device key an Android installation sends as `device`, derived from Settings.Secure
    // ANDROID_ID so an uninstall and reinstall under the same signing key and user is the SAME
    // device on the console rather than a new one with a new key and a new inner address (H-12).
    // Empty when there is no id, and the caller falls back to the stored installation uuid.
    // The shape fits the console's DEVICE_SHAPE, `^[A-Za-z0-9-]{8,64}$`.
    QString deviceKeyFromAndroidId(const QString &androidId);

    // Whether an answer must be written into the server list. The list is encrypted and a write
    // resets the UI model, so a poll that brought back exactly the stored profile writes nothing
    // (H-18). `stored` says the server is in the list at all - a missing one is always written.
    bool serverNeedsWrite(bool stored, const QString &sessionServerId, const QString &sessionIdentity,
                          const QString &serverId, const QString &identity);

    // Whether a console answer to a sign-in or refresh refuses the profile stored on this device:
    // 401 (the credential is dead: a removed device, a changed password), 404 (no rent carries the
    // idx - the window of a retire-first swap, before the operator hangs the idx on the new rent,
    // audit H3-V2), 409 (the rent the idx names now refuses this device: its device limit, H3-3).
    // Nothing opens such a profile unasked; a Connect the customer presses still does.
    bool refusesStoredProfile(int httpStatus);

    // What the session's re-point routine does once the tunnel it closed reports Disconnected
    // (plan 28). `refreshBusy`: the refresh it owed found another one on the wire and asked nothing
    // (H2-V1) - ask again shortly rather than reopen unrefreshed. `signInNeeded`: an earlier refresh
    // was answered 401. `refreshStatus`: the status of the refresh made while the tunnel was down (0
    // when none was made or none answered); one that refuses the stored profile means that profile
    // is most likely what stalled the tunnel, so it is not reopened unasked (plan 30 T02, H3-3,
    // H3-V2). `tunnelUp`: the customer reopened it meanwhile.
    enum class AfterClose { AskAgain, Leave, Reopen };
    AfterClose afterTunnelClosed(bool refreshBusy, bool hasSessionServer, bool signInNeeded, int refreshStatus,
                                 bool tunnelUp);
}

#endif // ARESSESSIONLOGIC_H
