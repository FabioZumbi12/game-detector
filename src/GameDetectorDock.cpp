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
#include <obs-frontend-api.h>
#include <QDialog>
#include <QDialogButtonBox>

GameDetectorDock::GameDetectorDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	detectedGameName = "Just Chatting";
	desiredCategory = "Just Chatting";
	this->desiredTitle = QString();
	statusLabel = new QLabel(obs_module_text("Status.Waiting"));
	statusLabel->setStyleSheet("margin-top: -4px;");
	statusLabel->setWordWrap(true);
	mainLayout->addWidget(statusLabel);

	twitchStatusLabel = new QLabel(this);
	trovoStatusLabel = new QLabel(this);
	// Title labels (separate from category/status labels)
	twitchTitleLabel = new QLabel(this);
	trovoTitleLabel = new QLabel(this);
	// Platform name labels (e.g. "Twitch:")
	twitchPlatformLabel = new QLabel(this);
	trovoPlatformLabel = new QLabel(this);
	twitchStatusLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -4px;");
	trovoStatusLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -4px;");
	twitchTitleLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -2px;");
	trovoTitleLabel->setStyleSheet("font-size: 8pt; color: #888888; margin-top: -2px;");
	twitchPlatformLabel->setStyleSheet("font-weight: bold; margin-top: -6px;");
	trovoPlatformLabel->setStyleSheet("font-weight: bold; margin-top: -6px;");
	twitchStatusLabel->setVisible(false);
	trovoStatusLabel->setVisible(false);
	twitchTitleLabel->setVisible(false);
	trovoTitleLabel->setVisible(false);
	twitchPlatformLabel->setVisible(false);
	trovoPlatformLabel->setVisible(false);
	twitchStatusLabel->setWordWrap(true);
	trovoStatusLabel->setWordWrap(true);
	twitchTitleLabel->setWordWrap(true);
	trovoTitleLabel->setWordWrap(true);
	twitchPlatformLabel->setWordWrap(true);
	trovoPlatformLabel->setWordWrap(true);

	mainLayout->addWidget(twitchPlatformLabel);
	mainLayout->addWidget(twitchTitleLabel);
	mainLayout->addWidget(twitchStatusLabel);
	mainLayout->addWidget(trovoPlatformLabel);
	mainLayout->addWidget(trovoTitleLabel);
	mainLayout->addWidget(trovoStatusLabel);

	QFormLayout *executionLayout = new QFormLayout();

	autoExecuteCheckbox = new QCheckBox(obs_module_text("Dock.AutoExecute"));
	executionLayout->addRow(autoExecuteCheckbox);
	blockAutoUpdateWhileStreamingCheckbox = new QCheckBox(obs_module_text("Dock.BlockAutoUpdateWhileStreaming"));
	executionLayout->addRow(blockAutoUpdateWhileStreamingCheckbox);

	QHBoxLayout *buttonsLayout = new QHBoxLayout();
	executeCommandButton = new QPushButton(obs_module_text("Dock.SetGame"));
	executeCommandButton->setCursor(Qt::PointingHandCursor);
	executeCommandButton->setFixedHeight(executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(executeCommandButton);

	manualGameButton = new QPushButton();
	manualGameButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
	manualGameButton->setToolTip(obs_module_text("Dock.ManualGame.Tooltip"));
	manualGameButton->setCursor(Qt::PointingHandCursor);
	manualGameButton->setFixedSize(executeCommandButton->sizeHint().height(),
				       executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(manualGameButton);

	settingsButton = new QPushButton();
	settingsButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
	settingsButton->setToolTip(obs_module_text("Dock.OpenSettings"));
	settingsButton->setCursor(Qt::PointingHandCursor);
	settingsButton->setFixedSize(executeCommandButton->sizeHint().height(),
				     executeCommandButton->sizeHint().height());
	buttonsLayout->addWidget(settingsButton);

	executionLayout->addRow(buttonsLayout);
	setJustChattingButton = new QPushButton(obs_module_text("Dock.SetJustChatting"));
	setJustChattingButton->setCursor(Qt::PointingHandCursor);
	setJustChattingButton->setFixedHeight(executeCommandButton->sizeHint().height());
	executionLayout->addRow(setJustChattingButton);

	mainLayout->addLayout(executionLayout);

	connect(executeCommandButton, &QPushButton::clicked, this, &GameDetectorDock::onExecuteCommandClicked);
	connect(manualGameButton, &QPushButton::clicked, this, [this]() {
		if (PlatformManager::get().isOnCooldown())
			return;

		QDialog dialog(this);
		dialog.setWindowTitle(obs_module_text("Dock.ManualGame.Title"));
		dialog.setMinimumWidth(300);
		QVBoxLayout *layout = new QVBoxLayout(&dialog);

		QCheckBox *setTitleCheck = new QCheckBox(obs_module_text("Dock.ManualGame.SetTitle"), &dialog);

		// Prefill title from first available platform (Twitch then Trovo) instead of config
		QString prefillTitle;
		if (!ConfigManager::get().getTwitchUserId().isEmpty() && !this->lastTwitchTitle.isEmpty()) {
			prefillTitle = this->lastTwitchTitle;
		} else if (!ConfigManager::get().getTrovoUserId().isEmpty() && !this->lastTrovoTitle.isEmpty()) {
			prefillTitle = this->lastTrovoTitle;
		}

		setTitleCheck->setChecked(!prefillTitle.isEmpty());
		layout->addWidget(setTitleCheck);

		layout->addWidget(new QLabel(obs_module_text("Dock.ManualGame.EnterTitle"), &dialog));
		QLineEdit *titleInput = new QLineEdit(&dialog);
		titleInput->setText(prefillTitle);
		titleInput->setEnabled(setTitleCheck->isChecked());
		connect(setTitleCheck, &QCheckBox::toggled, titleInput, &QLineEdit::setEnabled);
		layout->addWidget(titleInput);

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

		QDialogButtonBox *buttonBox =
			new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout->addWidget(buttonBox);

		connect(buttonBox, &QDialogButtonBox::accepted, [&, twitchCheck, trovoCheck]() {
			QString gameName = input->text().trimmed();
			if (gameName.isEmpty())
				return;

			QStringList platforms;
			if (twitchCheck && twitchCheck->isChecked())
				platforms << "Twitch";
			if (trovoCheck && trovoCheck->isChecked())
				platforms << "Trovo";

			if ((twitchCheck || trovoCheck) && platforms.isEmpty())
				return;

			if (!platforms.isEmpty()) {
				PlatformManager::get().setProperty("targetPlatforms", platforms);
			}

			buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
			input->setEnabled(false);
			if (twitchCheck)
				twitchCheck->setEnabled(false);
			if (trovoCheck)
				trovoCheck->setEnabled(false);
			statusLabel->setText(obs_module_text("Dock.ManualGame.Updating"));
			QString inputTitle = titleInput->text().trimmed();
			bool willSetTitle = true;
			if (setTitleCheck)
				willSetTitle = setTitleCheck->isChecked();
			QString title = willSetTitle ? inputTitle : QString();

			this->desiredCategory = gameName;
			this->desiredTitle = inputTitle;

			// do not persist last title in config; use runtime prefill from fetched platform titles
			disconnect(&PlatformManager::get(), &PlatformManager::categoryUpdateFinished, &dialog, nullptr);
			connect(&PlatformManager::get(), &PlatformManager::categoryUpdateFinished, &dialog,
				[&, gameName](bool success, const QString &name, const QString &error) {
					if (name.compare(gameName, Qt::CaseInsensitive) == 0) {
						if (success)
							dialog.accept();
						else {
							statusLabel->setText(
								QString(obs_module_text("Dock.ManualGame.Error"))
									.arg(error));
							buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
							input->setEnabled(true);
							if (twitchCheck)
								twitchCheck->setEnabled(true);
							if (trovoCheck)
								trovoCheck->setEnabled(true);
						}
					}
				});

			if (!PlatformManager::get().updateCategory(gameName, title, true)) {
				statusLabel->setText(obs_module_text("Dock.ManualGame.Cooldown"));
				buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
				input->setEnabled(true);
				if (twitchCheck)
					twitchCheck->setEnabled(true);
				if (trovoCheck)
					trovoCheck->setEnabled(true);
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

	connect(&PlatformManager::get(), &PlatformManager::categoriesFetched, this,
		&GameDetectorDock::onCategoriesFetched);

	connect(&PlatformManager::get(), &PlatformManager::cooldownStarted, this, &GameDetectorDock::onCooldownStarted);
	connect(&PlatformManager::get(), &PlatformManager::cooldownFinished, this,
		&GameDetectorDock::onCooldownFinished);

	connect(autoExecuteCheckbox, &QCheckBox::checkStateChanged, this, &GameDetectorDock::onSettingsChanged);
	connect(blockAutoUpdateWhileStreamingCheckbox, &QCheckBox::checkStateChanged, this,
		&GameDetectorDock::onSettingsChanged);

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
	obs_data_set_bool(settings, "block_auto_update_while_streaming",
			  blockAutoUpdateWhileStreamingCheckbox->isChecked());

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
	PlatformManager::get().updateCategory(desiredCategory, QString(), true);
}

void GameDetectorDock::loadSettingsFromConfig()
{
	autoExecuteCheckbox->blockSignals(true);
	autoExecuteCheckbox->setChecked(ConfigManager::get().getExecuteAutomatically());
	autoExecuteCheckbox->blockSignals(false);
	blockAutoUpdateWhileStreamingCheckbox->blockSignals(true);
	blockAutoUpdateWhileStreamingCheckbox->setChecked(ConfigManager::get().getBlockAutoUpdateWhileStreaming());
	blockAutoUpdateWhileStreamingCheckbox->blockSignals(false);
	checkWarningsAndStatus();
}

void GameDetectorDock::onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString)
{
	if (!success) {
		bool twitchConfigured = !ConfigManager::get().getTwitchUserId().isEmpty();
		bool trovoConfigured = !ConfigManager::get().getTrovoUserId().isEmpty();

		if (errorString.contains("Twitch") && !twitchConfigured)
			return;
		if (errorString.contains("Trovo") && !trovoConfigured)
			return;
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
		QString data = categories.value("Twitch");
		QString category, title;
		int sep = data.indexOf("|||");
		if (sep >= 0) {
			category = data.left(sep);
			title = data.mid(sep + 3);
		} else {
			category = data;
		}
		if (category.isEmpty())
			category = obs_module_text("Status.CategoryNotAvailable");
		twitchStatusLabel->setText(QString(obs_module_text("Dock.Platform.Category")).arg(category));
		// store last known title for prefill
		this->lastTwitchTitle = title.trimmed();
		if (title.isEmpty()) {
			twitchTitleLabel->setText("");
			twitchTitleLabel->setVisible(false);
		} else {
			twitchTitleLabel->setText(QString(obs_module_text("Dock.Platform.Title")).arg(title));
			// set platform label text
			twitchPlatformLabel->setText(obs_module_text("Dock.PlatformName.Twitch"));
			twitchTitleLabel->setVisible(true);
		}
	}

	if (trovoStatusLabel->isVisible() && categories.contains("Trovo")) {
		QString data = categories.value("Trovo");
		QString category, title;
		int sep = data.indexOf("|||");
		if (sep >= 0) {
			category = data.left(sep);
			title = data.mid(sep + 3);
		} else {
			category = data;
		}
		if (category.isEmpty())
			category = obs_module_text("Status.CategoryNotAvailable");
		trovoStatusLabel->setText(QString(obs_module_text("Dock.Platform.Category")).arg(category));
		if (title.isEmpty()) {
			trovoTitleLabel->setText("");
			trovoTitleLabel->setVisible(false);
		} else {
			trovoTitleLabel->setText(QString(obs_module_text("Dock.Platform.Title")).arg(title));
			// set platform label text
			trovoPlatformLabel->setText(obs_module_text("Dock.PlatformName.Trovo"));
			trovoTitleLabel->setVisible(true);
		}
		// store last known title for prefill
		this->lastTrovoTitle = title.trimmed();
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
	auto trovoManager = PlatformManager::get().findChild<TrovoAuthManager *>();
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
			if (remaining > 0)
				onCooldownStarted(remaining);
		}
		return;
	}

	bool shouldAutoUpdateNow = !blockAutoUpdateWhileStreamingCheckbox->isChecked() ||
				   obs_frontend_streaming_active();
	if (autoExecuteCheckbox->isChecked() && shouldAutoUpdateNow) {
		PlatformManager::get().updateCategory(desiredCategory);
	}

	restoreStatusLabel();

	if (twitchConnected || trovoConnected) {
		PlatformManager::get().fetchCurrentCategories();
	}

	if (twitchConnected) {
		twitchPlatformLabel->setVisible(true);
		if (twitchPlatformLabel->text().isEmpty())
			twitchPlatformLabel->setText(obs_module_text("Dock.PlatformName.Twitch"));
		twitchStatusLabel->setVisible(true);
		if (twitchStatusLabel->text().isEmpty()) {
			twitchStatusLabel->setText(obs_module_text("Status.Fetching"));
		}
	} else {
		twitchPlatformLabel->setVisible(false);
		twitchPlatformLabel->setText("");
		twitchStatusLabel->setVisible(false);
		twitchStatusLabel->setText("");
	}

	if (trovoConnected) {
		trovoPlatformLabel->setVisible(true);
		if (trovoPlatformLabel->text().isEmpty())
			trovoPlatformLabel->setText(obs_module_text("Dock.PlatformName.Trovo"));
		trovoStatusLabel->setVisible(true);
		if (trovoStatusLabel->text().isEmpty()) {
			trovoStatusLabel->setText(obs_module_text("Status.Fetching"));
		}
	} else {
		trovoPlatformLabel->setVisible(false);
		trovoPlatformLabel->setText("");
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
	if (executeCommandButton)
		executeCommandButton->setEnabled(false);
	if (setJustChattingButton)
		setJustChattingButton->setEnabled(false);
	if (manualGameButton)
		manualGameButton->setEnabled(false);
	cooldownUpdateTimer->setProperty("remaining", seconds);
	updateCooldownLabel();
	cooldownUpdateTimer->start(1000);
}

void GameDetectorDock::onCooldownFinished()
{
	cooldownUpdateTimer->stop();
	GameDetector::get().startScanning();
	if (executeCommandButton)
		executeCommandButton->setEnabled(true);
	if (setJustChattingButton)
		setJustChattingButton->setEnabled(true);
	if (manualGameButton)
		manualGameButton->setEnabled(true);
	checkWarningsAndStatus();
}

void GameDetectorDock::updateCooldownLabel()
{
	int remaining = cooldownUpdateTimer->property("remaining").toInt();
	if (remaining >= 0) {
		QString timeStr = QTime(0, 0).addSecs(remaining).toString("mm:ss");
		QString currentGameText = QString(obs_module_text("Status.Playing")).arg(desiredCategory);
		statusLabel->setText(QString(obs_module_text("Dock.OnCooldown")).arg(currentGameText).arg(timeStr));
		cooldownUpdateTimer->setProperty("remaining", remaining - 1);
	} else {
		onCooldownFinished();
	}
}

GameDetectorDock::~GameDetectorDock()
{
	if (cooldownUpdateTimer->isActive())
		cooldownUpdateTimer->stop();
	if (saveDelayTimer->isActive())
		saveDelayTimer->stop();
	if (statusCheckTimer->isActive())
		statusCheckTimer->stop();
}
