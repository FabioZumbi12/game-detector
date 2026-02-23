#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QObject>
#include <QCoreApplication>
#include <obs-module.h>

#include "ConfigManager.h"
#include "GameDetector.h"

#include "GameDetectorSettingsDialog.h"
#include "GameDetectorDock.h"
#include "PlatformManager.h"
#include "TwitchAuthManager.h"

static obs_hotkey_id g_set_game_hotkey_id;
static obs_hotkey_id g_rescan_games_hotkey_id;
static obs_hotkey_id g_set_just_chatting_hotkey_id;
static GameDetectorDock *g_dock_widget = nullptr;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("GameDetector", "en-US")

static void open_settings_dialog(void *private_data)
{
	Q_UNUSED(private_data);
	GameDetectorSettingsDialog dialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	dialog.exec();
}

static void set_game_hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);

	if (pressed && g_dock_widget) {
		g_dock_widget->onExecuteCommandClicked();
	}
}

static void set_just_chatting_hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);

	if (pressed && g_dock_widget) {
		g_dock_widget->onSetJustChattingClicked();
	}
}

static void rescan_games_hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);

	if (pressed) {
		blog(LOG_INFO, "[GameDetector] Hotkey 'Rescan Games' pressed.");
		bool scanSteam = ConfigManager::get().getScanSteam();
		bool scanEpic = ConfigManager::get().getScanEpic();
		bool scanGog = ConfigManager::get().getScanGog();
		bool scanUbisoft = ConfigManager::get().getScanUbisoft();

		GameDetector::get().rescanForGames(scanSteam, scanEpic, scanGog, scanUbisoft);
		auto conn = std::make_shared<QMetaObject::Connection>();
		*conn = QObject::connect(&GameDetector::get(), &GameDetector::automaticScanFinished,
					 [conn](const QList<std::tuple<QString, QString, QString>> &foundGames) {
						 GameDetector::get().mergeAndSaveGames(foundGames);
						 GameDetector::get().loadGamesFromConfig();
						 QObject::disconnect(*conn);
					 });
	}
}

static void save_hotkeys(obs_data_t *save_data, bool saving, void *private_data)
{
	Q_UNUSED(save_data);
	Q_UNUSED(saving);
	Q_UNUSED(private_data);

	obs_data_array_t *set_game_hotkey_data = obs_hotkey_save(g_set_game_hotkey_id);
	ConfigManager::get().setHotkeyData(ConfigManager::HOTKEY_SET_GAME_KEY, set_game_hotkey_data);
	obs_data_array_release(set_game_hotkey_data);

	obs_data_array_t *rescan_games_hotkey_data = obs_hotkey_save(g_rescan_games_hotkey_id);
	ConfigManager::get().setHotkeyData(ConfigManager::HOTKEY_RESCAN_GAMES_KEY, rescan_games_hotkey_data);
	obs_data_array_release(rescan_games_hotkey_data);

	obs_data_array_t *set_jc_hotkey_data = obs_hotkey_save(g_set_just_chatting_hotkey_id);
	ConfigManager::get().setHotkeyData(ConfigManager::HOTKEY_SET_JUST_CHATTING_KEY, set_jc_hotkey_data);
	obs_data_array_release(set_jc_hotkey_data);
}

