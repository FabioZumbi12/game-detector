#include "GameDetectorSettingsDialog.h"
#include "GameDetector.h"
#include "IconProvider.h"
#include "ConfigManager.h"
#include "TwitchAuthManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QStyle>
#include <QDesktopServices>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <obs-data.h>

GameDetectorSettingsDialog::GameDetectorSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(obs_module_text("Settings.WindowTitle"));
	setMinimumSize(600, 500);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	mainLayout->addWidget(new QLabel(obs_module_text("Settings.Header")));
	QLabel *headerLabel = new QLabel(obs_module_text("Settings.Description"));
	headerLabel->setWordWrap(true);
	mainLayout->addWidget(headerLabel);

	QFrame *separator1 = new QFrame();
	separator1->setFrameShape(QFrame::HLine);
	separator1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator1);

	// --- Seção da Tabela de Jogos ---
	mainLayout->addWidget(new QLabel(obs_module_text("Settings.GameList")));

	manualGamesTable = new QTableWidget();
	manualGamesTable->setColumnCount(3);
	manualGamesTable->setHorizontalHeaderLabels(QStringList() << obs_module_text("Table.Header.Name") << obs_module_text("Table.Header.Executable") << "Caminho");
	manualGamesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	// Oculta a coluna de caminho, ela é apenas para uso interno
	manualGamesTable->setColumnHidden(2, true);
	mainLayout->addWidget(manualGamesTable);

	QHBoxLayout *tableButtonsLayout = new QHBoxLayout();
	addGameButton = new QPushButton(obs_module_text("Settings.AddGame"));
	removeGameButton = new QPushButton(obs_module_text("Settings.RemoveGame"));
	clearTableButton = new QPushButton(obs_module_text("Settings.ClearList"));
	tableButtonsLayout->addWidget(addGameButton);
	tableButtonsLayout->addWidget(removeGameButton);
	tableButtonsLayout->addWidget(clearTableButton);
	rescanButton = new QPushButton(obs_module_text("Settings.ScanGames"));
	rescanButton->setProperty("baseText", rescanButton->text());
	tableButtonsLayout->addWidget(rescanButton);
	tableButtonsLayout->addStretch(1);
	mainLayout->addLayout(tableButtonsLayout);

	connect(&GameDetector::get(), &GameDetector::gameFoundDuringScan, this, [=](int totalFound) {
		QString base = rescanButton->property("baseText").toString();
		rescanButton->setText(QString("%1 (%2)").arg(base).arg(totalFound));
	});

	mainLayout->addWidget(new QLabel(obs_module_text("Settings.ScanGames.HelpText")));

    QFrame *separator2 = new QFrame();
	separator2->setFrameShape(QFrame::HLine);
	separator2->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator2);

	// --- Seção do Token ---
	mainLayout->addWidget(new QLabel(obs_module_text("Settings.TwitchConnection")));

	authStatusLabel = new QLabel(obs_module_text("Auth.NotConnected"));
	mainLayout->addWidget(authStatusLabel);

	QHBoxLayout *authButtonsLayout = new QHBoxLayout();
	authButton = new QPushButton(obs_module_text("Auth.Connect"));
	authButtonsLayout->addWidget(authButton);

	disconnectButton = new QPushButton(obs_module_text("Auth.Disconnect"));
	authButtonsLayout->addWidget(disconnectButton);
	authButtonsLayout->addStretch(1);
	mainLayout->addLayout(authButtonsLayout);

	QLabel *helpLabel = new QLabel(obs_module_text("Auth.HelpText"));
	helpLabel->setWordWrap(true);
	mainLayout->addWidget(helpLabel);

	mainLayout->addStretch(1);
	QLabel *developerLabel = new QLabel(
		"<small><a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a></small>");
	developerLabel->setOpenExternalLinks(true);
	mainLayout->addWidget(developerLabel);

	// --- Botões OK/Cancel ---
	QHBoxLayout *dialogButtonsLayout = new QHBoxLayout();
	okButton = new QPushButton(obs_module_text("OK"));
	cancelButton = new QPushButton(obs_module_text("Cancel"));
	dialogButtonsLayout->addStretch(1);
	dialogButtonsLayout->addWidget(okButton);
	dialogButtonsLayout->addWidget(cancelButton);
	mainLayout->addLayout(dialogButtonsLayout);

	// --- Conexões ---
	connect(addGameButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onAddGameClicked);
	connect(removeGameButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onRemoveGameClicked);
	connect(clearTableButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onClearTableClicked);
	connect(rescanButton, &QPushButton::clicked, this, [this]() {
		rescanButton->setEnabled(false);
		rescanButton->setText(obs_module_text("Settings.Scanning"));
		GameDetector::get().rescanForGames();
	});
	connect(authButton, &QPushButton::clicked, &TwitchAuthManager::get(), &TwitchAuthManager::startAuthentication);

	connect(disconnectButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onDisconnectClicked);
	connect(&GameDetector::get(), &GameDetector::automaticScanFinished, this, &GameDetectorSettingsDialog::onAutomaticScanFinished);
	connect(&TwitchAuthManager::get(), &TwitchAuthManager::authenticationFinished, this, &GameDetectorSettingsDialog::onAuthenticationFinished);

	connect(okButton, &QPushButton::clicked, this, [this]() {
		saveSettings();
		accept();
	});
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	loadSettings();
}

