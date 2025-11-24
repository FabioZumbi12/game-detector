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

ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {}

// -------------------------------------------------------------------
// LOAD
// -------------------------------------------------------------------
void ConfigManager::load()
{
	this->settings = obs_data_create_from_json_file(obs_module_config_path("config.json"));

	if (!settings) {
		blog(LOG_INFO, "[GameDetector] No config found. Creating new one...");
		settings = obs_data_create();

		// Defaults
		obs_data_set_string(settings, COMMAND_KEY, "!setgame {game}");
		obs_data_set_string(settings, COMMAND_NO_GAME_KEY, "!setgame just chatting");
		obs_data_set_bool(settings, EXECUTE_AUTOMATICALLY_KEY, false);
		obs_data_set_int(settings, TWITCH_ACTION_MODE_KEY, 0);
		obs_data_set_bool(settings, SCAN_STEAM_KEY, true); // Padrão
		obs_data_set_bool(settings, SCAN_EPIC_KEY, true);  // Padrão
		obs_data_set_bool(settings, SCAN_GOG_KEY, true);   // Padrão
		obs_data_set_bool(settings, SCAN_UBISOFT_KEY, true); // Padrão
		obs_data_set_string(settings, TWITCH_CHANNEL_LOGIN_KEY, "");

		obs_data_array_t *empty_array = obs_data_array_create();
		obs_data_set_array(settings, MANUAL_GAMES_KEY, empty_array);
		obs_data_array_release(empty_array);

		return;
	}

	blog(LOG_INFO, "[GameDetector] Settings loaded.");

	// ---------------------------
	// Garantir chaves existentes
	// ---------------------------
	if (!obs_data_has_user_value(settings, COMMAND_KEY))
		obs_data_set_string(settings, COMMAND_KEY, "!setgame {game}");

	if (!obs_data_has_user_value(settings, COMMAND_NO_GAME_KEY))
		obs_data_set_string(settings, COMMAND_NO_GAME_KEY, "!setgame just chatting");

	if (!obs_data_has_user_value(settings, REFRESH_TOKEN_KEY))
		obs_data_set_string(settings, REFRESH_TOKEN_KEY, "");

	if (!obs_data_has_user_value(settings, USER_ID_KEY))
		obs_data_set_string(settings, USER_ID_KEY, "");

	if (!obs_data_has_user_value(settings, TOKEN_KEY))
		obs_data_set_string(settings, TOKEN_KEY, "");

	if (!obs_data_has_user_value(settings, EXECUTE_AUTOMATICALLY_KEY))
		obs_data_set_bool(settings, EXECUTE_AUTOMATICALLY_KEY, false);

	if (!obs_data_has_user_value(settings, TWITCH_ACTION_MODE_KEY))
		obs_data_set_int(settings, TWITCH_ACTION_MODE_KEY, 0);

	if (!obs_data_has_user_value(settings, SCAN_STEAM_KEY))
		obs_data_set_bool(settings, SCAN_STEAM_KEY, true);

	if (!obs_data_has_user_value(settings, SCAN_EPIC_KEY))
		obs_data_set_bool(settings, SCAN_EPIC_KEY, true);

	if (!obs_data_has_user_value(settings, SCAN_GOG_KEY))
		obs_data_set_bool(settings, SCAN_GOG_KEY, true);

	if (!obs_data_has_user_value(settings, SCAN_UBISOFT_KEY))
		obs_data_set_bool(settings, SCAN_UBISOFT_KEY, true);

	if (!obs_data_has_user_value(settings, TWITCH_CHANNEL_LOGIN_KEY))
		obs_data_set_string(settings, TWITCH_CHANNEL_LOGIN_KEY, "");

	if (!obs_data_has_user_value(settings, MANUAL_GAMES_KEY)) {
		obs_data_array_t *empty_array = obs_data_array_create();
		obs_data_set_array(settings, MANUAL_GAMES_KEY, empty_array);
		obs_data_array_release(empty_array);
	}
}

// -------------------------------------------------------------------
// SAVE
// -------------------------------------------------------------------
void ConfigManager::setSettings(obs_data_t *settings_data)
{
	this->settings = settings_data;
}

