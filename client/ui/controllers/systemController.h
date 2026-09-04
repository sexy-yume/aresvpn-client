#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <QByteArray>
#include <QObject>

class SystemController : public QObject
{
    Q_OBJECT
public:
    explicit SystemController(QObject *parent = nullptr);

    static bool saveFile(const QString &fileName, const QString &data);
    static bool saveFile(const QString &fileName, const QByteArray &data);
    static bool readFile(const QString &fileName, QByteArray &data);
    static bool readFile(const QString &fileName, QString &data);

public slots:
    QString getFileName(const QString &acceptLabel, const QString &nameFilter, const QString &selectedFile = "",
                        const bool isSaveMode = false, const QString &defaultSuffix = "");

    void setQmlRoot(QObject *qmlRoot);

    // AresVPN Client (AresProject ROADMAP 18-3d): read one of the licence texts EMBEDDED in the
    // binary, so GPL-3 section 5's Appropriate Legal Notice can be displayed by the running
    // program rather than only shipped beside it.
    //
    // IT IS DELIBERATELY NOT A FILE READER. `path` must be an embedded resource under
    // `:/licenses/` with no `..` in it, and anything else returns an empty string - because a
    // QML-callable "read me any path" slot on a controller the whole UI can reach is a general
    // file-disclosure primitive, and this needs exactly four files that are compiled into the
    // executable. The narrow door is the point.
    //
    // WHY NOT XMLHttpRequest, which needs no C++ at all: it was tried first and MEASURED - under
    // `-platform offscreen` a synchronous `XMLHttpRequest` on `qrc:/licenses/GPL-3.0.txt` returned
    // an empty string while the resource was demonstrably in the binary (rcc had emitted all four
    // aliases into qrc_licenses.cpp). The mechanism was not chased further and is recorded here as
    // UNEXPLAINED (#L010) rather than guessed at, because reading an embedded resource through an
    // HTTP object was the wrong shape regardless: QFile on a resource cannot be asynchronous, has
    // no request state to get wrong, and can report WHY it failed.
    QString readBundledLicence(const QString &path);

    bool isAuthenticated();
    void sendTouch(float x, float y);

signals:
    void fileDialogClosed(const bool isAccepted);

private:
    QObject *m_qmlRoot;
};

#endif // SYSTEMCONTROLLER_H
