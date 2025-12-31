// d:\ProjetosOBS\OBSGameDetector\src\PlatformManager.cpp
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

    // Inicializa os serviços suportados
    TwitchServiceAdapter *twitch = new TwitchServiceAdapter(this);
    TrovoAuthManager *trovo = new TrovoAuthManager(this);

    // Conecta sinais dos serviços ao Manager
    auto forwardSignal = [this](bool success, QString gameName, QString error) {
        emit categoryUpdateFinished(success, gameName, error);
    };
    connect(twitch, &IPlatformService::categoryUpdateFinished, this, forwardSignal);
    connect(trovo, &IPlatformService::categoryUpdateFinished, this, forwardSignal);

    // Inicializa watchers (legado/backup)
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

    // Envia para todos os serviços registrados
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

    // Itera sobre todos os serviços (Twitch, Trovo) e solicita atualização
    auto services = findChildren<IPlatformService *>();
    for (auto service : services) {
        service->updateCategory(gameName);
    }

    // Atualizamos o estado local assumindo que o processo iniciou
    setLastSetCategory(gameName);
    setCooldown();

    return true;
}

void PlatformManager::onGameIdReceived()
{
    // Lógica movida para os Adapters específicos
}

void PlatformManager::onCategoryUpdateCompleted()
{
    // Lógica movida para os Adapters específicos
}

void PlatformManager::onChatMessageSent()
{
    // Lógica movida para os Adapters específicos
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
