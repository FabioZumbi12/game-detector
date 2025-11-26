#include "TwitchAuthManager.h"
#include "ConfigManager.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QUrlQuery>

static size_t auth_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	((std::string *)userp)->append((char *)contents, realsize);
	return realsize;
}

TwitchAuthManager::TwitchAuthManager(QObject *parent) : QObject(parent)
{
	server = new QTcpServer(this);

	connect(server, &QTcpServer::newConnection, this, &TwitchAuthManager::onNewConnection);
	connect(this, &TwitchAuthManager::authenticationDataNeedsClearing, this, &TwitchAuthManager::clearAuthentication, Qt::QueuedConnection);
}

TwitchAuthManager::~TwitchAuthManager()
{
	if (server->isListening())
		server->close();
}

void TwitchAuthManager::loadToken()
{
	auto settings = ConfigManager::get().getSettings();
	accessToken = obs_data_get_string(settings, "twitch_access_token");
	userId = obs_data_get_string(settings, "twitch_user_id");
}

void TwitchAuthManager::startAuthentication(int mode)
{
	if (isAuthenticating) {
		blog(LOG_INFO, "[GameDetector/Auth] Authentication already in progress, ignoring new request.");
		return;
	}

	if (!server->listen(QHostAddress::LocalHost, 30000)) {
		blog(LOG_ERROR, "[GameDetector/Auth] Could not start local server.");
		emit authenticationFinished(false, "");
		return;
	}

	blog(LOG_INFO, "[GameDetector/Auth] Local server started at http://localhost:30000/");

	QUrl authUrl("https://id.twitch.tv/oauth2/authorize");
	QUrlQuery query;

	query.addQueryItem("client_id", CLIENT_ID);
	query.addQueryItem("redirect_uri", REDIRECT_URI);
	query.addQueryItem("response_type", "token");

	bool useUnifiedAuth = ConfigManager::get().getUnifiedAuth();
	int actionMode = (mode == -1) ? ConfigManager::get().getTwitchActionMode() : mode;

	if (useUnifiedAuth) {
		blog(LOG_INFO, "[GameDetector/Auth] Starting unified authentication.");
		query.addQueryItem("scope", "user:write:chat channel:manage:broadcast");
	} else if (actionMode == 0) { // Modo Comando de Chat
		blog(LOG_INFO, "[GameDetector/Auth] Starting authentication in mode: Chat Command");
		query.addQueryItem("scope", "user:write:chat");
	} else { // Modo API
		blog(LOG_INFO, "[GameDetector/Auth] Starting authentication in mode: API");
		query.addQueryItem("scope", "channel:manage:broadcast");
	}

	authUrl.setQuery(query);

	QDesktopServices::openUrl(authUrl);
	isAuthenticating = true;
}

