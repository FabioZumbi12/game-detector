#pragma once

#include "IPlatformService.h"
#include <QTcpServer>
#include <QFuture>
#include <QJsonObject>
#include <QThreadPool>
#include <QTimer>
#include <QDateTime>
#include <QPointer>
#include <QList>

class TrovoAuthManager : public IPlatformService {
    Q_OBJECT

public:
    explicit TrovoAuthManager(QObject *parent = nullptr);
    ~TrovoAuthManager();

    void startAuthentication(int mode = -1, int unifiedAuth = -1);
    bool isAuthenticated() const override;
    void updateCategory(const QString &gameName, const QString &title = QString()) override;
    void sendChatMessage(const QString &message) override;
    
    void loadToken();

    QFuture<QString>getChannelCategory();
    QFuture<QString> getChannelTitle();

signals:
    void authenticationFinished(bool success, QString message);
    void authenticationTimerTick(int remainingSeconds);

private:
    QTcpServer *server;
    QString accessToken;
    QString refreshToken;
    QString userId;
    bool isAuthenticating = false;

    QTimer *authTimeoutTimer = nullptr;
    QDateTime lastRefreshAttempt;
    int authRemainingSeconds = 0;
    QThreadPool threadPool;
    QList<QPointer<QTcpSocket>> clientSockets;

    const QString AUTH_API_URL = "https://trovo-obs.areaz12server.net.br";
    const QString CLIENT_ID = "b07641be5083b975423de98ee83e8e0a";

    void onNewConnection();
    void onAuthTimerTick();
    void fetchUserInfo();
    void searchAndSetCategory(const QString &gameName, const QString &title = QString());
    bool refreshAccessToken();

    QFuture<std::pair<long, QString>> performPOST(const QString &url, const QJsonObject &body, const QString &token);
    QFuture<std::pair<long, QString>> performGET(const QString &url, const QString &token);

    std::pair<long, QString> performPOSTSync(const QString &url, const QJsonObject &body, const QString &token);
    std::pair<long, QString> performGETSync(const QString &url, const QString &token);
};
