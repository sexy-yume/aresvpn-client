// AresVPN Client - tests for client/core/controllers/aresSessionLogic (audit 2026-11-14 H-17).
// Copyright (c) 2026 AresVPN. Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// Each case names the plant it catches: the one-line defect in aresSessionLogic.cpp that turns it red.
#include <QRegularExpression>
#include <QTest>

#include "core/controllers/aresSessionLogic.h"

using namespace aresSession;

class TestAresSessionLogic : public QObject
{
    Q_OBJECT

private slots:
    // Plant: stop skipping `#` lines in identityDigest -> a renewal (which rewrites only the comment)
    // reads as a new identity and reconnects every customer's tunnel.
    void renewalIsNotAnIdentityChange()
    {
        const QString before = QStringLiteral("# ARES rent 7 device 3 - expires 2026-12-01\n[Interface]\nPrivateKey = A\n");
        const QString after = QStringLiteral("# ARES rent 7 device 3 - expires 2027-01-01\n[Interface]\nPrivateKey = A\n");
        QCOMPARE(identityDigest(before), identityDigest(after));
        QVERIFY(identityDigest(before) != identityDigest(QStringLiteral("[Interface]\nPrivateKey = B\n")));
    }

    // Plant: change the trim or the join in identityDigest -> every session stored by an earlier build
    // carries a digest this build no longer produces, and every installed client reconnects once.
    void digestIsPinned()
    {
        const QString text = QStringLiteral("# ARES rent 7 device 3 - expires 2026-12-01\n[Interface]\n"
                                            "PrivateKey = A\n  Address = 10.0.0.2/32  \n\n");
        QCOMPARE(identityDigest(text),
                 QStringLiteral("5275e874a19e689c0ce2d36ec9ea1a4f1d60b1dd878d947e9b5d028d287bfa01"));
    }

