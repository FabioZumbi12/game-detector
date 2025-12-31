#include "TwitchAuthManager.h"
#include "ConfigManager.h"
#include "NetworkCommon.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QTimer>

static const QString SVG_SUCCESS = "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 24 24' fill='none' stroke='#4caf50' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M22 11.08V12a10 10 0 1 1-5.93-9.14'></path><polyline points='22 4 12 14.01 9 11.01'></polyline></svg>";
static const QString SVG_ERROR = "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 24 24' fill='none' stroke='#ff5252' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='10'></circle><line x='15' y='9' x2='9' y2='15'></line><line x='9' y='9' x2='15' y2='15'></line></svg>";
static const QString SVG_LOADING = "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 24 24' fill='none' stroke='#9146FF' stroke-width='2' stroke-linecap='round' stroke-linejoin='round' class='spin'><line x1='12' y1='1' x2='12' y2='5'></line><line x1='12' y1='19' x2='12' y2='23'></line><line x1='4.22' y1='4.22' x2='7.05' y2='7.05'></line><line x1='16.95' y1='16.95' x2='19.78' y2='19.78'></line><line x1='1' y1='12' x2='5' y2='12'></line><line x1='19' y1='12' x2='23' y2='12'></line><line x1='4.22' y1='19.78' x2='7.05' y2='16.95'></line><line x1='16.95' y1='7.05' x2='19.78' y2='4.22'></line></svg>";

static QString GetAuthPageTemplate(const QString &title, const QString &bodyContent, const QString &script = QString())
{
	QString css =
		"body { background-color: #18191e; color: #e0e6ed; font-family: 'Lexend', sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; text-align: center; }"
		".panel { background-color: #1e2025; padding: 40px; border-radius: 8px; box-shadow: 0 4px 15px rgba(0, 0, 0, 0.5); max-width: 400px; width: 90%; border: 1px solid #26282d; display: flex; align-items: center; text-align: left; }"
		"h1 { margin-top: 0; margin-bottom: 15px; font-size: 24px; }"
		"p { color: #9aa5b1; margin-bottom: 0; line-height: 1.5; }"
		".footer { margin-top: 30px; font-size: 12px; color: #5c6370; max-width: 400px; line-height: 1.5; }"
		".footer a { color: #9146FF; text-decoration: none; }"
		".icon-box { margin-bottom: 0; margin-right: 20px; display: flex; justify-content: center; flex-shrink: 0; }"
		".spin { animation: spin 2s linear infinite; }"
		"@keyframes spin { 100% { transform: rotate(360deg); } }";

	QString footerText = QString("This page is not owned by, associated with, or part of <a href=\"https://twitch.tv\" target=\"_blank\">Twitch</a>.<br>Developed by FabioZumbi12");

	return QString(
		"<!DOCTYPE html>"
		"<html>"
		"<head>"
		"<meta charset='UTF-8'>"
		"<title>%1</title>"
		"<link href='https://fonts.googleapis.com/css2?family=Lexend:wght@300;400;500;600&display=swap' rel='stylesheet'/>"
		"<style>%2</style>"
		"</head>"
		"<body>"
		"<div class='panel'>%3</div>"
		"<div class='footer'>%4</div>"
		"<script>%5</script>"
		"</body>"
		"</html>")
		.arg(title, css, bodyContent, footerText, script);
}

TwitchAuthManager::TwitchAuthManager(QObject *parent) : QObject(parent)
{
	server = new QTcpServer(this);
	authTimeoutTimer = new QTimer(this);

	connect(server, &QTcpServer::newConnection, this, &TwitchAuthManager::onNewConnection);
	connect(this, &TwitchAuthManager::authenticationDataNeedsClearing, this, &TwitchAuthManager::clearAuthentication, Qt::QueuedConnection);
	connect(authTimeoutTimer, &QTimer::timeout, this, &TwitchAuthManager::onAuthTimerTick);
	threadPool.setMaxThreadCount(4);
}

TwitchAuthManager::~TwitchAuthManager()
{
	if (server->isListening())
		server->close();
}

void TwitchAuthManager::shutdown()
{
	threadPool.waitForDone();
}

void TwitchAuthManager::loadToken()
{
	auto settings = ConfigManager::get().getSettings();
	accessToken = obs_data_get_string(settings, "twitch_access_token");
	userId = obs_data_get_string(settings, "twitch_user_id");
}

