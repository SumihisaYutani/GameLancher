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
#include <QTableView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_appManager(new AppManager(this))
    , m_appLauncher(new AppLauncher(this))
    , m_iconExtractor(new IconExtractor(this))
    , m_appListModel(new AppListModel(this))
    , m_isGridView(false)
    , m_selectedAppId("")
    , m_mainTimer(new QTimer(this))
    , m_resizeTimer(new QTimer(this))
    , m_progressBar(nullptr)
    , m_loadingLabel(nullptr)
    , m_loadTimer(new QTimer(this))
    , m_isLoading(false)
    , m_iconTimer(new QTimer(this))
    , m_iconCacheProgress(0)
{
    ui->setupUi(this);
    setupConnections();
    setupProgressBar();

    // モデルの設定
    m_appListModel->setIconCache(&m_iconCache32px);
    ui->listTableView->setModel(m_appListModel);
    
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
    
    // リストビューイベント（QTableView）
    connect(ui->listTableView, &QTableView::clicked, this, &MainWindow::onListItemClicked);
    connect(ui->listTableView, &QTableView::doubleClicked, this, &MainWindow::onListItemDoubleClicked);

    // QTableViewの設定
    ui->listTableView->setIconSize(QSize(32, 32));
    ui->listTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->listTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listTableView->setAlternatingRowColors(false);
    ui->listTableView->setShowGrid(false);

    // スクロールパフォーマンス最適化
    ui->listTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    ui->listTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listTableView->setAutoScroll(false);
    ui->listTableView->setWordWrap(false);

    // 行ヘッダー非表示、行高さ固定
    ui->listTableView->verticalHeader()->setVisible(false);
    ui->listTableView->verticalHeader()->setDefaultSectionSize(40);
    ui->listTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // 列ヘッダー設定
    QHeaderView *header = ui->listTableView->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Interactive);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->resizeSection(0, 200);
    
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
        // キャッシュ構築のみ実行（設定フェーズは不要）
        buildIconCacheStep();
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
    // グリッドビューは無効化 - リストビューに切り替え
    switchToListView();
}

void MainWindow::updateListView()
{
    if (m_isLoading) return;

    QElapsedTimer listTimer;
    listTimer.start();

    // フィルタリング
    QList<AppInfo> apps = m_appManager->getApps();
    if (!m_currentFilter.isEmpty()) {
        apps = m_appManager->searchApps(m_currentFilter);
    }

    // アプリ情報をキャッシュ
    m_appList = apps;

    qDebug() << "LIST VIEW: Processing" << apps.size() << "apps (model-based)";

    // モデルにデータを設定（一括更新）
    m_appListModel->setApps(apps);

    qDebug() << "LIST VIEW: Completed in" << listTimer.elapsed() << "ms for" << apps.size() << "apps";

    // フィルタリング時は既存キャッシュを使用、初期表示時のみキャッシュ構築
    if (m_currentFilter.isEmpty() && !apps.isEmpty()) {
        qDebug() << "Initial load - starting icon preload";
        preloadAllIconsAsync(apps);
    } else if (!m_currentFilter.isEmpty()) {
        // フィルタリング時はアイコン再描画を通知
        m_appListModel->notifyAllIconsUpdated();
    }
}

void MainWindow::clearListView()
{
    m_appListModel->clear();
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


// リストビューイベント
void MainWindow::onListItemClicked(const QModelIndex &index)
{
    if (index.isValid()) {
        m_selectedAppId = m_appListModel->getAppId(index.row());
        ui->removeAppButton->setEnabled(!m_selectedAppId.isEmpty());
    }
}

void MainWindow::onListItemDoubleClicked(const QModelIndex &index)
{
    if (index.isValid()) {
        QString appId = m_appListModel->getAppId(index.row());
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
    // モデルを通じて更新
    m_appListModel->updateApp(app);
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

        // モデルにアイコン更新を通知（表示をリフレッシュ）
        m_appListModel->notifyAllIconsUpdated();

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
                                .arg(recentApp->name, AppListModel::formatLastLaunch(recentApp->lastLaunch));
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
          AppListModel::formatLastLaunch(app->lastLaunch),
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




void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // グリッドビューの場合のみ、リサイズに応じてレイアウトを更新
    if (m_isGridView && m_resizeTimer) {
        m_resizeTimer->start();
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

    // アイコンキャッシュ構築を開始
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
    qDebug() << "=== onIconCacheCompleted ===";
    qDebug() << "Icon cache construction completed!" << m_iconCache32px.size() << "icons cached";

    // プログレスバーを隠す
    m_loadingLabel->setVisible(false);
    m_progressBar->setVisible(false);

    // 全アイコン更新を通知（モデルが自動的にキャッシュからアイコンを取得）
    m_appListModel->notifyAllIconsUpdated();

    qDebug() << "All icons ready from cache";
}


