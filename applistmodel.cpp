#include "applistmodel.h"
#include <QDateTime>
#include <QDebug>

AppListModel::AppListModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_iconCache(nullptr)
{
}

AppListModel::~AppListModel()
{
}

int AppListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_apps.size();
}

int AppListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant AppListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size())
        return QVariant();

    const AppInfo &app = m_apps.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnName:       return app.name;
        case ColumnPath:       return app.path;
        case ColumnLastLaunch: return formatLastLaunch(app.lastLaunch);
        case ColumnLaunchCount: return formatLaunchCount(app.launchCount);
        }
        break;

    case Qt::DecorationRole:
        if (index.column() == ColumnName && m_iconCache) {
            if (m_iconCache->contains(app.path)) {
                return m_iconCache->value(app.path);
            }
        }
        break;

    case AppIdRole:
        return app.id;

    case AppPathRole:
        return app.path;
    }

    return QVariant();
}

QVariant AppListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case ColumnName:        return tr("アプリ名");
    case ColumnPath:        return tr("パス");
    case ColumnLastLaunch:  return tr("最終起動");
    case ColumnLaunchCount: return tr("起動回数");
    }
    return QVariant();
}

void AppListModel::setApps(const QList<AppInfo> &apps)
{
    beginResetModel();
    m_apps = apps;
    endResetModel();
}

void AppListModel::clear()
{
    beginResetModel();
    m_apps.clear();
    endResetModel();
}

void AppListModel::addApp(const AppInfo &app)
{
    int row = m_apps.size();
    beginInsertRows(QModelIndex(), row, row);
    m_apps.append(app);
    endInsertRows();
}

void AppListModel::removeApp(const QString &appId)
{
    int row = findRow(appId);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_apps.removeAt(row);
        endRemoveRows();
    }
}

void AppListModel::updateApp(const AppInfo &app)
{
    int row = findRow(app.id);
    if (row >= 0) {
        m_apps[row] = app;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    }
}

QString AppListModel::getAppId(int row) const
{
    if (row >= 0 && row < m_apps.size()) {
        return m_apps.at(row).id;
    }
    return QString();
}

int AppListModel::findRow(const QString &appId) const
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps.at(i).id == appId) {
            return i;
        }
    }
    return -1;
}

AppInfo AppListModel::getApp(int row) const
{
    if (row >= 0 && row < m_apps.size()) {
        return m_apps.at(row);
    }
    return AppInfo();
}

int AppListModel::appCount() const
{
    return m_apps.size();
}

void AppListModel::setIconCache(QMap<QString, QIcon> *iconCache)
{
    m_iconCache = iconCache;
}

void AppListModel::notifyIconUpdated(int row)
{
    if (row >= 0 && row < m_apps.size()) {
        QModelIndex idx = index(row, ColumnName);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
}

void AppListModel::notifyAllIconsUpdated()
{
    if (!m_apps.isEmpty()) {
        emit dataChanged(index(0, ColumnName), index(m_apps.size() - 1, ColumnName), {Qt::DecorationRole});
    }
}

QString AppListModel::formatLastLaunch(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return QObject::tr("なし");
    }

    QDateTime now = QDateTime::currentDateTime();
    qint64 secondsAgo = dateTime.secsTo(now);

    if (secondsAgo < 60) {
        return QObject::tr("たった今");
    } else if (secondsAgo < 3600) {
        return QString("%1分前").arg(secondsAgo / 60);
    } else if (secondsAgo < 86400) {
        return QString("%1時間前").arg(secondsAgo / 3600);
    } else if (secondsAgo < 604800) {
        return QString("%1日前").arg(secondsAgo / 86400);
    } else {
        return dateTime.toString("yyyy/MM/dd");
    }
}

QString AppListModel::formatLaunchCount(int count)
{
    return QString("%1回").arg(count);
}
