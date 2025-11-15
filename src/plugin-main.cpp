#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QDockWidget>
#include <obs-module.h>

#include "ConfigManager.h"
#include "GameDetector.h"

#include "GameDetectorSettingsDialog.h"
#include "GameDetectorDock.h"

static GameDetectorDock *g_dock_widget = nullptr;
static GameDetectorSettingsDialog *g_settings_dialog = nullptr;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("OBSGameDetector", "en-US")

static void open_settings_dialog(void *private_data)
{
	Q_UNUSED(private_data);

	if (!g_settings_dialog) {
		g_settings_dialog =
			new GameDetectorSettingsDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	}

	g_settings_dialog->show();
	g_settings_dialog->raise();
}

static GameDetectorDock* get_dock()
{
	return g_dock_widget;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[OBSGameDetector] Plugin carregado.");

	// Dock
	GameDetectorDock *dockWidget = new GameDetectorDock();
	obs_frontend_add_dock_by_id("obs_game_detector", "Game Detector", dockWidget);
	g_dock_widget = dockWidget;
	blog(LOG_INFO, "[OBSGameDetector] Dock registrado.");

	obs_frontend_add_tools_menu_item("Configurações do Game Detector", open_settings_dialog, nullptr);
	blog(LOG_INFO, "[OBSGameDetector] Item de menu de ferramentas adicionado.");

	ConfigManager::get().load();
	get_dock()->loadSettingsFromConfig();
	blog(LOG_INFO, "[OBSGameDetector] Caminho do arquivo de config: %s", obs_module_config_path("config.json"));

	// Carrega a lista de jogos do arquivo de config e inicia o monitoramento de processos
	GameDetector::get().loadGamesFromConfig();
	GameDetector::get().startScanning();

	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[OBSGameDetector] Plugin descarregado.");

	// Para o detector de jogos
	GameDetector::get().stopScanning();

	// Salva as configurações ao descarregar o plugin
	obs_data_t *settings = ConfigManager::get().getSettings();
	if (settings) {
		ConfigManager::get().save(settings);
		obs_data_release(settings);
	}
}