#include "GameDetector.h"
#include <obs-module.h>
#include "ConfigManager.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QDirIterator>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <QJsonArray>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <vector>

// Linka as bibliotecas necessárias do Windows no momento da compilação
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")
#endif

GameDetector &GameDetector::get()
{
	static GameDetector instance;
	return instance;
}

GameDetector::GameDetector(QObject *parent) : QObject(parent)
{
	scanTimer = new QTimer(this);
	periodicScanTimer = new QTimer(this);
	gameDbWatcher = new QFutureWatcher<QList<std::tuple<QString, QString, QString>>>(this);

	// Conecta o sinal de timeout do timer ao nosso slot de escaneamento
	connect(gameDbWatcher, &QFutureWatcher<QList<std::tuple<QString, QString, QString>>>::finished, this,
		&GameDetector::onGameScanFinished);
	connect(scanTimer, &QTimer::timeout, this, &GameDetector::scanProcesses);
	connect(periodicScanTimer, &QTimer::timeout, this, &GameDetector::onPeriodicScanTriggered);

	// Converte a lista de substrings para um QSet para buscas mais rápidas.
	// Isso é feito uma vez no construtor para otimizar a função isExeIgnored.
	const QStringList ignoreSubstrings = {
		"7z", "presentmon", "dxsetup", "errorreporter", "crashpad", "buildpatchtool", "redmod", "dotnet", "bepinex",
		"vcredist", "vc_redist", "redist", "prereq", "crashreport", "swarm", "unrealpak", "bink2", "bootstrap",
		"shadercompile", "epicwebhelper", "svn", "python", "dumpmini", "datacollector", "testhost", "unrealgame",
		"shipping"
	};
	for (const QString& str : ignoreSubstrings) {
		ignoreSubstringsSet.insert(str);
	}
}

void GameDetector::startScanning()
{
	blog(LOG_INFO, "[GameDetector] Starting detection via process scanning.");
	startProcessMonitoring();
}

void GameDetector::startProcessMonitoring()
{
	if (!scanTimer->isActive()) {
		blog(LOG_INFO, "[GameDetector] Starting process monitoring.");
		scanTimer->start(5000); // Verifica a cada 5 segundos
	}
}

void GameDetector::rescanForGames(bool scanSteam, bool scanEpic, bool scanGog, bool scanUbisoft)
{
	if (gameDbWatcher->isRunning()) {
		blog(LOG_INFO, "[GameDetector] Game scan is already in progress.");
		return;
	}

	this->tempScanSteam = scanSteam; // Assumindo que estas variáveis de membro existem
	this->tempScanEpic = scanEpic;   // ou serão adicionadas no header.
	this->tempScanGog = scanGog;
	this->tempScanUbisoft = scanUbisoft;

	// Executes game search in a separate thread to avoid blocking the OBS UI
	blog(LOG_INFO, "[GameDetector] Starting background game scan...");
	QFuture<QList<std::tuple<QString, QString, QString>>> future =
		QtConcurrent::run([this]() { return populateGameExecutables(); });
	gameDbWatcher->setFuture(future);
}

void GameDetector::onGameScanFinished()
{
	blog(LOG_INFO, "[GameDetector] Game scan completed. Starting process monitoring.");

	// Emite o sinal com os jogos encontrados para a UI
	emit automaticScanFinished(gameDbWatcher->result());

}

void GameDetector::onSettingsChanged()
{
	stopScanning();

	currentGameProcess.clear();
	emit noGameDetected();

	startProcessMonitoring();

	// Recarrega a lista de jogos, pois pode ter sido alterada
	loadGamesFromConfig();
}

