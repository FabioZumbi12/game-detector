#include "GameDetectorSettingsDialog.h"
#include "GameDetector.h"
#include "GameListDialog.h"
#include "IconProvider.h"
#include "ConfigManager.h"
#include "TwitchAuthManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <obs.h>
#include <obs-module.h>
#include <obs-data.h>

GameDetectorSettingsDialog::GameDetectorSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(obs_module_text("Settings.WindowTitle"));
	setMinimumSize(800, 450);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	mainLayout->addWidget(new QLabel(obs_module_text("Settings.Header")));
	QLabel *headerLabel = new QLabel(obs_module_text("Settings.Description"));
	headerLabel->setWordWrap(true);
	mainLayout->addWidget(headerLabel);

	QGroupBox *scanGroup = new QGroupBox(obs_module_text("Settings.ScanOptions"));
	QVBoxLayout *scanLayout = new QVBoxLayout();

	QLabel *scanLabel = new QLabel(obs_module_text("Settings.ScanFrom"));
	scanLayout->addWidget(scanLabel);

	QVBoxLayout *scanOptionsLayout = new QVBoxLayout();
	scanOptionsLayout->setContentsMargins(20, 5, 10, 5);
	scanSteamCheckbox = new QCheckBox("Steam");
	scanEpicCheckbox = new QCheckBox("Epic Games");
	scanGogCheckbox = new QCheckBox("GOG Galaxy");
	scanUbiCheckbox = new QCheckBox("Ubisoft Connect");
	scanOptionsLayout->addWidget(scanSteamCheckbox);
	scanOptionsLayout->addWidget(scanEpicCheckbox);
	scanOptionsLayout->addWidget(scanGogCheckbox);
	scanOptionsLayout->addWidget(scanUbiCheckbox);
	scanLayout->addLayout(scanOptionsLayout);

	scanOnStartupCheckbox = new QCheckBox(obs_module_text("Settings.ScanOnStartup"));
	scanLayout->addWidget(scanOnStartupCheckbox);

	QHBoxLayout *autoScanLayout = new QHBoxLayout();
	scanPeriodicallyCheckbox = new QCheckBox(obs_module_text("Settings.ScanPeriodically"));
	autoScanLayout->addWidget(scanPeriodicallyCheckbox);
	scanIntervalSpinbox = new QSpinBox();
	scanIntervalSpinbox->setRange(1, 10080);
	scanIntervalSpinbox->setSuffix(obs_module_text("Settings.Minutes"));
	autoScanLayout->addWidget(scanIntervalSpinbox);
	autoScanLayout->addStretch(1);
	scanLayout->addLayout(autoScanLayout);

	manageGamesButton = new QPushButton(obs_module_text("Settings.ManageGames"));
	scanLayout->addWidget(manageGamesButton);

	scanGroup->setLayout(scanLayout);
	mainLayout->addWidget(scanGroup);

	QGroupBox *authGroup = new QGroupBox(obs_module_text("Settings.TwitchConnection"));
	QVBoxLayout *authLayout = new QVBoxLayout();

	authStatusLabel = new QLabel(obs_module_text("Auth.NotConnected"));
	authLayout->addWidget(authStatusLabel);

	QHBoxLayout *authButtonsLayout = new QHBoxLayout();
	authButton = new QPushButton(obs_module_text("Auth.Connect"));
	authButtonsLayout->addWidget(authButton);

	disconnectButton = new QPushButton(obs_module_text("Auth.Disconnect"));
	authButtonsLayout->addWidget(disconnectButton);
	authButtonsLayout->addStretch(1);
	authLayout->addLayout(authButtonsLayout);

	QLabel *helpLabel = new QLabel(obs_module_text("Auth.HelpText"));
	helpLabel->setWordWrap(true);
	authLayout->addWidget(helpLabel);
	authLayout->addStretch(1);

	authGroup->setLayout(authLayout);

	QGroupBox *twitchActionGroup = new QGroupBox(obs_module_text("Settings.TwitchAction"));

	QFormLayout *twitchActionComboBoxLayout = new QFormLayout();
	twitchActionComboBox = new QComboBox();
	twitchActionComboBox->addItem(obs_module_text("Settings.TwitchAction.SendCommand"), 0);
	twitchActionComboBox->addItem(obs_module_text("Settings.TwitchAction.ChangeCategory"), 1);
	twitchActionComboBoxLayout->addWidget(twitchActionComboBox);
	
	unifiedAuthCheckbox = new QCheckBox(obs_module_text("Settings.TwitchAction.UnifiedAuth"));
	unifiedAuthCheckbox->setToolTip(obs_module_text("Settings.TwitchAction.UnifiedAuth.Tooltip"));
	twitchActionComboBoxLayout->addWidget(unifiedAuthCheckbox);

	QFormLayout *twitchActionLayout = new QFormLayout();
	twitchActionLayout->addRow(twitchActionComboBoxLayout);

	commandLabel = new QLabel(obs_module_text("Settings.Command.GameDetected"));
	commandInput = new QLineEdit();
	commandInput->setPlaceholderText(obs_module_text("Settings.Command.GameDetected.Placeholder"));
	twitchActionLayout->addRow(commandLabel, commandInput);

	noGameCommandLabel = new QLabel(obs_module_text("Settings.Command.NoGame"));
	noGameCommandInput = new QLineEdit();
	noGameCommandInput->setPlaceholderText(obs_module_text("Settings.Command.NoGame.Placeholder"));
	twitchActionLayout->addRow(noGameCommandLabel, noGameCommandInput);

	QHBoxLayout *delayLayout = new QHBoxLayout();
	delayLabel = new QLabel(obs_module_text("Settings.ActionDelay"));
	delaySpinBox = new QSpinBox();
	delaySpinBox->setRange(5, 300);
	delaySpinBox->setSuffix(obs_module_text("Settings.Seconds"));
	twitchActionLayout->addRow(delayLabel, delaySpinBox);

	connect(twitchActionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		disconectOnChangeComboBox(index);
	});
	connect(unifiedAuthCheckbox, &QCheckBox::checkStateChanged, this, [this](int state) {
		Q_UNUSED(state);
		disconectOnChangeComboBox(twitchActionComboBox->currentIndex());
	});

	twitchActionGroup->setLayout(twitchActionLayout);

	QHBoxLayout *authAndActionLayout = new QHBoxLayout();
	authAndActionLayout->addWidget(authGroup, 1);
	authAndActionLayout->addWidget(twitchActionGroup, 1);

	mainLayout->addLayout(authAndActionLayout);

	mainLayout->addStretch(1);

	QHBoxLayout *dialogButtonsLayout = new QHBoxLayout();
	QLabel *developerLabel = new QLabel(
		"<a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a>");
	developerLabel->setOpenExternalLinks(true);
	okButton = new QPushButton(obs_module_text("OK"));
	cancelButton = new QPushButton(obs_module_text("Cancel"));
	dialogButtonsLayout->addStretch(1);
	dialogButtonsLayout->addWidget(developerLabel);
	dialogButtonsLayout->addWidget(okButton);
	dialogButtonsLayout->addWidget(cancelButton);
	mainLayout->addLayout(dialogButtonsLayout);

	connect(manageGamesButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onManageGamesClicked);
	connect(authButton, &QPushButton::clicked, this, [this]() {
		TwitchAuthManager::get().startAuthentication(twitchActionComboBox->currentIndex());
	});
	connect(disconnectButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onDisconnectClicked);
	connect(&TwitchAuthManager::get(), &TwitchAuthManager::authenticationFinished, this, &GameDetectorSettingsDialog::onAuthenticationFinished);

	connect(okButton, &QPushButton::clicked, this, [this]() {
		saveSettings();
		accept();
	});
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	loadSettings();

	connect(scanPeriodicallyCheckbox, &QCheckBox::checkStateChanged, this, [this](int state) {
		scanIntervalSpinbox->setEnabled(state == Qt::Checked);
	});

}

