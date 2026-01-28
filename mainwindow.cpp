#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QResizeEvent>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QElapsedTimer>
#include <QFileIconProvider>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_appManager(new AppManager(this))
    , m_appLauncher(new AppLauncher(this))
    , m_iconExtractor(new IconExtractor(this))
    , m_isGridView(false)
    , m_selectedAppId("")
    , m_gridLayout(nullptr)
    , m_mainTimer(new QTimer(this))
    , m_resizeTimer(new QTimer(this))
    , m_progressBar(nullptr)
    , m_loadingLabel(nullptr)
    , m_loadTimer(new QTimer(this))
    , m_isLoading(false)
    , m_isMonitoringResponse(false)
    , m_iconTimer(new QTimer(this))
    , m_iconCacheProgress(0)
    , m_iconSetProgress(0)
    , m_currentIconTask(0)
{
    ui->setupUi(this);
    setupConnections();
    setupProgressBar();
    
    // 初期状態をリストビューに変更（軽量表示のため）
    m_isGridView = false;
    ui->viewStackedWidget->setCurrentIndex(1);
    ui->actionGridView->setChecked(false);
    ui->actionListView->setChecked(true);
    
    // スクロールエリアのパフォーマンス最適化
    ui->gridScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->gridScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->gridScrollArea->setWidgetResizable(true);
    // スムーススクロールを有効化
    ui->gridScrollArea->verticalScrollBar()->setSingleStep(20);
    ui->gridScrollArea->verticalScrollBar()->setPageStep(100);
    
    // アプリケーションロードと初回描画（非同期で実行）
    loadApplicationsAsync();
    updateStatusBar();
    
    // 統合メインタイマーの設定（複数機能を統合してパフォーマンス向上）
    m_mainTimer->setInterval(2000); // 2秒間隔に変更（CPUリソース節約）
    connect(m_mainTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_mainTimer->start();
    
    // リサイズタイマーの設定（スクロールパフォーマンス最適化）
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(500); // 500msに延長して頻繁な更新を防ぐ
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() {
        if (m_isGridView) {
            updateGridView();
        }
    });
}

MainWindow::~MainWindow()
{
    // タイマーを停止・削除（統合最適化後）
    if (m_mainTimer) {
        m_mainTimer->stop();
        delete m_mainTimer;
        m_mainTimer = nullptr;
    }
    
    if (m_resizeTimer) {
        m_resizeTimer->stop();
        delete m_resizeTimer;
        m_resizeTimer = nullptr;
    }
    
    if (m_loadTimer) {
        m_loadTimer->stop();
        delete m_loadTimer;
        m_loadTimer = nullptr;
    }
    
    if (m_iconTimer) {
        m_iconTimer->stop();
        delete m_iconTimer;
        m_iconTimer = nullptr;
    }
    
    // AppWidgetのクリア
    clearGridView();
    
    // コアコンポーネントの削除
    if (m_appManager) {
        delete m_appManager;
        m_appManager = nullptr;
    }
    
    if (m_appLauncher) {
        delete m_appLauncher;
        m_appLauncher = nullptr;
    }
    
    if (m_iconExtractor) {
        delete m_iconExtractor;
        m_iconExtractor = nullptr;
    }
    
    delete ui;
}

void MainWindow::setupConnections()
{
    // ツールバーボタン
    connect(ui->addAppButton, &QPushButton::clicked, this, &MainWindow::onAddAppButtonClicked);
    connect(ui->removeAppButton, &QPushButton::clicked, this, &MainWindow::onRemoveAppButtonClicked);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
    connect(ui->viewModeButton, &QToolButton::clicked, this, &MainWindow::onViewModeButtonClicked);
    
    // 検索（リアルタイム検索を無効化）
    // connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    
    // 絞り込みボタン
    connect(ui->filterButton, &QPushButton::clicked, this, &MainWindow::onFilterButtonClicked);
    
    // Enterキーでも絞り込み実行
    connect(ui->searchLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onFilterButtonClicked);
    
    // リストビューイベント
    connect(ui->listTreeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onListItemClicked);
    connect(ui->listTreeWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onListItemDoubleClicked);
    
    // スクロール監視でオンデマンドアイコンローディング
    connect(ui->listTreeWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onScrollValueChanged);
    
    // アイコンサイズをセル高に合わせて設定
    ui->listTreeWidget->setIconSize(QSize(32, 32)); // 40pxセルに合うサイズ
    // ui->listTreeWidget->setUniformRowHeights(true); // スクロールパフォーマンス最適化のため無効化
    ui->listTreeWidget->setRootIsDecorated(false); // インデントなし
    
    // 緊急最適化: さらなるパフォーマンス向上
    ui->listTreeWidget->setAlternatingRowColors(false); // 交互色を無効化
    ui->listTreeWidget->setSortingEnabled(false); // ソートを無効化
    ui->listTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection); // 選択モードを簡素化
    
    // スクロールパフォーマンス最適化
    ui->listTreeWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // ピクセル単位スクロール（スムーズ）
    ui->listTreeWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel); // ピクセル単位スクロール
    ui->listTreeWidget->setAutoScroll(false); // 自動スクロール無効
    ui->listTreeWidget->setIndentation(0); // インデント無効でシンプル化
    
    // 大量データ用の最適化設定
    ui->listTreeWidget->setLayoutDirection(Qt::LeftToRight); // レイアウト最適化
    ui->listTreeWidget->setWordWrap(false); // ワードラップ無効
    
    // セルの高さを40px固定 + アイコンキャッシュ方式
    ui->listTreeWidget->setStyleSheet(
        "QTreeWidget::item { "
        "    height: 40px; "  // セルの高さを明確に40pxに指定
        "    padding: 4px; "
        "}"
        "QTreeWidget::item:selected { "
        "    background-color: #3daee9; "
        "    color: white; "
        "}"
    );
    
    // パフォーマンス最適化: 行の高さをQtのデフォルトに任せる
    // ui->listTreeWidget->setUniformRowHeights(true); // これもコメントアウト
    
    // アプリケーション管理イベント
    connect(m_appManager, &AppManager::appAdded, this, &MainWindow::onAppAdded);
    connect(m_appManager, &AppManager::appsAdded, this, &MainWindow::onAppsAdded);
    connect(m_appManager, &AppManager::appRemoved, this, &MainWindow::onAppRemoved);
    connect(m_appManager, &AppManager::appUpdated, this, &MainWindow::onAppUpdated);
    
    // アプリケーション起動イベント
    connect(m_appLauncher, &AppLauncher::launched, this, &MainWindow::onAppLaunched);
    connect(m_appLauncher, &AppLauncher::finished, this, &MainWindow::onAppLaunchFinished);
    connect(m_appLauncher, &AppLauncher::errorOccurred, this, &MainWindow::onAppLaunchError);
    
    // メニューアクション
    connect(ui->actionAddApp, &QAction::triggered, this, &MainWindow::onActionAddApp);
    connect(ui->actionDiscoverApps, &QAction::triggered, this, &MainWindow::onActionDiscoverApps);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit);
    connect(ui->actionGridView, &QAction::triggered, this, &MainWindow::onActionGridView);
    connect(ui->actionListView, &QAction::triggered, this, &MainWindow::onActionListView);
    connect(ui->actionRefresh, &QAction::triggered, this, &MainWindow::onActionRefresh);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout);
    connect(ui->actionClearIconCache, &QAction::triggered, this, &MainWindow::onActionClearIconCache);
    
    // ロード関連の接続
    connect(m_loadTimer, &QTimer::timeout, this, &MainWindow::onLoadingFinished);
    
    // UI応答性監視は削除（パフォーマンス最適化のため）
    
    // 統合アイコンタイマーの設定（パフォーマンス最適化）
    m_iconTimer->setSingleShot(false);
    m_iconTimer->setInterval(100); // 100ms間隔で統合処理
    connect(m_iconTimer, &QTimer::timeout, this, [this]() {
        switch(m_currentIconTask) {
            case 0: buildIconCacheStep(); break;
            case 1: setIconsStep(); break; 
            case 2: loadVisibleIcons(); break;
        }
    });
    
    // UI応答性監視は削除（パフォーマンス最適化のため）
}