void GameDetector::setupPeriodicScan()
{
	if (periodicScanTimer->isActive()) {
		periodicScanTimer->stop();
	}

	bool enabled = ConfigManager::get().getScanPeriodically();
	if (enabled) {
		int minutes = ConfigManager::get().getScanPeriodicallyInterval();
		if (minutes > 0) {
			long long intervalMs = (long long)minutes * 60 * 1000;
			periodicScanTimer->start(intervalMs);
			blog(LOG_INFO, "[GameDetector] Periodic scan enabled. Interval: %d minutes.", minutes);
		}
	} else {
		blog(LOG_INFO, "[GameDetector] Periodic scan disabled.");
	}
}

void GameDetector::onPeriodicScanTriggered()
{
	blog(LOG_INFO, "[GameDetector] Periodic scan triggered.");
	// Inicia a varredura usando as configurações salvas
	bool scanSteam = ConfigManager::get().getScanSteam();
	bool scanEpic = ConfigManager::get().getScanEpic();
	bool scanGog = ConfigManager::get().getScanGog();
	bool scanUbisoft = ConfigManager::get().getScanUbisoft();

	rescanForGames(scanSteam, scanEpic, scanGog, scanUbisoft);

	// Conecta para salvar os jogos encontrados neste scan periódico e depois se desconecta.
	auto conn = std::make_shared<QMetaObject::Connection>();
	*conn = connect(this, &GameDetector::automaticScanFinished,
			[this, conn](const QList<std::tuple<QString, QString, QString>> &foundGames) {
				this->mergeAndSaveGames(foundGames);
				this->loadGamesFromConfig(); // Recarrega a lista para o monitoramento
				QObject::disconnect(*conn);  // Auto-desconexão
			});
}

void GameDetector::stopScanning()
{
	if (scanTimer->isActive()) {
		blog(LOG_INFO, "[GameDetector] Stopping process scanning.");
		scanTimer->stop();
	}
}

// Lista de nomes de executáveis completos para ignorar.
// A correspondência deve ser exata (ignorando maiúsculas/minúsculas).
const QSet<QString> ignoreFullNames = {
	"7za.exe", "compatibility.exe", "ispc.exe", "openssl.exe", "scc.exe",
	"interchangeworker.exe", "zen.exe", "applicationframehost.exe", "shellexperiencehost.exe",
	"ndp462-kb3151800-x86-x64-allos-enu.exe", "ndp472-kb4054530-x86-x64-allos-enu.exe",
	"ue4prereqsetup_x64.exe", "ueprereqsetup_x64.exe"
	"eaanticheat.installer.exe", "common.extprotocol.executor.exe", "eztransxp.extprotocol.exe",
	"lec.extprotocol.exe", "unrealandroidfiletool.exe", "unrealbuildtool.exe", "automationtool.exe",
	"csvcollate.exe", "csvconvert.exe", "csvfilter.exe", "csvinfo.exe", "csvsplit.exe", "csvtosvg.exe",
	"perfreporttool.exe", "regressionsreport.exe", "iphonepackager.exe", "networkprofiler.exe", "oidctoken.exe",
	"swarmagent.exe", "swarmcoordinator.exe", "containerize.exe",
	"microsoft.codeanalysis.workspaces.msbuild.buildhost.exe", "createdump.exe", "plink.exe", "pscp.exe",
	"putty.exe", "node-bifrost.exe", "datasmithcadworker.exe", "hhc.exe", "apphost.exe",
	"livecodingconsole.exe", "livelinkhub.exe", "switchboardlistener.exe", "switchboardlistenerhelper.exe",
	"unrealeditor-cmd.exe", "unrealeditor-win64-debuggame-cmd.exe", "unrealeditor-win64-debuggame.exe",
	"unrealeditor.exe", "unrealfrontend.exe", "unrealinsights.exe", "unreallightmass.exe",
	"unrealmultiuserserver.exe", "unrealmultiuserslateserver.exe", "unrealobjectptrtool.exe",
	"unrealpackagetool.exe", "unrealrecoverysvc.exe", "unrealtraceserver.exe", "xgecontrolworker.exe",
	"zendashboard.exe", "zenserver.exe", "ubaagent.exe", "ubacacheservice.exe", "ubacli.exe", "ubaobjtool.exe",
	"ubastorageproxy.exe", "ubatest.exe", "ubatestapp.exe", "ubavisualizer.exe", "unitycrashhandler64.exe",
	"unitycrashhandler32.exe", "egodumper.exe", "mod_tools.exe", "steamworksexample.exe", "singlefilehost.exe",
	"t32.exe", "t64.exe", "t64-arm.exe", "w32.exe", "w64.exe", "w64-arm.exe", "cli.exe", "cli-32.exe",
	"cli-64.exe", "cli-arm64.exe", "gui.exe", "gui-32.exe", "gui-64.exe", "gui-arm64.exe", "pip.exe", "pip3.exe",
	"pip3.11.exe", "x86_64-w64-mingw32-nmakehlp.exe", "diff.exe", "diff3.exe", "diff4.exe", "cl-filter.exe",
	"d2u.exe", "u2d.exe", "rsync.exe", "ssh.exe", "ssh-agent.exe", "ssh-keygen.exe", "ideviceactivation.exe",
	"idevicebackup.exe", "idevicebackup2.exe", "idevicedate.exe", "idevicedebug.exe",
	"idevicedebugserverproxy.exe", "idevicediagnostics.exe", "ideviceenterrecovery.exe", "idevicefs.exe",
	"ideviceimagemounter.exe", "ideviceinfo.exe", "ideviceinstaller.exe", "idevicename.exe",
	"idevicenotificationproxy.exe", "idevicepair.exe", "ideviceprovision.exe", "idevicerestore.exe",
	"idevicescreenshot.exe", "idevicesyslog.exe", "idevice_id.exe", "ios_webkit_debug_proxy.exe", "iproxy.exe",
	"irecovery.exe", "itcpconnect.exe", "plistutil.exe", "plist_cmp.exe", "plist_test.exe", "usbmuxd.exe",
	"clang++.exe", "iree-compile.exe", "ld.lld.exe", "torch-mlir-import-onnx.exe", "interlacedcapture.exe",
	"timecodeburner.exe", "timecodecapture.exe", "arcoreimg.exe", "sqlite3.exe", "recast.exe"
};

