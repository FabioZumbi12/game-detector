#include "GameDetectorDock.h"
#include "GameDetector.h"
#include "PlatformManager.h"
#include "TwitchAuthManager.h"
#include "ConfigManager.h"
#include "GameDetectorSettingsDialog.h"
#include "TrovoAuthManager.h"

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
#include <QDialog>
#include <QDialogButtonBox>

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	detectedGameName = "Just Chatting";
	desiredCategory = "Just Chatting";
	statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	statusLabel->setStyleSheet("margin-top: -4px;");
	statusLabel->setWordWrap(true);
	mainLayout->addWidget(statusLabel);

	twitchStatusLabel = new QLabel(this);
	trovoStatusLabel = new QLabel(this);
	twitchStatusLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -4px;");
	trovoStatusLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -4px;");
	twitchStatusLabel->setVisible(false);
	trovoStatusLabel->setVisible(false);
	twitchStatusLabel->setWordWrap(true);
	trovoStatusLabel->setWordWrap(true);

	mainLayout->addWidget(twitchStatusLabel);
	mainLayout->addWidget(trovoStatusLabel);

	QFormLayout *executionLayout = new QFormLayout();

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	executionLayout->addRow(autoExecuteCheckbox);

	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	executeCommandButton = new QPushButton(obs_module_text("Dock.SetGame"));
	executeCommandButton->setFixedHeight(executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(executeCommandButton);

	manualGameButton = new QPushButton();
	manualGameButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
	manualGameButton->setToolTip(obs_module_text("Dock.ManualGame.Tooltip"));
	manualGameButton->setFixedSize(executeCommandButton->sizeHint().height(), executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(manualGameButton);

	settingsButton = new QPushButton();
	settingsButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
	settingsButton->setToolTip(obs_module_text("Dock.OpenSettings"));
	settingsButton->setCursor(Qt::PointingHandCursor);
	settingsButton->setFixedSize(executeCommandButton->sizeHint().height(), executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(settingsButton);

	executionLayout->addRow(buttonsLayout);
	setJustChattingButton = new QPushButton(obs_module_text("Dock.SetJustChatting"));
	setJustChattingButton->setFixedHeight(executeCommandButton->sizeHint().height());
	executionLayout->addRow(setJustChattingButton);

	mainLayout->addLayout(executionLayout);

	connect(executeCommandButton, &QPushButton::clicked, this,
			&GameDetectorDock::onExecuteCommandClicked);
	connect(manualGameButton, &QPushButton::clicked, this, [this]() {
		if (PlatformManager::get().isOnCooldown()) return;

		QDialog dialog(this);
		dialog.setWindowTitle(obs_module_text("Dock.ManualGame.Title"));
		dialog.setMinimumWidth(300);
		QVBoxLayout *layout = new QVBoxLayout(&dialog);

		layout->addWidget(new QLabel(obs_module_text("Dock.ManualGame.EnterName"), &dialog));
		QLineEdit *input = new QLineEdit(&dialog);
		input->setText(this->desiredCategory);
		layout->addWidget(input);

		bool twitchConfigured = !ConfigManager::get().getTwitchUserId().isEmpty();
		bool trovoConfigured = !ConfigManager::get().getTrovoUserId().isEmpty();
		QCheckBox *twitchCheck = nullptr;
		QCheckBox *trovoCheck = nullptr;

		if (twitchConfigured) {
			twitchCheck = new QCheckBox("Twitch", &dialog);
			twitchCheck->setChecked(true);
			layout->addWidget(twitchCheck);
		}

		if (trovoConfigured) {
			trovoCheck = new QCheckBox("Trovo", &dialog);
			trovoCheck->setChecked(true);
			layout->addWidget(trovoCheck);
		}

		QLabel *statusLabel = new QLabel(&dialog);
		statusLabel->setStyleSheet("color: #888; font-size: 9pt;");
		layout->addWidget(statusLabel);

		QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout->addWidget(buttonBox);

		connect(buttonBox, &QDialogButtonBox::accepted, [&, twitchCheck, trovoCheck]() {
			QString gameName = input->text().trimmed();
			if (gameName.isEmpty()) return;

			QStringList platforms;
			if (twitchCheck && twitchCheck->isChecked()) platforms << "Twitch";
			if (trovoCheck && trovoCheck->isChecked()) platforms << "Trovo";

			if ((twitchCheck || trovoCheck) && platforms.isEmpty()) return;

			if (!platforms.isEmpty()) {
				PlatformManager::get().setProperty("targetPlatforms", platforms);
			}

			buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
			input->setEnabled(false);
			if (twitchCheck) twitchCheck->setEnabled(false);
			if (trovoCheck) trovoCheck->setEnabled(false);
			statusLabel->setText(obs_module_text("Dock.ManualGame.Updating"));
			this->desiredCategory = gameName;

			disconnect(&PlatformManager::get(), &PlatformManager::categoryUpdateFinished, &dialog, nullptr);
			connect(&PlatformManager::get(), &PlatformManager::categoryUpdateFinished, &dialog,
				[&, gameName](bool success, const QString &name, const QString &error) {
					if (name.compare(gameName, Qt::CaseInsensitive) == 0) {
						if (success) dialog.accept();
						else {
							statusLabel->setText(QString(obs_module_text("Dock.ManualGame.Error")).arg(error));
							buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
							input->setEnabled(true);
							if (twitchCheck) twitchCheck->setEnabled(true);
							if (trovoCheck) trovoCheck->setEnabled(true);
						}
					}
				});
			
			if (!PlatformManager::get().updateCategory(gameName, true)) {
				statusLabel->setText(obs_module_text("Dock.ManualGame.Cooldown"));
				buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
				input->setEnabled(true);
				if (twitchCheck) twitchCheck->setEnabled(true);
				if (trovoCheck) trovoCheck->setEnabled(true);
			}
		});
		connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		dialog.exec();
	});
	connect(setJustChattingButton, &QPushButton::clicked, this, &GameDetectorDock::onSetJustChattingClicked);
	connect(&GameDetector::get(), &GameDetector::gameDetected, this, &GameDetectorDock::onGameDetected);
	connect(&GameDetector::get(), &GameDetector::noGameDetected, this, &GameDetectorDock::onNoGameDetected);
	connect(&PlatformManager::get(), &PlatformManager::categoryUpdateFinished, this,
		QOverload<bool, const QString &, const QString &>::of(&GameDetectorDock::onCategoryUpdateFinished));
	connect(&PlatformManager::get(), &PlatformManager::authenticationRequired, this,
		&GameDetectorDock::onAuthenticationRequired);

	connect(&PlatformManager::get(), &PlatformManager::categoriesFetched, this, &GameDetectorDock::onCategoriesFetched);

	connect(&PlatformManager::get(), &PlatformManager::cooldownStarted, this, &GameDetectorDock::onCooldownStarted);
	connect(&PlatformManager::get(), &PlatformManager::cooldownFinished, this, &GameDetectorDock::onCooldownFinished);


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
	if (PlatformManager::get().isOnCooldown()) {
		return;
	}
	this->desiredCategory = detectedGameName;
	PlatformManager::get().updateCategory(desiredCategory);
}

void GameDetectorDock::onSetJustChattingClicked()
{
	if (PlatformManager::get().isOnCooldown()) {
		return;
	}
	this->desiredCategory = "Just Chatting";
	PlatformManager::get().updateCategory(desiredCategory, true);
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
	if (!success) {
		bool twitchConfigured = !ConfigManager::get().getTwitchUserId().isEmpty();
		bool trovoConfigured = !ConfigManager::get().getTrovoUserId().isEmpty();

		if (errorString.contains("Twitch") && !twitchConfigured) return;
		if (errorString.contains("Trovo") && !trovoConfigured) return;
	}

	cooldownUpdateTimer->stop();
	if (success) {
		statusLabel->setText(QString(obs_module_text("Dock.CategoryUpdated")).arg(gameName));
		PlatformManager::get().fetchCurrentCategories();
	} else {
		statusLabel->setText(QString(errorString).arg(gameName));
	}

	QTimer::singleShot(3000, this, &GameDetectorDock::restoreStatusLabel);
}

void GameDetectorDock::onCategoriesFetched(const QHash<QString, QString> &categories)
{
	if (twitchStatusLabel->isVisible() && categories.contains("Twitch")) {
		QString category = categories.value("Twitch");
		if (category.isEmpty())
			category = obs_module_text("Status.CategoryNotAvailable");
		twitchStatusLabel->setText(QString("Twitch: %1").arg(category));
	}

	if (trovoStatusLabel->isVisible() && categories.contains("Trovo")) {
		QString category = categories.value("Trovo");
		if (category.isEmpty())
			category = obs_module_text("Status.CategoryNotAvailable");
		trovoStatusLabel->setText(QString("Trovo: %1").arg(category));
	}
}

void GameDetectorDock::checkWarningsAndStatus()
{
	if (GameDetector::get().isGameListEmpty()) {
        statusLabel->setText(obs_module_text("Status.Warning.NoGames"));
        return;
	}

	bool twitchConnected = !ConfigManager::get().getTwitchUserId().isEmpty();
	bool trovoConnected = false;
	auto trovoManager = PlatformManager::get().findChild<TrovoAuthManager*>();
	if (trovoManager) {
		trovoConnected = trovoManager->isAuthenticated();
	}

	if (!twitchConnected && !trovoConnected) {
		statusLabel->setText(obs_module_text("Status.Warning.NotConnected"));
		twitchStatusLabel->setVisible(false);
		trovoStatusLabel->setVisible(false);
		return;
	}

	if (PlatformManager::get().isOnCooldown()) {
		if (!cooldownUpdateTimer->isActive()) {
			int remaining = PlatformManager::get().getCooldownRemaining();
			if (remaining > 0) onCooldownStarted(remaining);
		}
		return;
	}

	if (autoExecuteCheckbox->isChecked()) {
		PlatformManager::get().updateCategory(desiredCategory);
	}

	restoreStatusLabel();

	if (twitchConnected || trovoConnected) {
		PlatformManager::get().fetchCurrentCategories();
	}

	if (twitchConnected) {
		twitchStatusLabel->setVisible(true);
		if (twitchStatusLabel->text().isEmpty()) {
			twitchStatusLabel->setText(obs_module_text("Status.Fetching"));
		}
	} else {
		twitchStatusLabel->setVisible(false);
		twitchStatusLabel->setText("");
	}

	if (trovoConnected) {
		trovoStatusLabel->setVisible(true);
		if (trovoStatusLabel->text().isEmpty()) {
			trovoStatusLabel->setText(obs_module_text("Status.Fetching"));
		}
	} else {
		trovoStatusLabel->setVisible(false);
		trovoStatusLabel->setText("");
	}
}

void GameDetectorDock::restoreStatusLabel()
{
	if (cooldownUpdateTimer->isActive()) {
	} else if (PlatformManager::get().isOnCooldown()) {
		cooldownUpdateTimer->start(1000);
	}

	if (detectedGameName != "Just Chatting") {
		statusLabel->setText(QString(obs_module_text("Status.Playing")).arg(detectedGameName));
	} else {
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
	GameDetector::get().stopScanning();
	if (executeCommandButton) executeCommandButton->setEnabled(false);
	if (setJustChattingButton) setJustChattingButton->setEnabled(false);
	if (manualGameButton) manualGameButton->setEnabled(false);
	cooldownUpdateTimer->setProperty("remaining", seconds);
	updateCooldownLabel();
	cooldownUpdateTimer->start(1000);
}

void GameDetectorDock::onCooldownFinished()
{
	cooldownUpdateTimer->stop();
	GameDetector::get().startScanning();
	if (executeCommandButton) executeCommandButton->setEnabled(true);
	if (setJustChattingButton) setJustChattingButton->setEnabled(true);
	if (manualGameButton) manualGameButton->setEnabled(true);
	checkWarningsAndStatus();
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
		onCooldownFinished();
	}
}

GameDetectorDock::~GameDetectorDock()
{
	if (cooldownUpdateTimer->isActive()) cooldownUpdateTimer->stop();
	if (saveDelayTimer->isActive()) saveDelayTimer->stop();
	if (statusCheckTimer->isActive()) statusCheckTimer->stop();
}