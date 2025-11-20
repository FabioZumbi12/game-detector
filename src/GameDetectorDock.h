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

class GameDetectorDock : public QWidget {
	Q_OBJECT

private:
	QLabel *statusLabel = nullptr;
	QLabel *twitchActionLabel = nullptr;
	QComboBox *twitchActionComboBox = nullptr;
	QLabel *commandLabel = nullptr;
	QLineEdit *commandInput = nullptr;
	QLabel *noGameCommandLabel = nullptr;
	QLineEdit *noGameCommandInput = nullptr;
	QPushButton *executeCommandButton = nullptr;
	QCheckBox *autoExecuteCheckbox = nullptr;

	QString configPath;
	QString detectedGameName;

	void executeAction(const QString &gameName);
	void updateActionModeUI(int index);

	QTimer *saveDelayTimer = nullptr;

public:
	explicit GameDetectorDock(QWidget *parent = nullptr);
	~GameDetectorDock();

	void loadSettingsFromConfig();

private slots:
	void onSettingsChanged();
	void saveDockSettings();
	void onGameDetected(const QString &gameName, const QString &processName);
	void onNoGameDetected();
	void onExecuteCommandClicked();
	void onCategoryUpdateFinished(bool success, const QString &gameName, const QString &errorString);
	void onAuthenticationRequired();
};

#endif // GAMEDETECTORDOCK_H