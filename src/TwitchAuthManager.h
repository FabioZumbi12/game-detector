#ifndef TWITCHAUTHMANAGER_H
#define TWITCHAUTHMANAGER_H

#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonObject>

class QTcpServer;
class QTcpSocket;
class QTimer;

class TwitchAuthManager : public QObject {
	Q_OBJECT

public:
	enum UpdateResult {
		Success,
		AuthError,
		Failed
	};

public:
	static TwitchAuthManager &get()
	{
		static TwitchAuthManager instance;
		return instance;
	}

	QString getAccessToken();
	QString getClientId();
	QString getUserId();
	std::pair<QString, QString> getTokenUserInfo();

	QFuture<QString> getGameId(const QString &gameName);
	QFuture<UpdateResult> updateChannelCategory(const QString &gameId);
	QFuture<bool> sendChatMessage(const QString &broadcasterId, const QString &senderId, const QString &message);

signals:
	void authenticationFinished(bool success, const QString &info); // Sinal emitido quando o fluxo de autenticação via navegador termina
	void reauthenticationNeeded();                               // Sinal emitido quando um token inválido é detectado em uma chamada de API
	void authenticationDataNeedsClearing();                      // Sinal para limpar os dados de autenticação de forma segura entre threads
	void authenticationTimerTick(int remainingSeconds);

private slots:
	void onNewConnection();
	void onAuthTimerTick();

public slots:
	void startAuthentication(int mode = -1, int unifiedAuth = -1);
	void clearAuthentication();
	void loadToken();

private:
	TwitchAuthManager(QObject *parent = nullptr);
	~TwitchAuthManager();

	QFuture<std::pair<long, QString>> performGET(const QString &url, const QString &token);
	QFuture<std::pair<long, QString>> performPATCH(const QString &url, const QJsonObject &body, const QString &token);
	QFuture<std::pair<long, QString>> performPOST(const QString &url, const QJsonObject &body, const QString &token);

	QString accessToken;
	QString userId;
	bool isAuthenticating = false;

	QTcpServer *server = nullptr;
	QTimer *authTimeoutTimer = nullptr;
	int authRemainingSeconds = 0;

	static constexpr const char *CLIENT_ID = "wl4mx2l4sgmdvpwoek6pjronpor9en";
	static constexpr const char *REDIRECT_URI = "http://localhost:30000/";
};

#endif // TWITCHAUTHMANAGER_H
