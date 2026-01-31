#include "appmanager.h"
#include "iconextractor.h"
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QDebug>

AppManager::AppManager(QObject *parent)
    : QObject(parent)
    , m_categoryManager(new CategoryManager(this))
{
    m_dataFilePath = getDefaultDataFilePath();
    initializeDataFile();
}

AppManager::~AppManager()
{
    saveApps();
}

bool AppManager::addApp(const AppInfo &app)
{
    qDebug() << "AppManager::addApp called with:" << app.name << app.path;
    
    // 同じパスのアプリが既に存在するかチェック
    for (const auto &existingApp : m_apps) {
        if (existingApp.path == app.path) {
            qWarning() << "App with same path already exists:" << app.path;
            return false;
        }
    }
    
    if (!app.isValid()) {
        qWarning() << "Invalid app data:" << app.name << app.path;
        qDebug() << "App validation failed - fileExists:" << app.fileExists();
        return false;
    }
    
    // アプリ追加時にアイコンキャッシュを生成（一度だけ）
    AppInfo appWithIcon = app;

    // 既にiconPathが設定されていてファイルが存在する場合はスキップ
    if (!app.iconPath.isEmpty() && QFileInfo::exists(app.iconPath)) {
        qDebug() << "Using provided icon path:" << app.iconPath;
    } else if (!app.path.isEmpty()) {
        // IconExtractorを使ってアイコンを生成・保存
        IconExtractor iconExtractor;
        // アプリケーションディレクトリ/iconsを使用
        QString iconDir = QApplication::applicationDirPath() + "/icons";
        QString iconPath = iconExtractor.generateIconPath(app.path, iconDir);

        // アイコンファイルが存在しない場合のみ生成
        if (!QFileInfo::exists(iconPath)) {
            qDebug() << "Generating icon for new app:" << app.name;
            QIcon extractedIcon = iconExtractor.extractIcon(app.path);
            if (!extractedIcon.isNull()) {
                // アイコンをファイルに保存
                QPixmap pixmap = extractedIcon.pixmap(64, 64);
                if (!pixmap.isNull()) {
                    // アイコンディレクトリを作成
                    QDir iconDir = QFileInfo(iconPath).absoluteDir();
                    if (!iconDir.exists()) {
                        iconDir.mkpath(".");
                    }
                    
                    // アイコンファイルとして保存
                    if (pixmap.save(iconPath, "PNG")) {
                        appWithIcon.iconPath = iconPath;
                        qDebug() << "Icon saved to:" << iconPath;
                    } else {
                        qWarning() << "Failed to save icon to:" << iconPath;
                    }
                } else {
                    qWarning() << "Failed to convert icon to pixmap for:" << app.name;
                }
            } else {
                qDebug() << "No icon extracted for:" << app.name;
            }
        } else {
            appWithIcon.iconPath = iconPath;
            qDebug() << "Using existing icon cache:" << iconPath;
        }
    }
    
    qDebug() << "Adding app to list, current count:" << m_apps.size();
    m_apps.append(appWithIcon);
    qDebug() << "App added, new count:" << m_apps.size();
    
    emit appAdded(appWithIcon);
    qDebug() << "appAdded signal emitted";
    
    bool saveResult = saveApps();
    qDebug() << "Save result:" << saveResult;
    
    return true;
}

int AppManager::addApps(const QList<AppInfo> &apps)
{
    qDebug() << "AppManager::addApps called with" << apps.size() << "apps";
    
    int addedCount = 0;
    
    for (const AppInfo &app : apps) {
        // 同じパスのアプリが既に存在するかチェック
        bool exists = false;
        for (const auto &existingApp : m_apps) {
            if (existingApp.path == app.path) {
                qWarning() << "App with same path already exists:" << app.path;
                exists = true;
                break;
            }
        }
        
        if (exists) {
            continue;
        }
        
        if (!app.isValid()) {
            qWarning() << "Invalid app data:" << app.name << app.path;
            continue;
        }
        
        m_apps.append(app);
        addedCount++;
        qDebug() << "Added app:" << app.name;
    }
    
    if (addedCount > 0) {
        emit appsAdded(addedCount);
        saveApps();
        qDebug() << "Successfully added" << addedCount << "apps in batch";
    }
    
    return addedCount;
}

