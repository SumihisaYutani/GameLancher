#ifndef APPLISTMODEL_H
#define APPLISTMODEL_H

#include <QAbstractTableModel>
#include <QIcon>
#include <QMap>
#include "appinfo.h"

class AppListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColumnName = 0,
        ColumnPath,
        ColumnLastLaunch,
        ColumnLaunchCount,
        ColumnCount
    };

    enum CustomRole {
        AppIdRole = Qt::UserRole,
        AppPathRole = Qt::UserRole + 1
    };

    explicit AppListModel(QObject *parent = nullptr);
    ~AppListModel() override;

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Data operations
    void setApps(const QList<AppInfo> &apps);
    void clear();
    void addApp(const AppInfo &app);
    void removeApp(const QString &appId);
    void updateApp(const AppInfo &app);

    // Data access
    QString getAppId(int row) const;
    int findRow(const QString &appId) const;
    AppInfo getApp(int row) const;
    int appCount() const;

    // Icon management
    void setIconCache(QMap<QString, QIcon> *iconCache);
    void notifyIconUpdated(int row);
    void notifyAllIconsUpdated();

    // Format utilities (static for shared use)
    static QString formatLastLaunch(const QDateTime &dateTime);
    static QString formatLaunchCount(int count);

private:
    QList<AppInfo> m_apps;
    QMap<QString, QIcon> *m_iconCache;
};

#endif // APPLISTMODEL_H
