#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QScrollArea>
#include <QApplication>
#include <QResizeEvent>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QElapsedTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_appManager(new AppManager(this))
    , m_appLauncher(new AppLauncher(this))
    , m_iconExtractor(new IconExtractor(this))
    , m_isGridView(false)
    , m_selectedAppId("")
    , m_gridLayout(nullptr)
    , m_statusTimer(new QTimer(this))
    , m_resizeTimer(new QTimer(this))
    , m_progressBar(nullptr)
    , m_loadingLabel(nullptr)
    , m_loadTimer(new QTimer(this))
    , m_uiUpdateTimer(new QTimer(this))
    , m_isLoading(false)
{
    ui->setupUi(this);
    setupConnections();
    setupProgressBar();
    
    // 初期状態をリストビューに変更（軽量表示のため）
    m_isGridView = false;
    ui->viewStackedWidget->setCurrentIndex(1);
    ui->actionGridView->setChecked(false);
    ui->actionListView->setChecked(true);
    
    // アプリケーションロードと初回描画（非同期で実行）
    loadApplicationsAsync();
    updateStatusBar();
    
    // ステータスバータイマーの設定
    m_statusTimer->setInterval(1000); // 1秒間隔
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start();
    
    // リサイズタイマーの設定
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(200); // 200ms後に実行
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() {
        if (m_isGridView) {
            updateGridView();
        }
    });
}

MainWindow::~MainWindow()
{
    // タイマーを停止・削除
    if (m_statusTimer) {
        m_statusTimer->stop();
        delete m_statusTimer;
        m_statusTimer = nullptr;
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
    
    if (m_uiUpdateTimer) {
        m_uiUpdateTimer->stop();
        delete m_uiUpdateTimer;
        m_uiUpdateTimer = nullptr;
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
    
    // ロード関連の接続
    connect(m_loadTimer, &QTimer::timeout, this, &MainWindow::onLoadingFinished);
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &MainWindow::onLoadingProgress);
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
    
    qDebug() << "LIST VIEW: Processing" << apps.size() << "apps (lightweight mode)";
    
    // リストビューで大量データを効率的に処理
    ui->listTreeWidget->setUpdatesEnabled(false); // 描画を一時停止
    
    // 一括でアイテムを作成（アイコンロードを省略して高速化）
    QList<QTreeWidgetItem*> items;
    items.reserve(apps.size());
    
    QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    
    for (const AppInfo &app : apps) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, app.id);
        item->setText(0, app.name);
        item->setText(1, app.path);
        item->setText(2, formatLastLaunch(app.lastLaunch));
        item->setText(3, formatLaunchCount(app.launchCount));
        
        // アイコンを軽量化: デフォルトアイコンのみ使用（高速化）
        item->setIcon(0, defaultIcon);
        
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
    
    qDebug() << "LIST VIEW: Completed in" << listTimer.elapsed() << "ms for" << apps.size() << "apps";
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
    if (m_isGridView) {
        switchToListView();
    } else {
        switchToGridView();
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
    switchToGridView();
}

void MainWindow::onActionListView()
{
    switchToListView();
}

void MainWindow::onActionRefresh()
{
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
    
    m_uiUpdateTimer->stop();
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
    
    // ウィンドウ幅に基づいて動的に列数を計算
    int availableWidth = gridWidget->width();
    if (availableWidth <= 0) {
        availableWidth = ui->gridScrollArea->width() - 20;
    }
    
    const int appWidgetWidth = 110;
    const int gridSpacing = 8;
    const int gridMargin = 10;
    
    int maxCols = qMax(1, (availableWidth - gridMargin * 2 + gridSpacing) / (appWidgetWidth + gridSpacing));
    maxCols = qMin(maxCols, 15);
    
    qDebug() << "Grid layout - Available width:" << availableWidth << "Calculated columns:" << maxCols;
    qDebug() << "Creating" << apps.size() << "app widgets for batch display";
    
    // シンプルな一括読み込み（安全のため段階的読み込みを無効化）
    QElapsedTimer widgetCreationTimer;
    widgetCreationTimer.start();
    
    QList<AppWidget*> newWidgets;
    newWidgets.reserve(apps.size()); // 全アプリ数に合わせて予約
    
    // 制限解除: 全てのアプリを表示
    int maxApps = apps.size(); // 制限なし
    qDebug() << "FULL DISPLAY: Showing all" << maxApps << "apps";
    
    for (int i = 0; i < maxApps; ++i) {
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
    
    qDebug() << "Widget creation took:" << widgetCreationTimer.elapsed() << "ms for" << newWidgets.size() << "widgets";
    
    // シンプルな同期レイアウト追加（安定化のため）
    QElapsedTimer layoutTimer;
    layoutTimer.start();
    
    // 全ウィジェットを一括でレイアウトに追加
    for (int i = 0; i < newWidgets.size(); ++i) {
        int row = i / maxCols;
        int col = i % maxCols;
        
        AppWidget *appWidget = newWidgets[i];
        m_gridLayout->addWidget(appWidget, row, col);
        m_appWidgets.append(appWidget);
    }
    
    qDebug() << "Layout addition took:" << layoutTimer.elapsed() << "ms";
    
    // レイアウトの調整
    if (!newWidgets.isEmpty()) {
        int lastRow = (newWidgets.size() - 1) / maxCols;
        int lastCol = (newWidgets.size() - 1) % maxCols;
        
        for (int i = lastCol + 1; i < maxCols; ++i) {
            m_gridLayout->setColumnStretch(i, 1);
        }
        m_gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), lastRow + 1, 0);
    }
    
    // 最終的なレイアウト更新
    gridWidget->updateGeometry();
    gridWidget->update();
    
    qDebug() << "updateGridViewAsync TOTAL took:" << asyncTimer.elapsed() << "ms for" << newWidgets.size() << "widgets";
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
    QMainWindow::resizeEvent(event);
    
    // グリッドビューの場合のみ、リサイズに応じてレイアウトを更新
    if (m_isGridView && m_resizeTimer) {
        m_resizeTimer->start(); // タイマーをリスタート
    }
}