void MainWindow::loadApplications()
{
    m_appManager->loadApps();
    refreshViews();
    updateAppCount();
}

void MainWindow::refreshViews()
{
    QElapsedTimer refreshTimer;
    refreshTimer.start();
    qDebug() << "MainWindow::refreshViews - Refreshing views, current mode:" << (m_isGridView ? "Grid" : "List");
    
    if (m_isGridView) {
        QElapsedTimer gridTimer;
        gridTimer.start();
        updateGridView();
        qDebug() << "updateGridView() took:" << gridTimer.elapsed() << "ms";
    } else {
        QElapsedTimer listTimer;
        listTimer.start();
        updateListView();
        qDebug() << "updateListView() took:" << listTimer.elapsed() << "ms";
    }
    
    qDebug() << "MainWindow::refreshViews - Views refreshed in" << refreshTimer.elapsed() << "ms";
}

void MainWindow::switchToGridView()
{
    m_isGridView = true;
    ui->viewStackedWidget->setCurrentIndex(0);
    ui->actionGridView->setChecked(true);
    ui->actionListView->setChecked(false);
    updateGridView();
}

void MainWindow::switchToListView()
{
    m_isGridView = false;
    ui->viewStackedWidget->setCurrentIndex(1);
    ui->actionGridView->setChecked(false);
    ui->actionListView->setChecked(true);
    updateListView();
}

void MainWindow::updateGridView()
{
    if (m_isLoading) return; // ロード中は更新しない
    
    // 最終的な解決策: グリッドビューをリストビューに切り替え
    qDebug() << "Grid view performance issue detected - switching to list view for better performance";
    switchToListView();
    
    // 以下の重いグリッド処理を無効化
    /*
    clearGridView();
    
    // グリッドレイアウトの準備 - UIファイルで定義済みのレイアウトを使用
    QWidget *gridWidget = ui->gridScrollAreaWidgetContents;
    if (!m_gridLayout) {
        // UIファイルで定義されたappGridLayoutを取得
        m_gridLayout = gridWidget->findChild<QGridLayout*>("appGridLayout");
        if (!m_gridLayout) {
            // 存在するレイアウトを取得するか新規作成
            m_gridLayout = qobject_cast<QGridLayout*>(gridWidget->layout());
            if (!m_gridLayout) {
                m_gridLayout = new QGridLayout(gridWidget);
                m_gridLayout->setSpacing(8);
                m_gridLayout->setContentsMargins(10, 10, 10, 10);
            }
        }
        qDebug() << "Grid layout initialized:" << (m_gridLayout ? "success" : "failed");
    }
    
    // フィルタリング
    QList<AppInfo> apps = m_appManager->getApps();
    if (!m_currentFilter.isEmpty()) {
        apps = m_appManager->searchApps(m_currentFilter);
    }
    
    if (apps.isEmpty()) {
        return; // アプリがない場合は処理しない
    }
    
    // アプリウィジェットの作成と配置を段階的に実行
    updateGridViewAsync(apps);
    */
}

void MainWindow::updateListView()
{
    if (m_isLoading) return; // ロード中は更新しない
    
    clearListView();
    
    QElapsedTimer listTimer;
    listTimer.start();
    
    // フィルタリング
    QList<AppInfo> apps = m_appManager->getApps();
    if (!m_currentFilter.isEmpty()) {
        apps = m_appManager->searchApps(m_currentFilter);
    }
    
    if (apps.isEmpty()) {
        return;
    }
    
    // アプリ情報をキャッシュ
    m_appList = apps;
    
    qDebug() << "LIST VIEW: Processing" << apps.size() << "apps (text-only mode)";
    
    ui->listTreeWidget->setUpdatesEnabled(false); // 描画を一時停止
    
    // 一括でアイテムを作成（アイコンなし、テキストのみ）
    QList<QTreeWidgetItem*> items;
    items.reserve(apps.size());
    
    for (const AppInfo &app : apps) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, app.id);
        item->setText(0, app.name);
        item->setText(1, app.path);
        item->setText(2, formatLastLaunch(app.lastLaunch));
        item->setText(3, formatLaunchCount(app.launchCount));
        
        // アイコンは後で事前キャッシュから設定
        // item->setIcon(0, QIcon()); // アイコンは設定しない
        
        items.append(item);
    }
    
    // 一括でツリーウィジェットに追加
    ui->listTreeWidget->addTopLevelItems(items);
    
    // カラムサイズの調整
    ui->listTreeWidget->header()->resizeSection(0, 200);
    ui->listTreeWidget->header()->resizeSection(1, 300);
    ui->listTreeWidget->header()->resizeSection(2, 150);
    ui->listTreeWidget->header()->resizeSection(3, 100);
    
    ui->listTreeWidget->setUpdatesEnabled(true); // 描画を再開
    
    qDebug() << "LIST VIEW: Completed in" << listTimer.elapsed() << "ms for" << apps.size() << "apps (text-only)";
    
    // フィルタリング時は既存キャッシュを使用、初期表示時のみキャッシュ構築
    if (m_currentFilter.isEmpty()) {
        // 初期表示時のみアイコンキャッシュを構築
        qDebug() << "Initial load - starting icon preload";
        preloadAllIconsAsync(apps);
    } else {
        // フィルタリング時はアイコン処理をスキップ（高速化のため）
        qDebug() << "Filtering - skipping icon processing for speed";
        // アイコンは後でオンデマンド表示または一切表示しない
    }
}

