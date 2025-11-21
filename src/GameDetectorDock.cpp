#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "TwitchChatBot.h"
#include "ConfigManager.h"

#include <QComboBox>
#include <QVBoxLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>
#include <QUrl>
#include <QDesktopServices>
#include <QTimer>
#include <obs.h>
#include <QHeaderView>
#include <QStyle>
#include <QCheckBox>
#include <QCursor>
#include <QFormLayout>
#include <QMessageBox>
#include "TwitchAuthManager.h"
#include "GameDetectorSettingsDialog.h"

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(5, 5, 5, 5); // Margens mais compactas
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	// --- Seção de Status ---
	statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	statusLabel->setWordWrap(true);
	statusLabel->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(statusLabel); 

	// --- Separador ---
	QFrame *separator1 = new QFrame();
	separator1->setFrameShape(QFrame::HLine);
	separator1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator1);

	// --- Seção de Ações da Twitch ---
	QFormLayout *actionLayout = new QFormLayout();
	actionLayout->setContentsMargins(0, 5, 0, 5);
	actionLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);

	twitchActionComboBox = new QComboBox();
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.SendCommand"), 0);
	twitchActionComboBox->addItem(obs_module_text("Dock.TwitchAction.ChangeCategory"), 1);
	actionLayout->addRow(obs_module_text("Dock.TwitchAction"), twitchActionComboBox);

	connect(twitchActionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		onSettingsChanged();
		updateActionModeUI(index);
	});

	// --- Widgets para o modo de comando ---
	commandLabel = new QLabel(obs_module_text("Dock.Command.GameDetected"));
	commandInput = new QLineEdit();
	commandInput->setPlaceholderText(obs_module_text("Dock.Command.GameDetected.Placeholder"));
	actionLayout->addRow(commandLabel, commandInput);

	noGameCommandLabel = new QLabel(obs_module_text("Dock.Command.NoGame"));
	noGameCommandInput = new QLineEdit();
	noGameCommandInput->setPlaceholderText(obs_module_text("Dock.Command.NoGame.Placeholder"));
	actionLayout->addRow(noGameCommandLabel, noGameCommandInput);

	mainLayout->addLayout(actionLayout);

	// --- Separador ---
	QFrame *separator2 = new QFrame();
	separator2->setFrameShape(QFrame::HLine);
	separator2->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator2);

	// --- Seção de Execução ---
	QFormLayout *executionLayout = new QFormLayout(); // Usar QFormLayout para consistência
	executionLayout->setContentsMargins(0, 5, 0, 5);

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	executionLayout->addRow(autoExecuteCheckbox);

	// Layout horizontal para os botões de ação
	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	executeCommandButton = new QPushButton(obs_module_text("Dock.ExecuteCommand"));
	buttonsLayout->addWidget(executeCommandButton);

	settingsButton = new QPushButton();
	settingsButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
	settingsButton->setToolTip(obs_module_text("Dock.OpenSettings"));
	settingsButton->setCursor(Qt::PointingHandCursor);
	settingsButton->setFixedSize(executeCommandButton->sizeHint().height(), executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(settingsButton);

	executionLayout->addRow(buttonsLayout);
	mainLayout->addLayout(executionLayout);

	connect(executeCommandButton, &QPushButton::clicked, this,
			&GameDetectorDock::onExecuteCommandClicked);

	// Conecta os sinais do detector de jogos aos nossos novos slots
	connect(&GameDetector::get(), &GameDetector::gameDetected, this, &GameDetectorDock::onGameDetected);
	connect(&GameDetector::get(), &GameDetector::noGameDetected, this, &GameDetectorDock::onNoGameDetected);
	connect(&TwitchChatBot::get(), &TwitchChatBot::categoryUpdateFinished, this,
		QOverload<bool, const QString &, const QString &>::of(&GameDetectorDock::onCategoryUpdateFinished));
	connect(&TwitchChatBot::get(), &TwitchChatBot::authenticationRequired, this,
		&GameDetectorDock::onAuthenticationRequired);

	// Conecta os sinais de mudança de texto ao nosso novo slot
	connect(commandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(noGameCommandInput, &QLineEdit::textChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(autoExecuteCheckbox, &QCheckBox::checkStateChanged, this, &GameDetectorDock::onSettingsChanged);

	// Timer para salvar com delay
	saveDelayTimer = new QTimer(this);
	saveDelayTimer->setSingleShot(true);
	saveDelayTimer->setInterval(1000); // 1 segundo de delay
	connect(saveDelayTimer, &QTimer::timeout, this, &GameDetectorDock::saveDockSettings);

	// Conecta o sinal global de configurações salvas para atualizar os avisos
	connect(&ConfigManager::get(), &ConfigManager::settingsSaved, this, &GameDetectorDock::checkWarningsAndStatus);

	// Timer para verificar status e avisos periodicamente
	statusCheckTimer = new QTimer(this);
	connect(statusCheckTimer, &QTimer::timeout, this, &GameDetectorDock::checkWarningsAndStatus);
	statusCheckTimer->start(5000); // Verifica a cada 5 segundos

	connect(settingsButton, &QPushButton::clicked, this, &GameDetectorDock::onSettingsButtonClicked);

	QLabel *developerLabel = new QLabel(
		"<small><a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a></small>");
	developerLabel->setOpenExternalLinks(true);
	developerLabel->setCursor(Qt::PointingHandCursor);
	mainLayout->addWidget(developerLabel);
	mainLayout->addStretch(1); // Adiciona um espaçador para alinhar tudo ao topo

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

	// Mostra "Salvo" temporariamente e depois restaura o status
	statusLabel->setText(obs_module_text("Dock.SettingsSaved"));
	statusLabel->setStyleSheet("font-weight: bold;");
	QTimer::singleShot(2000, this, &GameDetectorDock::checkWarningsAndStatus);
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
	checkWarningsAndStatus();
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
		statusLabel->setText(QString(obs_module_text("Dock.CategoryUpdated")).arg(gameName));
		statusLabel->setStyleSheet("font-weight: bold;");
	} else {
		statusLabel->setText(errorString.arg(gameName));
		statusLabel->setStyleSheet("font-weight: bold; color: #ff1a1a;");
	}

	// Restaura o status principal após 3 segundos
	QTimer::singleShot(3000, this, &GameDetectorDock::restoreStatusLabel);
}

void GameDetectorDock::checkWarningsAndStatus()
{
	// 1. Verifica se há avisos pendentes. Eles têm a maior prioridade.
	obs_data_array_t *gamesArray = ConfigManager::get().getManualGames();
	bool noGames = (obs_data_array_count(gamesArray) == 0);
	obs_data_array_release(gamesArray);

	if (noGames) {
		statusLabel->setText(obs_module_text("Status.Warning.NoGames"));
		return;
	}

	bool notConnected = ConfigManager::get().getUserId().isEmpty();
	if (notConnected) {
		statusLabel->setText(obs_module_text("Status.Warning.NotConnected"));
		return;
	}

	// 2. Se não houver avisos, restaura o status normal do jogo.
	restoreStatusLabel();
}

void GameDetectorDock::restoreStatusLabel()
{
	// 2. Se não houver avisos, mostra o status normal do jogo.
	statusLabel->setStyleSheet("font-weight: bold;"); // Restaura o estilo padrão
	if (!detectedGameName.isEmpty())
		statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(detectedGameName));
	else
		statusLabel->setText(obs_module_text("Status.Waiting"));
}

void GameDetectorDock::onSettingsButtonClicked()
{
	if (!settingsDialog) {
		// Usa 'this' como pai para que a janela de diálogo possa ser centralizada em relação ao dock se necessário
		settingsDialog = new GameDetectorSettingsDialog(this);
		// Garante que o ponteiro seja zerado quando a janela for fechada
		connect(settingsDialog, &QDialog::finished, this, [this](int result) { settingsDialog = nullptr; });
	}
	settingsDialog->show();
	settingsDialog->raise();
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