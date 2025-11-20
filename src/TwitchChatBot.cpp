#include "TwitchChatBot.h"
#include "TwitchAuthManager.h"
#include "ConfigManager.h"

#include <QJsonDocument>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QJsonArray>
#include <obs-module.h>

#include <QTimer>
#include <QDebug>

TwitchChatBot::TwitchChatBot()
{
}

TwitchChatBot::~TwitchChatBot()
{
	// Destrutor agora está vazio.
}

void TwitchChatBot::sendChatMessage(const QString &message)
{
	QString broadcasterId = ConfigManager::get().getUserId();
	QString senderId = broadcasterId; // A mensagem é enviada pelo próprio dono do canal

	if (broadcasterId.isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Tentativa de enviar mensagem sem autenticação.");
		emit authenticationRequired();
		return;
	}

	QFuture<bool> future = TwitchAuthManager::get().sendChatMessage(broadcasterId, senderId, message);

	// Usa um watcher local que se autodestruirá
	QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
	connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, message]() {
		if (watcher->result()) {
			blog(LOG_INFO, "[GameDetector/ChatBot] Mensagem enviada via API: %s", message.toStdString().c_str());
		} else {
			blog(LOG_WARNING, "[GameDetector/ChatBot] Falha ao enviar mensagem via API.");
		}
		watcher->deleteLater();
	});
	watcher->setFuture(future);
}

bool TwitchChatBot::updateCategory(const QString &gameName)
{
	blog(LOG_INFO, "[GameDetector/ChatBot] Alterando categoria para: %s", gameName.toStdString().c_str());

	// --------------------------
	// 1. Pede ao AuthManager o gameID
	// --------------------------

	if (TwitchAuthManager::get().getUserId().isEmpty()) {
		blog(LOG_WARNING, "[GameDetector/ChatBot] Tentativa de alterar categoria sem autenticação.");
		emit authenticationRequired();
		return false;
	}

	QFuture<QString> gameIdFuture = TwitchAuthManager::get().getGameId(gameName);

	// Usamos um watcher para processar o resultado quando estiver pronto
	QFutureWatcher<QString> *gameIdWatcher = new QFutureWatcher<QString>();
	connect(gameIdWatcher, &QFutureWatcher<QString>::finished, this, [this, gameIdWatcher, gameName]() {
		QString gameId = gameIdWatcher->result();

		if (gameId.isEmpty()) {
			blog(LOG_ERROR, "[GameDetector/ChatBot] GameID não encontrado para '%s'.", gameName.toStdString().c_str());
			emit categoryUpdateFinished(false, gameName, obs_module_text("Dock.CategoryUpdateFailed.GameNotFound"));
			return;
		}

		// --------------------------------------------------
		// 2. Atualiza categoria pelo AuthManager (APENAS ELE)
		// --------------------------------------------------
		QFuture<bool> updateFuture = TwitchAuthManager::get().updateChannelCategory(gameId);

		// Criamos um novo watcher para o segundo passo
		QFutureWatcher<bool> *updateWatcher = new QFutureWatcher<bool>();
		connect(updateWatcher, &QFutureWatcher<bool>::finished, this, [this, updateWatcher, gameName]() {
			if (updateWatcher->result()) {
				blog(LOG_INFO, "[GameDetector/ChatBot] Categoria atualizada com sucesso para '%s'.", gameName.toStdString().c_str());
				emit categoryUpdateFinished(true, gameName);
			} else {
				blog(LOG_ERROR, "[GameDetector/ChatBot] Falha ao atualizar categoria para '%s'.", gameName.toStdString().c_str());
				emit categoryUpdateFinished(false, gameName, obs_module_text("Dock.CategoryUpdateFailed"));
			}
			updateWatcher->deleteLater(); // Limpa o watcher auxiliar
		});
		updateWatcher->setFuture(updateFuture);
	});
	// Deleta o watcher do gameId após a conclusão, independentemente do resultado
	connect(gameIdWatcher, &QFutureWatcher<QString>::finished, gameIdWatcher, &QObject::deleteLater);
	gameIdWatcher->setFuture(gameIdFuture);

	return true;
}