void TwitchAuthManager::startAuthentication(int mode, int unifiedAuth)
{
	if (isAuthenticating) {
		blog(LOG_INFO, "[GameDetector/TwitchAuth] Authentication already in progress, ignoring new request.");
		return;
	}

	if (server->isListening()) server->close();

	if (!server->listen(QHostAddress::LocalHost, 30000)) {
		blog(LOG_ERROR, "[GameDetector/TwitchAuth] Could not start local server.");
		emit authenticationFinished(false, "");
		return;
	}

	blog(LOG_INFO, "[GameDetector/TwitchAuth] Local server started at http://localhost:30000/");

	QUrl authUrl("https://id.twitch.tv/oauth2/authorize");
	QUrlQuery query;

	query.addQueryItem("client_id", CLIENT_ID);
	query.addQueryItem("redirect_uri", REDIRECT_URI);
	query.addQueryItem("response_type", "token");

	bool useUnifiedAuth = (unifiedAuth == -1) ? ConfigManager::get().getUnifiedAuth() : (bool)unifiedAuth;
	int actionMode = (mode == -1) ? ConfigManager::get().getActionMode() : mode;

	if (useUnifiedAuth) {
		blog(LOG_INFO, "[GameDetector/TwitchAuth] Starting unified authentication.");
		query.addQueryItem("scope", "user:write:chat channel:manage:broadcast");
	} else if (actionMode == 0) {
		blog(LOG_INFO, "[GameDetector/TwitchAuth] Starting authentication in mode: Chat Command");
		query.addQueryItem("scope", "user:write:chat");
	} else {
		blog(LOG_INFO, "[GameDetector/TwitchAuth] Starting authentication in mode: API");
		query.addQueryItem("scope", "channel:manage:broadcast");
	}

	authUrl.setQuery(query);

	QDesktopServices::openUrl(authUrl);
	isAuthenticating = true;

	authRemainingSeconds = 30;
	emit authenticationTimerTick(authRemainingSeconds);
	authTimeoutTimer->start(1000);
}

void TwitchAuthManager::clearAuthentication()
{
	accessToken.clear();
	userId.clear();
	ConfigManager::get().setTwitchToken("");
	ConfigManager::get().setTwitchUserId("");
	ConfigManager::get().setTwitchChannelLogin("");
	ConfigManager::get().save(ConfigManager::get().getSettings());
	blog(LOG_INFO, "[GameDetector/TwitchAuth] In-memory and persisted authentication data cleared.");
}

void TwitchAuthManager::onAuthTimerTick()
{
	authRemainingSeconds--;
	emit authenticationTimerTick(authRemainingSeconds);

	if (authRemainingSeconds <= 0) {
		authTimeoutTimer->stop();
		if (server->isListening()) server->close();
		isAuthenticating = false;
		emit authenticationFinished(false, "Timeout");
	}
}

void TwitchAuthManager::onNewConnection()
{
	QTcpSocket *clientSocket = server->nextPendingConnection();
	if (!clientSocket)
		return;

	authTimeoutTimer->stop();
	emit authenticationTimerTick(0);

	connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
		if (!clientSocket || !clientSocket->isValid())
			return;

		QString request = clientSocket->readAll();
		QStringList reqLines = request.split("\r\n");
		if (reqLines.isEmpty()) {
			clientSocket->disconnectFromHost();
			return;
		}

		QString firstLine = reqLines.first();
		QString path = firstLine.split(" ")[1];

		QUrl url(path);
		QString token = QUrlQuery(url.query()).queryItemValue("token");

		if (!token.isEmpty()) {

			QString content = QString("<div class='icon-box'>%1</div><div><h1 style='color: #4caf50'>%2</h1><p>%3</p></div>")
				.arg(SVG_SUCCESS, obs_module_text("Auth.Page.Success.Title"), obs_module_text("Auth.Page.Success.Message"));
			QString script = "setTimeout(function() { window.close(); }, 3000);";
			QString successPage = GetAuthPageTemplate(obs_module_text("Auth.Page.Title"), content, script);

			QString httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + successPage;
			clientSocket->write(httpResponse.toUtf8());
			clientSocket->disconnectFromHost();
			server->close();
 
			isAuthenticating = false;
			if (token.isEmpty()) {
				emit authenticationFinished(false, obs_module_text("Auth.Error.EmptyToken"));
				return;
			}
 
			auto settings = ConfigManager::get().getSettings();
			accessToken = token;
			auto [newUserId, loginName] = getTokenUserInfo();
			userId = newUserId;

			if (!userId.isEmpty()) {
				obs_data_set_string(settings, "twitch_access_token", accessToken.toStdString().c_str());
				obs_data_set_string(settings, "twitch_user_id", userId.toStdString().c_str());
				obs_data_set_string(settings, "twitch_channel_login", loginName.toStdString().c_str());
				obs_data_set_string(settings, "twitch_refresh_token", "");
				ConfigManager::get().save(settings);
				emit authenticationFinished(true, loginName);
			} else {
				obs_data_set_string(settings, "twitch_access_token", "");
				obs_data_set_string(settings, "twitch_user_id", "");
				clearAuthentication();
				emit authenticationFinished(false, obs_module_text("Auth.Error.GetUserIdFailed"));
			}
 
		} else if (path.startsWith("/")) {
			isAuthenticating = false;
 
			QString errorPage = QString(
				"<!DOCTYPE html><html><head><title>%1</title></head><body>"
				"<script>"
				"let p = new URLSearchParams(window.location.hash.substring(1));"
				"let t = p.get('access_token');"
				"let error = p.get('error_description');"
				"if (t) { window.location.replace('/?token=' + t); }"
				"else { document.body.innerHTML = '%2' + '%3'.replace('%1', error || obs_module_text(\"Auth.Error.TokenNotFound\")); }"
				"</script>"
				"</body></html>").arg(obs_module_text("Auth.Page.Title"), obs_module_text("Auth.Page.Error.Title"), obs_module_text("Auth.Page.Error.Message"));
 
			QString httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + errorPage;
			clientSocket->write(httpResponse.toUtf8());
			clientSocket->disconnectFromHost();
		} else {
			clientSocket->write("HTTP/1.1 404 Not Found\r\n\r\n");
			clientSocket->disconnectFromHost();
		}
	});

	connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}

