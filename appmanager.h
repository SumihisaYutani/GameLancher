#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include "appinfo.h"
#include "categorymanager.h"

class AppManager : public QObject
{
    Q_OBJECT

public:
    explicit AppManager(QObject *parent = nullptr);
    ~AppManager();
    
    // アプリ管理
    bool addApp(const AppInfo &app);
    int addApps(const QList<AppInfo> &apps); // 一括追加機能
    bool removeApp(const QString &appId);
    bool updateApp(const QString &appId, const AppInfo &updatedApp);
    AppInfo* findApp(const QString &appId);
    
    // データ取得
    QList<AppInfo> getApps() const;
    QList<AppInfo> searchApps(const QString &keyword) const;
    QList<AppInfo> getAppsByCategory(const QString &category) const;
    QList<AppInfo> searchAppsInCategory(const QString &keyword, const QString &category) const;
    int getAppCount() const;
    int getAppCountByCategory(const QString &category) const;
    
    // データ永続化
    bool loadApps();
    bool saveApps();
    
    // 統計
    AppInfo* getMostLaunchedApp();
    AppInfo* getRecentlyLaunchedApp();
    
    // カテゴリ管理
    CategoryManager* getCategoryManager() const;
    QStringList getUsedCategories() const;
    void updateAppCategory(const QString &appId, const QString &category);
    
    // データファイル管理
    void setDataFilePath(const QString &filePath);
    QString getDataFilePath() const;
    
    // バリデーション
    bool validateAppData() const;
    void cleanupInvalidApps();

signals:
    void appAdded(const AppInfo &app);
    void appsAdded(int count); // 一括追加完了シグナル
    void appRemoved(const QString &appId);
    void appUpdated(const AppInfo &app);
    void dataLoaded();
    void dataSaved();

private:
    QList<AppInfo> m_apps;
    QString m_dataFilePath;
    CategoryManager *m_categoryManager;
    
    void initializeDataFile();
    QString getDefaultDataFilePath() const;
};

#endif // APPMANAGER_H