QList<std::tuple<QString, QString, QString>> GameDetector::populateGameExecutables()
{
	QList<std::tuple<QString, QString, QString>> foundGames;

#ifdef _WIN32

	knownGameExes.clear();
	gameNameMap.clear();

	if (this->tempScanSteam) {
		// ==== STEAM ====
		QSettings steamSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
		QString steamPath = steamSettings.value("InstallPath").toString();
		if (!steamPath.isEmpty()) {
			QString libraryFile = steamPath + "/steamapps/libraryfolders.vdf";
			QFile f(libraryFile);
			QStringList libraryPaths;
			libraryPaths << steamPath + "/steamapps/common";

			if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QByteArray data = f.readAll();
				f.close();
				QRegularExpression re("\"path\"\\s+\"([^\"]+)\"");
				auto matches = re.globalMatch(QString::fromUtf8(data));
				while (matches.hasNext()) {
					QString lib = matches.next().captured(1);
					libraryPaths << lib + "/steamapps/common";
				}
			}

			// Percorre cada biblioteca
			for (const QString &library : libraryPaths) {
				if (!QDir(library).exists())
					continue;

				QDirIterator it(library, QDir::Dirs | QDir::NoDotAndDotDot);
				while (it.hasNext()) {
					QString gameFolder = it.next();

					// Procura o executável principal na pasta do jogo
					QString exePath;
					QString exeName;

					// 1. Busca primeiro na pasta raiz do jogo
					QDirIterator rootIt(gameFolder, QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
					while (rootIt.hasNext()) {
						QString candidate = rootIt.next();
						QString candidateName = QFileInfo(candidate).fileName();
						if (!this->isExeIgnored(candidateName)) {
							exePath = candidate;
							exeName = candidateName;
							break;
						}
					}

					// 2. Se não encontrou, busca em subpastas comuns de binários
					if (exePath.isEmpty()) {
						const QStringList commonBinarySubfolders = {"bin", "Binaries/Win64", "Binaries/Win32", "x64", "x86"};
						for (const QString &subfolder : commonBinarySubfolders) {
							QDir subDir(gameFolder + "/" + subfolder);
							if (!subDir.exists()) continue;

							QDirIterator subIt(subDir.absolutePath(), QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
							while (subIt.hasNext()) {
								QString candidate = subIt.next();
								QString candidateName = QFileInfo(candidate).fileName();
								if (!this->isExeIgnored(candidateName)) {
									exePath = candidate;
									exeName = candidateName;
									break;
								}
							}
							if (!exePath.isEmpty()) {
								break;
							}
						}
					}

					// 3. Como último recurso, faz a busca recursiva (lógica antiga)
					if (exePath.isEmpty()) {
						QDirIterator recursiveIt(gameFolder, QStringList() << "*.exe", QDir::Files, QDirIterator::Subdirectories);
						while (recursiveIt.hasNext()) {
							QString candidate = recursiveIt.next();
							QString candidateName = QFileInfo(candidate).fileName();
							if (!this->isExeIgnored(candidateName)) {
								exePath = candidate;
								exeName = candidateName;
								break;
							}
						}
					}

					if (exePath.isEmpty() || exeName.isEmpty())
						continue;

					// Sobe na árvore de diretórios para encontrar o nome real do jogo,
					// ignorando pastas de binários.
					QDir gameDir = QFileInfo(exePath).dir();
					const QSet<QString> binaryFolderNames = {"bin", "binaries", "win64", "win_x64", "x64", "shipping"};
					while(binaryFolderNames.contains(gameDir.dirName().toLower())) {
						if (!gameDir.cdUp()) break;
					}
					QString friendlyName = gameDir.dirName();

					if (!knownGameExes.contains(exeName)) {
						foundGames.append({friendlyName, exeName, exePath});

						knownGameExes.insert(exeName);
						gameNameMap.insert(exeName, friendlyName);

						emit gameFoundDuringScan(foundGames.size());
					}
				}
			}
		}
	}

	if (this->tempScanEpic) {
		// ==== EPIC ====
		bool foundViaRegistry = false;
		QSettings epicSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Epic Games\\EpicGamesLauncher", QSettings::NativeFormat);
		QString appDataPath = epicSettings.value("AppDataPath").toString();

		if (!appDataPath.isEmpty()) {
			QString manifestsDir = appDataPath + "Manifests";
			QDirIterator it(manifestsDir, QStringList() << "*.item", QDir::Files);

			while (it.hasNext()) {
				QString manifestPath = it.next();
				QFile manifestFile(manifestPath);
				if (manifestFile.open(QIODevice::ReadOnly)) {
					QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
					manifestFile.close();

					if (doc.isObject()) {
						QJsonObject obj = doc.object();
						QString friendlyName = obj["DisplayName"].toString();
						QString exeName = obj["LaunchExecutable"].toString();
						QString installPath = obj["InstallLocation"].toString();
						QString exePath = QDir::toNativeSeparators(installPath + "/" + exeName);

						if (friendlyName.isEmpty() || exeName.isEmpty() || !QFileInfo::exists(exePath) || this->isExeIgnored(exeName)) {
							continue;
						}
						if (!knownGameExes.contains(QFileInfo(exePath).fileName())) {
							foundGames.append({friendlyName, QFileInfo(exePath).fileName(), exePath});
							knownGameExes.insert(QFileInfo(exePath).fileName());
							gameNameMap.insert(QFileInfo(exePath).fileName(), friendlyName);
							emit gameFoundDuringScan(foundGames.size());
							foundViaRegistry = true;
						}
					}
				}
			}
		}
		// Fallback para o método antigo se o registro falhar
		if (!foundViaRegistry) {
			QString epicFilePath = "C:/ProgramData/Epic/UnrealEngineLauncher/LauncherInstalled.dat";
			QFile epicFile(epicFilePath);
			if (epicFile.exists() && epicFile.open(QIODevice::ReadOnly)) {
				QJsonDocument doc = QJsonDocument::fromJson(epicFile.readAll());
				epicFile.close();

				if (doc.isObject()) {
					QJsonArray arr = doc.object()["InstallationList"].toArray();
					for (const QJsonValue &v : arr) {
						QString installPath = v.toObject()["InstallLocation"].toString();
						QString friendlyName = v.toObject()["DisplayName"].toString();

						if (installPath.isEmpty() || !QDir(installPath).exists()) continue;
						if (friendlyName.isEmpty()) friendlyName = QFileInfo(installPath).fileName();

						QString exePath;
						QString exeName;

						// Lógica de busca de executável (mantida como fallback)
						const QStringList commonBinarySubfolders = {"", "bin", "Binaries/Win64", "Binaries/Win32", "x64", "x86", "Shipping"};
						for (const QString &subfolder : commonBinarySubfolders) {
							QDir subDir(installPath + (subfolder.isEmpty() ? "" : "/" + subfolder));
							if (!subDir.exists()) continue;

							QDirIterator subIt(subDir.absolutePath(), QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
							while (subIt.hasNext()) {
								QString candidate = subIt.next();
								QString candidateName = QFileInfo(candidate).fileName();
								if (!this->isExeIgnored(candidateName)) {
									exePath = candidate;
									exeName = candidateName;
									break;
								}
							}
							if (!exePath.isEmpty()) break;
						}

						if (!exeName.isEmpty() && !knownGameExes.contains(exeName)) {
							foundGames.append({friendlyName, exeName, exePath});
							knownGameExes.insert(exeName);
							gameNameMap.insert(exeName, friendlyName);
							emit gameFoundDuringScan(foundGames.size());
						}
					}
				}
			}
		}
	}

	if (this->tempScanGog) {
		// ==== GOG Galaxy ====
		QSettings gogSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games", QSettings::NativeFormat);

		// As subchaves são os IDs dos jogos
		QStringList gameIds = gogSettings.childGroups();

		for (const QString &gameId : gameIds) {
			gogSettings.beginGroup(gameId);

			QString friendlyName = gogSettings.value("gameName").toString();
			QString exePath = gogSettings.value("exe").toString(); // GOG nos dá o caminho completo!
			QString exeName = QFileInfo(exePath).fileName();

			gogSettings.endGroup();

			// Validação
			if (exePath.isEmpty() || !QFileInfo::exists(exePath) || this->isExeIgnored(exeName)) {
				continue;
			}

			// Adiciona se ainda não foi encontrado
			if (!knownGameExes.contains(exeName)) {
				foundGames.append({friendlyName, exeName, exePath});

				knownGameExes.insert(exeName);
				gameNameMap.insert(exeName, friendlyName);

				emit gameFoundDuringScan(foundGames.size());
			}
		}
	}

	if (this->tempScanUbisoft) {
		// ==== Ubisoft Connect ====
		QSettings ubiSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Ubisoft\\Launcher\\Installs", QSettings::NativeFormat);
		QStringList gameIds = ubiSettings.childGroups();

		for (const QString &gameId : gameIds) {
			ubiSettings.beginGroup(gameId);

			QString installPath = ubiSettings.value("InstallDir").toString();
			QString friendlyName = ubiSettings.value("DisplayName").toString();

			ubiSettings.endGroup();

			if (friendlyName.isEmpty()) {
				friendlyName = QDir(installPath).dirName();
			}

			if (installPath.isEmpty() || !QDir(installPath).exists()) {
				continue;
			}

			QString exePath;
			QString exeName;

			// Lógica de busca de executável
			const QStringList commonBinarySubfolders = {"", "bin", "bin_plus", "bin_x64"};
			for (const QString &subfolder : commonBinarySubfolders) {
				QDir subDir(installPath + (subfolder.isEmpty() ? "" : "/" + subfolder));
				if (!subDir.exists()) continue;

				QDirIterator subIt(subDir.absolutePath(), QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
				while (subIt.hasNext()) {
					QString candidate = subIt.next();
					QString candidateName = QFileInfo(candidate).fileName();
					if (!this->isExeIgnored(candidateName)) {
						exePath = candidate;
						exeName = candidateName;
						break;
					}
				}
				if (!exePath.isEmpty()) break;
			}

			if (!exeName.isEmpty() && !knownGameExes.contains(exeName)) {
				foundGames.append({friendlyName, exeName, exePath});
				knownGameExes.insert(exeName);
				gameNameMap.insert(exeName, friendlyName);
				emit gameFoundDuringScan(foundGames.size());
			}
		}
	}

#endif

	blog(LOG_INFO, "[GameDetector] Scan finished. %d games found.", foundGames.size());
	return foundGames;
}

void GameDetector::mergeAndSaveGames(const QList<std::tuple<QString, QString, QString>> &foundGames)
{
	if (foundGames.isEmpty()) {
		return;
	}

	blog(LOG_INFO, "[GameDetector] Merging and saving games from startup/periodic scan.");

	obs_data_t *settings = ConfigManager::get().getSettings();
	obs_data_array_t *gamesArray = ConfigManager::get().getManualGames();
	if (!gamesArray) {
		gamesArray = obs_data_array_create();
	}

	QSet<QString> existingPaths;
	size_t count = obs_data_array_count(gamesArray);
	for (size_t i = 0; i < count; ++i) {
		obs_data_t *item = obs_data_array_item(gamesArray, i);
		existingPaths.insert(obs_data_get_string(item, "path"));
		obs_data_release(item);
	}

	int addedCount = 0;
	for (const auto &gameTuple : foundGames) {
		QString exePath = std::get<2>(gameTuple);
		if (!existingPaths.contains(exePath)) {
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, "name", std::get<0>(gameTuple).toStdString().c_str());
			obs_data_set_string(item, "exe", std::get<1>(gameTuple).toStdString().c_str());
			obs_data_set_string(item, "path", exePath.toStdString().c_str());
			obs_data_array_push_back(gamesArray, item);
			obs_data_release(item);
			addedCount++;
		}
	}

	if (addedCount > 0) {
		obs_data_set_array(settings, ConfigManager::MANUAL_GAMES_KEY, gamesArray);
		ConfigManager::get().save(settings);
		blog(LOG_INFO, "[GameDetector] Added %d new games to the configuration.", addedCount);
	}

}

bool GameDetector::isExeIgnored(const QString &exeName) {
	const QString lowerExeName = exeName.toLower();

	// 1. Verifica se o nome completo está na lista de ignorados (verificação rápida)
	if (ignoreFullNames.contains(lowerExeName)) {
		return true;
	}

	// 2. Verifica se o nome contém alguma das substrings a serem ignoradas
	for (const QString &substring : ignoreSubstringsSet) {
		if (lowerExeName.contains(substring)) {
			return true;
		}
	}
	return false; // Otimizado usando QSet no construtor
}

void GameDetector::loadGamesFromConfig()
{
	knownGameExes.clear();
	gameNameMap.clear();

	// 3. Carregar jogos manuais da configuração
	obs_data_array_t *manualGames = ConfigManager::get().getManualGames();
	if (manualGames) {
		size_t count = obs_data_array_count(manualGames);
		blog(LOG_INFO, "[GameDetector] Loading %d games from manual list.", count);
		for (size_t i = 0; i < count; ++i) {
			obs_data_t *item = obs_data_array_item(manualGames, i);
			QString exeName = obs_data_get_string(item, "exe");
			QString gameName = obs_data_get_string(item, "name");
			knownGameExes.insert(exeName);
			gameNameMap.insert(exeName, gameName);
			obs_data_release(item);
		}
		obs_data_array_release(manualGames);
	}
}

void GameDetector::scanProcesses()
{
#ifdef _WIN32
	DWORD processes[1024], bytesReturned;
	if (!EnumProcesses(processes, sizeof(processes), &bytesReturned)) {
		return;
	}

	unsigned int numProcesses = bytesReturned / sizeof(DWORD);
	bool gameFoundThisScan = false;

	for (unsigned int i = 0; i < numProcesses; i++) {
		if (processes[i] != 0) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
			if (hProcess) {
				HMODULE hMod;
				DWORD cbNeeded;
				if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
					wchar_t processPath[MAX_PATH];
					if (GetModuleFileNameExW(hProcess, hMod, processPath, sizeof(processPath) / sizeof(wchar_t))) {
						QString processName = QFileInfo(QString::fromWCharArray(processPath)).fileName();

						// Nova lógica de detecção: verifica se o nome do processo está na nossa lista de jogos conhecidos
						// Esta verificação é rápida (usando QSet) e evita a criação de QFileInfo para cada processo.
						if (knownGameExes.contains(processName)) {

							QString qProcessPath = QString::fromWCharArray(processPath);
							// Usa o nome manual se existir, senão pega a descrição do arquivo
							QString friendlyName = gameNameMap.value(processName);
							if (friendlyName.isEmpty()) {
								friendlyName = getFileDescription(qProcessPath);
							}
							// Se é um jogo diferente do que já estava rodando, emite o sinal
							if (processName != currentGameProcess) {
								currentGameProcess = processName;
								blog(LOG_INFO, "[GameDetector] Game detected: %s (Process: %s)", friendlyName.toStdString().c_str(), processName.toStdString().c_str());
								emit gameDetected(friendlyName, processName);
							}
							gameFoundThisScan = true;
							CloseHandle(hProcess);
							// Para a busca ao encontrar o primeiro jogo
							goto cleanup; // The use of goto here is acceptable to break out of nested loops
						}
					}
				}
				CloseHandle(hProcess);
			}
		}
	}

cleanup:
	// Se nenhum jogo foi encontrado nesta varredura, mas havia um antes, emite o sinal de "noGameDetected"
	if (!gameFoundThisScan && !currentGameProcess.isEmpty()) {
		blog(LOG_INFO, "[GameDetector] Game '%s' is no longer running.", currentGameProcess.toStdString().c_str());
		currentGameProcess.clear();
		emit noGameDetected();
	}
#endif
}