bool AppManager::removeApp(const QString &appId)
{
    qDebug() << "AppManager::removeApp - Attempting to remove app with ID:" << appId;
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].id == appId) {
            QString appName = m_apps[i].name;
            qDebug() << "AppManager::removeApp - Found app at index" << i << ":" << appName;
            m_apps.removeAt(i);
            qDebug() << "AppManager::removeApp - App removed from list, emitting signal";
            emit appRemoved(appId);
            qDebug() << "AppManager::removeApp - Signal emitted, saving apps";
            saveApps();
            qDebug() << "AppManager::removeApp - Successfully removed app:" << appName;
            return true;
        }
    }
    qWarning() << "AppManager::removeApp - App not found:" << appId;
    return false;
}

bool AppManager::updateApp(const QString &appId, const AppInfo &updatedApp)
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].id == appId) {
            m_apps[i] = updatedApp;
            emit appUpdated(updatedApp);
            saveApps();
            return true;
        }
    }
    return false;
}

AppInfo* AppManager::findApp(const QString &appId)
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].id == appId) {
            return &m_apps[i];
        }
    }
    return nullptr;
}

QList<AppInfo> AppManager::getApps() const
{
    return m_apps;
}

QList<AppInfo> AppManager::searchApps(const QString &keyword) const
{
    QList<AppInfo> results;
    QString lowerKeyword = keyword.toLower();
    
    for (const auto &app : m_apps) {
        if (app.name.toLower().contains(lowerKeyword) ||
            app.description.toLower().contains(lowerKeyword) ||
            app.path.toLower().contains(lowerKeyword)) {
            results.append(app);
        }
    }
    
    return results;
}

QList<AppInfo> AppManager::getAppsByCategory(const QString &category) const
{
    if (category == "すべて" || category.isEmpty()) {
        return m_apps;
    }
    
    QList<AppInfo> results;
    for (const auto &app : m_apps) {
        if (app.category == category) {
            results.append(app);
        }
    }
    return results;
}

QList<AppInfo> AppManager::searchAppsInCategory(const QString &keyword, const QString &category) const
{
    QList<AppInfo> categoryApps = getAppsByCategory(category);
    
    if (keyword.isEmpty()) {
        return categoryApps;
    }
    
    QList<AppInfo> results;
    QString lowerKeyword = keyword.toLower();
    
    for (const auto &app : categoryApps) {
        if (app.name.toLower().contains(lowerKeyword) ||
            app.description.toLower().contains(lowerKeyword) ||
            app.path.toLower().contains(lowerKeyword)) {
            results.append(app);
        }
    }
    
    return results;
}

int AppManager::getAppCount() const
{
    return m_apps.size();
}

int AppManager::getAppCountByCategory(const QString &category) const
{
    return getAppsByCategory(category).size();
}

bool AppManager::loadApps()
{
    QFile file(m_dataFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open apps data file for reading:" << m_dataFilePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format in apps data file";
        return false;
    }
    
    QJsonObject rootObj = doc.object();
    
    // カテゴリ情報の読み込み
    if (rootObj.contains("categories")) {
        QJsonObject categoryData;
        categoryData["categories"] = rootObj["categories"];
        m_categoryManager->fromJson(categoryData);
    }
    
    QJsonArray appsArray = rootObj["apps"].toArray();
    
    m_apps.clear();

    // アイコン保存用ディレクトリ
    QString iconDir = QApplication::applicationDirPath() + "/icons";
    QDir(iconDir).mkpath(".");
    IconExtractor iconExtractor;
    bool needsSave = false;

    for (const auto &value : appsArray) {
        if (value.isObject()) {
            AppInfo app;
            app.fromJson(value.toObject());
            if (app.isValid()) {
                // 既存データのiconPath修正: exeファイルパスが設定されている場合は空にする
                if (!app.iconPath.isEmpty() && app.iconPath.endsWith(".exe", Qt::CaseInsensitive)) {
                    qDebug() << "Fixing invalid iconPath for app:" << app.name;
                    app.iconPath = "";
                }

                // iconPathが空、存在しない、またはグレーアイコン（200バイト以下）の場合、アイコンを再生成
                bool needsRegenerate = app.iconPath.isEmpty() || !QFileInfo::exists(app.iconPath);
                if (!needsRegenerate && QFileInfo::exists(app.iconPath)) {
                    QFileInfo iconInfo(app.iconPath);
                    if (iconInfo.size() <= 200) {
                        // グレーアイコン（小さいファイル）なので再生成
                        needsRegenerate = true;
                        QFile::remove(app.iconPath);  // 古いファイルを削除
                    }
                }

                if (needsRegenerate) {
                    QString iconPath = iconExtractor.generateIconPath(app.path, iconDir);
                    // 既存の小さいファイルも削除
                    if (QFileInfo::exists(iconPath) && QFileInfo(iconPath).size() <= 200) {
                        QFile::remove(iconPath);
                    }
                    if (!QFileInfo::exists(iconPath)) {
                        // アイコン抽出・保存
                        if (iconExtractor.extractAndSaveIcon(app.path, iconPath)) {
                            app.iconPath = iconPath;
                            needsSave = true;
                            qDebug() << "Generated icon for:" << app.name << "->" << iconPath;
                        }
                    } else {
                        app.iconPath = iconPath;
                        needsSave = true;
                    }
                }

                m_apps.append(app);
            }
        }
    }

    // アイコンパスが更新された場合は保存
    if (needsSave) {
        saveApps();
    }

    emit dataLoaded();
    qDebug() << "Loaded" << m_apps.size() << "applications";
    return true;
}

