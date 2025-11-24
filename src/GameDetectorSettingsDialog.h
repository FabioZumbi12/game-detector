#ifndef GAMEDETECTORSETTINGSDIALOG_H
#define GAMEDETECTORSETTINGSDIALOG_H

#include <QDialog>
#include <tuple>
#include <QPair>
#include <obs-module.h>

// Forward declarations
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QCheckBox;

class GameDetectorSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit GameDetectorSettingsDialog(QWidget *parent = nullptr);
	~GameDetectorSettingsDialog();

private:
	void loadSettings();
	void saveSettings();

	// UI Elements
	QTableWidget *manualGamesTable = nullptr;
	QPushButton *addGameButton = nullptr;
	QPushButton *clearTableButton = nullptr;
	QCheckBox *scanSteamCheckbox = nullptr;
	QCheckBox *scanEpicCheckbox = nullptr;
	QCheckBox *scanGogCheckbox = nullptr;
	QCheckBox *scanUbiCheckbox = nullptr;
	QPushButton *rescanButton = nullptr;
	QLabel      *authStatusLabel = nullptr;
	QPushButton *authButton      = nullptr;
	QPushButton *disconnectButton = nullptr;
	QPushButton *okButton = nullptr;
	QPushButton *cancelButton = nullptr;

private slots:
	void onAddGameClicked();
	void onClearTableClicked();
	void onAutomaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames);
	void onAuthenticationFinished(bool success, const QString &username);
	void onDisconnectClicked();
};

#endif // GAMEDETECTORSETTINGSDIALOG_H
