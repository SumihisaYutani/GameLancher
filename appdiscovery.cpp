#include "appdiscovery.h"
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QFileIconProvider>
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QMimeType>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

AppDiscovery::AppDiscovery(QObject *parent)
    : QObject(parent)
    , m_canceled(false)
    , m_currentProgress(0)
    , m_totalProgress(0)
{
}

QList<AppInfo> AppDiscovery::scanFolder(const QString &path, bool recursive)
{
    QList<AppInfo> results;
    ScanOptions options;
    options.maxDepth = recursive ? 5 : 1;
    
    emit scanStarted();
    m_canceled = false;
    
    scanFolderRecursive(path, results, options, 0);
    
    emit scanFinished(results.size());
    return results;
}

QList<AppInfo> AppDiscovery::scanFolders(const QStringList &paths, const ScanOptions &options)
{
    return scanFoldersInternal(paths, options, true);
}

QList<AppInfo> AppDiscovery::scanFoldersInternal(const QStringList &paths, const ScanOptions &options, bool emitSignals)
{
    QList<AppInfo> results;
    
    if (emitSignals) {
        emit scanStarted();
    }
    m_canceled = false;
    
    m_totalProgress = paths.size();
    m_currentProgress = 0;
    
    for (const QString &path : paths) {
        if (m_canceled) {
            if (emitSignals) {
                emit scanCanceled();
            }
            return results;
        }
        
        if (emitSignals) {
            emit scanProgress(m_currentProgress, m_totalProgress, path);
        }
        scanFolderRecursive(path, results, options, 0);
        m_currentProgress++;
        
        QCoreApplication::processEvents();
    }
    
    // 重複を除去
    results = mergeDuplicates(results);
    
    if (emitSignals) {
        emit scanFinished(results.size());
    }
    return results;
}

void AppDiscovery::scanFolderRecursive(const QString &path, QList<AppInfo> &results, 
                                     const ScanOptions &options, int currentDepth)
{
    if (m_canceled || currentDepth >= options.maxDepth) {
        return;
    }
    
    if (shouldExcludePath(path, options)) {
        return;
    }
    
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }
    
    // 実行ファイルをスキャン
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.exe", 
                                          QDir::Files | QDir::Readable);
    
    for (const QFileInfo &fileInfo : files) {
        if (m_canceled) return;
        
        if (isValidExecutable(fileInfo) && !shouldExcludeFile(fileInfo, options)) {
            AppInfo app = createAppInfoFromFile(fileInfo);
            if (!app.name.isEmpty()) {
                results.append(app);
                emit appDiscovered(app);
            }
        }
        
        QCoreApplication::processEvents();
    }
    
    // サブディレクトリを再帰的にスキャン
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    for (const QFileInfo &dirInfo : dirs) {
        if (m_canceled) return;
        
        scanFolderRecursive(dirInfo.absoluteFilePath(), results, options, currentDepth + 1);
        QCoreApplication::processEvents();
    }
}

bool AppDiscovery::isValidExecutable(const QFileInfo &fileInfo)
{
    if (!fileInfo.isFile() || !fileInfo.isExecutable()) {
        return false;
    }
    
    // ファイルサイズが極端に小さい場合は除外
    if (fileInfo.size() < 10240) { // 10KB未満
        return false;
    }
    
    return fileInfo.suffix().toLower() == "exe";
}

