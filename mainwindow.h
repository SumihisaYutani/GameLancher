#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QLabel>
#include "appinfo.h"
#include "appmanager.h"
#include "applauncher.h"
#include "iconextractor.h"
#include "addappdialog.h"
#include "appdiscoverydialog.h"
#include "applistmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // UI イベント
    void onAddAppButtonClicked();
    void onRemoveAppButtonClicked();
    void onSettingsButtonClicked();
    void onViewModeButtonClicked();
    void onSearchTextChanged();
    void onFilterButtonClicked();
    
    // ロード関連
    void onLoadingFinished();
    void onLoadingProgress();
    
    
    // リストビューイベント
    void onListItemClicked(const QModelIndex &index);
    void onListItemDoubleClicked(const QModelIndex &index);
    
    // アプリ管理イベント
    void onAppAdded(const AppInfo &app);
    void onAppsAdded(int count);
    void onAppRemoved(const QString &appId);
    void onAppUpdated(const AppInfo &app);
    
    // 起動イベント
    void onAppLaunched(const QString &appId);
    void onAppLaunchFinished(const QString &appId, int exitCode);
    void onAppLaunchError(const QString &appId, const QString &error);
    
    // メニューアクション
    void onActionAddApp();
    void onActionDiscoverApps();
    void onActionExit();
    void onActionGridView();
    void onActionListView();
    void onActionRefresh();
    void onActionAbout();
    void onActionClearIconCache();
    
    // その他
    void updateStatusBar();
    

private:
    void setupConnections();
    void loadApplications();
    void loadApplicationsAsync();
    void setupProgressBar();
    void showLoadingProgress();
    void hideLoadingProgress();
    void refreshViews();
    void switchToGridView();
    void switchToListView();
    void updateGridView();
    void updateListView();
    void clearListView();
    void updateAppCount();
    void filterApplications();
    bool launchApplication(const QString &appId);
    void showAppContextMenu(const QString &appId, const QPoint &globalPos);
    void editApplication(const QString &appId);
    void removeApplication(const QString &appId);
    void showAppProperties(const QString &appId);
    
    
    Ui::MainWindow *ui;
    
    // コアコンポーネント
    AppManager *m_appManager;
    AppLauncher *m_appLauncher;
    IconExtractor *m_iconExtractor;
    AppListModel *m_appListModel;
    
    // UI 状態
    bool m_isGridView;
    QString m_currentFilter;
    QString m_selectedAppId;
    
    
    // 統合タイマー（パフォーマンス最適化）
    QTimer *m_mainTimer; // メインタイマー - 複数機能を統合
    QTimer *m_resizeTimer; // リサイズ専用（必要時のみ）
    
    // プログレスバーとロード状態管理
    QProgressBar *m_progressBar;
    QLabel *m_loadingLabel;
    QTimer *m_loadTimer; // ロード完了検知
    
    // ロード状態管理
    bool m_isLoading;
    
    // 32pxアイコンキャッシュシステム
    QMap<QString, QIcon> m_iconCache32px;
    QIcon getOrCreateIcon32px(const QString &filePath);
    void clearIconCache();
    
    // アイコンキャッシュの事前構築
    void preloadAllIconsAsync(const QList<AppInfo> &apps);
    void buildIconCacheStep();
    void onIconCacheCompleted();

    // アプリ情報のキャッシュ
    QList<AppInfo> m_appList;
    QList<AppInfo> m_iconCacheQueue; // アイコンキャッシュ構築待ちキュー
    QTimer *m_iconTimer; // アイコンキャッシュ構築用タイマー
    int m_iconCacheProgress;
};

#endif // MAINWINDOW_H
