#include "TrovoAuthManager.h"
#include "ConfigManager.h"
#include "NetworkCommon.h"
#include <obs-module.h>
#include <curl/curl.h>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>

TrovoAuthManager::TrovoAuthManager(QObject *parent) : IPlatformService(parent)
{
    server = new QTcpServer(this);
    authTimeoutTimer = new QTimer(this);
    connect(server, &QTcpServer::newConnection, this, &TrovoAuthManager::onNewConnection);
    connect(authTimeoutTimer, &QTimer::timeout, this, &TrovoAuthManager::onAuthTimerTick);
    loadToken();
    threadPool.setMaxThreadCount(4);
}

TrovoAuthManager::~TrovoAuthManager()
{
    if (server->isListening()) server->close();
    threadPool.waitForDone();

    for (auto sock : clientSockets) {
        if (sock) {
            if (sock->isOpen()) {
                sock->disconnectFromHost();
                sock->close();
            }
            sock->deleteLater();
        }
    }
    clientSockets.clear();
}

void TrovoAuthManager::loadToken()
{
    auto settings = ConfigManager::get().getSettings();
    accessToken = ConfigManager::get().getTrovoToken();
    userId = ConfigManager::get().getTrovoUserId();
    refreshToken = obs_data_get_string(settings, "trovo_refresh_token");
}

bool TrovoAuthManager::isAuthenticated() const
{
    return !accessToken.isEmpty() && !userId.isEmpty();
}

void TrovoAuthManager::startAuthentication(int mode, int unifiedAuth)
{
    if (isAuthenticating) return;
    
    if (server->isListening()) server->close();

    if (!server->listen(QHostAddress::LocalHost, 31000)) return;

    QUrl authUrl("https://open.trovo.live/page/login.html");
    QUrlQuery query;
    query.addQueryItem("client_id", CLIENT_ID);
    query.addQueryItem("response_type", "code");
    
    bool useUnifiedAuth = (unifiedAuth == -1) ? ConfigManager::get().getUnifiedAuth() : (bool)unifiedAuth;
    int actionMode = (mode == -1) ? ConfigManager::get().getActionMode() : mode;

    QString scope;
    if (useUnifiedAuth) {
        scope = "channel_details_self+channel_update_self+user_details_self+chat_send_self";
    } else {
        scope = (actionMode == 0) ? "channel_details_self+user_details_self+chat_send_self" : "channel_details_self+channel_update_self+user_details_self";
    }
    query.addQueryItem("scope", scope);
    query.addQueryItem("redirect_uri", AUTH_API_URL);
    authUrl.setQuery(query);
    QDesktopServices::openUrl(authUrl);
    isAuthenticating = true;

    authRemainingSeconds = 30;
    emit authenticationTimerTick(authRemainingSeconds);
    authTimeoutTimer->start(1000);
}

void TrovoAuthManager::onAuthTimerTick()
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

void TrovoAuthManager::onNewConnection()
{
    QTcpSocket *clientSocket = server->nextPendingConnection();
    if (!clientSocket) return;
    clientSockets.append(clientSocket);

    connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
        if (clientSocket) {
            clientSocket->deleteLater();
        }
        clientSockets.removeAll(clientSocket);
    });

    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        QString request = clientSocket->readAll();
        QStringList reqLines = request.split("\r\n");
        if (reqLines.isEmpty()) return;

        QString firstLine = reqLines.first();
        
        if (firstLine.startsWith("OPTIONS")) {
            clientSocket->write("HTTP/1.1 204 No Content\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, OPTIONS\r\nAccess-Control-Allow-Headers: *\r\n\r\n");
            clientSocket->flush();
            clientSocket->disconnectFromHost();
            return;
        }

        QString path = firstLine.split(" ")[1];
        QUrl url(path);
        QUrlQuery query(url.query());
        QString token = query.queryItemValue("token");
        QString refresh = query.queryItemValue("refresh_token");

        if (!token.isEmpty()) {
            blog(LOG_INFO, "[GameDetector/TrovoAuth] Token extracted. Fetching user info...");
            QString msg = "Token received.";
            clientSocket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n\r\n" + msg.toUtf8());
            clientSocket->flush();
            clientSocket->disconnectFromHost();
            
            authTimeoutTimer->stop();
            emit authenticationTimerTick(0);
            server->close();
            isAuthenticating = false;
            this->accessToken = token;
            this->refreshToken = refresh;
            fetchUserInfo();
        } else {
            blog(LOG_WARNING, "[GameDetector/TrovoAuth] No token found in request: %s", path.toStdString().c_str());
            clientSocket->write("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
            clientSocket->flush();
            clientSocket->disconnectFromHost();
        }
    });
}

