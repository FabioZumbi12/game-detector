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

	// Funções para encontrar os jogos no sistema
	QList<std::tuple<QString, QString, QString>> populateGameExecutables();

	// Função auxiliar para obter a descrição de um arquivo EXE
	QString getFileDescription(const QString &filePath);

	// Função auxiliar para verificar se um executável está na lista de ignorados
	bool isExeIgnored(const QString &exeName);
public: // NOLINT(readability-redundant-access-specifiers)
	static GameDetector &get();

	// Inicia e para o escaneamento de processos
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
	// Sinal emitido quando um novo jogo é detectado
	void gameDetected(const QString &gameName, const QString &processName);
	// Sinal emitido quando o jogo anteriormente detectado é fechado
	void noGameDetected();
	// Sinal emitido quando a varredura automática termina, com a lista de jogos encontrados
	void automaticScanFinished(const QList<std::tuple<QString, QString, QString>> &foundGames);
	void gameFoundDuringScan(int totalFound); // emitido a cada jogo encontrado


private slots:
	// Slot que executa a verificação de processos
	void scanProcesses();
	// Slot chamado quando a varredura assíncrona de jogos termina
	void onGameScanFinished();
	void onPeriodicScanTriggered();
};

#endif // GAMEDETECTOR_H
