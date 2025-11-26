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
	QPushButton *executeCommandButton = nullptr;
	QPushButton *setJustChattingButton = nullptr;
	QPushButton *settingsButton = nullptr;
	GameDetectorSettingsDialog *settingsDialog = nullptr;
	QCheckBox *autoExecuteCheckbox = nullptr;
	QTimer *cooldownUpdateTimer = nullptr;

	QString configPath;
	QString detectedGameName;
	QString desiredCategory = "Just Chatting";
 
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

private slots:
	void onSettingsChanged();
	void saveDockSettings();
	void onGameDetected(const QString &gameName);
	void onNoGameDetected();
	void onExecuteCommandClicked();
	void onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString);
	void onAuthenticationRequired();
	void onSettingsButtonClicked();
	void checkWarningsAndStatus();
};

#endif // GAMEDETECTORDOCK_H