void GameDetectorSettingsDialog::loadSettings()
{
	QString userId = ConfigManager::get().getUserId();
	QString loginName = ConfigManager::get().getTwitchChannelLogin();

	if (!userId.isEmpty()) {
		onAuthenticationFinished(true, loginName);
	} else {
		onAuthenticationFinished(false, "");
	}

	twitchActionComboBox->blockSignals(true);
	twitchActionComboBox->setCurrentIndex(ConfigManager::get().getTwitchActionMode());
	twitchActionComboBox->blockSignals(false);
	unifiedAuthCheckbox->setChecked(ConfigManager::get().getUnifiedAuth());
	commandInput->setText(ConfigManager::get().getCommand());
	noGameCommandInput->setText(ConfigManager::get().getNoGameCommand());
	delaySpinBox->setValue(ConfigManager::get().getTwitchActionDelay());
	updateActionModeUI(twitchActionComboBox->currentIndex());

	scanSteamCheckbox->setChecked(ConfigManager::get().getScanSteam());
	scanEpicCheckbox->setChecked(ConfigManager::get().getScanEpic());
	scanGogCheckbox->setChecked(ConfigManager::get().getScanGog());
	scanUbiCheckbox->setChecked(ConfigManager::get().getScanUbisoft());
	scanOnStartupCheckbox->setChecked(ConfigManager::get().getScanOnStartup());
	scanPeriodicallyCheckbox->setChecked(ConfigManager::get().getScanPeriodically());
	scanIntervalSpinbox->setValue(ConfigManager::get().getScanPeriodicallyInterval());
}

