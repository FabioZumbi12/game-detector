#include "TrovoAuthManager.h"
#include "ConfigManager.h"
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

static size_t trovo_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    ((std::string *)userp)->append((char *)contents, realsize);
    return realsize;
}

TrovoAuthManager::TrovoAuthManager(QObject *parent) : IPlatformService(parent)
{
    server = new QTcpServer(this);
    authTimeoutTimer = new QTimer(this);
    connect(server, &QTcpServer::newConnection, this, &TrovoAuthManager::onNewConnection);
    connect(authTimeoutTimer, &QTimer::timeout, this, &TrovoAuthManager::onAuthTimerTick);
    loadToken();
}

TrovoAuthManager::~TrovoAuthManager()
{
    if (server->isListening()) server->close();
    pendingTask.waitForFinished();
}

void TrovoAuthManager::loadToken()
{
    auto settings = ConfigManager::get().getSettings();
    accessToken = ConfigManager::get().getTrovoToken();
    userId = ConfigManager::get().getTrovoUserId();
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
        scope = "channel_update_self+user_details_self+chat_send_self";
    } else {
        scope = (actionMode == 0) ? "user_details_self+chat_send_self" : "channel_update_self+user_details_self";
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

    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);

    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        QString request = clientSocket->readAll();
        blog(LOG_INFO, "[GameDetector/TrovoAuth] Request received: %s", request.toStdString().c_str());

        QStringList reqLines = request.split("\r\n");
        if (reqLines.isEmpty()) return;

        QString firstLine = reqLines.first();
        
        // Handle CORS Preflight
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
    auto future = performGET("https://open-api.trovo.live/openplatform/validate", accessToken);
    pendingTask = QtConcurrent::run([this, future]() {
        auto result = future.result();
        if (result.first == 200) {
            blog(LOG_INFO, "[GameDetector/TrovoAuth] User info fetched successfully.");
            QJsonDocument doc = QJsonDocument::fromJson(result.second.toUtf8());
            this->userId = doc.object()["uid"].toString();
            QString nickName = doc.object()["nick_name"].toString();
            
            ConfigManager::get().setTrovoToken(accessToken);
            ConfigManager::get().setTrovoUserId(userId);
            ConfigManager::get().setTrovoChannelLogin(nickName);
            ConfigManager::get().save(ConfigManager::get().getSettings());
            emit authenticationFinished(true, nickName);
        } else {
            blog(LOG_ERROR, "[GameDetector/TrovoAuth] Failed to fetch user info. HTTP Code: %ld, Response: %s", result.first, result.second.toStdString().c_str());
            emit authenticationFinished(false, obs_module_text("Auth.Error.GetUserIdFailed"));
        }
    });
}

void TrovoAuthManager::updateCategory(const QString &gameName)
{
    int actionMode = ConfigManager::get().getActionMode();

    if (actionMode == 0) { // Chat Command
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
    auto future = performPOST("https://open-api.trovo.live/openplatform/searchcategory", body, accessToken);

    pendingTask = QtConcurrent::run([this, future, gameName]() {
        auto result = future.result();
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
        auto updateFuture = performPOST("https://open-api.trovo.live/openplatform/channels/update", updateBody, accessToken);
        
        if (updateFuture.result().first == 200) {
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
    performPOST("https://open-api.trovo.live/openplatform/chat/send", body, accessToken);
}

QFuture<std::pair<long, QString>> TrovoAuthManager::performPOST(const QString &url, const QJsonObject &body, const QString &token)
{
    return QtConcurrent::run([this, url, body, token]() -> std::pair<long, QString> {
        CURL *curl = curl_easy_init();
        if (!curl) return {0, ""};
        long http_code = 0;
        std::string response;
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, ("Client-ID: " + CLIENT_ID.toStdString()).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");
        if (!token.isEmpty()) {
            std::string auth = "Authorization: OAuth " + token.toStdString();
            headers = curl_slist_append(headers, auth.c_str());
        }
        QJsonDocument doc(body);
        std::string json = doc.toJson(QJsonDocument::Compact).toStdString();
        curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, trovo_curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {http_code, QString::fromStdString(response)};
    });
}

QFuture<std::pair<long, QString>> TrovoAuthManager::performGET(const QString &url, const QString &token)
{
    return QtConcurrent::run([this, url, token]() -> std::pair<long, QString> {
        CURL *curl = curl_easy_init();
        if (!curl) return {0, ""};
        long http_code = 0;
        std::string response;
        struct curl_slist *headers = nullptr;

        headers = curl_slist_append(headers, "Accept: application/json");
        std::string clientIdHeader = "Client-ID: " + CLIENT_ID.toStdString();
        headers = curl_slist_append(headers, clientIdHeader.c_str());

        if (!token.isEmpty()) {
            std::string authHeader = "Authorization: OAuth " + token.toStdString();
            headers = curl_slist_append(headers, authHeader.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, trovo_curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {http_code, QString::fromStdString(response)};
    });
}