bool AppManager::saveApps()
{
    QJsonObject rootObj;
    QJsonArray appsArray;
    
    for (const auto &app : m_apps) {
        appsArray.append(app.toJson());
    }
    
    rootObj["apps"] = appsArray;
    rootObj["version"] = "1.0";
    rootObj["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // カテゴリ情報も保存
    QJsonObject categoryObj = m_categoryManager->toJson();
    rootObj.insert("categories", categoryObj["categories"]);
    
    QJsonDocument doc(rootObj);
    
    QFile file(m_dataFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot open apps data file for writing:" << m_dataFilePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    emit dataSaved();
    qDebug() << "Saved" << m_apps.size() << "applications to" << m_dataFilePath;
    return true;
}

AppInfo* AppManager::getMostLaunchedApp()
{
    if (m_apps.isEmpty()) {
        return nullptr;
    }
    
    AppInfo* mostLaunched = &m_apps[0];
    for (int i = 1; i < m_apps.size(); ++i) {
        if (m_apps[i].launchCount > mostLaunched->launchCount) {
            mostLaunched = &m_apps[i];
        }
    }
    
    return mostLaunched->launchCount > 0 ? mostLaunched : nullptr;
}

AppInfo* AppManager::getRecentlyLaunchedApp()
{
    if (m_apps.isEmpty()) {
        return nullptr;
    }
    
    AppInfo* recentlyLaunched = nullptr;
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].lastLaunch.isValid()) {
            if (!recentlyLaunched || m_apps[i].lastLaunch > recentlyLaunched->lastLaunch) {
                recentlyLaunched = &m_apps[i];
            }
        }
    }
    
    return recentlyLaunched;
}

void AppManager::setDataFilePath(const QString &filePath)
{
    m_dataFilePath = filePath;
}

QString AppManager::getDataFilePath() const
{
    return m_dataFilePath;
}

bool AppManager::validateAppData() const
{
    for (const auto &app : m_apps) {
        if (!app.isValid()) {
            return false;
        }
    }
    return true;
}

void AppManager::cleanupInvalidApps()
{
    auto it = m_apps.begin();
    while (it != m_apps.end()) {
        if (!it->isValid()) {
            QString removedId = it->id;
            it = m_apps.erase(it);
            emit appRemoved(removedId);
            qDebug() << "Removed invalid app:" << it->name;
        } else {
            ++it;
        }
    }
    saveApps();
}

void AppManager::initializeDataFile()
{
    QFileInfo fileInfo(m_dataFilePath);
    QDir dir = fileInfo.absoluteDir();
    
    // ディレクトリが存在しない場合は作成
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // ファイルが存在しない場合は空のJSONファイルを作成
    if (!fileInfo.exists()) {
        QJsonObject rootObj;
        rootObj["apps"] = QJsonArray();
        rootObj["version"] = "1.0";
        rootObj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(rootObj);
        QFile file(m_dataFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
            qDebug() << "Created new apps data file:" << m_dataFilePath;
        }
    }
}

QString AppManager::getDefaultDataFilePath() const
{
    // アプリケーション実行ディレクトリ下に直接保存
    QString appDir = QApplication::applicationDirPath();
    return QDir(appDir).filePath("apps.json");
}

CategoryManager* AppManager::getCategoryManager() const
{
    return m_categoryManager;
}

QStringList AppManager::getUsedCategories() const
{
    QStringList usedCategories;
    for (const auto &app : m_apps) {
        if (!usedCategories.contains(app.category)) {
            usedCategories.append(app.category);
        }
    }
    return usedCategories;
}

void AppManager::updateAppCategory(const QString &appId, const QString &category)
{
    AppInfo *app = findApp(appId);
    if (app) {
        app->category = category;
        emit appUpdated(*app);
        saveApps();
    }
}