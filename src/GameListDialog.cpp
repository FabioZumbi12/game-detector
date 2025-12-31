#include "GameListDialog.h"
#include "GameDetector.h"
#include "IconProvider.h"
#include "ConfigManager.h"
#include "GameDetectorSettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QStyle>
#include <QFileDialog>
#include <QGroupBox>
#include <QCheckBox>
#include <obs-module.h>

GameListDialog::GameListDialog(GameDetectorSettingsDialog *parent) : QDialog(parent)
{
	parentWidget = parent;
	setWindowTitle(obs_module_text("GameList.WindowTitle"));
	setMinimumSize(800, 600);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	QGroupBox *gamesGroup = new QGroupBox(obs_module_text("Settings.GameList"));
	QVBoxLayout *gamesLayout = new QVBoxLayout();

	manualGamesTable = new QTableWidget();
	manualGamesTable->setColumnCount(5);
	manualGamesTable->setHorizontalHeaderLabels(QStringList() << ""
								<< obs_module_text("Table.Header.Name")
								<< obs_module_text("Table.Header.Executable")
								<< obs_module_text("Table.Header.Path")
								<< obs_module_text("Table.Header.Actions"));
	manualGamesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	manualGamesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	manualGamesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	manualGamesTable->setColumnHidden(3, true);
	manualGamesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	gamesLayout->addWidget(manualGamesTable);

	QHBoxLayout *tableButtonsLayout = new QHBoxLayout();
	addGameButton = new QPushButton(obs_module_text("Settings.AddGame"));
	addGameButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
	clearTableButton = new QPushButton(obs_module_text("Settings.ClearList"));
	clearTableButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
	toggleAllButton = new QPushButton(obs_module_text("GameList.ToggleAll"));
	toggleAllButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
	rescanButton = new QPushButton(obs_module_text("Settings.ScanGames"));
	rescanButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	rescanButton->setProperty("baseText", rescanButton->text());
	tableButtonsLayout->addWidget(addGameButton);
	tableButtonsLayout->addWidget(toggleAllButton);
	tableButtonsLayout->addWidget(clearTableButton);
	tableButtonsLayout->addWidget(rescanButton);
	tableButtonsLayout->addStretch(1);
	gamesLayout->addLayout(tableButtonsLayout);

	gamesGroup->setLayout(gamesLayout);
	mainLayout->addWidget(gamesGroup);

	QHBoxLayout *dialogButtonsLayout = new QHBoxLayout();
	okButton = new QPushButton(obs_module_text("OK"));
	cancelButton = new QPushButton(obs_module_text("Cancel"));
    QLabel *developerLabel = new QLabel(
		"<a href=\"https://github.com/FabioZumbi12\" style=\"color: gray; text-decoration: none;\"><i>Developed by FabioZumbi12</i></a>");
	developerLabel->setOpenExternalLinks(true);
	dialogButtonsLayout->addStretch(1);
	dialogButtonsLayout->addWidget(developerLabel);
	dialogButtonsLayout->addWidget(okButton);
	dialogButtonsLayout->addWidget(cancelButton);
	mainLayout->addLayout(dialogButtonsLayout);

	connect(addGameButton, &QPushButton::clicked, this, &GameListDialog::onAddGameClicked);
	connect(clearTableButton, &QPushButton::clicked, this, &GameListDialog::onClearTableClicked);
	connect(toggleAllButton, &QPushButton::clicked, this, &GameListDialog::onToggleAllClicked);
	connect(rescanButton, &QPushButton::clicked, this, [this]() {
		rescanButton->setEnabled(false);
		rescanButton->setText(obs_module_text("Settings.Scanning"));
		parentWidget->rescanGames();
	});
	connect(&GameDetector::get(), &GameDetector::automaticScanFinished, this,
		&GameListDialog::onAutomaticScanFinished);
	connect(okButton, &QPushButton::clicked, this, [this]() {
		saveGames();
		accept();
	});

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	loadGames();
}

GameListDialog::~GameListDialog() {}