    // Plant: drop `base.remove("pw")` in withAnswer -> the password stays on the device after the
    // console issued a token (D268, H-3).
    void tokenReplacesPassword()
    {
        QVariantMap base;
        base.insert(QStringLiteral("id"), QStringLiteral("alice"));
        base.insert(QStringLiteral("pw"), QStringLiteral("secret"));
        base.insert(QStringLiteral("idx"), QStringLiteral("odin_1"));

        const QVariantMap withToken = withAnswer(base, QStringLiteral("ares-rent-7"), QStringLiteral("3"),
                                                 QStringLiteral("d1"), QStringLiteral("\"e1\""), QStringLiteral("tok"));
        QVERIFY(!withToken.contains(QStringLiteral("pw")));
        QCOMPARE(withToken.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
        QCOMPARE(withToken.value(QStringLiteral("serverId")).toString(), QStringLiteral("ares-rent-7"));
        QCOMPARE(withToken.value(QStringLiteral("etag")).toString(), QStringLiteral("\"e1\""));

        // A console without tokens: the password is the only way to refresh, so it must stay.
        const QVariantMap noToken = withAnswer(base, QStringLiteral("ares-rent-7"), QStringLiteral("3"),
                                               QStringLiteral("d1"), QString(), QString());
        QCOMPARE(noToken.value(QStringLiteral("pw")).toString(), QStringLiteral("secret"));
        QVERIFY(!noToken.contains(QStringLiteral("token")));
    }

    // Plants: invert the min_version comparison (a current build refused, an ancient one let in);
    // drop the https test (an offer that downloads over plain HTTP).
    void manifestJudgement()
    {
        // An ordinary escaped literal, not a raw string: moc's tokenizer does not read raw string
        // literals, lost the class's closing brace inside this one and generated no meta-object.
        const QByteArray body = "{\"windows\":{\"latest\":\"5.0.3.0\",\"min_version\":\"5.0.1.0\","
                                "\"url\":\"https://example.org/AresVPNClient.exe\"},"
                                "\"android\":{\"latest\":\"5.0.3.0\",\"url\":\"http://example.org/a.apk\"}}";
        UpdateInfo info;
        QVERIFY(judgeManifest(body, QStringLiteral("windows"), QStringLiteral("5.0.0.0"), &info));
        QVERIFY(info.available);
        QVERIFY(info.required);

        QVERIFY(judgeManifest(body, QStringLiteral("windows"), QStringLiteral("5.0.2.0"), &info));
        QVERIFY(info.available);
        QVERIFY(!info.required);

        QVERIFY(judgeManifest(body, QStringLiteral("windows"), QStringLiteral("5.0.3.0"), &info));
        QVERIFY(!info.available);
        QVERIFY(!info.required);

        QVERIFY(!judgeManifest(body, QStringLiteral("android"), QStringLiteral("5.0.0.0"), &info));
        QVERIFY(!info.available && !info.required && info.url.isEmpty());

        QVERIFY(!judgeManifest(body, QStringLiteral("ios"), QStringLiteral("5.0.0.0"), &info));
        QVERIFY(!judgeManifest(QByteArray("not json"), QStringLiteral("windows"), QStringLiteral("5.0.0.0"), &info));
    }

    // Plant: fall back from "localhost" straight to the OS name -> every phone on one Android release
    // is called "Android 14" on the console's device page (H-11).
    void deviceNameFallsBackToTheModel()
    {
        QCOMPARE(chooseDeviceName(QStringLiteral("DESKTOP-1"), QStringLiteral("x"), QStringLiteral("Windows 11")),
                 QStringLiteral("DESKTOP-1"));
        QCOMPARE(chooseDeviceName(QStringLiteral("localhost"), QStringLiteral("SM-S918N"), QStringLiteral("Android 14")),
                 QStringLiteral("SM-S918N"));
        QCOMPARE(chooseDeviceName(QString(), QString(), QStringLiteral("Android 14")), QStringLiteral("Android 14"));
        QCOMPARE(chooseDeviceName(QString(65, QLatin1Char('n')), QString(), QString()).size(), 64);
    }

    // Plants: salt or hash the id differently per call (a reinstall becomes a new device again,
    // H-12); return the raw id (the console stores a cross-app identifier); emit a shape the
    // console's DEVICE_SHAPE refuses (every Android sign-in answers 400).
    void androidDeviceKey()
    {
        const QString key = deviceKeyFromAndroidId(QStringLiteral("9f8e7d6c5b4a3921"));
        QCOMPARE(key, deviceKeyFromAndroidId(QStringLiteral("9f8e7d6c5b4a3921")));
        QVERIFY(key != deviceKeyFromAndroidId(QStringLiteral("0000000000000001")));
        QVERIFY(!key.contains(QStringLiteral("9f8e7d6c5b4a3921")));
        QVERIFY(QRegularExpression(QStringLiteral("^[A-Za-z0-9-]{8,64}$")).match(key).hasMatch());
        QVERIFY(deviceKeyFromAndroidId(QString()).isEmpty());
        QVERIFY(deviceKeyFromAndroidId(QStringLiteral("   ")).isEmpty());
    }

    // Plants: return true unconditionally (every poll rewrites the encrypted list and resets the UI
    // model, H-18); return false for a missing server (a refresh after the list lost the entry
    // leaves the session pointing at nothing).
    void serverWriteOnlyOnChange()
    {
        const QString id = QStringLiteral("ares-rent-7");
        QVERIFY(!serverNeedsWrite(true, id, QStringLiteral("d1"), id, QStringLiteral("d1")));
        QVERIFY(serverNeedsWrite(false, id, QStringLiteral("d1"), id, QStringLiteral("d1")));
        QVERIFY(serverNeedsWrite(true, id, QStringLiteral("d1"), id, QStringLiteral("d2")));
        QVERIFY(serverNeedsWrite(true, QStringLiteral("ares-rent-6"), QStringLiteral("d1"), id, QStringLiteral("d1")));
        QVERIFY(serverNeedsWrite(true, id, QString(), id, QStringLiteral("d1")));
    }

    // Plants: drop the busy test (the stall cycle reopens on the stale profile while a poll that
    // left through the closing tunnel is still on the wire, then ignores the dead tunnel for an
    // hour, H2-V1); drop the sign-in test (a 401 from the direct refresh - a removed device, a rent
    // swapped on a console that asks for a sign-in - reopens the tunnel on the retired profile and
    // leaves the customer in Connecting, plan 30 T02).
    void reopenAfterTheTunnelClosed()
    {
        QVERIFY(afterTunnelClosed(false, true, false, false) == AfterClose::Reopen);
        QVERIFY(afterTunnelClosed(true, true, false, false) == AfterClose::AskAgain);
        QVERIFY(afterTunnelClosed(false, true, true, false) == AfterClose::Leave);
        QVERIFY(afterTunnelClosed(false, false, false, false) == AfterClose::Leave);  // signed out
        QVERIFY(afterTunnelClosed(false, true, false, true) == AfterClose::Leave);    // reopened by hand
    }
};

QTEST_GUILESS_MAIN(TestAresSessionLogic)
#include "tst_aresSessionLogic.moc"
