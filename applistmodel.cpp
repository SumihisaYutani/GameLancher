#include "applistmodel.h"
#include <QDateTime>
#include <QDebug>

AppListModel::AppListModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_iconCache(nullptr)
    , m_currentPage(0)
    , m_itemsPerPage(50)
{
}

AppListModel::~AppListModel()
{
}

int AppListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;

    // ページング: 現在のページに表示する行数を返す
    int startIndex = m_currentPage * m_itemsPerPage;
    int remaining = m_apps.size() - startIndex;
    if (remaining <= 0) return 0;
    return qMin(remaining, m_itemsPerPage);
}

int AppListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

int AppListModel::actualIndex(int row) const
{
    return m_currentPage * m_itemsPerPage + row;
}

QVariant AppListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int actual = actualIndex(index.row());
    if (actual < 0 || actual >= m_apps.size())
        return QVariant();

    const AppInfo &app = m_apps.at(actual);

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnName:       return app.name;
        case ColumnPath:       return app.path;
        case ColumnLastLaunch: {
            // キャッシュが有効かチェック（10秒以内なら再利用）
            QDateTime now = QDateTime::currentDateTime();
            if (!app.cachedLastLaunchStr.isEmpty() &&
                app.cachedLastLaunchTime.isValid() &&
                app.cachedLastLaunchTime.secsTo(now) < 10) {
                return app.cachedLastLaunchStr;
            }
            // キャッシュを更新
            app.cachedLastLaunchStr = formatLastLaunch(app.lastLaunch);
            app.cachedLastLaunchTime = now;
            return app.cachedLastLaunchStr;
        }
        case ColumnLaunchCount: {
            // キャッシュが有効かチェック
            if (!app.cachedLaunchCountStr.isEmpty()) {
                return app.cachedLaunchCountStr;
            }
            // キャッシュを更新
            app.cachedLaunchCountStr = formatLaunchCount(app.launchCount);
            return app.cachedLaunchCountStr;
        }
        }
        break;

    case Qt::DecorationRole:
        // アイコン表示は無効化（パフォーマンス改善）
        break;

    case AppIdRole:
        return app.id;

    case AppPathRole:
        return app.path;

    case IconPathRole:
        return app.iconPath;
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
    m_currentPage = 0;  // データ変更時は最初のページへ
    endResetModel();
}

void AppListModel::clear()
{
    beginResetModel();
    m_apps.clear();
    m_currentPage = 0;
    endResetModel();
}

void AppListModel::addApp(const AppInfo &app)
{
    // 最後のページに追加される場合のみ表示更新
    int newIndex = m_apps.size();
    int newPage = newIndex / m_itemsPerPage;

    if (newPage == m_currentPage) {
        int row = newIndex - m_currentPage * m_itemsPerPage;
        beginInsertRows(QModelIndex(), row, row);
        m_apps.append(app);
        endInsertRows();
    } else {
        m_apps.append(app);
    }
}

void AppListModel::removeApp(const QString &appId)
{
    int actual = -1;
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps.at(i).id == appId) {
            actual = i;
            break;
        }
    }

    if (actual >= 0) {
        beginResetModel();
        m_apps.removeAt(actual);
        // ページ調整
        if (m_currentPage >= totalPages() && m_currentPage > 0) {
            m_currentPage = totalPages() - 1;
        }
        endResetModel();
    }
}

void AppListModel::updateApp(const AppInfo &app)
{
    int actual = -1;
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps.at(i).id == app.id) {
            actual = i;
            break;
        }
    }

    if (actual >= 0) {
        m_apps[actual] = app;
        // 現在のページに表示されている場合のみ更新
        int startIndex = m_currentPage * m_itemsPerPage;
        int endIndex = startIndex + rowCount();
        if (actual >= startIndex && actual < endIndex) {
            int row = actual - startIndex;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }
    }
}

QString AppListModel::getAppId(int row) const
{
    int actual = actualIndex(row);
    if (actual >= 0 && actual < m_apps.size()) {
        return m_apps.at(actual).id;
    }
    return QString();
}

int AppListModel::findRow(const QString &appId) const
{
    for (int i = 0; i < m_apps.size(); ++i) {
        if (m_apps.at(i).id == appId) {
            // 現在のページ内の行番号を返す
            int startIndex = m_currentPage * m_itemsPerPage;
            if (i >= startIndex && i < startIndex + m_itemsPerPage) {
                return i - startIndex;
            }
            return -1;  // 現在のページにない
        }
    }
    return -1;
}

AppInfo AppListModel::getApp(int row) const
{
    int actual = actualIndex(row);
    if (actual >= 0 && actual < m_apps.size()) {
        return m_apps.at(actual);
    }
    return AppInfo();
}

int AppListModel::appCount() const
{
    return m_apps.size();
}

void AppListModel::setIconCache(QMap<QString, QPixmap> *iconCache)
{
    m_iconCache = iconCache;
}

void AppListModel::setIconLoader(std::function<QPixmap(const QString&)> loader)
{
    m_iconLoader = loader;
}

void AppListModel::notifyIconUpdated(int row)
{
    if (row >= 0 && row < rowCount()) {
        QModelIndex idx = index(row, ColumnName);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
}

void AppListModel::notifyAllIconsUpdated()
{
    int count = rowCount();
    if (count > 0) {
        emit dataChanged(index(0, ColumnName), index(count - 1, ColumnName), {Qt::DecorationRole});
    }
}

// Pagination methods
void AppListModel::setPage(int page)
{
    if (page < 0 || page >= totalPages()) return;
    if (page == m_currentPage) return;

    // layoutChangedを使用（beginResetModelより軽量）
    emit layoutAboutToBeChanged();
    m_currentPage = page;
    emit layoutChanged();
}

void AppListModel::setItemsPerPage(int count)
{
    if (count <= 0 || count == m_itemsPerPage) return;

    beginResetModel();
    m_itemsPerPage = count;
    m_currentPage = 0;
    endResetModel();
}

int AppListModel::totalPages() const
{
    if (m_apps.isEmpty()) return 0;
    return (m_apps.size() + m_itemsPerPage - 1) / m_itemsPerPage;
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