void GameListDialog::loadGames()
{
	obs_data_array_t *gamesArray = ConfigManager::get().getManualGames();
	if (gamesArray) {
		manualGamesTable->setRowCount(0);
		size_t count = obs_data_array_count(gamesArray);
		for (size_t i = 0; i < count; ++i) {
			obs_data_t *item = obs_data_array_item(gamesArray, i);
			QString gameName = obs_data_get_string(item, "name");
			QString exeName = obs_data_get_string(item, "exe");
			QString exePath = obs_data_get_string(item, "path");
			bool enabled = obs_data_get_bool(item, "enabled");

			int newRow = manualGamesTable->rowCount();
			manualGamesTable->insertRow(newRow);

			QCheckBox *enabledCheckbox = new QCheckBox();
			enabledCheckbox->setChecked(enabled);
			QWidget *checkboxWidget = new QWidget();
			QHBoxLayout *checkboxLayout = new QHBoxLayout(checkboxWidget);
			checkboxLayout->addWidget(enabledCheckbox);
			checkboxLayout->setAlignment(Qt::AlignCenter);
			checkboxLayout->setContentsMargins(0,0,0,0);
			manualGamesTable->setCellWidget(newRow, 0, checkboxWidget);

			QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);
			nameItem->setIcon(IconProvider::getIconForFile(exePath));
			manualGamesTable->setItem(newRow, 1, nameItem);
			QTableWidgetItem *exeItem = new QTableWidgetItem(exeName);
			exeItem->setFlags(exeItem->flags() & ~Qt::ItemIsEditable);
			manualGamesTable->setItem(newRow, 2, exeItem);
			manualGamesTable->setItem(newRow, 3, new QTableWidgetItem(exePath));

			QPushButton *removeRowButton = new QPushButton();
			removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
			connect(removeRowButton, &QPushButton::clicked, this, [this, removeRowButton]() {
				for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
					if (manualGamesTable->cellWidget(i, 4) == removeRowButton) {
						manualGamesTable->removeRow(i);
						break;
					}
				}
			});
			manualGamesTable->setCellWidget(newRow, 4, removeRowButton);

			obs_data_release(item);
		}
		obs_data_array_release(gamesArray);
	}
}

void GameListDialog::saveGames()
{
	obs_data_t *settings = ConfigManager::get().getSettings();

	obs_data_array_t *gamesArray = obs_data_array_create();
	for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
		QWidget* cellWidget = manualGamesTable->cellWidget(i, 0);
		QCheckBox* checkbox = cellWidget ? cellWidget->findChild<QCheckBox*>() : nullptr;

		obs_data_t *item = obs_data_create();
		obs_data_set_bool(item, "enabled", checkbox ? checkbox->isChecked() : true);
		obs_data_set_string(item, "name", manualGamesTable->item(i, 1)->text().toStdString().c_str());
		obs_data_set_string(item, "exe", manualGamesTable->item(i, 2)->text().toStdString().c_str());
		obs_data_set_string(item, "path", manualGamesTable->item(i, 3)->text().toStdString().c_str());
		obs_data_array_push_back(gamesArray, item);
		obs_data_release(item);
	}
	obs_data_set_array(settings, ConfigManager::MANUAL_GAMES_KEY, gamesArray);
	obs_data_array_release(gamesArray);

	ConfigManager::get().save(settings);
	GameDetector::get().onSettingsChanged();
	GameDetector::get().setupPeriodicScan();
}

void GameListDialog::onAddGameClicked()
{
	QString filePath = QFileDialog::getOpenFileName(this, "Select Game Executable", "", "Executables (*.exe)");
	if (filePath.isEmpty()) {
		return;
	}

	QFileInfo fileInfo(filePath);
	QString exeName = fileInfo.fileName();
	QDir gameDir = fileInfo.dir();
	const QSet<QString> binaryFolderNames = {"bin", "binaries", "win64", "win_x64", "x64", "shipping"};
	while (binaryFolderNames.contains(gameDir.dirName().toLower())) {
		if (!gameDir.cdUp())
			break;
	}
	QString gameName = gameDir.dirName();

	if (gameName.isEmpty() || binaryFolderNames.contains(gameName.toLower())) {
		gameName = fileInfo.completeBaseName();
	}

	int newRow = manualGamesTable->rowCount();
	manualGamesTable->insertRow(newRow);

	QCheckBox *enabledCheckbox = new QCheckBox();
	enabledCheckbox->setChecked(true);
	QWidget *checkboxWidget = new QWidget();
	QHBoxLayout *checkboxLayout = new QHBoxLayout(checkboxWidget);
	checkboxLayout->addWidget(enabledCheckbox);
	checkboxLayout->setAlignment(Qt::AlignCenter);
	checkboxLayout->setContentsMargins(0,0,0,0);
	manualGamesTable->setCellWidget(newRow, 0, checkboxWidget);

	QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);
	nameItem->setIcon(IconProvider::getIconForFile(filePath));
	manualGamesTable->setItem(newRow, 1, nameItem);
	QTableWidgetItem *exeItem = new QTableWidgetItem(exeName);
	exeItem->setFlags(exeItem->flags() & ~Qt::ItemIsEditable);
	manualGamesTable->setItem(newRow, 2, exeItem);
	manualGamesTable->setItem(newRow, 3, new QTableWidgetItem(filePath));

	QPushButton *removeRowButton = new QPushButton();
	removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
	connect(removeRowButton, &QPushButton::clicked, this, [this, removeRowButton]() {
		for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
			if (manualGamesTable->cellWidget(i, 4) == removeRowButton) {
				manualGamesTable->removeRow(i);
				break;
			}
		}
	});
	manualGamesTable->setCellWidget(newRow, 4, removeRowButton);
}

