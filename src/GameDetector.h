#ifndef GAMEDETECTOR_H
#define GAMEDETECTOR_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QSet>
#include <QHash>
#include <tuple>
#include <QFutureWatcher>

class GameDetector : public QObject {
	Q_OBJECT

private:
	QTimer *scanTimer;
	QTimer *periodicScanTimer;
	QFutureWatcher<QList<std::tuple<QString, QString, QString>>> *gameDbWatcher;
	QString currentGameProcess;
	QHash<QString, QString> gameNameMap; // exe -> friendly name
	QSet<QString> knownGameExes;
	QSet<QString> ignoreSubstringsSet;
	bool isUsingSourceDetection;

	bool tempScanSteam = true;
	bool tempScanEpic = true;
	bool tempScanGog = true;
	bool tempScanUbisoft = true;

	explicit GameDetector(QObject *parent = nullptr);

	QList<std::tuple<QString, QString, QString>> populateGameExecutables();

	QString getFileDescription(const QString &filePath);

	bool isExeIgnored(const QString &exeName);
public:
	static GameDetector &get();

	void startScanning();
	void startProcessMonitoring();
	void rescanForGames(bool scanSteam, bool scanEpic, bool scanGog, bool scanUbisoft);
	void stopScanning();
	void loadGamesFromConfig();
	void onSettingsChanged();
	void setupPeriodicScan();
	bool isGameListEmpty() const;
	void mergeAndSaveGames(const QList<std::tuple<QString, QString, QString>> &foundGames);

signals:
	void gameDetected(const QString &gameName);
	void noGameDetected();
	void automaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames);
	void gameFoundDuringScan(int totalFound);


private slots:
	void scanProcesses();
	void onGameScanFinished();
	void onPeriodicScanTriggered();
};

#endif // GAMEDETECTOR_H