void GameDetectorSettingsDialog::saveSettings()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_set_int(settings, "twitch_action_mode", twitchActionComboBox->currentData().toInt());
	obs_data_set_bool(settings, "twitch_unified_auth", unifiedAuthCheckbox->isChecked());
	obs_data_set_string(settings, "twitch_command_message", commandInput->text().toStdString().c_str());
	obs_data_set_string(settings, "twitch_command_no_game", noGameCommandInput->text().toStdString().c_str());
	obs_data_set_int(settings, ConfigManager::TWITCH_ACTION_DELAY_KEY, delaySpinBox->value());

	obs_data_set_bool(settings, ConfigManager::SCAN_STEAM_KEY, scanSteamCheckbox->isChecked());
	obs_data_set_bool(settings, ConfigManager::SCAN_EPIC_KEY, scanEpicCheckbox->isChecked());
	obs_data_set_bool(settings, ConfigManager::SCAN_GOG_KEY, scanGogCheckbox->isChecked());
	obs_data_set_bool(settings, ConfigManager::SCAN_UBISOFT_KEY, scanUbiCheckbox->isChecked());
	obs_data_set_bool(settings, ConfigManager::SCAN_ON_STARTUP_KEY, scanOnStartupCheckbox->isChecked());
	obs_data_set_bool(settings, ConfigManager::SCAN_PERIODICALLY_KEY, scanPeriodicallyCheckbox->isChecked());
	obs_data_set_int(settings, ConfigManager::SCAN_PERIODICALLY_INTERVAL_KEY, scanIntervalSpinbox->value());

	ConfigManager::get().save(settings);

	GameDetector::get().onSettingsChanged();
	GameDetector::get().setupPeriodicScan();
}

void GameDetectorSettingsDialog::rescanGames()
{
	GameDetector::get().rescanForGames(scanSteamCheckbox->isChecked(), scanEpicCheckbox->isChecked(),
					   scanGogCheckbox->isChecked(), scanUbiCheckbox->isChecked());
}

void GameDetectorSettingsDialog::disconectOnChangeComboBox(int index)
{
	// Se as permissões unificadas estiverem ativas, não precisa desconectar ao mudar o modo.
	if (unifiedAuthCheckbox->isChecked()) {
		updateActionModeUI(index);
		return;
	}
	onDisconnectClicked();
	updateActionModeUI(index);
}

void GameDetectorSettingsDialog::updateActionModeUI(int index)
{
	bool isApiMode = (index == 1);

	commandLabel->setVisible(!isApiMode);
	commandInput->setVisible(!isApiMode);
	noGameCommandLabel->setVisible(!isApiMode);
	noGameCommandInput->setVisible(!isApiMode);

	delayLabel->setVisible(true);
	delaySpinBox->setVisible(true);
}

void GameDetectorSettingsDialog::onAuthenticationFinished(bool success, const QString &username)
{
	if (success) {
		authStatusLabel->setText(QString(obs_module_text("Auth.ConnectedAs")).arg(username));
		authButton->setText(obs_module_text("Auth.Reconnect"));
		disconnectButton->setVisible(true);
	} else {
		authStatusLabel->setText(obs_module_text("Auth.NotConnected"));
		authButton->setText(obs_module_text("Auth.Connect")); 
		disconnectButton->setVisible(false);
		if (!username.isEmpty()) {
			blog(LOG_WARNING, "[GameDetector/Auth] Authentication failed: %s", username.toStdString().c_str());
		}
	}
}

void GameDetectorSettingsDialog::onDisconnectClicked()
{
	TwitchAuthManager::get().clearAuthentication();
	onAuthenticationFinished(false, "");
	blog(LOG_INFO, "[GameDetector/Auth] User disconnected.");
}

void GameDetectorSettingsDialog::onManageGamesClicked()
{
	GameListDialog dialog(this);
	dialog.exec();
}
