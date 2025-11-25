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
	// Inicializa os watchers como membros da classe
	gameIdWatcher = new QFutureWatcher<QString>(this);
	categoryUpdateWatcher = new QFutureWatcher<void *>(this);
	chatMessageWatcher = new QFutureWatcher<bool>(this);

	// Conecta os sinais dos watchers a slots dedicados
	connect(gameIdWatcher, &QFutureWatcher<QString>::finished, this, &TwitchChatBot::onGameIdReceived);
	connect(categoryUpdateWatcher, &QFutureWatcher<void *>::finished, this, &TwitchChatBot::onCategoryUpdateCompleted);
	connect(chatMessageWatcher, &QFutureWatcher<bool>::finished, this, &TwitchChatBot::onChatMessageSent);
}

TwitchChatBot::~TwitchChatBot()
{
	// Destrutor agora está vazio.
}

void TwitchChatBot::sendChatMessage(const QString &message)
{
	if (chatMessageWatcher->isRunning()) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Chat message sending already in progress. Ignoring new request.");
		return;
	}

	QString broadcasterId = ConfigManager::get().getUserId();
	QString senderId = broadcasterId; // A mensagem é enviada pelo próprio dono do canal

	if (broadcasterId.isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Attempt to send message without authentication.");
		emit authenticationRequired();
		return;
	}

	// Armazena a mensagem para usar no slot de conclusão
	chatMessageWatcher->setProperty("message", message);

	QFuture<bool> future = TwitchAuthManager::get().sendChatMessage(broadcasterId, senderId, message);
	chatMessageWatcher->setFuture(future);
}

bool TwitchChatBot::updateCategory(const QString &gameName)
{
	blog(LOG_INFO, "[GameDetector/ChatBot] Changing category to: %s", gameName.toStdString().c_str());
	if (lastSetCategoryName == gameName) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Category '%s' is already set. Skipping update.", gameName.toStdString().c_str());
		// emit categoryUpdateFinished(true, gameName); // Notifica a UI que "deu certo"
		return true;
	}

	// --------------------------
	if (gameIdWatcher->isRunning() || categoryUpdateWatcher->isRunning()) {
		blog(LOG_INFO, "[GameDetector/ChatBot] Category update already in progress. Ignoring new request.");
		return false;
	}
	// --------------------------
	// 1. Pede ao AuthManager o gameID
	// --------------------------

	if (TwitchAuthManager::get().getUserId().isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Attempt to change category without authentication.");
		emit authenticationRequired();
		return false;
	}

	// Armazena o nome do jogo para usar nos slots de conclusão
	gameIdWatcher->setProperty("gameName", gameName);

	QFuture<QString> gameIdFuture = TwitchAuthManager::get().getGameId(gameName);
	gameIdWatcher->setFuture(gameIdFuture);

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

	// Armazena o nome do jogo para o próximo passo
	categoryUpdateWatcher->setProperty("gameName", gameName);

	// Inicia a atualização da categoria
	QFuture<TwitchAuthManager::UpdateResult> updateFuture = TwitchAuthManager::get().updateChannelCategory(gameId);

	// O QFutureWatcher não pode observar um QFuture<enum>, então usamos um truque
	// com QFuture<void*> e esperamos o resultado em um contexto diferente.
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
	} else {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Failed to send message via API.");
	}
}
