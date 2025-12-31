#pragma once

#include <string>
#include <curl/curl.h>
#include <QString>
#include <obs-module.h>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>
#include <exception>
#include <utility>

static size_t auth_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    ((std::string *)userp)->append((char *)contents, realsize);
    return realsize;
}

inline std::pair<long, QString> ExecuteNetworkRequest(const QString &url, const QString &method, struct curl_slist *headers, const std::string &body = "", bool verbose = false)
{
    CURL *curl = curl_easy_init();
    if (!curl) return {0, ""};

    long http_code = 0;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    if (verbose) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        blog(LOG_ERROR, "[NetworkCommon] cURL error: %s", curl_easy_strerror(res));
        return {0, ""};
    }

    return {http_code, QString::fromStdString(response)};
}

template <typename Func>
auto RunTaskSafe(QThreadPool *pool, const char *context, Func &&func) -> QFuture<decltype(func())>
{
    using ReturnType = decltype(func());
    return QtConcurrent::run(pool, [func = std::forward<Func>(func), context]() mutable -> ReturnType {
        try {
            return func();
        } catch (const std::exception &e) {
            blog(LOG_ERROR, "[%s] Exception caught: %s", context, e.what());
            if constexpr (!std::is_void_v<ReturnType>) {
                return ReturnType{};
            }
        } catch (...) {
            blog(LOG_ERROR, "[%s] Unknown exception caught.", context);
            if constexpr (!std::is_void_v<ReturnType>) {
                return ReturnType{};
            }
        }
    });
}