GameDetectorSettingsDialog::~GameDetectorSettingsDialog() {}

void GameDetectorSettingsDialog::loadSettings()
{
	QString userId = ConfigManager::get().getUserId();
	QString loginName = ConfigManager::get().getTwitchChannelLogin();

	if (!userId.isEmpty()) {
		onAuthenticationFinished(true, loginName);
	} else {
		onAuthenticationFinished(false, "");
	}

	obs_data_array_t *gamesArray = ConfigManager::get().getManualGames();
	if (gamesArray) {
		manualGamesTable->setRowCount(0);
		size_t count = obs_data_array_count(gamesArray);
		for (int i = (int)count - 1; i >= 0; --i) {
			obs_data_t *item = obs_data_array_item(gamesArray, i);
			QString gameName = obs_data_get_string(item, "name");
			QString exeName  = obs_data_get_string(item, "exe");
			QString exePath  = obs_data_get_string(item, "path");

			if (QFileInfo::exists(exePath)) {
				int newRow = 0; // Inserir no topo
				manualGamesTable->insertRow(newRow);
				QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);

				QIcon icon = IconProvider::getIconForFile(exePath);
				nameItem->setIcon(icon.isNull() ? style()->standardIcon(QStyle::SP_DesktopIcon) : icon);

				manualGamesTable->setItem(newRow, 0, nameItem);
				manualGamesTable->setItem(newRow, 1, new QTableWidgetItem(exeName));
				manualGamesTable->setItem(newRow, 2, new QTableWidgetItem(exePath));
			} else {
				// O jogo não existe mais, remove da lista
				obs_data_array_erase(gamesArray, i);
			}
			obs_data_release(item);
		}
		obs_data_array_release(gamesArray);
	}
}

void GameDetectorSettingsDialog::saveSettings()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_array_t *gamesArray = obs_data_array_create();
	for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "name", manualGamesTable->item(i, 0)->text().toStdString().c_str());
		obs_data_set_string(item, "exe", manualGamesTable->item(i, 1)->text().toStdString().c_str());
		obs_data_set_string(item, "path", manualGamesTable->item(i, 2)->text().toStdString().c_str());
		obs_data_array_push_back(gamesArray, item);
		obs_data_release(item);
	}
	obs_data_set_array(settings, "manual_games_list", gamesArray);
	obs_data_array_release(gamesArray);

	ConfigManager::get().save(settings);

	// Notifica o detector que as configurações foram alteradas
	GameDetector::get().onSettingsChanged();
}