static GameDetectorDock *get_dock()
{
	return g_dock_widget;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[GameDetector] Plugin loaded.");

	ConfigManager::get().load();
	TwitchAuthManager::get().loadToken();

	GameDetectorDock *dockWidget = new GameDetectorDock();
	obs_frontend_add_dock_by_id("game_detector", "Game Detector", dockWidget);
	g_dock_widget = dockWidget;
	blog(LOG_INFO, "[GameDetector] Dock registered.");

	obs_frontend_add_tools_menu_item(obs_module_text("Settings.ToolsMenu"), open_settings_dialog, nullptr);
	blog(LOG_INFO, "[GameDetector] Tools menu item added.");

	obs_frontend_add_save_callback(save_hotkeys, nullptr);

	g_set_game_hotkey_id = obs_hotkey_register_frontend(ConfigManager::HOTKEY_SET_GAME_KEY,
							    obs_module_text("Hotkey.SetGame"), set_game_hotkey_callback,
							    nullptr);
	blog(LOG_INFO, "[GameDetector] Hotkey 'Set Game' registered with ID: %d", g_set_game_hotkey_id);

	g_set_just_chatting_hotkey_id = obs_hotkey_register_frontend(ConfigManager::HOTKEY_SET_JUST_CHATTING_KEY,
								     obs_module_text("Hotkey.SetJustChatting"),
								     set_just_chatting_hotkey_callback, nullptr);
	blog(LOG_INFO, "[GameDetector] Hotkey 'Set Just Chatting' registered with ID: %d",
	     g_set_just_chatting_hotkey_id);

	g_rescan_games_hotkey_id = obs_hotkey_register_frontend(ConfigManager::HOTKEY_RESCAN_GAMES_KEY,
								obs_module_text("Hotkey.RescanGames"),
								rescan_games_hotkey_callback, nullptr);
	blog(LOG_INFO, "[GameDetector] Hotkey 'Rescan Games' registered with ID: %d", g_rescan_games_hotkey_id);

	obs_data_array_t *set_game_hotkey_data = ConfigManager::get().getHotkeyData(ConfigManager::HOTKEY_SET_GAME_KEY);
	obs_hotkey_load(g_set_game_hotkey_id, set_game_hotkey_data);
	obs_data_array_release(set_game_hotkey_data);

	obs_data_array_t *set_jc_hotkey_data =
		ConfigManager::get().getHotkeyData(ConfigManager::HOTKEY_SET_JUST_CHATTING_KEY);
	obs_hotkey_load(g_set_just_chatting_hotkey_id, set_jc_hotkey_data);
	obs_data_array_release(set_jc_hotkey_data);

	obs_data_array_t *rescan_games_hotkey_data =
		ConfigManager::get().getHotkeyData(ConfigManager::HOTKEY_RESCAN_GAMES_KEY);
	obs_hotkey_load(g_rescan_games_hotkey_id, rescan_games_hotkey_data);
	obs_data_array_release(rescan_games_hotkey_data);

	get_dock()->loadSettingsFromConfig();
	blog(LOG_INFO, "[GameDetector] Config file path: %s", obs_module_config_path("config.json"));

	GameDetector::get().loadGamesFromConfig();
	GameDetector::get().startScanning();
	GameDetector::get().setupPeriodicScan();
	if (ConfigManager::get().getScanOnStartup()) {
		blog(LOG_INFO, "[GameDetector] Performing scan on startup.");
		bool scanSteam = ConfigManager::get().getScanSteam();
		bool scanEpic = ConfigManager::get().getScanEpic();
		bool scanGog = ConfigManager::get().getScanGog();
		bool scanUbisoft = ConfigManager::get().getScanUbisoft();
		GameDetector::get().rescanForGames(scanSteam, scanEpic, scanGog, scanUbisoft);
		auto conn = std::make_shared<QMetaObject::Connection>();
		*conn = QObject::connect(&GameDetector::get(), &GameDetector::automaticScanFinished,
					 [conn](const QList<std::tuple<QString, QString, QString>> &foundGames) {
						 GameDetector::get().mergeAndSaveGames(foundGames);
						 GameDetector::get().loadGamesFromConfig();
						 QObject::disconnect(*conn);
					 });
	}

	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(save_hotkeys, nullptr);

	obs_hotkey_unregister(g_set_game_hotkey_id);
	obs_hotkey_unregister(g_rescan_games_hotkey_id);
	obs_hotkey_unregister(g_set_just_chatting_hotkey_id);

	GameDetector::get().stopScanning();
	TwitchAuthManager::get().shutdown();
	PlatformManager::get().shutdown();
	ConfigManager::get().save(ConfigManager::get().getSettings());
	ConfigManager::get().shutdown();

	if (g_dock_widget) {
		obs_frontend_remove_dock("game_detector");
		QMetaObject::invokeMethod(g_dock_widget, "deleteLater", Qt::QueuedConnection);
		QCoreApplication::processEvents();
		g_dock_widget = nullptr;
	}

	blog(LOG_INFO, "[GameDetector] Plugin unloaded.");
}