void MainWindow::clearGridView()
{
    qDebug() << "MainWindow::clearGridView - Clearing" << m_appWidgets.size() << "widgets";
    
    // レイアウトからウィジェットを削除（ウィジェット自体は削除しない）
    if (m_gridLayout) {
        while (m_gridLayout->count() > 0) {
            QLayoutItem *item = m_gridLayout->takeAt(0);
            if (item) {
                // QLayoutItemのみを削除、ウィジェットは後で削除
                delete item;
            }
        }
    }
    
    // ウィジェットを安全に削除
    for (AppWidget *widget : std::as_const(m_appWidgets)) {
        if (widget) {
            widget->setParent(nullptr); // 親子関係を断つ
            widget->deleteLater(); // 安全な削除
        }
    }
    m_appWidgets.clear();
    
    qDebug() << "MainWindow::clearGridView - Grid view cleared";
}

void MainWindow::clearListView()
{
    ui->listTreeWidget->clear();
}

void MainWindow::updateAppCount()
{
    int totalCount = m_appManager->getAppCount();
    int filteredCount = m_currentFilter.isEmpty() ? totalCount : m_appManager->searchApps(m_currentFilter).size();
    
    QString text;
    if (m_currentFilter.isEmpty()) {
        text = QString("登録アプリ: %1個").arg(totalCount);
    } else {
        text = QString("登録アプリ: %1個 (フィルタ結果: %2個)").arg(totalCount).arg(filteredCount);
    }
    
    ui->appCountLabel->setText(text);
}

void MainWindow::filterApplications()
{
    refreshViews();
    updateAppCount();
}

bool MainWindow::launchApplication(const QString &appId)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) {
        QMessageBox::warning(this, "エラー", "アプリケーションが見つかりません。");
        return false;
    }
    
    if (!app->fileExists()) {
        QMessageBox::warning(this, "エラー", 
                           QString("アプリケーションファイルが見つかりません: %1").arg(app->path));
        return false;
    }
    
    bool success = m_appLauncher->launch(*app);
    if (success) {
        // 軽量UI更新（AppManager更新は不要）
        updateStatusBar();
    }
    
    return success;
}

// UI イベントハンドラ
void MainWindow::onAddAppButtonClicked()
{
    qDebug() << "Add app button clicked";
    AddAppDialog dialog(m_appManager->getCategoryManager(), this);
    if (dialog.exec() == QDialog::Accepted) {
        AppInfo newApp = dialog.getAppInfo();
        qDebug() << "Dialog accepted, app info:" << newApp.name << newApp.path;
        if (m_appManager->addApp(newApp)) {
            qDebug() << "App added successfully, total apps:" << m_appManager->getAppCount();
            statusBar()->showMessage("アプリケーションを追加しました: " + newApp.name, 3000);
        } else {
            qDebug() << "Failed to add app";
            QMessageBox::warning(this, "エラー", "アプリケーションの追加に失敗しました。");
        }
    } else {
        qDebug() << "Dialog cancelled or failed";
    }
}

void MainWindow::onRemoveAppButtonClicked()
{
    if (m_selectedAppId.isEmpty()) {
        QMessageBox::information(this, "情報", "削除するアプリケーションを選択してください。");
        return;
    }
    
    removeApplication(m_selectedAppId);
}

void MainWindow::onSettingsButtonClicked()
{
    QMessageBox::information(this, "設定", "設定機能は今後のバージョンで実装予定です。");
}

void MainWindow::onViewModeButtonClicked()
{
    // パフォーマンス問題のためグリッドビューを無効化
    if (m_isGridView) {
        switchToListView();
    } else {
        // グリッドビューへの切り替えを禁止
        qDebug() << "Grid view disabled for performance reasons";
        // switchToGridView(); // コメントアウト
    }
}

void MainWindow::onSearchTextChanged()
{
    // リアルタイム検索は無効化済み（パフォーマンス改善のため）
}

void MainWindow::onFilterButtonClicked()
{
    m_currentFilter = ui->searchLineEdit->text().trimmed();
    filterApplications();
}

// アプリウィジェットイベント
void MainWindow::onAppWidgetClicked(const QString &appId)
{
    // 既に同じアプリが選択済みの場合は何もしない
    if (m_selectedAppId == appId) {
        return;
    }
    
    QString previousSelectedId = m_selectedAppId;
    m_selectedAppId = appId;
    ui->removeAppButton->setEnabled(!appId.isEmpty());
    
    // 最適化: 変更が必要なウィジェットのみ更新
    for (AppWidget *widget : std::as_const(m_appWidgets)) {
        const QString &widgetId = widget->getAppInfo().id;
        bool shouldBeSelected = (widgetId == appId);
        
        // 状態変更が必要な場合のみ更新
        if (widget->isSelected() != shouldBeSelected) {
            widget->setSelected(shouldBeSelected);
        }
    }
}

void MainWindow::onAppWidgetDoubleClicked(const QString &appId)
{
    qDebug() << "MainWindow::onAppWidgetDoubleClicked - Launching app:" << appId;
    launchApplication(appId);
}

void MainWindow::onAppWidgetRightClicked(const QString &appId, const QPoint &globalPos)
{
    Q_UNUSED(globalPos)
    showAppContextMenu(appId, globalPos);
}

void MainWindow::onAppEditRequested(const QString &appId)
{
    editApplication(appId);
}

