#include "appmanager.h"
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QJsonObject>
#include <QFile>
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
    
    qDebug() << "Adding app to list, current count:" << m_apps.size();
    m_apps.append(app);
    qDebug() << "App added, new count:" << m_apps.size();
    
    emit appAdded(app);
    qDebug() << "appAdded signal emitted";
    
    bool saveResult = saveApps();
    qDebug() << "Save result:" << saveResult;
    
    return true;
}

bool AppManager::removeApp(const QString &appId)
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps[i].id == appId) {
            m_apps.removeAt(i);
            emit appRemoved(appId);
            saveApps();
            return true;
        }
    }
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
    for (const auto &value : appsArray) {
        if (value.isObject()) {
            AppInfo app;
            app.fromJson(value.toObject());
            if (app.isValid()) {
                m_apps.append(app);
            }
        }
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
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        // フォールバック: アプリケーション実行ディレクトリ
        appDataDir = QApplication::applicationDirPath();
    }
    return QDir(appDataDir).filePath("apps.json");
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