QString TwitchAuthManager::getAccessToken()
{
	return accessToken;
}

QString TwitchAuthManager::getClientId()
{
	return CLIENT_ID;
}

QString TwitchAuthManager::getUserId()
{
	return userId;
}

QFuture<std::pair<long, QString>> TwitchAuthManager::performGET(const QString &url, const QString &token)
{
	return RunTaskSafe(&threadPool, "TwitchAuth/performGET", [this, url, token]() -> std::pair<long, QString> {
		return performGETSync(url, token);
	});
}

QFuture<std::pair<long, QString>> TwitchAuthManager::performPATCH(const QString &url, const QJsonObject &body, const QString &token)
{
	return RunTaskSafe(&threadPool, "TwitchAuth/performPATCH", [this, url, body, token]() -> std::pair<long, QString> {
		return performPATCHSync(url, body, token);
	});
}

QFuture<std::pair<long, QString>> TwitchAuthManager::performPOST(const QString &url, const QJsonObject &body, const QString &token)
{
	return RunTaskSafe(&threadPool, "TwitchAuth/performPOST", [this, url, body, token]() -> std::pair<long, QString> {
		return performPOSTSync(url, body, token);
	});
}

std::pair<QString, QString> TwitchAuthManager::getTokenUserInfo()
{
	if (accessToken.isEmpty())
		return {"", ""};

	QString url = "https://api.twitch.tv/helix/users";
	auto [http_code, json] = performGETSync(url, accessToken);

	if (http_code == 200) {
		QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
		if (doc.isObject()) {
			QJsonArray arr = doc["data"].toArray();
			if (!arr.isEmpty()) {
				QJsonObject userObject = arr.first().toObject();
				return {userObject["id"].toString(), userObject["login"].toString()};
			}
		}
	}
	return {"", ""};
}

QFuture<QString> TwitchAuthManager::getGameId(const QString &gameName)
{
	QString url = "https://api.twitch.tv/helix/games?name=" + QUrl::toPercentEncoding(gameName);

	return RunTaskSafe(&threadPool, "TwitchAuth/getGameId", [this, url]() mutable -> QString {
		auto [http_code, json] = performGETSync(url, accessToken);

		if (http_code != 200)
			return "";

		QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
		if (!doc.isObject())
			return "";

		QJsonArray arr = doc["data"].toArray();
		if (arr.isEmpty())
			return "";

		return arr.first().toObject()["id"].toString();
	});
}

QFuture<TwitchAuthManager::UpdateResult> TwitchAuthManager::updateChannelCategory(const QString &gameId)
{
	QString url = "https://api.twitch.tv/helix/channels?broadcaster_id=" + userId;

	QJsonObject body;
	body["game_id"] = gameId;

	return RunTaskSafe(&threadPool, "TwitchAuth/updateChannelCategory", [this, url, body]() mutable -> UpdateResult {
		auto [http_code, json] = performPATCHSync(url, body, accessToken);
		
		if (http_code == 204) {
			return UpdateResult::Success;
		} else if (http_code == 401) {
			return UpdateResult::AuthError;
		}
		return UpdateResult::Failed;
	});
}