void MainWindow::onAppRemoveRequested(const QString &appId)
{
    removeApplication(appId);
}

void MainWindow::onAppPropertiesRequested(const QString &appId)
{
    showAppProperties(appId);
}

// リストビューイベント
void MainWindow::onListItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item) {
        m_selectedAppId = item->data(0, Qt::UserRole).toString();
        ui->removeAppButton->setEnabled(!m_selectedAppId.isEmpty());
    }
}

void MainWindow::onListItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item) {
        QString appId = item->data(0, Qt::UserRole).toString();
        launchApplication(appId);
    }
}

// アプリ管理イベント
void MainWindow::onAppAdded(const AppInfo &app)
{
    Q_UNUSED(app)
    refreshViews();
    updateAppCount();
}

void MainWindow::onAppsAdded(int count)
{
    qDebug() << "MainWindow::onAppsAdded - Added" << count << "apps in batch";
    refreshViews();
    updateAppCount();
    statusBar()->showMessage(QString("%1個のアプリケーションを追加しました").arg(count), 3000);
}

void MainWindow::onAppRemoved(const QString &appId)
{
    qDebug() << "MainWindow::onAppRemoved - Removing app:" << appId;
    refreshViews();
    updateAppCount();
    if (m_selectedAppId == appId) {
        m_selectedAppId.clear();
        ui->removeAppButton->setEnabled(false);
        qDebug() << "Cleared selected app ID and disabled remove button";
    }
}

void MainWindow::onAppUpdated(const AppInfo &app)
{
    // 特定のアプリのみ更新（全体再構築を避ける）
    AppWidget* widget = findAppWidget(app.id);
    if (widget) {
        widget->updateAppInfo(app);
    }
    
    // ステータスバーの軽量更新
    updateStatusBar();
}

// 起動イベント
void MainWindow::onAppLaunched(const QString &appId)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (app) {
        statusBar()->showMessage(QString("起動しました: %1").arg(app->name), 3000);
    }
}

void MainWindow::onAppLaunchFinished(const QString &appId, int exitCode)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (app) {
        QString message = QString("%1 が終了しました (Exit Code: %2)").arg(app->name).arg(exitCode);
        statusBar()->showMessage(message, 3000);
        
        // 軽量なステータス更新のみ（全体再構築は不要）
        updateStatusBar();
    }
}

void MainWindow::onAppLaunchError(const QString &appId, const QString &error)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (app) {
        QString message = QString("起動エラー: %1 - %2").arg(app->name, error);
        QMessageBox::warning(this, "起動エラー", message);
    }
}

// メニューアクション
void MainWindow::onActionAddApp()
{
    onAddAppButtonClicked();
}

void MainWindow::onActionDiscoverApps()
{
    AppDiscoveryDialog dialog(m_appManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshViews();
        updateAppCount();
        statusBar()->showMessage("アプリケーションの自動検出が完了しました", 3000);
    }
}

void MainWindow::onActionExit()
{
    close();
}

void MainWindow::onActionGridView()
{
    // パフォーマンス問題のためグリッドビューを無効化
    qDebug() << "Grid view disabled for performance reasons - staying in list view";
    // switchToGridView(); // コメントアウト
}

void MainWindow::onActionListView()
{
    switchToListView();
}

void MainWindow::onActionRefresh()
{
    qDebug() << "Refresh requested - keeping icon cache";
    // リフレッシュではアイコンキャッシュは保持（パフォーマンス重視）
    // アイコンキャッシュクリアは専用メニューから実行
    loadApplications();
    statusBar()->showMessage("アプリケーションリストを更新しました", 2000);
}

void MainWindow::onActionAbout()
{
    QMessageBox::about(this, "Game Launcherについて",
                      "Game Launcher v1.0\n\n"
                      "Windows用アプリケーションランチャー\n"
                      "Qt " QT_VERSION_STR " で開発\n\n"
                      "© 2026 Game Launcher Project");
}

void MainWindow::onActionClearIconCache()
{
    int ret = QMessageBox::question(this, "アイコンキャッシュクリア",
                                   QString("現在 %1 個のアイコンがキャッシュされています。\n"
                                          "すべてのアイコンキャッシュをクリアして再構築しますか？")
                                          .arg(m_iconCache32px.size()),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        qDebug() << "User requested icon cache clear";
        
        // アイコンキャッシュをクリア
        clearIconCache();
        
        // リストビューからアイコンを削除
        for (int i = 0; i < ui->listTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = ui->listTreeWidget->topLevelItem(i);
            if (item) {
                item->setIcon(0, QIcon()); // アイコンを削除
            }
        }
        
        // アイコンキャッシュを再構築
        if (!m_appList.isEmpty()) {
            preloadAllIconsAsync(m_appList);
        }
        
        statusBar()->showMessage("アイコンキャッシュをクリアしました。再構築中...", 3000);
    }
}

void MainWindow::updateStatusBar()
{
    // 最近起動したアプリの情報を表示
    AppInfo *recentApp = m_appManager->getRecentlyLaunchedApp();
    if (recentApp) {
        QString lastLaunchText = QString("最終起動: %1 (%2)")
                                .arg(recentApp->name, formatLastLaunch(recentApp->lastLaunch));
        ui->lastLaunchLabel->setText(lastLaunchText);
    } else {
        ui->lastLaunchLabel->setText("最終起動: なし");
    }
}

// ヘルパー関数
void MainWindow::showAppContextMenu(const QString &appId, const QPoint &globalPos)
{
    Q_UNUSED(appId)
    Q_UNUSED(globalPos)
    // AppWidgetで実装済み
}

void MainWindow::editApplication(const QString &appId)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) {
        QMessageBox::warning(this, "エラー", "アプリケーションが見つかりません。");
        return;
    }
    
    AddAppDialog dialog(*app, m_appManager->getCategoryManager(), this);
    dialog.setEditMode(true);
    
    if (dialog.exec() == QDialog::Accepted) {
        AppInfo updatedApp = dialog.getAppInfo();
        updatedApp.id = app->id; // IDは変更しない
        
        if (m_appManager->updateApp(appId, updatedApp)) {
            statusBar()->showMessage("アプリケーション情報を更新しました: " + updatedApp.name, 3000);
        } else {
            QMessageBox::warning(this, "エラー", "アプリケーション情報の更新に失敗しました。");
        }
    }
}