bool AppDiscovery::shouldExcludeFile(const QFileInfo &fileInfo, const ScanOptions &options)
{
    QString fileName = fileInfo.fileName().toLower();
    
    for (const QString &pattern : options.excludePatterns) {
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern.toLower()));
        if (regex.match(fileName).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

bool AppDiscovery::shouldExcludePath(const QString &path, const ScanOptions &options)
{
    QString lowerPath = path.toLower();
    
    // 一般的な除外パス
    QStringList commonExcludes = {
        "windows", "system32", "syswow64", "temp", "tmp",
        "cache", "logs", "recycle", "$recycle.bin"
    };
    
    for (const QString &exclude : commonExcludes) {
        if (lowerPath.contains(exclude)) {
            return true;
        }
    }
    
    for (const QString &excludePath : options.excludePaths) {
        if (lowerPath.startsWith(excludePath.toLower())) {
            return true;
        }
    }
    
    return false;
}

AppInfo AppDiscovery::createAppInfoFromFile(const QFileInfo &fileInfo)
{
    AppInfo app;
    app.path = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    app.name = extractDisplayName(fileInfo);
    app.category = detectCategory(fileInfo);
    app.createdAt = QDateTime::currentDateTime();
    
    // アイコンの抽出を試行
    QString iconPath = fileInfo.absoluteFilePath();
    app.iconPath = iconPath; // IconExtractorで後で処理される
    
    qDebug() << "Discovered app:" << app.name << "at" << app.path;
    
    return app;
}

QString AppDiscovery::extractDisplayName(const QFileInfo &fileInfo)
{
    QString baseName = fileInfo.baseName();
    
    // 一般的な接尾語を除去
    QStringList suffixesToRemove = {
        "launcher", "game", "client", "setup", "install", 
        "x64", "x86", "win32", "win64", "32bit", "64bit"
    };
    
    QString cleanName = baseName;
    for (const QString &suffix : suffixesToRemove) {
        QRegularExpression regex("\\b" + suffix + "\\b", QRegularExpression::CaseInsensitiveOption);
        cleanName.replace(regex, "");
    }
    
    // 余分な空白を除去し、最初の文字を大文字に
    cleanName = cleanName.trimmed();
    if (!cleanName.isEmpty()) {
        cleanName[0] = cleanName[0].toUpper();
    }
    
    return cleanName.isEmpty() ? baseName : cleanName;
}

QString AppDiscovery::detectCategory(const QFileInfo &fileInfo)
{
    QString path = fileInfo.absoluteFilePath().toLower();
    QString name = fileInfo.fileName().toLower();
    
    return guessCategory(name, path);
}

QString AppDiscovery::guessCategory(const QString &name, const QString &path)
{
    // ゲーム関連キーワード
    for (const QString &keyword : getGameKeywords()) {
        if (name.contains(keyword) || path.contains(keyword)) {
            return "ゲーム";
        }
    }
    
    // 開発関連キーワード
    for (const QString &keyword : getDevelopmentKeywords()) {
        if (name.contains(keyword) || path.contains(keyword)) {
            return "開発";
        }
    }
    
    // ビジネス関連キーワード
    for (const QString &keyword : getBusinessKeywords()) {
        if (name.contains(keyword) || path.contains(keyword)) {
            return "ビジネス";
        }
    }
    
    // メディア関連キーワード
    for (const QString &keyword : getMediaKeywords()) {
        if (name.contains(keyword) || path.contains(keyword)) {
            return "メディア";
        }
    }
    
    // ツール関連キーワード
    for (const QString &keyword : getToolKeywords()) {
        if (name.contains(keyword) || path.contains(keyword)) {
            return "ツール";
        }
    }
    
    return "その他";
}

QStringList AppDiscovery::getGameKeywords()
{
    return QStringList() << "game" << "steam" << "epic" << "gog" << "origin"
                        << "uplay" << "battle.net" << "minecraft" << "unity"
                        << "unreal" << "fps" << "rpg" << "mmo" << "arcade";
}

QStringList AppDiscovery::getDevelopmentKeywords()
{
    return QStringList() << "visual studio" << "code" << "dev" << "git"
                        << "python" << "java" << "node" << "npm" << "compiler"
                        << "debugger" << "ide" << "editor" << "qt" << "android studio";
}

QStringList AppDiscovery::getBusinessKeywords()
{
    return QStringList() << "office" << "word" << "excel" << "powerpoint"
                        << "outlook" << "teams" << "zoom" << "skype" << "slack"
                        << "adobe" << "acrobat" << "reader" << "calculator";
}

QStringList AppDiscovery::getMediaKeywords()
{
    return QStringList() << "vlc" << "media" << "player" << "music" << "video"
                        << "photo" << "image" << "audio" << "spotify" << "iTunes"
                        << "photoshop" << "premiere" << "audacity" << "gimp";
}

QStringList AppDiscovery::getToolKeywords()
{
    return QStringList() << "tool" << "utility" << "manager" << "browser"
                        << "chrome" << "firefox" << "explorer" << "notepad"
                        << "archive" << "zip" << "rar" << "antivirus" << "clean";
}

QList<AppInfo> AppDiscovery::discoverAllApps(const ScanOptions &options)
{
    QList<AppInfo> allApps;
    
    emit scanStarted();
    m_canceled = false;
    
    // オプションに基づいてスキャンパスを構築
    QStringList paths;
    
    // Program Filesスキャンのオプション
    if (options.scanProgramFiles) {
        paths << "C:/Program Files";
        paths << "C:/Program Files (x86)";
        
        // ユーザーローカル
        QString userLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!userLocal.isEmpty()) {
            QDir userDir(userLocal);
            userDir.cdUp(); // AppLocalDataLocationから一つ上のディレクトリ
            paths << userDir.absolutePath();
        }
    }
    
    // ユーザー指定のパスを追加
    paths.append(options.includePaths);
    
    // パスをスキャン
    bool folderScanExecuted = false;
    if (!paths.isEmpty()) {
        QList<AppInfo> folderApps = scanFoldersInternal(paths, options, true); // シグナルを発行
        allApps.append(folderApps);
        folderScanExecuted = true;
        
        if (m_canceled) {
            emit scanCanceled();
            return allApps;
        }
    }
    
    // ショートカットをスキャン
    if (options.scanDesktop || options.scanStartMenu) {
        QList<AppInfo> shortcutApps = discoverShortcuts();
        allApps.append(shortcutApps);
    }
    
    if (m_canceled) {
        emit scanCanceled();
        return allApps;
    }
    
    // Steamアプリを検出
    if (options.scanSteam) {
        QList<AppInfo> steamApps = discoverSteamGames();
        allApps.append(steamApps);
    }
    
    // 重複を除去
    allApps = mergeDuplicates(allApps);
    
    // シグナルは一度だけ発行
    if (!folderScanExecuted) {
        // フォルダスキャンが実行されなかった場合のみ発行
        emit scanFinished(allApps.size());
    }
    return allApps;
}

