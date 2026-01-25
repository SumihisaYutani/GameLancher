#ifndef APPDISCOVERYDIALOG_H
#define APPDISCOVERYDIALOG_H

#include <QDialog>
#include <QFileDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMessageBox>
#include <QTimer>
#include <QCheckBox>

#include "appdiscovery.h"
#include "appmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class AppDiscoveryDialog;
}
QT_END_NAMESPACE

class AppDiscoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AppDiscoveryDialog(AppManager *appManager, QWidget *parent = nullptr);
    ~AppDiscoveryDialog();

private slots:
    void startScan();
    void stopScan();
    void addPath();
    void removePath();
    void clearPaths();
    void addSelectedApps();
    void selectAllApps();
    void selectNoneApps();
    void addToExcludeList();
    void addExcludePattern();
    void clearExcludePatterns();
    void onScanProgress(int current, int total, const QString &currentPath);
    void onAppDiscovered(const AppInfo &app);
    void onScanStarted();
    void onScanFinished(int totalFound);
    void onScanCanceled();
    void onItemSelectionChanged();
    void previewApp(int row, int column);

private:
    void setupUI();
    void updateSelectedCount();
    void addAppToResults(const AppInfo &app);
    ScanOptions getCurrentScanOptions();
    QList<AppInfo> getSelectedApps();
    void setUIEnabled(bool enabled);
    void loadExcludeList();
    void saveExcludeList();
    bool isAppExcluded(const AppInfo &app);
    void removeSelectedFromResults();
    bool isAppExcludedByPattern(const AppInfo &app);
    void addPatternToExcludeFile(const QString &pattern);
    QStringList getExcludePatterns();
    void removeAppsMatchingPattern(const QString &pattern);
    void updatePathButtonStates();

    Ui::AppDiscoveryDialog *ui;
    
    // Data
    AppManager *m_appManager;
    AppDiscovery *m_appDiscovery;
    QList<AppInfo> m_discoveredApps;
    bool m_scanInProgress;
    QHash<QString, QPixmap> m_iconCache;  // アイコンキャッシュ
    QHash<QString, QPixmap> m_iconCacheForPath;  // パスベースのアイコンキャッシュ
    QStringList m_excludeList;  // 除外リスト（パス）
    QStringList m_excludePatterns;  // 除外パターン（ワイルドカード）
    
    // Constants
    enum ColumnIndex {
        COL_SELECTED = 0,
        COL_ICON = 1,
        COL_NAME = 2,
        COL_PATH = 3,
        COL_CATEGORY = 4,
        COL_SIZE = 5
    };
};

#endif // APPDISCOVERYDIALOG_H