void GameListDialog::onClearTableClicked()
{
	manualGamesTable->setRowCount(0);
}

void GameListDialog::onToggleAllClicked()
{
	int rowCount = manualGamesTable->rowCount();
	if (rowCount == 0) {
		return;
	}

	// Determina o novo estado com base no primeiro item
	QWidget *firstCellWidget = manualGamesTable->cellWidget(0, 0);
	QCheckBox *firstCheckbox = firstCellWidget ? firstCellWidget->findChild<QCheckBox *>() : nullptr;
	bool newState = firstCheckbox ? !firstCheckbox->isChecked() : true;

	// Aplica o novo estado a todos os checkboxes
	for (int i = 0; i < rowCount; ++i) {
		QWidget *cellWidget = manualGamesTable->cellWidget(i, 0);
		QCheckBox *checkbox = cellWidget ? cellWidget->findChild<QCheckBox *>() : nullptr;
		if (checkbox)
			checkbox->setChecked(newState);
	}
}

void GameListDialog::onAutomaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames)
{
	QSet<QString> existingPaths;
	for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
		existingPaths.insert(manualGamesTable->item(i, 3)->text());
	}

	for (const auto &gameTuple : foundGames) {
		QString gameName = std::get<0>(gameTuple);
		QString exeName = std::get<1>(gameTuple);
		QString exePath = std::get<2>(gameTuple);

		if (existingPaths.contains(exePath)) {
			continue; // Pula se o jogo já estiver na lista
		}

		int newRow = manualGamesTable->rowCount();
		manualGamesTable->insertRow(newRow);

		QCheckBox *enabledCheckbox = new QCheckBox();
		enabledCheckbox->setChecked(true);
		QWidget *checkboxWidget = new QWidget();
		QHBoxLayout *checkboxLayout = new QHBoxLayout(checkboxWidget);
		checkboxLayout->addWidget(enabledCheckbox);
		checkboxLayout->setAlignment(Qt::AlignCenter);
		checkboxLayout->setContentsMargins(0,0,0,0);
		manualGamesTable->setCellWidget(newRow, 0, checkboxWidget);

		QTableWidgetItem *nameItem = new QTableWidgetItem(gameName);
		nameItem->setIcon(IconProvider::getIconForFile(exePath));
		manualGamesTable->setItem(newRow, 1, nameItem);
		QTableWidgetItem *exeItem = new QTableWidgetItem(exeName);
		exeItem->setFlags(exeItem->flags() & ~Qt::ItemIsEditable);
		manualGamesTable->setItem(newRow, 2, exeItem);
		manualGamesTable->setItem(newRow, 3, new QTableWidgetItem(exePath));

		QPushButton *removeRowButton = new QPushButton();
		removeRowButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
		connect(removeRowButton, &QPushButton::clicked, this, [this, removeRowButton]() {
			for (int i = 0; i < manualGamesTable->rowCount(); ++i) {
				if (manualGamesTable->cellWidget(i, 4) == removeRowButton) {
					manualGamesTable->removeRow(i);
					break;
				}
			}
		});
		manualGamesTable->setCellWidget(newRow, 4, removeRowButton);
	}

	rescanButton->setEnabled(true);
	rescanButton->setText(obs_module_text("Settings.ScanGames"));
}