QStringList AppDiscovery::getDefaultScanPaths()
{
    QStringList paths;
    
    // Program Files
    paths << "C:/Program Files";
    paths << "C:/Program Files (x86)";
    
    // ユーザーローカル
    QString userLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!userLocal.isEmpty()) {
        QDir userDir(userLocal);
        userDir.cdUp(); // AppLocalDataLocationから一つ上のディレクトリ
        paths << userDir.absolutePath();
    }
    
    return paths;
}

QList<AppInfo> AppDiscovery::mergeDuplicates(const QList<AppInfo> &apps)
{
    QList<AppInfo> uniqueApps;
    QStringList seenPaths;
    
    for (const AppInfo &app : apps) {
        QString normalizedPath = QDir::fromNativeSeparators(app.path.toLower());
        
        if (!seenPaths.contains(normalizedPath)) {
            uniqueApps.append(app);
            seenPaths.append(normalizedPath);
        }
    }
    
    return uniqueApps;
}

void AppDiscovery::cancelScan()
{
    m_canceled = true;
    qDebug() << "Scan canceled by user";
}

// Steam検出の基本実装（簡易版）
QList<AppInfo> AppDiscovery::discoverSteamGames()
{
    QList<AppInfo> steamApps;
    
    QString steamPath = findSteamPath();
    if (steamPath.isEmpty()) {
        qDebug() << "Steam not found";
        return steamApps;
    }
    
    qDebug() << "Steam found at:" << steamPath;
    
    // steamapps/commonディレクトリをスキャン
    QString commonPath = steamPath + "/steamapps/common";
    QDir commonDir(commonPath);
    
    if (commonDir.exists()) {
        QFileInfoList gameDirs = commonDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QFileInfo &gameDir : gameDirs) {
            ScanOptions options;
            options.maxDepth = 2;
            
            QList<AppInfo> gameApps = scanFolder(gameDir.absoluteFilePath(), false);
            for (AppInfo &app : gameApps) {
                if (app.category == "その他") {
                    app.category = "ゲーム";
                }
            }
            steamApps.append(gameApps);
        }
    }
    
    return steamApps;
}

QString AppDiscovery::findSteamPath()
{
    // よくあるSteamインストールパス
    QStringList commonPaths = {
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam",
        "D:/Steam",
        "E:/Steam"
    };
    
    for (const QString &path : commonPaths) {
        if (QDir(path).exists()) {
            return path;
        }
    }
    
    return QString();
}

// ショートカット検出の基本実装
QList<AppInfo> AppDiscovery::discoverShortcuts()
{
    QList<AppInfo> shortcuts;
    
    // デスクトップショートカット
    shortcuts.append(discoverDesktopShortcuts());
    
    // スタートメニューショートカット
    shortcuts.append(discoverStartMenuShortcuts());
    
    return shortcuts;
}

QList<AppInfo> AppDiscovery::discoverDesktopShortcuts()
{
    QList<AppInfo> shortcuts;
    
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir desktop(desktopPath);
    
    QFileInfoList lnkFiles = desktop.entryInfoList(QStringList() << "*.lnk", QDir::Files);
    
    for (const QFileInfo &lnkFile : lnkFiles) {
        AppInfo app = createAppInfoFromShortcut(lnkFile.absoluteFilePath());
        if (!app.name.isEmpty()) {
            shortcuts.append(app);
        }
    }
    
    return shortcuts;
}

QList<AppInfo> AppDiscovery::discoverStartMenuShortcuts()
{
    // 簡易実装 - 後で詳細に実装
    return QList<AppInfo>();
}

AppInfo AppDiscovery::createAppInfoFromShortcut(const QString &shortcutPath)
{
    AppInfo app;
    
    QString target = resolveShortcutTarget(shortcutPath);
    if (!target.isEmpty()) {
        QFileInfo targetInfo(target);
        if (targetInfo.exists() && isValidExecutable(targetInfo)) {
            app = createAppInfoFromFile(targetInfo);
        }
    }
    
    return app;
}

QString AppDiscovery::resolveShortcutTarget(const QString &shortcutPath)
{
    // Windows LNKファイルの解析は複雑なので、簡易実装
    // 実際の実装では、Windows APIまたはサードパーティライブラリが必要
    return QString();
}

// レジストリ検出は後で実装
QList<AppInfo> AppDiscovery::discoverInstalledApps()
{
    return QList<AppInfo>();
}

QStringList AppDiscovery::getProgramFilesPaths()
{
    return QStringList() << "C:/Program Files" << "C:/Program Files (x86)";
}