void GameDetectorSettingsDialog::onAddGameClicked()
{
	QString filePath = QFileDialog::getOpenFileName(this, "Selecionar Executável do Jogo", "", "Executáveis (*.exe)");
	if (filePath.isEmpty()) {
		return;
	}

	QFileInfo fileInfo(filePath);
	QString exeName = fileInfo.fileName(); // "Cyberpunk2077.exe"

	// Lógica para encontrar o nome amigável a partir da pasta pai,
	// subindo de diretórios se necessário.
	QDir gameDir = fileInfo.dir();
	const QSet<QString> binaryFolderNames = {"bin", "binaries", "win64", "win_x64", "x64", "shipping"};
	while(binaryFolderNames.contains(gameDir.dirName().toLower())) {
		if (!gameDir.cdUp()) break;
	}
	QString gameName = gameDir.dirName(); // "Cyberpunk 2077"

	// Se o nome da pasta for genérico, usa o nome base do arquivo como fallback.
	if (gameName.isEmpty() || binaryFolderNames.contains(gameName.toLower())) {
		gameName = fileInfo.completeBaseName();
	}

	int newRow = manualGamesTable->rowCount();
	manualGamesTable->insertRow(newRow);
	QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);
	nameItem->setIcon(IconProvider::getIconForFile(filePath));
	manualGamesTable->setItem(newRow, 0, nameItem);
	manualGamesTable->setItem(newRow, 1, new QTableWidgetItem(exeName));
	manualGamesTable->setItem(newRow, 2, new QTableWidgetItem(filePath));
}

void GameDetectorSettingsDialog::onRemoveGameClicked()
{
	int currentRow = manualGamesTable->currentRow();
	if (currentRow >= 0) {
		manualGamesTable->removeRow(currentRow);
	}
}

void GameDetectorSettingsDialog::onClearTableClicked()
{
	manualGamesTable->setRowCount(0);
}

void GameDetectorSettingsDialog::onAutomaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames)
{
	manualGamesTable->blockSignals(true);

	QSet<QString> existingExes;
	for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
		QTableWidgetItem* item = manualGamesTable->item(i, 1);
		if (item) {
			existingExes.insert(manualGamesTable->item(i, 1)->text());
		}
	}

	bool itemsAdded = false;
	for (const auto &gameTuple : foundGames) {
		const QString &gameName = std::get<0>(gameTuple);
		const QString &exeName = std::get<1>(gameTuple);
		const QString &exePath = std::get<2>(gameTuple);

		if (!existingExes.contains(exeName)) {
			itemsAdded = true;
			int newRow = manualGamesTable->rowCount();
			manualGamesTable->insertRow(newRow);
			QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);
			nameItem->setIcon(IconProvider::getIconForFile(exePath));
			manualGamesTable->setItem(newRow, 0, nameItem);
			manualGamesTable->setItem(newRow, 1, new QTableWidgetItem(exeName));
			manualGamesTable->setItem(newRow, 2, new QTableWidgetItem(exePath));
			existingExes.insert(exeName);
		}
	}
	manualGamesTable->blockSignals(false);

	if (itemsAdded) {
		saveSettings(); // Salva as configurações para persistir os jogos adicionados
	}

	rescanButton->setEnabled(true);
	rescanButton->setText(obs_module_text("Settings.ScanGames"));
}

void GameDetectorSettingsDialog::onAuthenticationFinished(bool success, const QString &username)
{
	if (success) {
		authStatusLabel->setText(QString(obs_module_text("Auth.ConnectedAs")).arg(username));
		authButton->setText(obs_module_text("Auth.Reconnect"));
		disconnectButton->setVisible(true);
	} else {
		authStatusLabel->setText(obs_module_text("Auth.NotConnected")); // Not Connected
		authButton->setText(obs_module_text("Auth.Connect"));           // Connect with Twitch
		disconnectButton->setVisible(false);                            // Disconnect
		if (!username.isEmpty()) { // If username is not empty, it means it's an error message
			blog(LOG_WARNING, "[GameDetector/Auth] Authentication failed: %s", username.toStdString().c_str());
		}
	}
}

void GameDetectorSettingsDialog::onDisconnectClicked()
{
	// Limpa os dados de autenticação em memória e persistidos no AuthManager
	// (que por sua vez, limpa o ConfigManager)
	TwitchAuthManager::get().clearAuthentication();
	
	// Atualiza a UI para o estado "não conectado"
	onAuthenticationFinished(false, "");

	blog(LOG_INFO, "[GameDetector/Auth] User disconnected.");
}
