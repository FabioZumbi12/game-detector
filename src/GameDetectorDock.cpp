#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "TwitchChatBot.h"
#include <obs-data.h>
#include "ConfigManager.h"
#include <QComboBox>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QDesktopServices>
#include <QTimer>
#include <obs.h>
#include <QHeaderView>
#include <QStyle>
#include <QCheckBox>
#include <QMessageBox>
#include "TwitchAuthManager.h"

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 10, 10, 10);
	mainLayout->setSpacing(10);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	statusLabel->setWordWrap(true);
	mainLayout->addWidget(statusLabel);

	QFrame *separator1 = new QFrame();
	separator1->setFrameShape(QFrame::HLine);
	separator1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator1);

	// Layout para a ação da Twitch
	QHBoxLayout *twitchActionLayout = new QHBoxLayout();
	twitchActionLabel = new QLabel(obs_module_text("Dock.TwitchAction"));
	twitchActionComboBox = new QComboBox();
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.SendCommand"), 0);
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.ChangeCategory"), 1);
	twitchActionLayout->addWidget(twitchActionLabel);
	twitchActionLayout->addWidget(twitchActionComboBox);
	mainLayout->addLayout(twitchActionLayout);

	connect(twitchActionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		onSettingsChanged();
		updateActionModeUI(index);
	});

	// Layout para o comando
	QHBoxLayout *commandLayout = new QHBoxLayout();
	commandLabel = new QLabel(obs_module_text("Dock.Command.GameDetected"));

	commandInput = new QLineEdit();
	commandInput->setPlaceholderText(obs_module_text("Dock.Command.GameDetected.Placeholder"));

	commandLayout->addWidget(commandLabel);
	commandLayout->addWidget(commandInput);
	mainLayout->addLayout(commandLayout);

	QHBoxLayout *noGameCommandLayout = new QHBoxLayout();
	noGameCommandLabel = new QLabel(obs_module_text("Dock.Command.NoGame"));

	noGameCommandInput = new QLineEdit();
	noGameCommandInput->setPlaceholderText(obs_module_text("Dock.Command.NoGame.Placeholder"));

	noGameCommandLayout->addWidget(noGameCommandLabel);
	noGameCommandLayout->addWidget(noGameCommandInput);
	mainLayout->addLayout(noGameCommandLayout);

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	mainLayout->addWidget(autoExecuteCheckbox);

	executeCommandButton = new QPushButton(obs_module_text("Dock.ExecuteCommand"));
	mainLayout->addWidget(executeCommandButton);
	connect(executeCommandButton, &QPushButton::clicked, this,
		&GameDetectorDock::onExecuteCommandClicked);

	// Conecta os sinais de mudança de texto ao nosso novo slot
	connect(commandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(noGameCommandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(autoExecuteCheckbox, &QCheckBox::checkStateChanged, this, &GameDetectorDock::onSettingsChanged);

	// Conecta os sinais do detector de jogos aos nossos novos slots
	connect(&GameDetector::get(), &GameDetector::gameDetected, this, &GameDetectorDock::onGameDetected);
	connect(&GameDetector::get(), &GameDetector::noGameDetected, this, &GameDetectorDock::onNoGameDetected);
	connect(&TwitchChatBot::get(), &TwitchChatBot::categoryUpdateFinished, this,
		QOverload<bool, const QString &, const QString &>::of(&GameDetectorDock::onCategoryUpdateFinished));
	connect(&TwitchChatBot::get(), &TwitchChatBot::authenticationRequired, this,
		&GameDetectorDock::onAuthenticationRequired);

	// Timer para salvar com delay
	saveDelayTimer = new QTimer(this);
	saveDelayTimer->setSingleShot(true);
	saveDelayTimer->setInterval(1000); // 1 segundo de delay
	connect(saveDelayTimer, &QTimer::timeout, this, &GameDetectorDock::saveDockSettings);

	// O botão de salvar foi removido, o salvamento agora é automático.

	mainLayout->addStretch(1);QLabel *developerLabel = new QLabel(
		"<small><a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a></small>");
	developerLabel->setOpenExternalLinks(true);
	mainLayout->addWidget(developerLabel);
	setLayout(mainLayout);
}

void GameDetectorDock::saveDockSettings()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_set_string(settings, "twitch_command_message", commandInput->text().toStdString().c_str());
	obs_data_set_string(settings, "twitch_command_no_game", noGameCommandInput->text().toStdString().c_str());
	obs_data_set_bool(settings, "execute_automatically", autoExecuteCheckbox->isChecked());
	obs_data_set_int(settings, "twitch_action_mode", twitchActionComboBox->currentData().toInt());

	ConfigManager::get().save(settings);

	// Feedback visual no label de status
	statusLabel->setText(obs_module_text("Dock.SettingsSaved"));

	QTimer::singleShot(2000, this, [this]() {
		// Restaura o status original, verificando se não mudou nesse meio tempo
		if (statusLabel->text() == obs_module_text("Dock.SettingsSaved")) {
			statusLabel->setText(this->detectedGameName.isEmpty() ? obs_module_text("Status.Waiting") : QString(obs_module_text("Status.Playing")).arg(this->detectedGameName));
		}
	});
}

