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

// Declaração antecipada para a função auxiliar
bool isExeIgnored(const QString &exeName);

GameDetector &GameDetector::get()
{
	static GameDetector instance;
	return instance;
}

GameDetector::GameDetector(QObject *parent) : QObject(parent)
{
	scanTimer = new QTimer(this);
	gameDbWatcher = new QFutureWatcher<QList<std::tuple<QString, QString, QString>>>(this);

	// Conecta o sinal de timeout do timer ao nosso slot de escaneamento
	connect(gameDbWatcher, &QFutureWatcher<QList<std::tuple<QString, QString, QString>>>::finished, this,
		&GameDetector::onGameScanFinished);
	connect(scanTimer, &QTimer::timeout, this, &GameDetector::scanProcesses);
}

void GameDetector::startScanning()
{
	blog(LOG_INFO, "[GameDetector] Iniciando detecção via escaneamento de processos.");
	startProcessMonitoring();
}

void GameDetector::startProcessMonitoring()
{
	if (!scanTimer->isActive()) {
		blog(LOG_INFO, "[GameDetector] Iniciando monitoramento de processos.");
		scanTimer->start(5000); // Verifica a cada 5 segundos
	}
}

void GameDetector::rescanForGames()
{
	if (gameDbWatcher->isRunning()) {
		blog(LOG_INFO, "[GameDetector] Varredura de jogos já está em andamento.");
		return;
	}
	// Executa a busca por jogos em uma thread separada para não bloquear a UI do OBS
	blog(LOG_INFO, "[GameDetector] Iniciando varredura de jogos em segundo plano...");
	QFuture<QList<std::tuple<QString, QString, QString>>> future = QtConcurrent::run([this]() { return populateGameExecutables(); });
	gameDbWatcher->setFuture(future);
}

