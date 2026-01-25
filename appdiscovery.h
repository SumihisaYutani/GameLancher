#ifndef APPDISCOVERY_H
#define APPDISCOVERY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSettings>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QApplication>
#include "appinfo.h"

struct ScanOptions {
    QStringList includePaths;
    QStringList excludePaths;
    QStringList excludePatterns;
    int maxDepth;
    bool scanDesktop;
    bool scanStartMenu;
    bool scanProgramFiles;
    bool scanSteam;
    
    ScanOptions() 
        : maxDepth(5)
        , scanDesktop(true)
        , scanStartMenu(true)
        , scanProgramFiles(true)
        , scanSteam(true)
    {
        // デフォルトの除外パターン
        excludePatterns << "*unins*.exe" << "*uninst*.exe" << "*uninstall*.exe"
                       << "*setup*.exe" << "*install*.exe" << "*update*.exe"
                       << "*patch*.exe" << "*config*.exe" << "*setting*.exe"
                       << "*launcher*.exe" << "*crash*.exe" << "*report*.exe";
    }
};

class AppDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit AppDiscovery(QObject *parent = nullptr);
    
    // 基本的なスキャン機能
    QList<AppInfo> scanFolder(const QString &path, bool recursive = true);
    QList<AppInfo> scanFolders(const QStringList &paths, const ScanOptions &options = ScanOptions());
    
    // Steam検出
    QList<AppInfo> discoverSteamGames();
    QString findSteamPath();
    
    // レジストリ検出
    QList<AppInfo> discoverInstalledApps();
    
    // ショートカット解析
    QList<AppInfo> discoverShortcuts();
    QList<AppInfo> discoverDesktopShortcuts();
    QList<AppInfo> discoverStartMenuShortcuts();
    
    // 統合スキャン
    QList<AppInfo> discoverAllApps(const ScanOptions &options = ScanOptions());
    
    // 重複除去・マージ
    QList<AppInfo> mergeDuplicates(const QList<AppInfo> &apps);
    
    // ユーティリティ関数
    bool isValidExecutable(const QFileInfo &fileInfo);
    bool isGameExecutable(const QFileInfo &fileInfo);
    QString detectCategory(const QFileInfo &fileInfo);
    QString extractDisplayName(const QFileInfo &fileInfo);
    
    // デフォルトパス取得
    QStringList getDefaultScanPaths();
    QStringList getProgramFilesPaths();
    
public slots:
    void cancelScan();

signals:
    void scanProgress(int current, int total, const QString &currentPath);
    void appDiscovered(const AppInfo &app);
    void scanStarted();
    void scanFinished(int totalFound);
    void scanCanceled();

private:
    bool m_canceled;
    int m_currentProgress;
    int m_totalProgress;
    
    // 内部ヘルパー関数
    QList<AppInfo> scanFoldersInternal(const QStringList &paths, const ScanOptions &options, bool emitSignals);
    void scanFolderRecursive(const QString &path, QList<AppInfo> &results, 
                           const ScanOptions &options, int currentDepth = 0);
    bool shouldExcludeFile(const QFileInfo &fileInfo, const ScanOptions &options);
    bool shouldExcludePath(const QString &path, const ScanOptions &options);
    AppInfo createAppInfoFromFile(const QFileInfo &fileInfo);
    AppInfo createAppInfoFromShortcut(const QString &shortcutPath);
    QString resolveShortcutTarget(const QString &shortcutPath);
    
    // Steam関連
    QList<AppInfo> parseSteamApps(const QString &steamPath);
    QString getSteamAppName(const QString &appManifestPath);
    
    // レジストリ関連
    QList<AppInfo> scanUninstallRegistry();
    AppInfo createAppInfoFromRegistry(const QSettings &regKey, const QString &keyName);
    
    // カテゴリ判定
    QString guessCategory(const QString &name, const QString &path);
    QStringList getGameKeywords();
    QStringList getBusinessKeywords();
    QStringList getToolKeywords();
    QStringList getMediaKeywords();
    QStringList getDevelopmentKeywords();
};

#endif // APPDISCOVERY_H