void GameDetectorDock::onSettingsChanged()
{
	saveDelayTimer->start(); // Reinicia o timer a cada alteração
}

void GameDetectorDock::onGameDetected(const QString &gameName, const QString &processName)
{
	this->detectedGameName = gameName;
	statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(gameName));

	if (autoExecuteCheckbox->isChecked()) {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 0) { // Enviar comando
			executeAction(gameName);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory(gameName);
		}
	}
}

void GameDetectorDock::onNoGameDetected()
{
	this->detectedGameName.clear();
	statusLabel->setText(obs_module_text("Status.Waiting"));

	// Executa o comando de "sem jogo"
	QString noGameCommand = noGameCommandInput->text();
	if (autoExecuteCheckbox->isChecked()) {
		int actionMode = ConfigManager::get().getTwitchActionMode();
		if (actionMode == 0) { // Enviar comando
			if (!noGameCommand.isEmpty())
				TwitchChatBot::get().sendChatMessage(noGameCommand);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory("Just Chatting");
		}
	}
}

void GameDetectorDock::onExecuteCommandClicked()
{
	int actionMode = ConfigManager::get().getTwitchActionMode();
	if (!detectedGameName.isEmpty()) {
		if (actionMode == 0) { // Enviar comando
			executeAction(detectedGameName);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory(detectedGameName);
		}
	} else {
		QString noGameCommand = noGameCommandInput->text();
		if (actionMode == 0) {
			TwitchChatBot::get().sendChatMessage(noGameCommand);
		} else { // Alterar categoria
			TwitchChatBot::get().updateCategory("Just Chatting");
		}
	}
}

void GameDetectorDock::loadSettingsFromConfig()
{
	// Bloqueia sinais para não disparar onSettingsChanged durante o carregamento
	commandInput->blockSignals(true);
	noGameCommandInput->blockSignals(true);
	autoExecuteCheckbox->blockSignals(true);
	twitchActionComboBox->blockSignals(true);

	commandInput->setText(ConfigManager::get().getCommand());
	noGameCommandInput->setText(ConfigManager::get().getNoGameCommand());
	autoExecuteCheckbox->setChecked(ConfigManager::get().getExecuteAutomatically());
	twitchActionComboBox->setCurrentIndex(ConfigManager::get().getTwitchActionMode());

	commandInput->blockSignals(false);
	noGameCommandInput->blockSignals(false);
	autoExecuteCheckbox->blockSignals(false);
	twitchActionComboBox->blockSignals(false);

	// Garante que a UI reflita o estado inicial
	updateActionModeUI(twitchActionComboBox->currentIndex());
}

void GameDetectorDock::executeAction(const QString &gameName)
{
	QString commandTemplate = commandInput->text();
	if (commandTemplate.isEmpty()) return;

	QString command = commandTemplate.replace("{game}", gameName);
	TwitchChatBot::get().sendChatMessage(command);
}

void GameDetectorDock::updateActionModeUI(int index)
{
	bool isApiMode = (index == 1);

	commandLabel->setVisible(!isApiMode);
	commandInput->setVisible(!isApiMode);
	noGameCommandLabel->setVisible(!isApiMode);
	noGameCommandInput->setVisible(!isApiMode);

	// Altera o texto do botão de execução manual
	executeCommandButton->setText(isApiMode ? obs_module_text("Dock.ExecuteAction")
						: obs_module_text("Dock.ExecuteCommand"));
}

void GameDetectorDock::onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString)
{
	if (success) {
		QString statusText = QString(obs_module_text("Dock.CategoryUpdated")).arg(gameName);
		statusLabel->setText(statusText);
	} else {
		QString statusText = errorString.arg(gameName);
		statusLabel->setText(statusText);
	}

	// Retorna ao status normal após alguns segundos
	QTimer::singleShot(3000, this, [this]() {
		statusLabel->setText(this->detectedGameName.isEmpty() ? obs_module_text("Status.Waiting") : QString(obs_module_text("Status.Playing")).arg(this->detectedGameName));
	});
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

GameDetectorDock::~GameDetectorDock()
{
	// O Qt parent/child system geralmente cuida da limpeza.
}