void GameDetector::onGameScanFinished()
{
	blog(LOG_INFO, "[GameDetector] Varredura de jogos concluída. Iniciando monitoramento de processos.");

	// Emite o sinal com os jogos encontrados para a UI
	emit automaticScanFinished(gameDbWatcher->result());

	// Recarrega a lista de jogos (que pode ter sido atualizada pela UI) para o monitoramento
	loadGamesFromConfig();
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

void GameDetector::stopScanning()
{
	if (scanTimer->isActive()) {
		blog(LOG_INFO, "[GameDetector] Parando escaneamento de processos.");
		scanTimer->stop();
	}
}

// Lista de substrings para ignorar. Se qualquer parte do nome do executável
// corresponder a uma dessas strings, ele será ignorado.
const QStringList ignoreSubstrings = {
	"launcher.exe", "7z", "presentmon", "dxsetup", "errorreporter", "crashpad", "buildpatchtool", "redmod", "dotnet", "bepinex",
	"vcredist", "vc_redist", "redist", "prereq", "crashreport", "swarm", "unrealpak", "bink2", "bootstrap",
	"shadercompile", "epicwebhelper", "svn", "python", "dumpmini", "datacollector", "testhost", "unrealgame",
	"shipping" // "Shipping.exe" é muito comum em jogos Unreal, mas o nome do jogo vem antes.
};

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
					if (!isExeIgnored(candidateName)) {
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
							if (!isExeIgnored(candidateName)) {
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
						if (!isExeIgnored(candidateName)) {
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

	// ==== EPIC ====
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

				if (installPath.isEmpty() || !QDir(installPath).exists())
					continue;

				// Se o nome do JSON estiver vazio, usa o nome da pasta como fallback
				if (friendlyName.isEmpty()) {
					friendlyName = QFileInfo(installPath).fileName();
				}

				QString exePath;
				QString exeName;

				// 1. Busca primeiro na pasta raiz do jogo
				QDirIterator rootIt(installPath, QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
				while (rootIt.hasNext()) {
					QString candidate = rootIt.next();
					QString candidateName = QFileInfo(candidate).fileName();
					if (!isExeIgnored(candidateName)) {
						exePath = candidate;
						exeName = candidateName;
						break;
					}
				}

				// 2. Se não encontrou, busca em subpastas comuns de binários
				if (exePath.isEmpty()) {
					const QStringList commonBinarySubfolders = {"bin", "Binaries/Win64", "Binaries/Win32", "x64", "x86", "Shipping"};
					for (const QString &subfolder : commonBinarySubfolders) {
						QDir subDir(installPath + "/" + subfolder);
						if (!subDir.exists()) continue;

						QDirIterator subIt(subDir.absolutePath(), QStringList() << "*.exe", QDir::Files, QDirIterator::NoIteratorFlags);
						while (subIt.hasNext()) {
							QString candidate = subIt.next();
							QString candidateName = QFileInfo(candidate).fileName();
							if (!isExeIgnored(candidateName)) {
								exePath = candidate;
								exeName = candidateName;
								break;
							}
						}
						if (!exePath.isEmpty()) break;
					}
				}

				// 3. Como último recurso, faz a busca recursiva
				if (exePath.isEmpty()) {
					QDirIterator recursiveIt(installPath, QStringList() << "*.exe", QDir::Files, QDirIterator::Subdirectories);
					while (recursiveIt.hasNext()) {
						QString candidate = recursiveIt.next();
						QString candidateName = QFileInfo(candidate).fileName();
						if (!isExeIgnored(candidateName)) {
							exePath = candidate;
							exeName = candidateName;
							break;
						}
					}
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

#endif

	blog(LOG_INFO, "[GameDetector] Varredura terminou. %d jogos encontrados.", foundGames.size());
	return foundGames;
}

/**
 * @brief Verifica se um nome de executável deve ser ignorado.
 *
 * Esta função auxiliar centraliza a lógica de verificação contra a `ignoreList`,
 * checando tanto por nomes completos quanto por correspondências parciais.
 *
 * @param exeName O nome do arquivo executável a ser verificado.
 * @return true se o executável deve ser ignorado, false caso contrário.
 */
bool isExeIgnored(const QString &exeName) {
	const QString lowerExeName = exeName.toLower();

	// 1. Verifica se o nome completo está na lista de ignorados (verificação rápida)
	if (ignoreFullNames.contains(lowerExeName)) {
		return true;
	}

	// 2. Verifica se o nome contém alguma das substrings a serem ignoradas
	for (const QString &substring : ignoreSubstrings) {
		if (lowerExeName.contains(substring)) {
			return true;
		}
	}
	return false;
}

void GameDetector::loadGamesFromConfig()
{
	knownGameExes.clear();
	gameNameMap.clear();

	// 3. Carregar jogos manuais da configuração
	obs_data_array_t *manualGames = ConfigManager::get().getManualGames();
	if (manualGames) {
		size_t count = obs_data_array_count(manualGames);
		blog(LOG_INFO, "[GameDetector] Carregando %d jogos da lista manual.", count);
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
					if (GetModuleFileNameEx(hProcess, hMod, processPath, sizeof(processPath) / sizeof(wchar_t))) {
						QString qProcessPath = QString::fromWCharArray(processPath);
						QString processName = QFileInfo(qProcessPath).fileName();

						// Nova lógica de detecção: verifica se o nome do processo está na nossa lista de jogos conhecidos
						if (knownGameExes.contains(processName)) {

							// Usa o nome manual se existir, senão pega a descrição do arquivo
							QString friendlyName = gameNameMap.value(processName);
							if (friendlyName.isEmpty()) {
								friendlyName = getFileDescription(qProcessPath);
							}

							// Se é um jogo diferente do que já estava rodando, emite o sinal
							if (processName != currentGameProcess) {
								currentGameProcess = processName;
								blog(LOG_INFO, "[GameDetector] Jogo detectado: %s (Processo: %s)", friendlyName.toStdString().c_str(), processName.toStdString().c_str());
								emit gameDetected(friendlyName, processName);
							}
							gameFoundThisScan = true;
							CloseHandle(hProcess);
							// Para a busca ao encontrar o primeiro jogo
							goto cleanup; // O uso de goto aqui é aceitável para sair de loops aninhados
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
		blog(LOG_INFO, "[GameDetector] Jogo '%s' não está mais em execução.", currentGameProcess.toStdString().c_str());
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