void TwitchAuthManager::clearAuthentication()
{
	accessToken.clear();
	userId.clear();
	ConfigManager::get().setToken("");
	ConfigManager::get().setUserId("");
	ConfigManager::get().setTwitchChannelLogin("");
	ConfigManager::get().save(ConfigManager::get().getSettings());
	blog(LOG_INFO, "[GameDetector/Auth] In-memory and persisted authentication data cleared.");
}
void TwitchAuthManager::onNewConnection()
{
	QTcpSocket *clientSocket = server->nextPendingConnection();
	if (!clientSocket)
		return;
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

			QString successPage = QString(
				"<!DOCTYPE html><html><head><title>%1</title></head><body style='font-family: sans-serif; background-color: #f4f4f4; text-align: center; padding-top: 50px;'>"
				"%2%3"
				"<script>setTimeout(function() { window.close(); }, 3000);</script></body></html>")
				.arg(obs_module_text("Auth.Page.Title"), obs_module_text("Auth.Page.Success.Title"), obs_module_text("Auth.Page.Success.Message"));

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
				clearAuthentication(); // Limpa os dados se a obtenção do usuário falhar
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
	return QtConcurrent::run([this, url, token]() -> std::pair<long, QString> {
		CURL *curl = curl_easy_init();
		if (!curl)
			return {0, ""};

		long http_code = 0;
		std::string response;
		struct curl_slist *headers = nullptr;

		std::string auth = "Authorization: Bearer " + token.toStdString();
		std::string cid = "Client-ID: " + getClientId().toStdString();

		headers = curl_slist_append(headers, auth.c_str());
		headers = curl_slist_append(headers, cid.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK) {
			blog(LOG_ERROR, "[GameDetector/Auth] cURL error (GET): %s", curl_easy_strerror(res));
			return {0, ""};
		}

		if (http_code < 200 || http_code >= 300) {
			if (http_code == 401) {
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in GET request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				emit authenticationDataNeedsClearing();
				emit reauthenticationNeeded();
				return {http_code, ""};
			} else if (http_code == 429) {
				blog(LOG_WARNING, "[GameDetector/Auth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in GET request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

QFuture<std::pair<long, QString>> TwitchAuthManager::performPATCH(const QString &url, const QJsonObject &body, const QString &token)
{
	return QtConcurrent::run([this, url, body, token]() -> std::pair<long, QString> {
		CURL *curl = curl_easy_init();
		if (!curl)
			return {0, ""};

		long http_code = 0;
		std::string response;

		struct curl_slist *headers = nullptr;
		std::string auth = "Authorization: Bearer " + token.toStdString();
		std::string cid = "Client-ID: " + getClientId().toStdString();

		headers = curl_slist_append(headers, auth.c_str());
		headers = curl_slist_append(headers, cid.c_str());
		headers = curl_slist_append(headers, "Content-Type: application/json");

		QJsonDocument doc(body);
		std::string json = doc.toJson(QJsonDocument::Compact).toStdString();

		curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK) {
			blog(LOG_ERROR, "[GameDetector/Auth] cURL error (PATCH): %s", curl_easy_strerror(res));
			return {0, ""};
		}

		if (http_code < 200 || http_code >= 300) {
			if (http_code == 401) {
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in PATCH request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				emit authenticationDataNeedsClearing();
				emit reauthenticationNeeded();
				return {http_code, ""};
			} else if (http_code == 429) {
				blog(LOG_WARNING, "[GameDetector/Auth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in PATCH request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

QFuture<std::pair<long, QString>> TwitchAuthManager::performPOST(const QString &url, const QJsonObject &body, const QString &token)
{
	return QtConcurrent::run([this, url, body, token]() -> std::pair<long, QString> {
		CURL *curl = curl_easy_init();
		if (!curl)
			return {0, ""};

		long http_code = 0;
		std::string response;

		struct curl_slist *headers = nullptr;
		std::string auth = "Authorization: Bearer " + token.toStdString();
		std::string cid = "Client-ID: " + getClientId().toStdString();

		headers = curl_slist_append(headers, auth.c_str());
		headers = curl_slist_append(headers, cid.c_str());
		headers = curl_slist_append(headers, "Content-Type: application/json");

		QJsonDocument doc(body);
		std::string json = doc.toJson(QJsonDocument::Compact).toStdString();

		curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK) {
			blog(LOG_ERROR, "[GameDetector/Auth] cURL error (POST): %s", curl_easy_strerror(res));
			return {0, ""};
		}

		if (http_code < 200 || http_code >= 300) {
			if (http_code == 401) {
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in POST request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				emit authenticationDataNeedsClearing();
				emit reauthenticationNeeded();
				return {http_code, ""};
			} else if (http_code == 429) {
				blog(LOG_WARNING, "[GameDetector/Auth] Twitch API rate limit exceeded (429 Too Many Requests). Please wait a moment and try again.");
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in POST request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

std::pair<QString, QString> TwitchAuthManager::getTokenUserInfo()
{
	if (accessToken.isEmpty())
		return {"", ""};

	QString url = "https://api.twitch.tv/helix/users";
	auto future = performGET(url, accessToken);
	future.waitForFinished();
	auto [http_code, json] = future.result();

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

	QFuture<std::pair<long, QString>> future = performGET(url, accessToken);

	return QtConcurrent::run([future]() mutable -> QString {
		future.waitForFinished();
		auto [http_code, json] = future.result();

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

	QFuture<std::pair<long, QString>> future = performPATCH(url, body, accessToken);

	return QtConcurrent::run([future]() mutable -> UpdateResult {
		future.waitForFinished();
		auto [http_code, json] = future.result();
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
		blog(LOG_WARNING, "[GameDetector/Auth] Attempt to send chat message with incomplete data.");
		QFuture<bool> future = QtConcurrent::run([=]() {
            return false;  // seu resultado
        });
		return future;
	}

	QString url = "https://api.twitch.tv/helix/chat/messages";

	QJsonObject body;
	body["broadcaster_id"] = broadcasterId;
	body["sender_id"] = senderId;
	body["message"] = message;

	QFuture<std::pair<long, QString>> future = performPOST(url, body, accessToken);

	return QtConcurrent::run([future]() mutable -> bool {
		future.waitForFinished();
		auto [http_code, json] = future.result();
		return http_code == 200;
	});
}