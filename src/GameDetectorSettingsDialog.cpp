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
#include <QGroupBox>
#include <obs-data.h>

GameDetectorSettingsDialog::GameDetectorSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(obs_module_text("Settings.WindowTitle"));
	setMinimumSize(800, 500);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	mainLayout->addWidget(new QLabel(obs_module_text("Settings.Header")));
	QLabel *headerLabel = new QLabel(obs_module_text("Settings.Description"));
	headerLabel->setWordWrap(true);
	mainLayout->addWidget(headerLabel);

	// --- Seção da Tabela de Jogos ---
	QGroupBox *gamesGroup = new QGroupBox(obs_module_text("Settings.GameList"));
	QVBoxLayout *gamesLayout = new QVBoxLayout();

	manualGamesTable = new QTableWidget();
	manualGamesTable->setColumnCount(4);
	manualGamesTable->setHorizontalHeaderLabels(QStringList() << obs_module_text("Table.Header.Name") << obs_module_text("Table.Header.Executable") << obs_module_text("Table.Header.Path") << obs_module_text("Table.Header.Actions"));
	manualGamesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	manualGamesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	// Oculta a coluna de caminho, ela é apenas para uso interno
	manualGamesTable->setColumnHidden(2, true);
	manualGamesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	gamesLayout->addWidget(manualGamesTable);
	
	// Layout para os botões abaixo da tabela
	QHBoxLayout *tableButtonsLayout = new QHBoxLayout();
	addGameButton = new QPushButton(obs_module_text("Settings.AddGame"));
	clearTableButton = new QPushButton(obs_module_text("Settings.ClearList"));
	rescanButton = new QPushButton(obs_module_text("Settings.ScanGames"));
	rescanButton->setProperty("baseText", rescanButton->text());
	tableButtonsLayout->addWidget(addGameButton);
	tableButtonsLayout->addWidget(clearTableButton);
	tableButtonsLayout->addWidget(rescanButton);
	tableButtonsLayout->addStretch(1);
	gamesLayout->addLayout(tableButtonsLayout);

	// --- Opções de Escaneamento ---
	QHBoxLayout *scanOptionsLayout = new QHBoxLayout();
	scanOptionsLayout->setContentsMargins(0, 5, 0, 5);
	QLabel* scanLabel = new QLabel(obs_module_text("Settings.ScanFrom"));
	scanOptionsLayout->addWidget(scanLabel);

	scanSteamCheckbox = new QCheckBox("Steam");
	scanEpicCheckbox = new QCheckBox("Epic Games");
	scanGogCheckbox = new QCheckBox("GOG Galaxy");
	scanUbiCheckbox = new QCheckBox("Ubisoft Connect");
	scanOptionsLayout->addWidget(scanSteamCheckbox);
	scanOptionsLayout->addWidget(scanEpicCheckbox);
	scanOptionsLayout->addWidget(scanGogCheckbox);
	scanOptionsLayout->addWidget(scanUbiCheckbox);
	scanOptionsLayout->addStretch(1);
	gamesLayout->addLayout(scanOptionsLayout);

	connect(&GameDetector::get(), &GameDetector::gameFoundDuringScan, this, [=](int totalFound) {
		QString base = rescanButton->property("baseText").toString();
		rescanButton->setText(QString("%1 (%2)").arg(base).arg(totalFound));
	});

	gamesGroup->setLayout(gamesLayout);
	mainLayout->addWidget(gamesGroup);

	// --- Seção do Token ---
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

	authGroup->setLayout(authLayout);
	mainLayout->addWidget(authGroup);

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
	connect(clearTableButton, &QPushButton::clicked, this, &GameDetectorSettingsDialog::onClearTableClicked);
	connect(rescanButton, &QPushButton::clicked, this, [this]() {
		bool scanSteam = scanSteamCheckbox->isChecked();
		bool scanEpic = scanEpicCheckbox->isChecked();
		bool scanGog = scanGogCheckbox->isChecked();
		bool scanUbi = scanUbiCheckbox->isChecked();
		rescanButton->setEnabled(false);
		rescanButton->setText(obs_module_text("Settings.Scanning"));
		GameDetector::get().rescanForGames(scanSteam, scanEpic, scanGog, scanUbi);
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

				QPushButton *removeRowButton = new QPushButton();
				removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
				removeRowButton->setToolTip(obs_module_text("Settings.RemoveGame"));
				removeRowButton->setCursor(Qt::PointingHandCursor);

				// Cria um container para o botão preencher a célula
				QWidget *containerWidget = new QWidget();

				// Conecta o botão para remover a linha específica
				connect(removeRowButton, &QPushButton::clicked, this, [this, containerWidget]() {
					for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
						if (manualGamesTable->cellWidget(i, 3) == containerWidget) {
							int rowToRemove = i;
							manualGamesTable->removeRow(rowToRemove);

							// Seleciona a linha anterior ou a primeira, se a removida era a primeira
							int newRowCount = manualGamesTable->rowCount();
							if (newRowCount > 0) {
								int rowToSelect = (rowToRemove > 0) ? (rowToRemove - 1) : 0;
								manualGamesTable->setCurrentCell(rowToSelect, 0);
							}
							break;
						}
					}
				});

				QHBoxLayout *buttonLayout = new QHBoxLayout(containerWidget);
				buttonLayout->setContentsMargins(0, 0, 0, 0); // Remove margens
				buttonLayout->addWidget(removeRowButton);
				containerWidget->setLayout(buttonLayout);

				manualGamesTable->setCellWidget(newRow, 3, containerWidget);
			} else {
				// O jogo não existe mais, remove da lista
				obs_data_array_erase(gamesArray, i);
			}
			obs_data_release(item);
		}
		obs_data_array_release(gamesArray);
	}

	scanSteamCheckbox->setChecked(ConfigManager::get().getScanSteam());
	scanEpicCheckbox->setChecked(ConfigManager::get().getScanEpic());
	scanGogCheckbox->setChecked(ConfigManager::get().getScanGog());
	scanUbiCheckbox->setChecked(ConfigManager::get().getScanUbisoft());
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
	obs_data_set_bool(settings, "scan_steam", scanSteamCheckbox->isChecked());
	obs_data_set_bool(settings, "scan_epic", scanEpicCheckbox->isChecked());
	obs_data_set_bool(settings, "scan_gog", scanGogCheckbox->isChecked());
	obs_data_set_bool(settings, "scan_ubisoft", scanUbiCheckbox->isChecked());

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

	QPushButton *removeRowButton = new QPushButton();
	removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
	removeRowButton->setToolTip(obs_module_text("Settings.RemoveGame"));
	removeRowButton->setCursor(Qt::PointingHandCursor);

	// Cria um container para o botão preencher a célula
	QWidget *containerWidget = new QWidget();

	// Conecta o botão para remover a linha específica
	connect(removeRowButton, &QPushButton::clicked, this, [this, containerWidget]() {
		for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
			if (manualGamesTable->cellWidget(i, 3) == containerWidget) {				
				int rowToRemove = i;
				manualGamesTable->removeRow(rowToRemove);

				// Seleciona a linha anterior ou a primeira, se a removida era a primeira
				int newRowCount = manualGamesTable->rowCount();
				if (newRowCount > 0) {
					int rowToSelect = (rowToRemove > 0) ? (rowToRemove - 1) : 0;
					manualGamesTable->setCurrentCell(rowToSelect, 0);
				}
				break;				
			}
		}
	});

	QHBoxLayout *buttonLayout = new QHBoxLayout(containerWidget);
	buttonLayout->setContentsMargins(0, 0, 0, 0); // Remove margens
	buttonLayout->addWidget(removeRowButton);
	containerWidget->setLayout(buttonLayout);
	manualGamesTable->setCellWidget(newRow, 3, containerWidget);
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

			QPushButton *removeRowButton = new QPushButton();
			removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
			removeRowButton->setToolTip(obs_module_text("Settings.RemoveGame"));
			removeRowButton->setCursor(Qt::PointingHandCursor);

			// Cria um container para o botão preencher a célula
			QWidget *containerWidget = new QWidget();

			connect(removeRowButton, &QPushButton::clicked, this, [this, containerWidget]() {
				for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
					if (manualGamesTable->cellWidget(i, 3) == containerWidget) {						
						int rowToRemove = i;
						manualGamesTable->removeRow(rowToRemove);

						// Seleciona a linha anterior ou a primeira, se a removida era a primeira
						int newRowCount = manualGamesTable->rowCount();
						if (newRowCount > 0) {
							int rowToSelect = (rowToRemove > 0) ? (rowToRemove - 1) : 0;
							manualGamesTable->setCurrentCell(rowToSelect, 0);
						}
						break;						
					}
				}
			});

			QHBoxLayout *buttonLayout = new QHBoxLayout(containerWidget);
			buttonLayout->setContentsMargins(0, 0, 0, 0); // Remove margens
			buttonLayout->addWidget(removeRowButton);
			containerWidget->setLayout(buttonLayout);
			manualGamesTable->setCellWidget(newRow, 3, containerWidget);
			existingExes.insert(exeName);
		}
	}
	manualGamesTable->blockSignals(false);

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
