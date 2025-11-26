#include "TwitchChatBot.h"
#include "TwitchAuthManager.h"
#include "ConfigManager.h"
#include "GameDetector.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <obs-module.h>

#include <QTimer>
#include <QDebug>
#include <QVariant>

TwitchChatBot::TwitchChatBot()
{
	gameIdWatcher = new QFutureWatcher<QString>(this);
	categoryUpdateWatcher = new QFutureWatcher<void *>(this);
	chatMessageWatcher = new QFutureWatcher<bool>(this);

	cooldownTimer = new QTimer(this);
	cooldownTimer->setSingleShot(true);
	connect(cooldownTimer, &QTimer::timeout, [this]() {
		onCooldown = false;
		emit cooldownFinished();
	});

	connect(gameIdWatcher, &QFutureWatcher<QString>::finished, this, &TwitchChatBot::onGameIdReceived);
	connect(categoryUpdateWatcher, &QFutureWatcher<void *>::finished, this, &TwitchChatBot::onCategoryUpdateCompleted);
	connect(chatMessageWatcher, &QFutureWatcher<bool>::finished, this, &TwitchChatBot::onChatMessageSent);
	connect(&TwitchAuthManager::get(), &TwitchAuthManager::reauthenticationNeeded, this, &TwitchChatBot::authenticationRequired);
}

TwitchChatBot::~TwitchChatBot()
{
	if (cooldownTimer->isActive()) {
		cooldownTimer->stop();
	}
}

bool TwitchChatBot::sendChatMessage(const QString &message)
{
	if (onCooldown) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Action is on cooldown. Ignoring new request.");
		return false;
	}

	if (chatMessageWatcher->isRunning()) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Chat message sending already in progress. Ignoring new request.");
		return false;
	}

	QString broadcasterId = ConfigManager::get().getUserId();
	QString senderId = broadcasterId;

	if (broadcasterId.isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Attempt to send message without authentication.");
		emit authenticationRequired();
		return false;
	}

	chatMessageWatcher->setProperty("message", message);

	QFuture<bool> future = TwitchAuthManager::get().sendChatMessage(broadcasterId, senderId, message);
	chatMessageWatcher->setFuture(future);
	return true;
}

bool TwitchChatBot::updateCategory(const QString &gameName)
{
	if (isOnCooldown()) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Action is on cooldown. Ignoring new request.");
		return false;
	}

	if (getLastSetCategory() == gameName) {
		blog(LOG_INFO, "[GameDetector] Category '%s' is already set. Skipping update.", gameName.toStdString().c_str());
		return false;
	}

	blog(LOG_INFO, "[GameDetector/ChatBot] Changing category to: %s", gameName.toStdString().c_str());

	int actionMode = ConfigManager::get().getTwitchActionMode();
	if (actionMode == 0) {
		if (gameName == "Just Chatting"){
			QString noGameCommand = ConfigManager::get().getNoGameCommand();
			if (!noGameCommand.isEmpty()) {
				if (sendChatMessage(noGameCommand))
					setLastSetCategory("Just Chatting");
			}
		} else {
			QString gameCommand = ConfigManager::get().getCommand();
			if (!gameCommand.isEmpty()) {
				if (sendChatMessage(gameCommand.replace("{game}", gameName)))
					setLastSetCategory(gameName);
			}
		}
	} else {
		if (gameIdWatcher->isRunning() || categoryUpdateWatcher->isRunning()) {
			blog(LOG_INFO, "[GameDetector/ChatBot] Category update already in progress. Ignoring new request.");
			return false;
		}

		if (TwitchAuthManager::get().getUserId().isEmpty()) {
			blog(LOG_WARNING, "[GameDetector/ChatBot] Attempt to change category without authentication.");
			emit authenticationRequired();
			return false;
		}

		gameIdWatcher->setProperty("gameName", gameName);

		QFuture<QString> gameIdFuture = TwitchAuthManager::get().getGameId(gameName);
		gameIdWatcher->setFuture(gameIdFuture);
	}

	return true;
}

void TwitchChatBot::onGameIdReceived()
{
	QString gameName = gameIdWatcher->property("gameName").toString();
	QString gameId = gameIdWatcher->result();

	if (gameId.isEmpty()) {
		blog(LOG_ERROR, "[GameDetector/ChatBot] GameID not found for '%s'.", gameName.toStdString().c_str());
		emit categoryUpdateFinished(false, gameName, obs_module_text("Dock.CategoryUpdateFailed.GameNotFound"));
		return;
	}

	categoryUpdateWatcher->setProperty("gameName", gameName);

	QFuture<TwitchAuthManager::UpdateResult> updateFuture = TwitchAuthManager::get().updateChannelCategory(gameId);

	QFuture<void *> adaptedFuture = QtConcurrent::run([updateFuture]() mutable -> void * {
		updateFuture.waitForFinished();
		return reinterpret_cast<void *>(updateFuture.result());
	});

	categoryUpdateWatcher->setFuture(adaptedFuture);
}

void TwitchChatBot::onCategoryUpdateCompleted()
{
	QString gameName = categoryUpdateWatcher->property("gameName").toString();
	auto result = static_cast<TwitchAuthManager::UpdateResult>(reinterpret_cast<intptr_t>(categoryUpdateWatcher->result()));

	if (result == TwitchAuthManager::UpdateResult::Success) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Category updated successfully to '%s'.", gameName.toStdString().c_str());
		this->lastSetCategoryName = gameName;
		setCooldown();
		emit categoryUpdateFinished(true, gameName);
	} else {
		QString errorMsg = (result == TwitchAuthManager::UpdateResult::AuthError)
					   ? obs_module_text("Dock.CategoryUpdateFailed.AuthError")
					   : obs_module_text("Dock.CategoryUpdateFailed");
		blog(LOG_ERROR, "[GameDetector/ChatBot] Failed to update category for '%s'. Reason: %s", gameName.toStdString().c_str(), errorMsg.toStdString().c_str());
		emit categoryUpdateFinished(false, gameName, errorMsg);
	}
}

void TwitchChatBot::onChatMessageSent()
{
	QString message = chatMessageWatcher->property("message").toString();
	if (chatMessageWatcher->result()) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Message sent via API: %s", message.toStdString().c_str());
		setCooldown();
	} else {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Failed to send message via API.");
	}
}

void TwitchChatBot::setCooldown()
{
	int delaySeconds = ConfigManager::get().getTwitchActionDelay();
	if (delaySeconds > 0) {
		onCooldown = true;
		cooldownTimer->start(delaySeconds * 1000);
		emit cooldownStarted(delaySeconds);
	}
}

bool TwitchChatBot::isOnCooldown() const
{
	return onCooldown;
}

int TwitchChatBot::getCooldownRemaining() const
{
	return onCooldown ? cooldownTimer->remainingTime() / 1000 : 0;
}

void TwitchChatBot::setLastSetCategory(const QString &categoryName)
{
	this->lastSetCategoryName = categoryName;
}

QString TwitchChatBot::getLastSetCategory() const
{
	return lastSetCategoryName;
}