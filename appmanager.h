#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include "appinfo.h"

class AppManager : public QObject
{
    Q_OBJECT

public:
    explicit AppManager(QObject *parent = nullptr);
    ~AppManager();
    
    // アプリ管理
    bool addApp(const AppInfo &app);
    bool removeApp(const QString &appId);
    bool updateApp(const QString &appId, const AppInfo &updatedApp);
    AppInfo* findApp(const QString &appId);
    
    // データ取得
    QList<AppInfo> getApps() const;
    QList<AppInfo> searchApps(const QString &keyword) const;
    int getAppCount() const;
    
    // データ永続化
    bool loadApps();
    bool saveApps();
    
    // 統計
    AppInfo* getMostLaunchedApp();
    AppInfo* getRecentlyLaunchedApp();
    
    // データファイル管理
    void setDataFilePath(const QString &filePath);
    QString getDataFilePath() const;
    
    // バリデーション
    bool validateAppData() const;
    void cleanupInvalidApps();

signals:
    void appAdded(const AppInfo &app);
    void appRemoved(const QString &appId);
    void appUpdated(const AppInfo &app);
    void dataLoaded();
    void dataSaved();

private:
    QList<AppInfo> m_apps;
    QString m_dataFilePath;
    
    void initializeDataFile();
    QString getDefaultDataFilePath() const;
};

#endif // APPMANAGER_H