void TrovoAuthManager::fetchUserInfo()
{
    (void)RunTaskSafe(&threadPool, "TrovoAuth/fetchUserInfo", [this]() {
        auto result = performGETSync("https://open-api.trovo.live/openplatform/validate", accessToken);
        
        if (result.first == 200) {
            blog(LOG_INFO, "[GameDetector/TrovoAuth] User info fetched successfully.");
            QJsonDocument doc = QJsonDocument::fromJson(result.second.toUtf8());
            this->userId = doc.object()["uid"].toString();
            QString nickName = doc.object()["nick_name"].toString();
            
            ConfigManager::get().setTrovoToken(accessToken);
            ConfigManager::get().setTrovoUserId(userId);
            ConfigManager::get().setTrovoChannelLogin(nickName);
            obs_data_set_string(ConfigManager::get().getSettings(), "trovo_refresh_token", refreshToken.toStdString().c_str());
            ConfigManager::get().save(ConfigManager::get().getSettings());
            emit authenticationFinished(true, nickName);
        } else {
            blog(LOG_ERROR, "[GameDetector/TrovoAuth] Failed to fetch user info. HTTP Code: %ld, Response: %s", result.first, result.second.toStdString().c_str());
            emit authenticationFinished(false, obs_module_text("Auth.Error.GetUserIdFailed"));
        }
    });
}

bool TrovoAuthManager::refreshAccessToken()
{
    if (lastRefreshAttempt.isValid() && lastRefreshAttempt.secsTo(QDateTime::currentDateTime()) < 5) {
        blog(LOG_INFO, "[GameDetector/TrovoAuth] Refresh token attempt skipped due to rate limit.");
        return false;
    }
    lastRefreshAttempt = QDateTime::currentDateTime();

    if (refreshToken.isEmpty()) return false;

    blog(LOG_INFO, "[GameDetector/TrovoAuth] Refreshing access token...");

    QJsonObject body;
    body["grant_type"] = "refresh_token";    
    body["refresh_token"] = refreshToken;

    auto [http_code, response] = performPOSTSync(AUTH_API_URL, body, "");

    if (http_code == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject json = doc.object();
        
        this->accessToken = json["access_token"].toString();
        this->refreshToken = json["refresh_token"].toString();

        ConfigManager::get().setTrovoToken(this->accessToken);
        obs_data_set_string(ConfigManager::get().getSettings(), "trovo_refresh_token", this->refreshToken.toStdString().c_str());
        ConfigManager::get().save(ConfigManager::get().getSettings());
        
        blog(LOG_INFO, "[GameDetector/TrovoAuth] Token refreshed successfully.");
        return true;
    }

    blog(LOG_WARNING, "[GameDetector/TrovoAuth] Failed to refresh token. HTTP: %ld Response: %s", http_code, response.toStdString().c_str());
    return false;
}

void TrovoAuthManager::updateCategory(const QString &gameName)
{
    int actionMode = ConfigManager::get().getActionMode();

    if (actionMode == 0) {
        QString cmd;
        if (gameName == "Just Chatting") {
            cmd = ConfigManager::get().getNoGameCommand();
        } else {
            cmd = ConfigManager::get().getCommand();
            cmd.replace("{game}", gameName);
        }
        if (!cmd.isEmpty()) sendChatMessage(cmd);
        emit categoryUpdateFinished(true, gameName, "Command sent");
        return;
    }

    if (!isAuthenticated()) {
        emit categoryUpdateFinished(false, gameName, obs_module_text("Trovo.Error.NotAuthenticated"));
        return;
    }
    searchAndSetCategory(gameName);
}

void TrovoAuthManager::searchAndSetCategory(const QString &gameName)
{
    QString searchTerm = gameName;
    if (gameName == "Just Chatting") {
        searchTerm = "ChitChat";
    }

    QJsonObject body;
    body["query"] = searchTerm;
    body["limit"] = 1;

    (void)RunTaskSafe(&threadPool, "TrovoAuth/searchAndSetCategory", [this, body, gameName]() {
        auto result = performPOSTSync("https://open-api.trovo.live/openplatform/searchcategory", body, accessToken);
        QString categoryId;
        if (result.first == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(result.second.toUtf8());
            QJsonArray list = doc.object()["category_info"].toArray();
            if (!list.isEmpty()) categoryId = list.first().toObject()["id"].toString();
        }

        if (categoryId.isEmpty()) {
            emit categoryUpdateFinished(false, gameName, obs_module_text("Trovo.Error.GameNotFound"));
            return;
        }

        QJsonObject updateBody;
        updateBody["channel_id"] = this->userId;
        updateBody["category_id"] = categoryId;
        auto updateResult = performPOSTSync("https://open-api.trovo.live/openplatform/channels/update", updateBody, accessToken);
        
        if (updateResult.first == 200) {
            emit categoryUpdateFinished(true, gameName, "");
        } else {
            emit categoryUpdateFinished(false, gameName, obs_module_text("Trovo.Error.UpdateFailed"));
        }
    });
}

