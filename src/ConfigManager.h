#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <obs.h>
#include <QObject>
#include <QString>

class ConfigManager : public QObject {
	Q_OBJECT

private:
	static constexpr const char *TWITCH_TOKEN_KEY = "twitch_access_token";
	static constexpr const char *TWITCH_REFRESH_TOKEN_KEY = "twitch_refresh_token";
	static constexpr const char *TWITCH_USER_ID_KEY = "twitch_user_id";
	static constexpr const char *TROVO_TOKEN_KEY = "trovo_access_token";
	static constexpr const char *TROVO_USER_ID_KEY = "trovo_user_id";
	static constexpr const char *TROVO_CHANNEL_LOGIN_KEY = "trovo_channel_login";
	static constexpr const char *COMMAND_KEY = "twitch_command_message";
	static constexpr const char *COMMAND_NO_GAME_KEY = "twitch_command_no_game";
	static constexpr const char *EXECUTE_AUTOMATICALLY_KEY = "execute_automatically";
	static constexpr const char *ACTION_MODE_KEY = "twitch_action_mode";
	static constexpr const char *TWITCH_UNIFIED_AUTH_KEY = "twitch_unified_auth";
	static constexpr const char *TWITCH_CHANNEL_LOGIN_KEY = "twitch_channel_login";

	obs_data_t *settings = nullptr;

	explicit ConfigManager(QObject *parent = nullptr);

public:
	static constexpr const char *HOTKEY_SET_GAME_KEY = "hotkey_set_game";
	static constexpr const char *HOTKEY_RESCAN_GAMES_KEY = "hotkey_rescan_games";

	static ConfigManager &get();

	void load();
	void save(obs_data_t *settings);
	void saveManualGames(obs_data_array_t *gamesArray);
	obs_data_t *getSettings() const;
	obs_data_array_t *getHotkeyData(const char *key) const;
	void setHotkeyData(const char *key, obs_data_array_t *hotkeyArray);
	QString getTwitchToken() const;
	QString getTwitchRefreshToken() const;
	QString getTwitchUserId() const;
	QString getTrovoToken() const;
	QString getTrovoUserId() const;
	QString getTrovoChannelLogin() const;
	QString getCommand() const;
	obs_data_array_t *getManualGames() const;
	QString getNoGameCommand() const;
	bool getExecuteAutomatically() const;
	int getActionMode() const;
	QString getTwitchChannelLogin() const;
	bool getUnifiedAuth() const;
	bool getScanSteam() const;
	bool getScanEpic() const;
	bool getScanGog() const;
	bool getScanUbisoft() const;
	bool getScanOnStartup() const;
	bool getScanPeriodically() const;
	int getScanPeriodicallyInterval() const;

	void setTwitchToken(const QString &value);
	void setTwitchRefreshToken(const QString &value);
	void setTwitchUserId(const QString &value);
	void setTrovoToken(const QString &value);
	void setTrovoUserId(const QString &value);
	void setTrovoChannelLogin(const QString &value);
	void setTwitchChannelLogin(const QString &value);
	int getActionDelay() const;


	static constexpr const char *SCAN_STEAM_KEY = "scan_steam";
	static constexpr const char *SCAN_EPIC_KEY = "scan_epic";
	static constexpr const char *SCAN_GOG_KEY = "scan_gog";
	static constexpr const char *SCAN_UBISOFT_KEY = "scan_ubisoft";
	static constexpr const char *MANUAL_GAMES_KEY = "manual_games_list";
	static constexpr const char *SCAN_ON_STARTUP_KEY = "scan_on_startup";
	static constexpr const char *SCAN_PERIODICALLY_KEY = "scan_periodically";
	static constexpr const char *SCAN_PERIODICALLY_INTERVAL_KEY = "scan_periodically_interval";
	static constexpr const char *ACTION_DELAY_KEY = "twitch_action_delay";

signals:
	void settingsSaved();
};

#endif // CONFIGMANAGER_H