void MainWindow::removeApplication(const QString &appId)
{
    qDebug() << "MainWindow::removeApplication - Starting removal for app ID:" << appId;
    
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) {
        qWarning() << "MainWindow::removeApplication - App not found:" << appId;
        QMessageBox::warning(this, "エラー", "アプリケーションが見つかりません。");
        return;
    }
    
    QString appName = app->name; // 削除前に名前を保存
    qDebug() << "MainWindow::removeApplication - Found app:" << appName;
    
    int ret = QMessageBox::question(this, "確認",
                                   QString("'%1' を削除しますか？").arg(appName),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        qDebug() << "MainWindow::removeApplication - User confirmed deletion, proceeding";
        if (m_appManager->removeApp(appId)) {
            statusBar()->showMessage("アプリケーションを削除しました: " + appName, 3000);
            qDebug() << "MainWindow::removeApplication - Successfully removed app:" << appName;
        } else {
            qWarning() << "MainWindow::removeApplication - Failed to remove app:" << appId;
            QMessageBox::warning(this, "エラー", "アプリケーションの削除に失敗しました。");
        }
    } else {
        qDebug() << "MainWindow::removeApplication - User cancelled deletion";
    }
}

void MainWindow::showAppProperties(const QString &appId)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) {
        QMessageBox::warning(this, "エラー", "アプリケーションが見つかりません。");
        return;
    }
    
    QString properties = QString(
        "<h3>%1</h3>"
        "<p><b>パス:</b> %2</p>"
        "<p><b>作成日:</b> %3</p>"
        "<p><b>起動回数:</b> %4回</p>"
        "<p><b>最終起動:</b> %5</p>"
        "<p><b>説明:</b> %6</p>"
    ).arg(app->name,
          app->path,
          app->createdAt.toString("yyyy/MM/dd hh:mm"),
          QString::number(app->launchCount),
          formatLastLaunch(app->lastLaunch),
          app->description.isEmpty() ? "なし" : app->description);
    
    QMessageBox::information(this, "アプリケーションのプロパティ", properties);
}

void MainWindow::setupProgressBar()
{
    // プログレスバーをステータスバーに追加
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 0); // インジケーター表示
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setMaximumWidth(200);
    
    m_loadingLabel = new QLabel("アプリケーションを読み込み中...", this);
    m_loadingLabel->setVisible(false);
    
    ui->statusbar->addPermanentWidget(m_loadingLabel);
    ui->statusbar->addPermanentWidget(m_progressBar);
}

void MainWindow::showLoadingProgress()
{
    m_loadingLabel->setVisible(true);
    m_progressBar->setVisible(true);
    ui->statusbar->showMessage("初期化中...");
}

void MainWindow::hideLoadingProgress()
{
    m_loadingLabel->setVisible(false);
    m_progressBar->setVisible(false);
    ui->statusbar->clearMessage();
}

void MainWindow::loadApplicationsAsync()
{
    if (m_isLoading) return;
    
    m_isLoading = true;
    showLoadingProgress();
    
    // まず最初の100個のみ表示して画面を早期表示
    m_loadTimer->setSingleShot(true);
    m_loadTimer->setInterval(10); // 10ms後に実行
    m_loadTimer->start();
}

void MainWindow::onLoadingFinished()
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    qDebug() << "=== PERFORMANCE ANALYSIS START ===";
    
    // アプリケーションデータをロード
    QElapsedTimer loadTimer;
    loadTimer.start();
    m_appManager->loadApps();
    qDebug() << "AppManager::loadApps() took:" << loadTimer.elapsed() << "ms";
    
    // m_uiUpdateTimer は統合されて削除済み
    hideLoadingProgress();
    m_isLoading = false;
    
    // UIの更新を段階的に実行
    QTimer::singleShot(50, this, [this, totalTimer]() mutable {
        QElapsedTimer viewTimer;
        viewTimer.start();
        refreshViews();
        qDebug() << "refreshViews() took:" << viewTimer.elapsed() << "ms";
        qDebug() << "Total time so far:" << totalTimer.elapsed() << "ms";
    });
    
    QTimer::singleShot(100, this, [this, totalTimer]() mutable {
        QElapsedTimer countTimer;
        countTimer.start();
        updateAppCount();
        qDebug() << "updateAppCount() took:" << countTimer.elapsed() << "ms";
        qDebug() << "=== TOTAL TIME:" << totalTimer.elapsed() << "ms ===";
    });
}

void MainWindow::onLoadingProgress()
{
    // プログレスバーのアニメーション（視覚的フィードバック）
    static int counter = 0;
    counter = (counter + 1) % 10;
    
    if (counter == 0) {
        m_loadingLabel->setText("アプリケーションを読み込み中.");
    } else if (counter == 3) {
        m_loadingLabel->setText("アプリケーションを読み込み中..");
    } else if (counter == 6) {
        m_loadingLabel->setText("アプリケーションを読み込み中...");
    }
}

