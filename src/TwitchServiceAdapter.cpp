#include "TwitchServiceAdapter.h"
#include "ConfigManager.h"
#include <obs-module.h>

TwitchServiceAdapter::TwitchServiceAdapter(QObject *parent) : IPlatformService(parent)
{
    gameIdWatcher = new QFutureWatcher<QString>(this);
    updateWatcher = new QFutureWatcher<TwitchAuthManager::UpdateResult>(this);
    messageWatcher = new QFutureWatcher<bool>(this);

    connect(gameIdWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        QString gameName = gameIdWatcher->property("gameName").toString();
        QString gameId = gameIdWatcher->result();
        if (gameId.isEmpty()) {
            emit categoryUpdateFinished(false, gameName, obs_module_text("Twitch.Error.GameNotFound"));
            return;
        }
        updateWatcher->setProperty("gameName", gameName);
        updateWatcher->setFuture(TwitchAuthManager::get().updateChannelCategory(gameId));
    });

    connect(updateWatcher, &QFutureWatcher<TwitchAuthManager::UpdateResult>::finished, this, [this]() {
        QString gameName = updateWatcher->property("gameName").toString();
        auto result = updateWatcher->result();
        if (result == TwitchAuthManager::UpdateResult::Success) {
            emit categoryUpdateFinished(true, gameName, "");
        } else {
            emit categoryUpdateFinished(false, gameName, obs_module_text("Twitch.Error.UpdateFailed"));
        }
    });
    
    connect(messageWatcher, &QFutureWatcher<bool>::finished, this, [this]() {
        emit messageSent(messageWatcher->result(), messageWatcher->property("message").toString());
    });
}

bool TwitchServiceAdapter::isAuthenticated() const {
    return !TwitchAuthManager::get().getAccessToken().isEmpty();
}

void TwitchServiceAdapter::updateCategory(const QString &gameName)
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
        emit categoryUpdateFinished(false, gameName, obs_module_text("Twitch.Error.NotAuthenticated"));
        return;
    }

    gameIdWatcher->setProperty("gameName", gameName);
    gameIdWatcher->setFuture(TwitchAuthManager::get().getGameId(gameName));
}

void TwitchServiceAdapter::sendChatMessage(const QString &message)
{
    QString uid = TwitchAuthManager::get().getUserId();
    if (uid.isEmpty()) return;
    
    messageWatcher->setProperty("message", message);
    messageWatcher->setFuture(TwitchAuthManager::get().sendChatMessage(uid, uid, message));
}
