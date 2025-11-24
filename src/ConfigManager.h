#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <obs.h>
#include <QObject>
#include <QString>

class ConfigManager : public QObject {
	Q_OBJECT

private:
	// ---- CHAVES EXISTENTES ----
	static constexpr const char *TOKEN_KEY = "twitch_access_token";
	static constexpr const char *REFRESH_TOKEN_KEY = "twitch_refresh_token";
	static constexpr const char *USER_ID_KEY = "twitch_user_id";
	static constexpr const char *COMMAND_KEY = "twitch_command_message";
	static constexpr const char *MANUAL_GAMES_KEY = "manual_games_list";
	static constexpr const char *COMMAND_NO_GAME_KEY = "twitch_command_no_game";
	static constexpr const char *EXECUTE_AUTOMATICALLY_KEY = "execute_automatically";
	static constexpr const char *TWITCH_ACTION_MODE_KEY = "twitch_action_mode";

	// ---- CHAVES DE ESCANEAMENTO ----
	static constexpr const char *SCAN_STEAM_KEY = "scan_steam";
	static constexpr const char *SCAN_EPIC_KEY = "scan_epic";
	static constexpr const char *SCAN_GOG_KEY = "scan_gog";
	static constexpr const char *SCAN_UBISOFT_KEY = "scan_ubisoft";

	// ---- NOVA CHAVE ----
	static constexpr const char *TWITCH_CHANNEL_LOGIN_KEY = "twitch_channel_login";

	obs_data_t *settings = nullptr;

	explicit ConfigManager(QObject *parent = nullptr);

public:
	static ConfigManager &get();

	// ---- LOAD & SAVE ----
	void load();
	void setSettings(obs_data_t *settings_data);
	void save(const QString &token, const QString &command);
	void save(obs_data_t *settings);
	void saveManualGames(obs_data_array_t *gamesArray);

	// ---- GETTERS ----
	obs_data_t *getSettings() const;
	QString getToken() const;
	QString getRefreshToken() const;
	QString getUserId() const;
	QString getCommand() const;
	obs_data_array_t *getManualGames() const;
	QString getNoGameCommand() const;
	bool getExecuteAutomatically() const;
	int getTwitchActionMode() const;
	QString getTwitchChannelLogin() const;
	bool getScanSteam() const;
	bool getScanEpic() const;
	bool getScanGog() const;
	bool getScanUbisoft() const;

	// ---- SETTERS ----
	void setToken(const QString &value);
	void setRefreshToken(const QString &value);
	void setUserId(const QString &value);
	void setTwitchChannelLogin(const QString &value); // NOVO

signals:
	void settingsSaved();
};

#endif // CONFIGMANAGER_H