QFuture<bool> TwitchAuthManager::sendChatMessage(const QString &broadcasterId, const QString &senderId, const QString &message)
{
	if (broadcasterId.isEmpty() || senderId.isEmpty() || message.isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/TwitchAuth] Attempt to send chat message with incomplete data.");
		QFuture<bool> future = QtConcurrent::run(&threadPool, [=]() {
            return false;
        });
		return future;
	}

	QString url = "https://api.twitch.tv/helix/chat/messages";

	QJsonObject body;
	body["broadcaster_id"] = broadcasterId;
	body["sender_id"] = senderId;
	body["message"] = message;

	return RunTaskSafe(&threadPool, "TwitchAuth/sendChatMessage", [this, url, body]() mutable -> bool {
		auto [http_code, json] = performPOSTSync(url, body, accessToken);
		return http_code == 200;
	});
}

std::pair<long, QString> TwitchAuthManager::performGETSync(const QString &url, const QString &token)
{
	struct curl_slist *headers = nullptr;

	std::string auth = "Authorization: Bearer " + token.toStdString();
	std::string cid = "Client-ID: " + getClientId().toStdString();

	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, cid.c_str());
	
	auto [http_code, response] = ExecuteNetworkRequest(url, "GET", headers);
	curl_slist_free_all(headers);

	if (http_code < 200 || http_code >= 300) {
		if (http_code == 401) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Invalid token (401 Unauthorized) in GET request to %s. Initiating reauthentication process.", url.toStdString().c_str());
			emit authenticationDataNeedsClearing();
			emit reauthenticationNeeded();
			return {http_code, ""};
		} else if (http_code == 429) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
		}
		blog(LOG_WARNING, "[GameDetector/TwitchAuth] Error in GET request to Twitch API (Status: %ld): %s", http_code, response.toStdString().c_str());
	}

	return {http_code, response};
}

std::pair<long, QString> TwitchAuthManager::performPATCHSync(const QString &url, const QJsonObject &body, const QString &token)
{
	struct curl_slist *headers = nullptr;
	std::string auth = "Authorization: Bearer " + token.toStdString();
	std::string cid = "Client-ID: " + getClientId().toStdString();

	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, cid.c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");

	QJsonDocument doc(body);
	std::string json = doc.toJson(QJsonDocument::Compact).toStdString();

	auto [http_code, response] = ExecuteNetworkRequest(url, "PATCH", headers, json);
	curl_slist_free_all(headers);

	if (http_code < 200 || http_code >= 300) {
		if (http_code == 401) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Invalid token (401 Unauthorized) in PATCH request to %s. Initiating reauthentication process.", url.toStdString().c_str());
			emit authenticationDataNeedsClearing();
			emit reauthenticationNeeded();
			return {http_code, ""};
		} else if (http_code == 429) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
		}
		blog(LOG_WARNING, "[GameDetector/TwitchAuth] Error in PATCH request to Twitch API (Status: %ld): %s", http_code, response.toStdString().c_str());
	}

	return {http_code, response};
}

std::pair<long, QString> TwitchAuthManager::performPOSTSync(const QString &url, const QJsonObject &body, const QString &token)
{
	struct curl_slist *headers = nullptr;
	std::string auth = "Authorization: Bearer " + token.toStdString();
	std::string cid = "Client-ID: " + getClientId().toStdString();

	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, cid.c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");

	QJsonDocument doc(body);
	std::string json = doc.toJson(QJsonDocument::Compact).toStdString();

	auto [http_code, response] = ExecuteNetworkRequest(url, "POST", headers, json);
	curl_slist_free_all(headers);

	if (http_code < 200 || http_code >= 300) {
		if (http_code == 401) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Invalid token (401 Unauthorized) in POST request to %s. Initiating reauthentication process.", url.toStdString().c_str());
			emit authenticationDataNeedsClearing();
			emit reauthenticationNeeded();
			return {http_code, ""};
		} else if (http_code == 429) {
			blog(LOG_WARNING, "[GameDetector/TwitchAuth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
		}
		blog(LOG_WARNING, "[GameDetector/TwitchAuth] Error in POST request to Twitch API (Status: %ld): %s", http_code, response.toStdString().c_str());
	}

	return {http_code, response};
}