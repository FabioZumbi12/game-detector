#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "TwitchChatBot.h"
#include <obs-data.h>
#include "ConfigManager.h"

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

static QLabel *g_statusLabel = nullptr;

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 10, 10, 10);
	mainLayout->setSpacing(10);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	g_statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	mainLayout->addWidget(g_statusLabel);

	QFrame *separator1 = new QFrame();
	separator1->setFrameShape(QFrame::HLine);
	separator1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator1);

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
	executeCommandButton->setEnabled(false);
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

	ConfigManager::get().save(settings);

	// Feedback visual no label de status
	QString originalStatus = g_statusLabel->text();
	g_statusLabel->setText(obs_module_text("Dock.SettingsSaved"));

	QTimer::singleShot(2000, this, [this]() {
		// Restaura o status original, verificando se não mudou nesse meio tempo
		if (g_statusLabel->text() == obs_module_text("Dock.SettingsSaved")) {
			g_statusLabel->setText(this->detectedGameName.isEmpty() ? obs_module_text("Status.Waiting") : QString(obs_module_text("Status.Playing")).arg(this->detectedGameName));
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
	g_statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(gameName));

	executeCommandButton->setEnabled(true);
	if (autoExecuteCheckbox->isChecked()) {
		executeGameCommand(gameName);
	} else {
		executeCommandButton->setText(QString(obs_module_text("Dock.ExecuteCommandFor")).arg(gameName));
	}
}

void GameDetectorDock::onNoGameDetected()
{
	this->detectedGameName.clear();
	g_statusLabel->setText(obs_module_text("Status.Waiting"));
	executeCommandButton->setEnabled(false);
	executeCommandButton->setText(obs_module_text("Dock.ExecuteCommand"));

	// Executa o comando de "sem jogo"
	QString noGameCommand = noGameCommandInput->text();
	if (autoExecuteCheckbox->isChecked()) {
		if (!noGameCommand.isEmpty()) {
			TwitchChatBot::get().sendMessage(noGameCommand);
		}
	} else {
		executeCommandButton->setEnabled(true);
	}
}

void GameDetectorDock::onExecuteCommandClicked()
{
	if (!detectedGameName.isEmpty())
		executeGameCommand(detectedGameName);
}

void GameDetectorDock::loadSettingsFromConfig()
{
	// Bloqueia sinais para não disparar onSettingsChanged durante o carregamento
	commandInput->blockSignals(true);
	noGameCommandInput->blockSignals(true);
	autoExecuteCheckbox->blockSignals(true);

	commandInput->setText(ConfigManager::get().getCommand());
	noGameCommandInput->setText(ConfigManager::get().getNoGameCommand());
	autoExecuteCheckbox->setChecked(ConfigManager::get().getExecuteAutomatically());

	commandInput->blockSignals(false);
	noGameCommandInput->blockSignals(false);
	autoExecuteCheckbox->blockSignals(false);
}

void GameDetectorDock::executeGameCommand(const QString &gameName)
{
	QString commandTemplate = commandInput->text();
	if (commandTemplate.isEmpty()) return;

	QString command = commandTemplate.replace("{game}", gameName);
	TwitchChatBot::get().sendMessage(command);
}

GameDetectorDock::~GameDetectorDock()
{
	// O Qt parent/child system geralmente cuida da limpeza.
}