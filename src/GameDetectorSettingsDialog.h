#ifndef GAMEDETECTORSETTINGSDIALOG_H
#define GAMEDETECTORSETTINGSDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QCheckBox;

class GameDetectorSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit GameDetectorSettingsDialog(QWidget *parent = nullptr);
	void rescanGames();

	void loadSettings();
	void saveSettings();
	void updateActionModeUI(int index);
	void disconectOnChangeComboBox(int index);

private:
	QPushButton *manageGamesButton = nullptr;
	QLabel      *authStatusLabel = nullptr;
	QPushButton *authButton      = nullptr;
	QPushButton *disconnectButton = nullptr;
	QPushButton *okButton = nullptr;
	QPushButton *cancelButton = nullptr;
	QLabel      *commandLabel = nullptr;
	QLineEdit   *commandInput = nullptr;
	QLabel      *noGameCommandLabel = nullptr;
	QLineEdit   *noGameCommandInput = nullptr;
	QLabel 		*delayLabel = nullptr;
	QSpinBox 	*delaySpinBox = nullptr;
	QComboBox   *twitchActionComboBox = nullptr;
	QCheckBox   *unifiedAuthCheckbox = nullptr;
	QCheckBox   *scanSteamCheckbox = nullptr;
	QCheckBox   *scanEpicCheckbox = nullptr;
	QCheckBox   *scanGogCheckbox = nullptr;
	QCheckBox   *scanUbiCheckbox = nullptr;
	QCheckBox   *scanOnStartupCheckbox = nullptr;
	QCheckBox   *scanPeriodicallyCheckbox = nullptr;
	QSpinBox    *scanIntervalSpinbox = nullptr;

private slots:
	void onAuthenticationFinished(bool success, const QString &username);
	void onDisconnectClicked();
	void onManageGamesClicked();
};

#endif // GAMEDETECTORSETTINGSDIALOG_H
