#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "TwitchChatBot.h"
#include "TwitchAuthManager.h"
#include "ConfigManager.h"
#include "GameDetectorSettingsDialog.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>
#include <QUrl>
#include <QDesktopServices>
#include <QStyle>
#include <QCheckBox>
#include <QCursor>
#include <QFormLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QTime>
#include <obs.h>

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(5, 5, 5, 5);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	detectedGameName = "Just Chatting";
	statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	statusLabel->setWordWrap(true);
	mainLayout->addWidget(statusLabel); 

	QFormLayout *executionLayout = new QFormLayout();
	executionLayout->setContentsMargins(0, 5, 0, 5);

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	executionLayout->addRow(autoExecuteCheckbox);

	// Layout horizontal para os botões de ação
	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	executeCommandButton = new QPushButton(obs_module_text("Dock.SetGame"));
	buttonsLayout->addWidget(executeCommandButton);

	settingsButton = new QPushButton();
	settingsButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
	settingsButton->setToolTip(obs_module_text("Dock.OpenSettings"));
	settingsButton->setCursor(Qt::PointingHandCursor);
	settingsButton->setFixedSize(executeCommandButton->sizeHint().height(), executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(settingsButton);

	executionLayout->addRow(buttonsLayout);

	setJustChattingButton = new QPushButton(obs_module_text("Dock.SetJustChatting"));
	executionLayout->addRow(setJustChattingButton);

	mainLayout->addLayout(executionLayout);

	connect(executeCommandButton, &QPushButton::clicked, this,
			&GameDetectorDock::onExecuteCommandClicked);
	connect(setJustChattingButton, &QPushButton::clicked, this, &GameDetectorDock::onSetJustChattingClicked);
	connect(&GameDetector::get(), &GameDetector::gameDetected, this, &GameDetectorDock::onGameDetected);
	connect(&GameDetector::get(), &GameDetector::noGameDetected, this, &GameDetectorDock::onNoGameDetected);
	connect(&TwitchChatBot::get(), &TwitchChatBot::categoryUpdateFinished, this,
		QOverload<bool, const QString &, const QString &>::of(&GameDetectorDock::onCategoryUpdateFinished));
	connect(&TwitchChatBot::get(), &TwitchChatBot::authenticationRequired, this,
		&GameDetectorDock::onAuthenticationRequired);

	connect(&TwitchChatBot::get(), &TwitchChatBot::cooldownStarted, this, &GameDetectorDock::onCooldownStarted);
	connect(&TwitchChatBot::get(), &TwitchChatBot::cooldownFinished, this, &GameDetectorDock::onCooldownFinished);


	connect(autoExecuteCheckbox, &QCheckBox::checkStateChanged, this, &GameDetectorDock::onSettingsChanged);

	saveDelayTimer = new QTimer(this);
	saveDelayTimer->setSingleShot(true);
	saveDelayTimer->setInterval(1000);
	connect(saveDelayTimer, &QTimer::timeout, this, &GameDetectorDock::saveDockSettings);

	connect(&ConfigManager::get(), &ConfigManager::settingsSaved, this, &GameDetectorDock::checkWarningsAndStatus);

	cooldownUpdateTimer = new QTimer(this);
	connect(cooldownUpdateTimer, &QTimer::timeout, this, &GameDetectorDock::updateCooldownLabel);

	statusCheckTimer = new QTimer(this);
	connect(statusCheckTimer, &QTimer::timeout, this, &GameDetectorDock::checkWarningsAndStatus);
	statusCheckTimer->start(5000);

	connect(settingsButton, &QPushButton::clicked, this, &GameDetectorDock::onSettingsButtonClicked);

	mainLayout->addStretch(1);

	setLayout(mainLayout);
}

void GameDetectorDock::saveDockSettings()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_set_bool(settings, "execute_automatically", autoExecuteCheckbox->isChecked());

	ConfigManager::get().save(settings);

	statusLabel->setText(obs_module_text("Dock.SettingsSaved"));
	QTimer::singleShot(2000, this, &GameDetectorDock::checkWarningsAndStatus);
}

void GameDetectorDock::onSettingsChanged()
{
	saveDelayTimer->start();
}

void GameDetectorDock::onGameDetected(const QString &gameName)
{
	this->detectedGameName = gameName;
	this->desiredCategory = gameName;
	checkWarningsAndStatus();
}

void GameDetectorDock::onNoGameDetected()
{
	this->detectedGameName = "Just Chatting";
	this->desiredCategory = "Just Chatting";
	checkWarningsAndStatus();
}

void GameDetectorDock::onExecuteCommandClicked()
{
	if (TwitchChatBot::get().isOnCooldown()) {
		return;
	}
	this->desiredCategory = detectedGameName;
	TwitchChatBot::get().updateCategory(desiredCategory);
}

void GameDetectorDock::onSetJustChattingClicked()
{
	if (TwitchChatBot::get().isOnCooldown()) {
		return;
	}
	this->desiredCategory = "Just Chatting";
	TwitchChatBot::get().updateCategory(desiredCategory);
}

