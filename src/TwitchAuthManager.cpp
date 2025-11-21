#include "TwitchAuthManager.h"
#include "ConfigManager.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>

// ------------------------------
// CURL Write Callback
// ------------------------------
static size_t auth_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	((std::string *)userp)->append((char *)contents, realsize);
	return realsize;
}

// ------------------------------
// Singleton
// ------------------------------
TwitchAuthManager::TwitchAuthManager(QObject *parent) : QObject(parent)
{
	server = new QTcpServer(this);

	connect(server, &QTcpServer::newConnection, this, &TwitchAuthManager::onNewConnection);

	// Carrega valores do Config
	auto settings = ConfigManager::get().getSettings();
	accessToken = obs_data_get_string(settings, "twitch_access_token");
	userId = obs_data_get_string(settings, "twitch_user_id");
	clientId = "au09dsnlplmvtlvwgenvvdup5cf458";
	connect(this, &TwitchAuthManager::reauthenticationNeeded, this, &TwitchAuthManager::handleReauthenticationRequest);
}

TwitchAuthManager::~TwitchAuthManager()
{
	if (server->isListening())
		server->close();
}

// ------------------------------
// Início da autenticação
// ------------------------------
void TwitchAuthManager::startAuthentication()
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

	query.addQueryItem("client_id", clientId);
	query.addQueryItem("redirect_uri", REDIRECT_URI);
	query.addQueryItem("response_type", "token");
	query.addQueryItem("scope", "user:write:chat channel:manage:broadcast");

	authUrl.setQuery(query);

	QDesktopServices::openUrl(authUrl);
	isAuthenticating = true; // Define a flag para indicar que a autenticação está em andamento
}

// ------------------------------
// Limpa dados de autenticação em memória
// ------------------------------
void TwitchAuthManager::clearAuthentication()
{
	accessToken.clear();
	userId.clear(); // Limpa o ID do usuário em memória
	ConfigManager::get().setToken("");
	ConfigManager::get().setUserId("");
	ConfigManager::get().setTwitchChannelLogin(""); // Limpa o login do canal também
	ConfigManager::get().save(ConfigManager::get().getSettings()); // Saves cleared settings
	blog(LOG_INFO, "[GameDetector/Auth] In-memory and persisted authentication data cleared.");
}

// ------------------------------
// Novo navegador conectado
// ------------------------------
void TwitchAuthManager::onNewConnection()
{
	// Cada conexão é tratada de forma independente para evitar problemas com múltiplas requisições (ex: favicon.ico)
	QTcpSocket *clientSocket = server->nextPendingConnection();
	if (!clientSocket)
		return;

	// Conecta o sinal readyRead a uma lambda que captura o socket atual
	connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
		if (!clientSocket || !clientSocket->isValid())
			return;

		QString request = clientSocket->readAll();
		QStringList reqLines = request.split("\r\n");
		if (reqLines.isEmpty()) {
			clientSocket->disconnectFromHost();
			return;
		}

		QString firstLine = reqLines.first(); // Ex: "GET /?token=... HTTP/1.1"
		QString path = firstLine.split(" ")[1];

		// A URL pode vir como /?token=... ou /#token=... ou apenas /
		QUrl url(path);
		QString token = QUrlQuery(url.query()).queryItemValue("token");

		if (!token.isEmpty()) {
			// 2) O token chegou via redirecionamento do JavaScript

			QString successPage = QString(
				"<!DOCTYPE html><html><head><title>%1</title></head><body style='font-family: sans-serif; background-color: #f4f4f4; text-align: center; padding-top: 50px;'>"
				"%2%3"
				"</body></html>")
				.arg(obs_module_text("Auth.Page.Title"), obs_module_text("Auth.Page.Success.Title"), obs_module_text("Auth.Page.Success.Message"));

			// Responde ao navegador e fecha o servidor e o socket
			QString httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + successPage;
			clientSocket->write(httpResponse.toUtf8());
			clientSocket->disconnectFromHost();
			server->close(); // Servidor cumpriu sua missão
 
			isAuthenticating = false; // Authentication has finished
			if (token.isEmpty()) {
				emit authenticationFinished(false, obs_module_text("Auth.Error.EmptyToken"));
				return;
			}
 
			// Salva o token e obtém o ID do usuário
			auto settings = ConfigManager::get().getSettings();
			accessToken = token;
			auto [newUserId, loginName] = getTokenUserInfo();
			userId = newUserId;

			if (!userId.isEmpty()) {
				obs_data_set_string(settings, "twitch_access_token", accessToken.toStdString().c_str());
				obs_data_set_string(settings, "twitch_user_id", userId.toStdString().c_str());
				obs_data_set_string(settings, "twitch_channel_login", loginName.toStdString().c_str());
				obs_data_set_string(settings, "twitch_refresh_token", ""); // Limpa refresh token antigo
				ConfigManager::get().save(settings);
				emit authenticationFinished(true, loginName);
			} else {
				obs_data_set_string(settings, "twitch_access_token", "");
				obs_data_set_string(settings, "twitch_user_id", "");
				emit authenticationFinished(false, obs_module_text("Auth.Error.GetUserIdFailed"));
			}
 
		} else if (path.startsWith("/")) {
			isAuthenticating = false; // Authentication has finished (if it gets here without a token, it's a failure)
 
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
 
			// 1) First request, serves the HTML with JavaScript to extract the token
			QString httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + errorPage;
			clientSocket->write(httpResponse.toUtf8());
			clientSocket->disconnectFromHost();
		} else {
			// Ignora outras requisições (ex: favicon.ico)
			clientSocket->write("HTTP/1.1 404 Not Found\r\n\r\n");
			clientSocket->disconnectFromHost();
		}
	});

	// Garante que o socket seja deletado quando a conexão for fechada
	connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);
}

