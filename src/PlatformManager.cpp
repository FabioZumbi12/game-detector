#include "PlatformManager.h"
#include "TwitchServiceAdapter.h"
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

bool PlatformManager::updateCategory(const QString &gameName)
{
    if (isOnCooldown()) {
        blog(LOG_INFO, "[GameDetector/PlatformManager] Action is on cooldown. Ignoring new request.");
        return false;
    }

    if (getLastSetCategory() == gameName) {
        return false;
    }

    blog(LOG_INFO, "[GameDetector/PlatformManager] Changing category to: %s", gameName.toStdString().c_str());

    auto services = findChildren<IPlatformService *>();
    for (auto service : services) {
        service->updateCategory(gameName);
    }

    setLastSetCategory(gameName);
    setCooldown();

    return true;
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