void GameDetectorDock::loadSettingsFromConfig()
{
	autoExecuteCheckbox->blockSignals(true);
	autoExecuteCheckbox->setChecked(ConfigManager::get().getExecuteAutomatically());
	autoExecuteCheckbox->blockSignals(false);
	checkWarningsAndStatus();
}

void GameDetectorDock::onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString)
{
	cooldownUpdateTimer->stop(); // Pausa o timer do cooldown para mostrar a mensagem
	if (success) {
		statusLabel->setText(QString(obs_module_text("Dock.CategoryUpdated")).arg(gameName));
	} else {
		statusLabel->setText(QString(errorString).arg(gameName));
	}

	QTimer::singleShot(3000, this, &GameDetectorDock::restoreStatusLabel); // Restaura o status após 3 segundos
}

void GameDetectorDock::checkWarningsAndStatus()
{
	if (GameDetector::get().isGameListEmpty()) {
        statusLabel->setText(obs_module_text("Status.Warning.NoGames"));
        return;
	}

	bool notConnected = ConfigManager::get().getUserId().isEmpty();
	if (notConnected) {
		statusLabel->setText(obs_module_text("Status.Warning.NotConnected"));
		return;
	}

	if (TwitchChatBot::get().isOnCooldown()) {
		if (!cooldownUpdateTimer->isActive()) {
			int remaining = TwitchChatBot::get().getCooldownRemaining();
			if (remaining > 0) onCooldownStarted(remaining);
		}
		return;
	}

	if (autoExecuteCheckbox->isChecked()) {
		TwitchChatBot::get().updateCategory(desiredCategory);
	}

	restoreStatusLabel();
}

void GameDetectorDock::restoreStatusLabel()
{
	if (cooldownUpdateTimer->isActive()) {
		// Se o timer já estiver ativo, não faz nada para não interromper a contagem
	} else if (TwitchChatBot::get().isOnCooldown()) {
		cooldownUpdateTimer->start(1000); // Retoma o timer do cooldown
	}

	if (detectedGameName != "Just Chatting")
		statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(detectedGameName));
	else if (TwitchChatBot::get().getLastSetCategory() == "Just Chatting") {
		statusLabel->setText(obs_module_text("Status.Waiting"));
	} else if (!TwitchChatBot::get().getLastSetCategory().isEmpty()) {
		statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(TwitchChatBot::get().getLastSetCategory()));
	}
	else {
		statusLabel->setText(obs_module_text("Status.Waiting"));
	}
}

void GameDetectorDock::onSettingsButtonClicked()
{
	GameDetectorSettingsDialog dialog(this);
	dialog.exec();
}

void GameDetectorDock::onAuthenticationRequired()
{
	QMessageBox msgBox;
	msgBox.setWindowTitle(obs_module_text("Auth.Required.Title"));
	msgBox.setText(obs_module_text("Auth.Required.Text"));
	msgBox.setInformativeText(obs_module_text("Auth.Required.Info"));
	msgBox.setIcon(QMessageBox::Question);
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msgBox.setDefaultButton(QMessageBox::Yes);
	msgBox.button(QMessageBox::Yes)->setText(obs_module_text("Auth.Required.Connect"));
	msgBox.button(QMessageBox::No)->setText(obs_module_text("Auth.Cancel"));
	if (msgBox.exec() == QMessageBox::Yes) {
		TwitchAuthManager::get().startAuthentication();
	}
}

void GameDetectorDock::onCooldownStarted(int seconds)
{
	GameDetector::get().stopScanning(); // Pausa a detecção de jogos
	cooldownUpdateTimer->setProperty("remaining", seconds);
	updateCooldownLabel();
	cooldownUpdateTimer->start(1000);
}

void GameDetectorDock::onCooldownFinished()
{
	cooldownUpdateTimer->stop();
	GameDetector::get().startScanning(); // Retoma a detecção de jogos
	checkWarningsAndStatus(); // Força uma verificação imediata do estado
}

void GameDetectorDock::updateCooldownLabel()
{
	int remaining = cooldownUpdateTimer->property("remaining").toInt();
	if (remaining >= 0) {
		QString timeStr = QTime(0, 0).addSecs(remaining).toString("mm:ss");
		QString currentGameText =
			QString(obs_module_text("Status.Playing"))
				.arg(desiredCategory);
		statusLabel->setText(QString(obs_module_text("Dock.OnCooldown"))
					 .arg(currentGameText).arg(timeStr));
		cooldownUpdateTimer->setProperty("remaining", remaining - 1);
	} else {
		onCooldownFinished(); // O contador chegou a zero, agora finaliza.
	}
}

GameDetectorDock::~GameDetectorDock()
{
	if (cooldownUpdateTimer->isActive()) cooldownUpdateTimer->stop();
	if (saveDelayTimer->isActive()) saveDelayTimer->stop();
	if (statusCheckTimer->isActive()) statusCheckTimer->stop();
}