void ConfigManager::save(obs_data_t *data)
{
	if (!data) {
		blog(LOG_ERROR, "[GameDetector] Attempt to save null config.");
		return;
	}

	const char *config_path_c = obs_module_config_path("config.json");
	if (!config_path_c) {
		blog(LOG_ERROR, "[GameDetector] Invalid path when saving config.");
		return;
	}

	QString path = QString::fromUtf8(config_path_c);
	QFileInfo info(path);
	QDir dir = info.dir();
	
	if (!dir.exists())
		dir.mkpath(".");

	if (obs_data_save_json(data, config_path_c)) {
		blog(LOG_INFO, "[GameDetector] Config salva em: %s", config_path_c);
		emit settingsSaved(); // Notifica que as configurações foram salvas
	} else {
		blog(LOG_WARNING, "[GameDetector] Failed to save config to: %s", config_path_c);
	}
}

void ConfigManager::save(const QString &token, const QString &command)
{
	if (!settings)
		return;

	obs_data_set_string(settings, TOKEN_KEY, token.toUtf8().constData());
	obs_data_set_string(settings, COMMAND_KEY, command.toUtf8().constData());

	save(settings);
}

void ConfigManager::saveManualGames(obs_data_array_t *gamesArray)
{
	if (!settings)
		return;

	obs_data_set_array(settings, MANUAL_GAMES_KEY, gamesArray);
	save(settings);
}

// -------------------------------------------------------------------
// GETTERS
// -------------------------------------------------------------------
obs_data_t *ConfigManager::getSettings() const
{
	return settings;
}

QString ConfigManager::getToken() const
{
	if (!settings)
		return "";
	return QString::fromUtf8(obs_data_get_string(settings, TOKEN_KEY));
}

QString ConfigManager::getRefreshToken() const
{
	if (!settings)
		return "";
	return QString::fromUtf8(obs_data_get_string(settings, REFRESH_TOKEN_KEY));
}

QString ConfigManager::getUserId() const
{
	if (!settings)
		return "";
	return QString::fromUtf8(obs_data_get_string(settings, USER_ID_KEY));
}

QString ConfigManager::getCommand() const
{
	if (!settings)
		return "!setgame {game}";
	return QString::fromUtf8(obs_data_get_string(settings, COMMAND_KEY));
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
	return QString::fromUtf8(obs_data_get_string(settings, COMMAND_NO_GAME_KEY));
}

bool ConfigManager::getExecuteAutomatically() const
{
	if (!settings)
		return false;
	return obs_data_get_bool(settings, EXECUTE_AUTOMATICALLY_KEY);
}

int ConfigManager::getTwitchActionMode() const
{
	if (!settings)
		return 0;
	return (int)obs_data_get_int(settings, TWITCH_ACTION_MODE_KEY);
}

QString ConfigManager::getTwitchChannelLogin() const
{
	if (!settings)
		return "";
	return QString::fromUtf8(obs_data_get_string(settings, TWITCH_CHANNEL_LOGIN_KEY));
}

bool ConfigManager::getScanSteam() const
{
	if (!settings)
		return true;
	return obs_data_get_bool(settings, SCAN_STEAM_KEY);
}

bool ConfigManager::getScanEpic() const
{
	if (!settings)
		return true;
	return obs_data_get_bool(settings, SCAN_EPIC_KEY);
}

bool ConfigManager::getScanGog() const
{
	if (!settings)
		return true;
	return obs_data_get_bool(settings, SCAN_GOG_KEY);
}

bool ConfigManager::getScanUbisoft() const
{
	if (!settings)
		return true;
	return obs_data_get_bool(settings, SCAN_UBISOFT_KEY);
}

// -------------------------------------------------------------------
// SETTERS
// -------------------------------------------------------------------
void ConfigManager::setToken(const QString &value)
{
	obs_data_set_string(settings, TOKEN_KEY, value.toUtf8().constData());
	save(settings);
}

void ConfigManager::setRefreshToken(const QString &value)
{
	obs_data_set_string(settings, REFRESH_TOKEN_KEY, value.toUtf8().constData());
	save(settings);
}

void ConfigManager::setUserId(const QString &value)
{
	obs_data_set_string(settings, USER_ID_KEY, value.toUtf8().constData());
	save(settings);
}

void ConfigManager::setTwitchChannelLogin(const QString &value)
{
	obs_data_set_string(settings, TWITCH_CHANNEL_LOGIN_KEY, value.toUtf8().constData());
	save(settings);
}