void TrovoAuthManager::sendChatMessage(const QString &message)
{
    if (!isAuthenticated()) return;
    QJsonObject body;
    body["content"] = message;
    body["channel_id"] = userId;
    (void)performPOST("https://open-api.trovo.live/openplatform/chat/send", body, accessToken);
}

QFuture<std::pair<long, QString>> TrovoAuthManager::performPOST(const QString &url, const QJsonObject &body, const QString &token)
{
    return RunTaskSafe(&threadPool, "TrovoAuth/performPOST", [this, url, body, token]() -> std::pair<long, QString> {
        return performPOSTSync(url, body, token);
    });
}

QFuture<std::pair<long, QString>> TrovoAuthManager::performGET(const QString &url, const QString &token)
{
    return RunTaskSafe(&threadPool, "TrovoAuth/performGET", [this, url, token]() -> std::pair<long, QString> {
        return performGETSync(url, token);
    });
}

std::pair<long, QString> TrovoAuthManager::performPOSTSync(const QString &url, const QJsonObject &body, const QString &token)
{
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("client-id: " + CLIENT_ID.toStdString()).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    if (!token.isEmpty()) {
        std::string auth = "Authorization: OAuth " + token.toStdString();
        headers = curl_slist_append(headers, auth.c_str());
    }
    QJsonDocument doc(body);
    std::string json = doc.toJson(QJsonDocument::Compact).toStdString();

    auto [http_code, response] = ExecuteNetworkRequest(url, "POST", headers, json);
    curl_slist_free_all(headers);

    if (http_code < 200 || http_code >= 300) {
        blog(LOG_WARNING, "[GameDetector/TrovoAuth] Error in POST request to Trovo API (Status: %ld): %s", http_code, response.toStdString().c_str());
        
        if (http_code == 401 && !refreshToken.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
            if (doc.isObject() && doc.object().value("error").toString() == "accessTokenExpired") {
                if (refreshAccessToken()) {
                    blog(LOG_INFO, "[GameDetector/TrovoAuth] Retrying POST request with new token...");
                    return performPOSTSync(url, body, accessToken);
                }
            }
        }
    }

    return {http_code, response};
}

std::pair<long, QString> TrovoAuthManager::performGETSync(const QString &url, const QString &token)
{
    struct curl_slist *headers = nullptr;

    headers = curl_slist_append(headers, "Accept: application/json");
    std::string clientIdHeader = "client-id: " + CLIENT_ID.toStdString();
    headers = curl_slist_append(headers, clientIdHeader.c_str());

    if (!token.isEmpty()) {
        std::string authHeader = "Authorization: OAuth " + token.toStdString();
        headers = curl_slist_append(headers, authHeader.c_str());
    }

    auto [http_code, response] = ExecuteNetworkRequest(url, "GET", headers, "", true);
    curl_slist_free_all(headers);

    if (http_code < 200 || http_code >= 300) {
        blog(LOG_WARNING, "[GameDetector/TrovoAuth] Error in GET request to Trovo API (Status: %ld): %s", http_code, response.toStdString().c_str());

        if (http_code == 401 && !refreshToken.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
            if (doc.isObject() && doc.object().value("error").toString() == "accessTokenExpired") {
                if (refreshAccessToken()) {
                    blog(LOG_INFO, "[GameDetector/TrovoAuth] Retrying GET request with new token...");
                    return performGETSync(url, accessToken);
                }
            }
        }
    }

    return {http_code, response};
}

QFuture<QString> TrovoAuthManager::getChannelCategory()
{
	if (!isAuthenticated()) {
		return QtConcurrent::run(&threadPool, []() { return QString(); });
	}

	return RunTaskSafe(&threadPool, "TrovoAuth/getChannelCategory", [this]() -> QString {
		auto [http_code, response] = performGETSync("https://open-api.trovo.live/openplatform/channel", accessToken);

		QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
		if (http_code == 200) {
			if (doc.isObject()) {
				return doc.object()["category_name"].toString();
			}
			return "Erro: Resposta da API inválida";
		}

		if (doc.isObject()) {
			QString message = doc.object()["message"].toString();
			if (!message.isEmpty()) {
				return "Erro: " + message;
			}
		}
		return QString("Erro: HTTP %1").arg(http_code);
	});
}