QString GameDetector::getFileDescription(const QString &filePath)
{
#ifdef _WIN32
	DWORD handle = 0;
	DWORD versionSize = GetFileVersionInfoSize(filePath.toStdWString().c_str(), &handle);
	if (versionSize == 0) {
		return "";
	}

	std::vector<BYTE> versionInfo(versionSize);
	if (!GetFileVersionInfo(filePath.toStdWString().c_str(), handle, versionSize, versionInfo.data())) {
		return "";
	}

	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} *lpTranslate;

	UINT cbTranslate = 0;
	if (VerQueryValue(versionInfo.data(), TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &cbTranslate)) {
		for (UINT i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++) {
			
			QString subBlock = QString("\\StringFileInfo\\%1%2\\FileDescription")
				.arg(lpTranslate[i].wLanguage, 4, 16, QChar('0'))
				.arg(lpTranslate[i].wCodePage, 4, 16, QChar('0'));

			LPVOID lpBuffer = nullptr;
			UINT cbBufSize = 0;
			if (VerQueryValue(versionInfo.data(), subBlock.toStdWString().c_str(), &lpBuffer, &cbBufSize)) {
				if (cbBufSize > 0) {
					return QString::fromWCharArray((LPCWSTR)lpBuffer);
				}
			}
		}
	}
#endif
	return "";
}

bool GameDetector::isGameListEmpty() const
{
	return knownGameExes.isEmpty();
}