// ------------------------------
// Retorna o access token atual
// ------------------------------
QString TwitchAuthManager::getAccessToken()
{
	return accessToken;
}

QString TwitchAuthManager::getClientId()
{
	return clientId;
}

QString TwitchAuthManager::getUserId()
{
	return userId;
}

// ------------------------------
// Curl GET
// ------------------------------
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
		std::string cid = "Client-ID: " + clientId.toStdString();

		headers = curl_slist_append(headers, auth.c_str());
		headers = curl_slist_append(headers, cid.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Não falhar em erros HTTP, para podermos ler o corpo

		CURLcode res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK) {
			blog(LOG_ERROR, "[GameDetector/Auth] cURL error (GET): %s", curl_easy_strerror(res));
			return {0, ""};
		}

		if (http_code < 200 || http_code >= 300) {
			if (http_code == 401) { // 401 Unauthorized
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in GET request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				QMetaObject::invokeMethod(this, "clearAuthentication", Qt::QueuedConnection);
				QMetaObject::invokeMethod(this, "reauthenticationNeeded", Qt::QueuedConnection);
				return {http_code, ""}; // Retorna erro e resposta vazia
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in GET request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

// ------------------------------
// Curl PATCH
// ------------------------------
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
		std::string cid = "Client-ID: " + clientId.toStdString();

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
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L); // Não falhar em erros HTTP, para podermos ler o corpo

		CURLcode res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (res != CURLE_OK) {
			blog(LOG_ERROR, "[GameDetector/Auth] cURL error (PATCH): %s", curl_easy_strerror(res));
			return {0, ""};
		}

		// Log an error if the response is not successful (2xx)
		if (http_code < 200 || http_code >= 300) {
			if (http_code == 401) { // 401 Unauthorized
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in PATCH request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				QMetaObject::invokeMethod(this, "clearAuthentication", Qt::QueuedConnection);
				QMetaObject::invokeMethod(this, "reauthenticationNeeded", Qt::QueuedConnection);
				return {http_code, ""}; // Retorna erro e resposta vazia
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in PATCH request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

// ------------------------------
// Curl POST
// ------------------------------
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
		std::string cid = "Client-ID: " + clientId.toStdString();

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
			if (http_code == 401) { // 401 Unauthorized
				blog(LOG_WARNING, "[GameDetector/Auth] Invalid token (401 Unauthorized) in POST request to %s. Initiating reauthentication process.", url.toStdString().c_str());
				QMetaObject::invokeMethod(this, "clearAuthentication", Qt::QueuedConnection);
				QMetaObject::invokeMethod(this, "reauthenticationNeeded", Qt::QueuedConnection);
				return {http_code, ""}; // Retorna erro e resposta vazia
			}
			blog(LOG_WARNING, "[GameDetector/Auth] Error in POST request to Twitch API (Status: %ld): %s", http_code, response.c_str());
		}
 
		return {http_code, QString::fromStdString(response)};
	});
}

// ------------------------------
// Obtém UserID do token
// ------------------------------
std::pair<QString, QString> TwitchAuthManager::getTokenUserInfo()
{
	if (accessToken.isEmpty())
		return {"", ""};

	QString url = "https://api.twitch.tv/helix/users";
	// This method is still synchronous because it is used during the initial authentication flow.
	// Converting it to asynchronous would require a major refactoring in the authentication flow.
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

// ------------------------------
// Procura GameID por nome
// ------------------------------
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

// ------------------------------
// Atualiza categoria do canal
// ------------------------------
QFuture<bool> TwitchAuthManager::updateChannelCategory(const QString &gameId)
{
	QString url = "https://api.twitch.tv/helix/channels?broadcaster_id=" + userId;

	QJsonObject body;
	body["game_id"] = gameId;

	QFuture<std::pair<long, QString>> future = performPATCH(url, body, accessToken);

	return QtConcurrent::run([future]() mutable -> bool {
		future.waitForFinished();
		auto [http_code, json] = future.result();
		return http_code == 204; // A API da Twitch retorna 204 No Content em caso de sucesso.
	});
}

// ------------------------------
// Envia mensagem para o chat
// ------------------------------
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

// ------------------------------
// Slot para lidar com a necessidade de reautenticação
// ------------------------------
void TwitchAuthManager::handleReauthenticationRequest()
{
	// The authentication flow is started by the UI (Settings or Dock) after user confirmation.
	// This signal just notifies the UI that it needs to happen.
}
