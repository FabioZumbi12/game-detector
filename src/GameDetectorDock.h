#ifndef GAMEDETECTORDOCK_H
#define GAMEDETECTORDOCK_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QCheckBox>
#include <obs-module.h>

class GameDetectorSettingsDialog;

class GameDetectorDock : public QWidget {
	Q_OBJECT

private: 
	QLabel *statusLabel = nullptr;
	QLabel *twitchStatusLabel = nullptr;
	QLabel *twitchPlatformLabel = nullptr;
	QLabel *twitchTitleLabel = nullptr;
	QLabel *trovoStatusLabel = nullptr;
	QLabel *trovoPlatformLabel = nullptr;
	QLabel *trovoTitleLabel = nullptr;
	QPushButton *executeCommandButton = nullptr;
	QPushButton *setJustChattingButton = nullptr;
	QPushButton *manualGameButton = nullptr;
	QPushButton *settingsButton = nullptr;
	GameDetectorSettingsDialog *settingsDialog = nullptr;
	QCheckBox *autoExecuteCheckbox = nullptr;
	QTimer *cooldownUpdateTimer = nullptr;

	QString configPath;
	QString detectedGameName;
	QString desiredCategory = "Just Chatting";
	QString desiredTitle = QString();
	QString lastTwitchTitle = QString();
	QString lastTrovoTitle = QString();
 
	void restoreStatusLabel();
	void onCooldownStarted(int seconds);
	void onCooldownFinished();
	void updateCooldownLabel();
	
	QTimer *saveDelayTimer = nullptr; 
	QTimer *statusCheckTimer = nullptr;

public:
	explicit GameDetectorDock(QWidget *parent = nullptr);
	~GameDetectorDock();

	void loadSettingsFromConfig();
	void onSetJustChattingClicked();

public slots:
	void onExecuteCommandClicked();

private slots:
	void onSettingsChanged();
	void saveDockSettings();
	void onGameDetected(const QString &gameName);
	void onNoGameDetected();
	void onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString);
	void onAuthenticationRequired();
	void onSettingsButtonClicked();
	void checkWarningsAndStatus();
	void onCategoriesFetched(const QHash<QString, QString> &categories);
};

#endif // GAMEDETECTORDOCK_H