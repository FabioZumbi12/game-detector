#ifndef GAMELISTDIALOG_H
#define GAMELISTDIALOG_H

#include <QDialog>
#include <tuple>
#include <QList>

// Forward declarations
class QTableWidget;
class QPushButton;
class QCheckBox;
class QSpinBox;
class GameDetectorSettingsDialog;

class GameListDialog : public QDialog {
	Q_OBJECT

public:
	explicit GameListDialog(GameDetectorSettingsDialog *parent = nullptr);
	~GameListDialog();

private:
	void loadGames();
	void saveGames();

	// UI Elements
	QTableWidget *manualGamesTable = nullptr;
	QPushButton *addGameButton = nullptr;
	QPushButton *clearTableButton = nullptr;
	QPushButton *rescanButton = nullptr;
	QPushButton *okButton = nullptr;
	QPushButton *cancelButton = nullptr;
	GameDetectorSettingsDialog *parentWidget = nullptr;

private slots:
	void onAddGameClicked();
	void onClearTableClicked();
	void onAutomaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames);
};

#endif // GAMELISTDIALOG_H
