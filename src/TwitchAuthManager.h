#ifndef TWITCHAUTHMANAGER_H
#define TWITCHAUTHMANAGER_H

#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QJsonDocument>
#include <QFuture>
#include <QJsonObject>
#include <QJsonArray>

class TwitchAuthManager : public QObject {
	Q_OBJECT

public:
	static TwitchAuthManager &get()
	{
		static TwitchAuthManager instance;
		return instance;
	}

	// === AUTENTICAÇÃO === //
	void startAuthentication();
	void clearAuthentication();

	// Tokens
	QString getAccessToken();
	QString getClientId();
	QString getUserId();
	std::pair<QString, QString> getTokenUserInfo();

	// === HELIX (Async) === //
	QFuture<QString> getGameId(const QString &gameName);
	QFuture<bool> updateChannelCategory(const QString &gameId);
	QFuture<bool> sendChatMessage(const QString &broadcasterId, const QString &senderId, const QString &message);

signals:
	void authenticationFinished(bool success, const QString &info); // Sinal emitido quando o fluxo de autenticação via navegador termina
	void reauthenticationNeeded();                               // Sinal emitido quando um token inválido é detectado em uma chamada de API

private slots:
	void onNewConnection();
	void handleReauthenticationRequest(); // Slot para lidar com a necessidade de reautenticação

private:
	TwitchAuthManager(QObject *parent = nullptr);
	~TwitchAuthManager();

	QFuture<std::pair<long, QString>> performGET(const QString &url, const QString &token);
	QFuture<std::pair<long, QString>> performPATCH(const QString &url, const QJsonObject &body, const QString &token);
	QFuture<std::pair<long, QString>> performPOST(const QString &url, const QJsonObject &body, const QString &token);

	// Estrutura OAuth interna
	QString clientId;
	QString accessToken;
	QString userId;
	bool isAuthenticating = false; // Flag para evitar múltiplas tentativas de autenticação

	// Local HTTP Server
	QTcpServer *server = nullptr;

	static constexpr const char *CLIENT_ID = "au09dsnlplmvtlvwgenvvdup5cf458"; // Para fluxo implícito
	static constexpr const char *REDIRECT_URI = "http://localhost:30000/";
};

#endif // TWITCHAUTHMANAGER_H