void MainWindow::updateGridViewAsync(const QList<AppInfo> &apps)
{
    QElapsedTimer asyncTimer;
    asyncTimer.start();
    
    QWidget *gridWidget = ui->gridScrollAreaWidgetContents;
    
    // パフォーマンス最適化: 列数を固定して計算コストを削減
    const int maxCols = 8; // 固定値でパフォーマンス向上
    
    // 以下の動的計算を無効化して高速化
    // int availableWidth = gridWidget->width();
    // if (availableWidth <= 0) {
    //     availableWidth = ui->gridScrollArea->width() - 20;
    // }
    // const int appWidgetWidth = 110;
    // const int gridSpacing = 8;
    // const int gridMargin = 10;
    // int maxCols = qMax(1, (availableWidth - gridMargin * 2 + gridSpacing) / (appWidgetWidth + gridSpacing));
    // maxCols = qMin(maxCols, 15);
    
    // パフォーマンス最適化: デバッグログを削減して処理を軽量化
    // qDebug() << "Grid layout - Available width:" << availableWidth << "Calculated columns:" << maxCols;
    // qDebug() << "Creating" << apps.size() << "app widgets for batch display";
    
    // シンプルな一括読み込み（安全のため段階的読み込みを無効化）
    // QElapsedTimer widgetCreationTimer;
    // widgetCreationTimer.start();
    
    // 表示数制限を解除: 全てのアプリを表示
    int maxVisibleApps = apps.size(); // 制限なし
    
    QList<AppWidget*> newWidgets;
    newWidgets.reserve(maxVisibleApps); // 表示するアプリ数に合わせて予約
    qDebug() << "UNLIMITED DISPLAY: Showing all" << maxVisibleApps << "apps without restriction";
    
    for (int i = 0; i < maxVisibleApps; ++i) {
        const AppInfo &app = apps[i];
        AppWidget *appWidget = new AppWidget(app, gridWidget);
        
        // シグナル接続
        connect(appWidget, &AppWidget::clicked, this, &MainWindow::onAppWidgetClicked);
        connect(appWidget, &AppWidget::doubleClicked, this, &MainWindow::onAppWidgetDoubleClicked);
        connect(appWidget, &AppWidget::editRequested, this, &MainWindow::onAppEditRequested);
        connect(appWidget, &AppWidget::removeRequested, this, &MainWindow::onAppRemoveRequested);
        connect(appWidget, &AppWidget::propertiesRequested, this, &MainWindow::onAppPropertiesRequested);
        
        newWidgets.append(appWidget);
    }
    
    // 残りのアプリ数を表示
    if (apps.size() > maxVisibleApps) {
        qDebug() << "Note:" << (apps.size() - maxVisibleApps) << "apps hidden for performance";
    }
    
    // パフォーマンス最適化: タイマーやログを無効化
    // qDebug() << "Widget creation took:" << widgetCreationTimer.elapsed() << "ms for" << newWidgets.size() << "widgets";
    
    // シンプルな同期レイアウト追加（安定化のため）
    // QElapsedTimer layoutTimer;
    // layoutTimer.start();
    
    // 全ウィジェットを一括でレイアウトに追加
    for (int i = 0; i < newWidgets.size(); ++i) {
        int row = i / maxCols;
        int col = i % maxCols;
        
        AppWidget *appWidget = newWidgets[i];
        m_gridLayout->addWidget(appWidget, row, col);
        m_appWidgets.append(appWidget);
    }
    
    // qDebug() << "Layout addition took:" << layoutTimer.elapsed() << "ms";
    
    // レイアウトの調整
    if (!newWidgets.isEmpty()) {
        int lastRow = (newWidgets.size() - 1) / maxCols;
        int lastCol = (newWidgets.size() - 1) % maxCols;
        
        for (int i = lastCol + 1; i < maxCols; ++i) {
            m_gridLayout->setColumnStretch(i, 1);
        }
        m_gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), lastRow + 1, 0);
    }
    
    // 最終的なレイアウト更新（スクロールパフォーマンス最適化）
    // updateGeometry()とupdate()を削除してスクロール時の固まりを解決
    // gridWidget->updateGeometry();
    // gridWidget->update();
    
    // qDebug() << "updateGridViewAsync TOTAL took:" << asyncTimer.elapsed() << "ms for" << newWidgets.size() << "widgets";
}

// 32pxアイコンキャッシュシステムの実装
QIcon MainWindow::getOrCreateIcon32px(const QString &filePath)
{
    // キャッシュにあるかチェック
    if (m_iconCache32px.contains(filePath)) {
        return m_iconCache32px[filePath];
    }
    
    QIcon resultIcon;
    
    // 1. 保存済みアイコンファイルを最優先で使用（登録時に生成済み）
    QString iconPath = m_iconExtractor->generateIconPath(filePath);
    if (QFileInfo::exists(iconPath)) {
        QPixmap pixmap(iconPath);
        if (!pixmap.isNull()) {
            // 32pxにリサイズしてキャッシュ（高速処理）
            QPixmap scaledPixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation);
            resultIcon = QIcon(scaledPixmap);
            // 保存済みアイコンが見つかったので、他の重い処理をスキップ
            m_iconCache32px[filePath] = resultIcon;
            return resultIcon;
        }
    }
    
    // 2. 保存済みアイコンがない場合、ファイルアイコンを取得
    if (QFileInfo::exists(filePath)) {
        QFileIconProvider iconProvider;
        QFileInfo fileInfo(filePath);
        resultIcon = iconProvider.icon(fileInfo);
    }
    
    // 3. それでもない場合はデフォルトアイコン
    if (resultIcon.isNull()) {
        resultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    
    // キャッシュに保存
    m_iconCache32px[filePath] = resultIcon;
    
    return resultIcon;
}

// アイコンキャッシュをクリア
void MainWindow::clearIconCache()
{
    qDebug() << "Clearing icon cache..." << m_iconCache32px.size() << "cached icons";
    m_iconCache32px.clear();
    qDebug() << "Icon cache cleared.";
}


AppWidget* MainWindow::findAppWidget(const QString &appId) const
{
    for (AppWidget *widget : m_appWidgets) {
        if (widget->getAppInfo().id == appId) {
            return widget;
        }
    }
    return nullptr;
}

QTreeWidgetItem* MainWindow::findListItem(const QString &appId) const
{
    for (int i = 0; i < ui->listTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->listTreeWidget->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == appId) {
            return item;
        }
    }
    return nullptr;
}

