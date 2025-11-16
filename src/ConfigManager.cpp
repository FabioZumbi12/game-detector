#include "ConfigManager.h"
#include <obs-data.h>
#include <obs-module.h>
#include <QFileInfo>
#include <QDir>

ConfigManager &ConfigManager::get()
{
	static ConfigManager instance;
	return instance;
}

ConfigManager::ConfigManager(QObject *parent) : QObject(parent)
{
}

void ConfigManager::load()
{
	this->settings = obs_data_create_from_json_file(obs_module_config_path("config.json"));
	if (!settings) {
		settings = obs_data_create();
		// Define o valor padrão quando um novo arquivo de configuração é criado
		obs_data_set_string(settings, COMMAND_KEY, "!setgame {game}");
		obs_data_set_string(settings, COMMAND_NO_GAME_KEY, "!setgame just chatting");
		blog(LOG_INFO, "[GameDetector] Arquivo de configuracao nao encontrado, criando um novo.");
	} else {
		blog(LOG_INFO, "[GameDetector] Configuracoes carregadas do arquivo.");
		// Garante que a chave exista, mesmo em configs antigas (para atualizações)
		if (!obs_data_has_user_value(settings, COMMAND_KEY)) {
			obs_data_set_string(settings, COMMAND_KEY, "!setgame {game}");
			blog(LOG_INFO, "[GameDetector] Chave de comando nao encontrada, definindo valor padrao.");
		}
		// Garante que a chave de client id exista
		if (!obs_data_has_user_value(settings, CLIENT_ID_KEY)) {
			obs_data_set_string(settings, CLIENT_ID_KEY, "");
		}
		// Garante que a lista de jogos manuais exista
		if (!obs_data_has_user_value(settings, MANUAL_GAMES_KEY)) {
			obs_data_array_t *empty_array = obs_data_array_create();
			obs_data_set_array(settings, MANUAL_GAMES_KEY, empty_array);
			obs_data_array_release(empty_array);
		}
		// Garante que a chave de comando "sem jogo" exista
		if (!obs_data_has_user_value(settings, COMMAND_NO_GAME_KEY)) {
			obs_data_set_string(settings, COMMAND_NO_GAME_KEY, "!setgame just chatting");
		}
		// Garante que a chave de execução automática exista
		if (!obs_data_has_user_value(settings, EXECUTE_AUTOMATICALLY_KEY)) {
			obs_data_set_bool(settings, EXECUTE_AUTOMATICALLY_KEY, false);
		}
	}
}

void ConfigManager::setSettings(obs_data_t *settings_data)
{
	this->settings = settings_data;
}

void ConfigManager::save(obs_data_t *settings)
{
	if (!settings) {
		blog(LOG_ERROR, "[GameDetector] Nao ha objeto de configuracao valido para salvar.");
		return;
	}

	const char *config_path_c = obs_module_config_path("config.json");
	if (config_path_c == nullptr) {
		blog(LOG_ERROR, "[GameDetector] Caminho de configuracao invalido.");
		return;
	}

	QString config_path_qstr = QString::fromUtf8(config_path_c);
	QFileInfo file_info(config_path_qstr);
	QDir dir = file_info.dir();

	if (!dir.exists()) {
		if (dir.mkpath(".")) {
			blog(LOG_INFO, "[GameDetector] Diretorio de configuracao criado em: %s", dir.path().toStdString().c_str());
		}
	}

	if (obs_data_save_json(settings, config_path_c)) {
		blog(LOG_INFO, "[GameDetector] Configuracoes salvas em: %s", config_path_c);
	} else {
		blog(LOG_WARNING, "[GameDetector] Falha ao salvar configuracoes em: %s", config_path_c);
	}
}

void ConfigManager::save(const QString &token, const QString &command)
{
	if (!settings) {
		blog(LOG_ERROR, "[GameDetector] Nao ha objeto de configuracao valido para salvar.");
		return;
	}

	obs_data_set_string(settings, TOKEN_KEY, token.toStdString().c_str());
	obs_data_set_string(settings, COMMAND_KEY, command.toStdString().c_str());

	this->save(settings);
}

void ConfigManager::saveManualGames(obs_data_array_t *gamesArray)
{
	obs_data_set_array(settings, MANUAL_GAMES_KEY, gamesArray);
	this->save(settings);
}

obs_data_t *ConfigManager::getSettings() const
{
	return settings;
}

QString ConfigManager::getToken() const
{
	if (!settings)
		return "";

	const char *token_str = obs_data_get_string(settings, TOKEN_KEY);
	if (!token_str)
		return "";

	QString token = QString::fromUtf8(token_str);
	return token;
}

QString ConfigManager::getClientId() const
{
	if (!settings)
		return "";

	const char *client_id_str = obs_data_get_string(settings, CLIENT_ID_KEY);
	if (!client_id_str)
		return "";

	return QString::fromUtf8(client_id_str);
}

QString ConfigManager::getCommand() const
{
	if (!settings)
		return "!setgame {game}";

	const char *command_str = obs_data_get_string(settings, COMMAND_KEY);
	if (!command_str)
		return "!setgame {game}";

	QString command = QString::fromUtf8(command_str);
	return command;
}

obs_data_array_t *ConfigManager::getManualGames() const
{
	if (!settings)
		return nullptr;

	return obs_data_get_array(settings, MANUAL_GAMES_KEY);
}

QString ConfigManager::getNoGameCommand() const
{
	if (!settings)
		return "!setgame just chatting";

	const char *command_str = obs_data_get_string(settings, COMMAND_NO_GAME_KEY);
	if (!command_str)
		return "!setgame just chatting";

	QString command = QString::fromUtf8(command_str);
	return command;
}

bool ConfigManager::getExecuteAutomatically() const
{
	if (!settings)
		return false;

	return obs_data_get_bool(settings, EXECUTE_AUTOMATICALLY_KEY);
}