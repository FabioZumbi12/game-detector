#include "PlatformManager.h"
#include "TwitchServiceAdapter.h"
#include "TwitchAuthManager.h"
#include "TrovoAuthManager.h"
#include "IPlatformService.h"
#include "ConfigManager.h"
#include "GameDetector.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <obs-module.h>

#include <QTimer>
#include <QDebug>
#include <QVariant>

PlatformManager::PlatformManager()
{
    lastSetCategoryName = "Just Chatting";

    TwitchServiceAdapter *twitch = new TwitchServiceAdapter(this);
    TrovoAuthManager *trovo = new TrovoAuthManager(this);

    auto forwardSignal = [this](bool success, QString gameName, QString error) {
        emit categoryUpdateFinished(success, gameName, error);
    };
    connect(twitch, &IPlatformService::categoryUpdateFinished, this, forwardSignal);
    connect(trovo, &IPlatformService::categoryUpdateFinished, this, forwardSignal);

    gameIdWatcher = new QFutureWatcher<QString>(this);
    categoryUpdateWatcher = new QFutureWatcher<void *>(this);
    chatMessageWatcher = new QFutureWatcher<bool>(this);

    categoryFetchWatcher = new QFutureWatcher<QHash<QString, QString>>(this);
    connect(categoryFetchWatcher, &QFutureWatcher<QHash<QString, QString>>::finished, this, [this]() {
        emit categoriesFetched(categoryFetchWatcher->result());
    });


    cooldownTimer = new QTimer(this);
    cooldownTimer->setSingleShot(true);
    connect(cooldownTimer, &QTimer::timeout, [this]() {
        onCooldown = false;
        emit cooldownFinished();
    });
}

PlatformManager::~PlatformManager()
{
    if (cooldownTimer->isActive()) {
        cooldownTimer->stop();
    }
}

void PlatformManager::shutdown()
{
    shuttingDown = true;

    if (cooldownTimer && cooldownTimer->isActive()) {
        cooldownTimer->stop();
    }

    if (categoryFetchWatcher && categoryFetchWatcher->isRunning()) {
        categoryFetchWatcher->cancel();
        categoryFetchWatcher->waitForFinished();
    }

    if (gameIdWatcher && gameIdWatcher->isRunning()) {
        gameIdWatcher->cancel();
        gameIdWatcher->waitForFinished();
    }

    if (chatMessageWatcher && chatMessageWatcher->isRunning()) {
        chatMessageWatcher->cancel();
        chatMessageWatcher->waitForFinished();
    }

    if (categoryUpdateWatcher && categoryUpdateWatcher->isRunning()) {
        categoryUpdateWatcher->cancel();
        categoryUpdateWatcher->waitForFinished();
    }

    auto services = findChildren<IPlatformService *>();
    qDeleteAll(services);
}

bool PlatformManager::sendChatMessage(const QString &message)
{
    if (onCooldown) {
        blog(LOG_INFO, "[GameDetector/PlatformManager] Action is on cooldown. Ignoring new request.");
        return false;
    }

    auto services = findChildren<IPlatformService *>();
    for (auto service : services) {
        service->sendChatMessage(message);
    }
    
    return true;
}

bool PlatformManager::updateCategory(const QString &gameName, const QString &title, bool force)
{
    if (isOnCooldown()) {
        blog(LOG_INFO, "[GameDetector/PlatformManager] Action is on cooldown. Ignoring new request.");
        return false;
    }

	if (!force && getLastSetCategory() == gameName) {
        return false;
    }

    QStringList targetPlatforms = this->property("targetPlatforms").toStringList();
    this->setProperty("targetPlatforms", QVariant());

    blog(LOG_INFO, "[GameDetector/PlatformManager] Changing category to: %s", gameName.toStdString().c_str());

    auto services = findChildren<IPlatformService *>();
    for (auto service : services) {
        bool shouldUpdate = true;
        if (!targetPlatforms.isEmpty()) {
            shouldUpdate = false;
            if (qobject_cast<TwitchServiceAdapter*>(service) && targetPlatforms.contains("Twitch")) shouldUpdate = true;
            else if (qobject_cast<TrovoAuthManager*>(service) && targetPlatforms.contains("Trovo")) shouldUpdate = true;
        }

        if (shouldUpdate) {
            service->updateCategory(gameName, title);
        }
    }

    setLastSetCategory(gameName);
    setCooldown();

    QTimer::singleShot(3000, this, [this]() {
        fetchCurrentCategories(true);
    });

    return true;
}

void PlatformManager::fetchCurrentCategories(bool force)
{
    if (!force) {
        if (categoryFetchWatcher->isRunning()) {
            return;
        }

        if (lastCategoryFetch.isValid() && lastCategoryFetch.secsTo(QDateTime::currentDateTime()) < 10) {
            return;
        }
    }

    lastCategoryFetch = QDateTime::currentDateTime();

    auto future = QtConcurrent::run([this]() {
        QHash<QString, QString> results;

        if (shuttingDown) {
            return results;
        }

        if (!ConfigManager::get().getTwitchUserId().isEmpty()) {
            QString category = TwitchAuthManager::get().getChannelCategory().result();
            QString title = TwitchAuthManager::get().getChannelTitle().result();
            results["Twitch"] = category + "|||" + title;
        }

        auto trovoManager = findChild<TrovoAuthManager*>();
        if (trovoManager && trovoManager->isAuthenticated()) {
            QString category = trovoManager->getChannelCategory().result();
            QString title = trovoManager->getChannelTitle().result();
            results["Trovo"] = category + "|||" + title;
        }
        return results;
    });
    categoryFetchWatcher->setFuture(future);
}

void PlatformManager::onGameIdReceived()
{
}

void PlatformManager::onCategoryUpdateCompleted()
{
}

void PlatformManager::onChatMessageSent()
{
}

void PlatformManager::setCooldown()
{
    int delaySeconds = ConfigManager::get().getActionDelay();
    if (delaySeconds > 0) {
        onCooldown = true;
        cooldownTimer->start(delaySeconds * 1000);
        emit cooldownStarted(delaySeconds);
    }
}

bool PlatformManager::isOnCooldown() const
{
    return onCooldown;
}

int PlatformManager::getCooldownRemaining() const
{
    return onCooldown ? cooldownTimer->remainingTime() / 1000 : 0;
}

void PlatformManager::setLastSetCategory(const QString &categoryName)
{
    this->lastSetCategoryName = categoryName;
}

QString PlatformManager::getLastSetCategory() const
{
    return lastSetCategoryName;
}