QString MainWindow::formatLastLaunch(const QDateTime &dateTime) const
{
    if (!dateTime.isValid()) {
        return "なし";
    }
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 secondsAgo = dateTime.secsTo(now);
    
    if (secondsAgo < 60) {
        return "たった今";
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

QString MainWindow::formatLaunchCount(int count) const
{
    return QString("%1回").arg(count);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    // UI応答性監視: リサイズイベントの記録
    if (m_isMonitoringResponse) {
        m_lastResponseTime.restart();
    }
    
    QMainWindow::resizeEvent(event);
    
    // グリッドビューの場合のみ、リサイズに応じてレイアウトを更新
    if (m_isGridView && m_resizeTimer) {
        m_resizeTimer->start(); // タイマーをリスタート
    }
}

// UI応答性監視の実装
// UI応答性監視機能は無効化（パフォーマンス最適化のため）
void MainWindow::startResponseMonitoring()
{
    // 無効化
    qDebug() << "UI response monitoring disabled for performance";
}

void MainWindow::stopResponseMonitoring()
{
    // 無効化
}

void MainWindow::checkUIResponse()
{
    if (!m_isMonitoringResponse) return;
    
    qint64 elapsed = m_lastResponseTime.elapsed();
    
    // 2秒以上応答がない場合、固まりと判定
    if (elapsed > 2000) {
        qWarning() << "*** UI FREEZE DETECTED ***";
        qWarning() << "No UI response for" << elapsed << "ms";
        qWarning() << "Current view mode:" << (m_isGridView ? "Grid" : "List");
        qWarning() << "App count:" << m_appManager->getAppCount();
        qWarning() << "Widgets count:" << m_appWidgets.size();
        
        // 監視をリセット
        m_lastResponseTime.restart();
    } else if (elapsed > 500) {
        // 500ms以上の遅延を警告
        qDebug() << "UI response delay detected:" << elapsed << "ms";
    }
}

// アイコンキャッシュの事前構築の実装
void MainWindow::preloadAllIconsAsync(const QList<AppInfo> &apps)
{
    qDebug() << "Starting preload of" << apps.size() << "icons in background";
    
    // キャッシュ構築キューを準備
    m_iconCacheQueue = apps;
    m_iconCacheProgress = 0;
    
    // プログレスバーを表示
    m_loadingLabel->setText("アイコンをキャッシュ中...");
    m_loadingLabel->setVisible(true);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, apps.size());
    m_progressBar->setValue(0);
    
    // アイコンキャッシュをメモリに読み込み（ファイルは登録時に生成済み）
    qDebug() << "Loading pre-generated icons into memory cache";
    m_iconCacheQueue = apps;
    m_iconCacheProgress = 0;
    
    // 高速アイコンキャッシュ構築（ファイルから読み込みのみ）
    m_currentIconTask = 0; // キャッシュ読み込み
    m_iconTimer->start();
}

void MainWindow::buildIconCacheStep()
{
    const int batchSize = 10; // 1ステップで10個ずつ処理
    int processed = 0;
    
    while (processed < batchSize && m_iconCacheProgress < m_iconCacheQueue.size()) {
        const AppInfo &app = m_iconCacheQueue[m_iconCacheProgress];
        
        // キャッシュに存在しない場合のみ構築
        if (!m_iconCache32px.contains(app.path)) {
            QIcon icon = getOrCreateIcon32px(app.path);
            // getOrCreateIcon32px内でキャッシュに保存される
        }
        
        m_iconCacheProgress++;
        processed++;
        
        // プログレスバー更新
        m_progressBar->setValue(m_iconCacheProgress);
        
        if (m_iconCacheProgress % 50 == 0) {
            qDebug() << "Icon cache progress:" << m_iconCacheProgress << "/" << m_iconCacheQueue.size();
        }
    }
    
    // 完了チェック
    if (m_iconCacheProgress >= m_iconCacheQueue.size()) {
        qDebug() << "=== CACHE CONSTRUCTION FINISHED ===";
        qDebug() << "Total processed:" << m_iconCacheProgress;
        qDebug() << "Cache size:" << m_iconCache32px.size();
        m_iconTimer->stop();
        qDebug() << "Timer stopped, calling onIconCacheCompleted()";
        onIconCacheCompleted();
    }
}

void MainWindow::onIconCacheCompleted()
{
    qDebug() << "=== onIconCacheCompleted START ===";
    qDebug() << "Icon cache construction completed!" << m_iconCache32px.size() << "icons cached";
    qDebug() << "App list size:" << m_appList.size();
    qDebug() << "List widget item count:" << (ui->listTreeWidget ? ui->listTreeWidget->topLevelItemCount() : -1);
    
    // プログレスバーのメッセージを変更
    m_loadingLabel->setText("アイコンを設定中...");
    m_progressBar->setRange(0, m_appList.size());
    m_progressBar->setValue(0);
    
    // 段階的アイコン設定を開始
    m_iconSetProgress = 0;
    
    qDebug() << "Starting unified icon timer with interval:" << m_iconTimer->interval() << "ms";
    qDebug() << "Timer active before start:" << m_iconTimer->isActive();
    m_currentIconTask = 1; // 設定フェーズ
    m_iconTimer->start();
    qDebug() << "Timer active after start:" << m_iconTimer->isActive();
    qDebug() << "Timer remaining time:" << m_iconTimer->remainingTime();
    
    // 強制的にsetIconsStepを1回実行してテスト（デバッグ完了のため無効化）
    // qDebug() << "*** MANUAL TEST: Calling setIconsStep directly ***";
    // setIconsStep();
    
    qDebug() << "=== onIconCacheCompleted END ===";
}

void MainWindow::setIconsStep()
{
    QElapsedTimer stepTimer;
    stepTimer.start();
    
    // デバッグログを50回に1回に削減
    if (m_iconSetProgress % 50 == 0) {
        qDebug() << "=== setIconsStep START - Progress:" << m_iconSetProgress 
                 << "/" << m_appList.size() << "===";
    }
    
    const int batchSize = 100; // 1ステップで100個ずつ設定（高速化）
    int processed = 0;
    
    ui->listTreeWidget->setUpdatesEnabled(false); // 描画を一時停止
    
    QElapsedTimer iconSetTimer;
    iconSetTimer.start();
    
    while (processed < batchSize && m_iconSetProgress < m_appList.size() && 
           m_iconSetProgress < ui->listTreeWidget->topLevelItemCount()) {
        
        QElapsedTimer itemTimer;
        itemTimer.start();
        
        QTreeWidgetItem *item = ui->listTreeWidget->topLevelItem(m_iconSetProgress);
        if (item && m_iconSetProgress < m_appList.size()) {
            const AppInfo &app = m_appList[m_iconSetProgress];
            if (m_iconCache32px.contains(app.path)) {
                QElapsedTimer setIconTimer;
                setIconTimer.start();
                item->setIcon(0, m_iconCache32px[app.path]);
                
                // 重い処理をデバッグ
                if (setIconTimer.elapsed() > 10) {
                    qWarning() << "SLOW setIcon for:" << app.name 
                               << "took:" << setIconTimer.elapsed() << "ms";
                }
            }
        }
        
        if (itemTimer.elapsed() > 5) {
            qDebug() << "Item processing for index" << m_iconSetProgress 
                     << "took:" << itemTimer.elapsed() << "ms";
        }
        
        m_iconSetProgress++;
        processed++;
    }
    
    ui->listTreeWidget->setUpdatesEnabled(true); // 描画を再開
    
    // UIイベント処理の無効化（パフォーマンス問題の原因）
    // QElapsedTimer processTimer;
    // processTimer.start();
    // QApplication::processEvents(); // ← これが200msかかる原因！
    // qDebug() << "processEvents() took:" << processTimer.elapsed() << "ms";
    
    // プログレスバー更新を完全無効化（200ms問題の原因）
    // if (m_iconSetProgress % 50 == 0) {  // 50ステップごとにのみ更新
    //     QElapsedTimer progressTimer;
    //     progressTimer.start();
    //     m_progressBar->setValue(m_iconSetProgress);
    //     if (progressTimer.elapsed() > 5) {
    //         qDebug() << "Progress bar update took:" << progressTimer.elapsed() << "ms";
    //     }
    // }
    
    // 完了チェック
    if (m_iconSetProgress >= m_appList.size() || 
        m_iconSetProgress >= ui->listTreeWidget->topLevelItemCount()) {
        
        qDebug() << "=== ICON SETTING COMPLETED ===";
        
        QElapsedTimer finishTimer;
        finishTimer.start();
        
        m_iconTimer->stop();
        
        // プログレスバーを隠す
        m_loadingLabel->setVisible(false);
        m_progressBar->setVisible(false);
        
        qDebug() << "All" << m_iconSetProgress << "icons set from cache";
        qDebug() << "Finish processing took:" << finishTimer.elapsed() << "ms";
        
        QElapsedTimer asyncTimer;
        asyncTimer.start();
        // 以降はスクロール時のオンデマンドローディングを有効化
        startAsyncIconLoading(m_appList);
        qDebug() << "startAsyncIconLoading took:" << asyncTimer.elapsed() << "ms";
    }
    
    // qDebug() << "=== setIconsStep TOTAL took:" << stepTimer.elapsed() << "ms ==="; // ログ削減
}

// オンデマンドアイコンローディングの実装（キャッシュから取得）
void MainWindow::startAsyncIconLoading(const QList<AppInfo> &apps)
{
    QElapsedTimer asyncLoadTimer;
    asyncLoadTimer.start();
    
    qDebug() << "startAsyncIconLoading called with" << apps.size() << "apps";
    
    m_appList = apps;
    
    // loadVisibleIcons を無効化（スクロール問題対策）
    // QTimer::singleShot(100, this, &MainWindow::loadVisibleIcons);
    
    qDebug() << "startAsyncIconLoading setup took:" << asyncLoadTimer.elapsed() << "ms (loadVisibleIcons disabled)";
}

void MainWindow::onScrollValueChanged()
{
    // スクロール時のアイコンロードを一時的に無効化（パフォーマンステスト）
    // アイコンロードは統合タイマーで処理
    return; // 完全に無効化
}

void MainWindow::loadVisibleIcons()
{
    QElapsedTimer visibleTimer;
    visibleTimer.start();
    
    qDebug() << "=== loadVisibleIcons START ===";
    
    if (m_appList.isEmpty() || !ui->listTreeWidget) {
        qDebug() << "loadVisibleIcons: Empty app list or null widget";
        return;
    }
    
    QElapsedTimer itemAtTimer;
    itemAtTimer.start();
    
    // QTreeWidgetの可視アイテム範囲を直接取得
    QTreeWidgetItem *topItem = ui->listTreeWidget->itemAt(0, 0);
    QTreeWidgetItem *bottomItem = ui->listTreeWidget->itemAt(0, ui->listTreeWidget->height() - 1);
    
    qDebug() << "itemAt() calls took:" << itemAtTimer.elapsed() << "ms";
    
    if (!topItem) topItem = ui->listTreeWidget->topLevelItem(0);
    if (!bottomItem) bottomItem = ui->listTreeWidget->topLevelItem(ui->listTreeWidget->topLevelItemCount() - 1);
    
    if (!topItem || !bottomItem) {
        qDebug() << "loadVisibleIcons: No visible items found";
        return;
    }
    
    int firstVisible = ui->listTreeWidget->indexOfTopLevelItem(topItem);
    int lastVisible = ui->listTreeWidget->indexOfTopLevelItem(bottomItem);
    
    // 範囲を拡張（前後5個ずつ余裕をもって）
    firstVisible = qMax(0, firstVisible - 5);
    lastVisible = qMin(ui->listTreeWidget->topLevelItemCount() - 1, lastVisible + 5);
    
    qDebug() << "Visible range:" << firstVisible << "-" << lastVisible 
             << "of" << ui->listTreeWidget->topLevelItemCount() << "items";
    
    // 表示範囲のアイコンのみロード
    int processedCount = 0;
    const int maxProcessPerCall = 100; // 表示範囲は一度に全て処理
    
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    for (int i = firstVisible; i <= lastVisible && processedCount < maxProcessPerCall; ++i) {
        if (i >= 0 && i < ui->listTreeWidget->topLevelItemCount() && i < m_appList.size()) {
            QElapsedTimer itemTimer;
            itemTimer.start();
            
            QTreeWidgetItem *item = ui->listTreeWidget->topLevelItem(i);
            if (item) {
                loadIconForItem(item, m_appList[i]);
                processedCount++;
                
                if (itemTimer.elapsed() > 5) {
                    qDebug() << "loadIconForItem for index" << i << "took:" << itemTimer.elapsed() << "ms";
                }
            }
        }
    }
    
    qDebug() << "Icon loading loop took:" << loadTimer.elapsed() << "ms for" << processedCount << "items";
    qDebug() << "=== loadVisibleIcons TOTAL took:" << visibleTimer.elapsed() << "ms ===";
}

void MainWindow::loadIconForItem(QTreeWidgetItem *item, const AppInfo &app)
{
    if (!item) return;
    
    // 現在のアイコン状態をチェック
    QIcon currentIcon = item->icon(0);
    bool hasIcon = !currentIcon.isNull();
    
    // アイコンが未設定の場合のみ、キャッシュから直接取得
    if (!hasIcon) {
        // キャッシュから直接取得（ファイルI/O一切なし）
        if (m_iconCache32px.contains(app.path)) {
            QIcon cachedIcon = m_iconCache32px.value(app.path);
            item->setIcon(0, cachedIcon);
            // qDebug() << "Set cached icon for:" << app.name; // ログ無効化で高速化
        }
        // キャッシュにない場合は何もしない（ファイルアクセス禁止）
    }
}