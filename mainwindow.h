#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QLabel>
#include "appinfo.h"
#include "appmanager.h"
#include "applauncher.h"
#include "iconextractor.h"
#include "addappdialog.h"
#include "appwidget.h"
#include "appdiscoverydialog.h"

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
    
    // アプリ操作
    void onAppWidgetClicked(const QString &appId);
    void onAppWidgetDoubleClicked(const QString &appId);
    void onAppWidgetRightClicked(const QString &appId, const QPoint &globalPos);
    void onAppEditRequested(const QString &appId);
    void onAppRemoveRequested(const QString &appId);
    void onAppPropertiesRequested(const QString &appId);
    
    // リストビューイベント
    void onListItemClicked(QTreeWidgetItem *item, int column);
    void onListItemDoubleClicked(QTreeWidgetItem *item, int column);
    
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
    
    // UI応答性監視
    void checkUIResponse();
    void startResponseMonitoring();
    void stopResponseMonitoring();

private:
    void setupConnections();
    void loadApplications();
    void loadApplicationsAsync();
    void setupProgressBar();
    void showLoadingProgress();
    void hideLoadingProgress();
    void updateGridViewAsync(const QList<AppInfo> &apps);
    void refreshViews();
    void switchToGridView();
    void switchToListView();
    void updateGridView();
    void updateListView();
    void clearGridView();
    void clearListView();
    void updateAppCount();
    void filterApplications();
    bool launchApplication(const QString &appId);
    void showAppContextMenu(const QString &appId, const QPoint &globalPos);
    void editApplication(const QString &appId);
    void removeApplication(const QString &appId);
    void showAppProperties(const QString &appId);
    
    // ヘルパー関数
    AppWidget* findAppWidget(const QString &appId) const;
    QTreeWidgetItem* findListItem(const QString &appId) const;
    QString formatLastLaunch(const QDateTime &dateTime) const;
    QString formatLaunchCount(int count) const;
    
    Ui::MainWindow *ui;
    
    // コアコンポーネント
    AppManager *m_appManager;
    AppLauncher *m_appLauncher;
    IconExtractor *m_iconExtractor;
    
    // UI 状態
    bool m_isGridView;
    QString m_currentFilter;
    QString m_selectedAppId;
    
    // グリッドレイアウト
    QGridLayout *m_gridLayout;
    QList<AppWidget*> m_appWidgets;
    
    // パフォーマンス最適化用
    int m_cachedColumns;
    int m_lastCalculatedWidth;
    
    // 統合タイマー（パフォーマンス最適化）
    QTimer *m_mainTimer; // メインタイマー - 複数機能を統合
    QTimer *m_resizeTimer; // リサイズ専用（必要時のみ）
    
    // プログレスバーとロード状態管理
    QProgressBar *m_progressBar;
    QLabel *m_loadingLabel;
    QTimer *m_loadTimer; // ロード完了検知
    
    // UI応答性監視用（最適化）
    QElapsedTimer m_lastResponseTime;
    
    // ロード状態管理
    bool m_isLoading;
    bool m_isMonitoringResponse;
    
    // 32pxアイコンキャッシュシステム
    QMap<QString, QIcon> m_iconCache32px;
    QIcon getOrCreateIcon32px(const QString &filePath);
    void clearIconCache();
    
    // アイコンキャッシュの事前構築
    void preloadAllIconsAsync(const QList<AppInfo> &apps);
    void buildIconCacheStep();
    void onIconCacheProgress();
    void onIconCacheCompleted();
    void setIconsStep(); // 段階的アイコン設定
    
    // オンデマンドアイコンローディング（統合最適化）
    void startAsyncIconLoading(const QList<AppInfo> &apps);
    void loadVisibleIcons();
    void onScrollValueChanged();
    void loadIconForItem(QTreeWidgetItem *item, const AppInfo &app);
    QList<AppInfo> m_appList; // アプリ情報のキャッシュ
    QList<AppInfo> m_iconCacheQueue; // アイコンキャッシュ構築待ちキュー
    QTimer *m_iconTimer; // 統合アイコンタイマー（キャッシュ+設定+ロード）
    int m_iconCacheProgress;
    int m_iconSetProgress; // アイコン設定進行状況
    int m_currentIconTask; // 現在のタスク: 0=キャッシュ, 1=設定, 2=ロード
};

#endif // MAINWINDOW_H
