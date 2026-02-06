#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QElapsedTimer>
#include <QFileIconProvider>
#include <QTableView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QListWidget>
#include <QDesktopServices>
#include <QDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>

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
    , m_currentPage(0)
    , m_itemsPerPage(50)
    , m_totalPages(0)
    , m_firstPageButton(nullptr)
    , m_prevPageButton(nullptr)
    , m_nextPageButton(nullptr)
    , m_lastPageButton(nullptr)
    , m_pageInfoLabel(nullptr)
{
    ui->setupUi(this);
    setupConnections();
    setupProgressBar();
    setupPagination();

    // モデルの設定
    m_appListModel->setIconCache(&m_iconCache32px);
    ui->listTableView->setModel(m_appListModel);

    // カスタムデリゲートでアイコンを直接描画（QIcon/QPixmapを経由しない）
    m_iconDelegate = new AppIconDelegate(this);
    ui->listTableView->setItemDelegate(m_iconDelegate);

    // 列ヘッダー設定（モデル設定後に行う必要あり）
    QHeaderView *header = ui->listTableView->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Interactive);  // アプリ名
    header->setSectionResizeMode(1, QHeaderView::Stretch);       // パス
    header->setSectionResizeMode(2, QHeaderView::Interactive);   // 最終起動
    header->setSectionResizeMode(3, QHeaderView::Interactive);   // 起動回数

    // デフォルトの列幅を設定
    header->resizeSection(0, 200);
    header->resizeSection(2, 100);
    header->resizeSection(3, 80);

    // 保存された列幅を復元
    restoreColumnWidths();

    // 列幅変更時に保存
    connect(header, &QHeaderView::sectionResized, this, &MainWindow::onColumnResized);

    // 選択状態の変更を監視（モデル設定後に接続）
    connect(ui->listTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &selected, const QItemSelection &deselected) {
        // 選択解除されたアプリをセットから削除
        for (const QModelIndex &index : deselected.indexes()) {
            if (index.column() == 0) {
                QString appId = m_appListModel->getAppId(index.row());
                m_selectedAppIds.remove(appId);
            }
        }
        // 新しく選択されたアプリをセットに追加
        for (const QModelIndex &index : selected.indexes()) {
            if (index.column() == 0) {
                QString appId = m_appListModel->getAppId(index.row());
                if (!appId.isEmpty()) {
                    m_selectedAppIds.insert(appId);
                }
            }
        }
        // 削除ボタンの有効/無効
        ui->removeAppButton->setEnabled(!m_selectedAppIds.isEmpty());
        // 単一選択の場合はm_selectedAppIdも更新
        if (m_selectedAppIds.size() == 1) {
            m_selectedAppId = *m_selectedAppIds.begin();
        } else {
            m_selectedAppId.clear();
        }
    });

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
    // 列幅を保存
    saveColumnWidths();

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
    
    // 検索機能の接続
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(ui->filterButton, &QPushButton::clicked, this, &MainWindow::onFilterButtonClicked);
    
    // リストビューイベント（QTableView）
    connect(ui->listTableView, &QTableView::doubleClicked, this, &MainWindow::onListItemDoubleClicked);

    // QTableViewの設定（パフォーマンス最適化）
    ui->listTableView->setIconSize(QSize(48, 48));
    ui->listTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->listTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);  // 複数選択可能
    ui->listTableView->setAlternatingRowColors(false);
    ui->listTableView->setShowGrid(false);
    ui->listTableView->setSortingEnabled(false);  // ソート無効でパフォーマンス改善
    ui->listTableView->setUpdatesEnabled(true);
    
    // コンテキストメニューを有効化
    ui->listTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listTableView, &QTableView::customContextMenuRequested,
            this, &MainWindow::onTableViewContextMenuRequested);

    // ページネーション用：スクロールバーを非表示
    ui->listTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->listTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    ui->listTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listTableView->setAutoScroll(false);
    ui->listTableView->setWordWrap(false);

    // 行ヘッダー非表示、行高さ固定（パフォーマンス重要）
    ui->listTableView->verticalHeader()->setVisible(false);
    ui->listTableView->verticalHeader()->setDefaultSectionSize(56);
    ui->listTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->listTableView->verticalHeader()->setMinimumSectionSize(56);
    ui->listTableView->verticalHeader()->setMaximumSectionSize(56);

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

    // シンプルにアプリ一覧を取得して表示
    QList<AppInfo> apps = m_appManager->getApps();
    m_appList = apps;
    m_appListModel->setApps(apps);
    updatePageControls();
}

void MainWindow::clearListView()
{
    m_appListModel->clear();
}

void MainWindow::updateAppCount()
{
    int displayedCount = m_appList.size();
    int totalCount = m_appManager->getAppCount();
    
    if (m_currentFilter.isEmpty()) {
        ui->appCountLabel->setText(QString("登録アプリ: %1個").arg(totalCount));
    } else {
        ui->appCountLabel->setText(QString("検索結果: %1個 / 全体: %2個").arg(displayedCount).arg(totalCount));
    }
}

void MainWindow::filterApplications()
{
    if (m_currentFilter.isEmpty()) {
        // フィルターが空の場合は全てのアプリを表示
        QList<AppInfo> apps = m_appManager->getApps();
        m_appList = apps;
        m_appListModel->setApps(apps);
    } else {
        // 検索キーワードでフィルタリング
        QList<AppInfo> filteredApps = m_appManager->searchApps(m_currentFilter);
        m_appList = filteredApps;
        m_appListModel->setApps(filteredApps);
    }
    updatePageControls();
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
    // m_selectedAppIdsから全ての選択されたアプリを取得（ページをまたいだ選択に対応）
    if (m_selectedAppIds.isEmpty()) {
        QMessageBox::information(this, "情報", "削除するアプリケーションを選択してください。");
        return;
    }

    // 選択されたアプリの情報を収集
    QStringList appIds;
    QStringList appPaths;
    QStringList appNames;

    for (const QString &appId : m_selectedAppIds) {
        AppInfo *app = m_appManager->findApp(appId);
        if (app) {
            appIds.append(appId);
            appPaths.append(app->path);
            appNames.append(app->name);
        }
    }

    if (appIds.isEmpty()) {
        return;
    }

    // カスタムダイアログを作成
    QDialog dialog(this);
    dialog.setWindowTitle("削除確認");
    dialog.setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // ヘッダーラベル
    QLabel *headerLabel = new QLabel(QString("%1個のアプリケーションを削除しますか？").arg(appIds.size()));
    headerLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    layout->addWidget(headerLabel);

    // アプリ一覧リスト
    QListWidget *listWidget = new QListWidget();
    listWidget->setMaximumHeight(200);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int i = 0; i < appNames.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(appNames[i]);
        item->setData(Qt::UserRole, appPaths[i]); // パスを保存
        listWidget->addItem(item);
    }
    
    // コンテキストメニューの接続
    connect(listWidget, &QListWidget::customContextMenuRequested, [&](const QPoint &pos) {
        QListWidgetItem *item = listWidget->itemAt(pos);
        if (!item) return;
        
        QString appPath = item->data(Qt::UserRole).toString();
        QFileInfo fileInfo(appPath);
        QString folderPath = fileInfo.dir().absolutePath();
        
        QMenu contextMenu;
        QAction *openFolderAction = contextMenu.addAction("フォルダを開く");
        connect(openFolderAction, &QAction::triggered, [folderPath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
        });
        
        contextMenu.exec(listWidget->mapToGlobal(pos));
    });
    
    layout->addWidget(listWidget);

    // ボタン
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *deleteBtn = new QPushButton("削除");
    QPushButton *excludeAndDeleteBtn = new QPushButton("除外リストに追加して削除");
    QPushButton *excludeParentAndDeleteBtn = new QPushButton("上位フォルダも除外して削除");
    QPushButton *cancelBtn = new QPushButton("キャンセル");

    buttonLayout->addStretch();
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addWidget(excludeAndDeleteBtn);
    buttonLayout->addWidget(excludeParentAndDeleteBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);

    // ボタンの接続
    int result = 0; // 0=キャンセル, 1=削除, 2=除外リストに追加して削除, 3=上位フォルダも除外して削除
    connect(deleteBtn, &QPushButton::clicked, [&]() { result = 1; dialog.accept(); });
    connect(excludeAndDeleteBtn, &QPushButton::clicked, [&]() { result = 2; dialog.accept(); });
    connect(excludeParentAndDeleteBtn, &QPushButton::clicked, [&]() { result = 3; dialog.accept(); });
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();

    if (result == 0) {
        return;
    }

    bool addToExcludeList = (result == 2);
    bool addParentToExcludeList = (result == 3);

    // 除外リストに追加
    if (addToExcludeList) {
        addPathsToExcludeList(appPaths);
    } else if (addParentToExcludeList) {
        // 一つ上のフォルダを除外リストに追加
        QStringList parentPaths = getParentDirectories(appPaths);
        addPathsToExcludeList(parentPaths);
        
        // 同じ親フォルダにある他のアプリも削除対象に追加
        QStringList additionalAppIds = findAppsInDirectories(parentPaths);
        for (const QString &additionalId : additionalAppIds) {
            if (!appIds.contains(additionalId)) {
                appIds.append(additionalId);
                AppInfo *additionalApp = m_appManager->findApp(additionalId);
                if (additionalApp) {
                    appNames.append(additionalApp->name);
                }
            }
        }
    }

    // アプリを削除
    int removedCount = 0;
    for (const QString &appId : appIds) {
        if (m_appManager->removeApp(appId)) {
            m_selectedAppIds.remove(appId);  // 選択リストからも削除
            removedCount++;
        }
    }

    if (removedCount > 0) {
        QString statusMsg = QString("%1個のアプリケーションを削除しました").arg(removedCount);
        if (addToExcludeList) {
            statusMsg += "（除外リストに追加済み）";
        } else if (addParentToExcludeList) {
            statusMsg += "（上位フォルダを除外リストに追加済み）";
        }
        statusBar()->showMessage(statusMsg, 3000);
        ui->removeAppButton->setEnabled(!m_selectedAppIds.isEmpty());
    }
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
    QString searchText = ui->searchLineEdit->text().trimmed();
    m_currentFilter = searchText;
    filterApplications();
}

void MainWindow::onFilterButtonClicked()
{
    QString searchText = ui->searchLineEdit->text().trimmed();
    m_currentFilter = searchText;
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
    // 新しいアイコンのキャッシュをクリア
    if (!app.iconPath.isEmpty()) {
        m_iconDelegate->clearCacheFor(app.iconPath);
    }
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
    // 更新されたアイコンのキャッシュをクリア
    if (!app.iconPath.isEmpty()) {
        m_iconDelegate->clearCacheFor(app.iconPath);
    }
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

void MainWindow::onTableViewContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->listTableView->indexAt(pos);
    if (!index.isValid()) return;
    
    QString appId = m_appListModel->getAppId(index.row());
    if (appId.isEmpty()) return;
    
    showAppContextMenu(appId, ui->listTableView->mapToGlobal(pos));
}

// ヘルパー関数
void MainWindow::showAppContextMenu(const QString &appId, const QPoint &globalPos)
{
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) return;
    
    QMenu contextMenu;
    
    // フォルダを開く
    QAction *openFolderAction = contextMenu.addAction("フォルダを開く");
    QFileInfo fileInfo(app->path);
    QString folderPath = fileInfo.dir().absolutePath();
    connect(openFolderAction, &QAction::triggered, [folderPath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
    });
    
    contextMenu.addSeparator();
    
    // 編集
    QAction *editAction = contextMenu.addAction("編集");
    connect(editAction, &QAction::triggered, [this, appId]() {
        editApplication(appId);
    });
    
    // 削除
    QAction *removeAction = contextMenu.addAction("削除");
    connect(removeAction, &QAction::triggered, [this, appId]() {
        removeApplication(appId);
    });
    
    contextMenu.addSeparator();
    
    // プロパティ
    QAction *propertiesAction = contextMenu.addAction("プロパティ");
    connect(propertiesAction, &QAction::triggered, [this, appId]() {
        showAppProperties(appId);
    });
    
    contextMenu.exec(globalPos);
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

void MainWindow::addPathsToExcludeList(const QStringList &paths)
{
    QString appDir = QApplication::applicationDirPath();
    QString excludeFilePath = QDir(appDir).filePath("exclude_list.txt");

    // 既存の除外リストを読み込み
    QStringList excludeList;
    QFile readFile(excludeFilePath);
    if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&readFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                excludeList.append(line);
            }
        }
        readFile.close();
    }

    // 新しいパスを追加
    int addedCount = 0;
    for (const QString &path : paths) {
        QString normalizedPath = QDir::fromNativeSeparators(path.toLower());
        if (!excludeList.contains(normalizedPath)) {
            excludeList.append(normalizedPath);
            addedCount++;
        }
    }

    // 除外リストを保存
    if (addedCount > 0) {
        QFile writeFile(excludeFilePath);
        if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&writeFile);
            for (const QString &excludePath : excludeList) {
                out << excludePath << "\n";
            }
            writeFile.close();
            qDebug() << "Added" << addedCount << "paths to exclude list";
        }
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


// 32pxアイコンキャッシュシステムの実装（QPixmap版で軽量化）
QPixmap MainWindow::getOrCreateIcon32px(const QString &filePath)
{
    // キャッシュにあるかチェック（constFindで1回の検索）
    auto it = m_iconCache32px.constFind(filePath);
    if (it != m_iconCache32px.constEnd()) {
        return *it;
    }

    QPixmap resultPixmap;

    // 1. 保存済みアイコンファイルを最優先で使用（登録時に生成済み）
    QString iconPath = m_iconExtractor->generateIconPath(filePath);
    if (QFileInfo::exists(iconPath)) {
        QPixmap pixmap(iconPath);
        if (!pixmap.isNull()) {
            resultPixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation);
            m_iconCache32px.insert(filePath, resultPixmap);
            return resultPixmap;
        }
    }

    // 2. 保存済みアイコンがない場合、ファイルアイコンを取得
    if (QFileInfo::exists(filePath)) {
        static QFileIconProvider s_iconProvider;
        QFileInfo fileInfo(filePath);
        QIcon fileIcon = s_iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            resultPixmap = fileIcon.pixmap(32, 32);
        }
    }

    // 3. それでもない場合はデフォルトアイコン
    if (resultPixmap.isNull()) {
        resultPixmap = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(32, 32);
    }

    // キャッシュに保存
    m_iconCache32px.insert(filePath, resultPixmap);

    return resultPixmap;
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

    // リストビューの表示行数を更新
    if (!m_isGridView) {
        updateVisibleRowCount();
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // 初回表示時に表示行数を設定
    static bool firstShow = true;
    if (firstShow) {
        firstShow = false;
        // 少し遅延させてレイアウトが確定してから計算
        QTimer::singleShot(100, this, &MainWindow::updateVisibleRowCount);
    }
}

void MainWindow::updateVisibleRowCount()
{
    // 親ウィジェット（リストビューを含むコンテナ）の高さを取得
    QWidget *container = ui->listTableView->parentWidget();
    if (!container) return;

    int containerHeight = container->height();
    int headerHeight = ui->listTableView->horizontalHeader()->height();
    int rowHeight = 56;  // 固定行高さ（48pxアイコン + 8pxパディング）

    // ページネーションコントロールの高さを考慮（約40px）
    int paginationHeight = 50;

    // 利用可能な高さからヘッダーとページネーションを引いて行数を計算
    int availableHeight = containerHeight - headerHeight - paginationHeight;
    int visibleRows = availableHeight / rowHeight;

    // 最低1行、最大100行
    visibleRows = qBound(1, visibleRows, 100);

    // テーブルビューの高さをピッタリに設定
    int tableHeight = headerHeight + (visibleRows * rowHeight);
    ui->listTableView->setFixedHeight(tableHeight);

    // 現在の設定と異なる場合のみ更新
    if (m_appListModel->itemsPerPage() != visibleRows) {
        qDebug() << "Updating visible rows:" << visibleRows << "(table height:" << tableHeight << "px)";
        m_appListModel->setItemsPerPage(visibleRows);
        updatePageControls();
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

    // 現在のページを再表示（アイコン更新のため）
    displayCurrentPage();

    qDebug() << "All icons ready from cache";
}

// ページネーション関連
void MainWindow::setupPagination()
{
    // ページナビゲーション用のウィジェットを作成
    QWidget *paginationWidget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(paginationWidget);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(5);

    // 最初へボタン
    m_firstPageButton = new QPushButton("<<", paginationWidget);
    m_firstPageButton->setFixedWidth(40);
    m_firstPageButton->setToolTip("最初のページ");
    connect(m_firstPageButton, &QPushButton::clicked, this, &MainWindow::onFirstPageClicked);

    // 前へボタン
    m_prevPageButton = new QPushButton("<", paginationWidget);
    m_prevPageButton->setFixedWidth(40);
    m_prevPageButton->setToolTip("前のページ");
    connect(m_prevPageButton, &QPushButton::clicked, this, &MainWindow::onPrevPageClicked);

    // ページ情報ラベル
    m_pageInfoLabel = new QLabel("0 / 0 ページ", paginationWidget);
    m_pageInfoLabel->setAlignment(Qt::AlignCenter);
    m_pageInfoLabel->setMinimumWidth(120);

    // 次へボタン
    m_nextPageButton = new QPushButton(">", paginationWidget);
    m_nextPageButton->setFixedWidth(40);
    m_nextPageButton->setToolTip("次のページ");
    connect(m_nextPageButton, &QPushButton::clicked, this, &MainWindow::onNextPageClicked);

    // 最後へボタン
    m_lastPageButton = new QPushButton(">>", paginationWidget);
    m_lastPageButton->setFixedWidth(40);
    m_lastPageButton->setToolTip("最後のページ");
    connect(m_lastPageButton, &QPushButton::clicked, this, &MainWindow::onLastPageClicked);

    // レイアウトに追加
    layout->addStretch();
    layout->addWidget(m_firstPageButton);
    layout->addWidget(m_prevPageButton);
    layout->addWidget(m_pageInfoLabel);
    layout->addWidget(m_nextPageButton);
    layout->addWidget(m_lastPageButton);
    layout->addStretch();

    // リストビューページのレイアウトに追加
    QVBoxLayout *listViewLayout = qobject_cast<QVBoxLayout*>(ui->listViewPage->layout());
    if (listViewLayout) {
        listViewLayout->addWidget(paginationWidget);
    }

    // スタイル設定
    QString buttonStyle = R"(
        QPushButton {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                       stop: 0 #ffffff, stop: 1 #f8fbff);
            border: 1px solid #b3d9ff;
            border-radius: 4px;
            padding: 5px;
            font-weight: bold;
            color: #1565c0;
        }
        QPushButton:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                       stop: 0 #e3f2fd, stop: 1 #bbdefb);
            border-color: #2196f3;
        }
        QPushButton:pressed {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                       stop: 0 #bbdefb, stop: 1 #90caf9);
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999999;
            border-color: #d5d5d5;
        }
    )";

    m_firstPageButton->setStyleSheet(buttonStyle);
    m_prevPageButton->setStyleSheet(buttonStyle);
    m_nextPageButton->setStyleSheet(buttonStyle);
    m_lastPageButton->setStyleSheet(buttonStyle);

    updatePageControls();
}

void MainWindow::updatePageControls()
{
    int currentPage = m_appListModel->currentPage();
    int totalPages = m_appListModel->totalPages();

    bool hasPrev = currentPage > 0;
    bool hasNext = currentPage < totalPages - 1;

    m_firstPageButton->setEnabled(hasPrev);
    m_prevPageButton->setEnabled(hasPrev);
    m_nextPageButton->setEnabled(hasNext);
    m_lastPageButton->setEnabled(hasNext);

    if (totalPages > 0) {
        m_pageInfoLabel->setText(QString("%1 / %2 ページ").arg(currentPage + 1).arg(totalPages));
    } else {
        m_pageInfoLabel->setText("0 / 0 ページ");
    }
}

void MainWindow::displayCurrentPage()
{
    // モデル内蔵のページング使用 - ページコントロールの更新のみ
    updatePageControls();
}

void MainWindow::onFirstPageClicked()
{
    // ページ切り替え前にシグナルをブロック（選択状態の誤削除を防ぐ）
    ui->listTableView->selectionModel()->blockSignals(true);
    m_appListModel->setPage(0);
    ui->listTableView->selectionModel()->blockSignals(false);
    updatePageControls();
    restoreSelectionOnPage();
}

void MainWindow::onPrevPageClicked()
{
    int currentPage = m_appListModel->currentPage();
    if (currentPage > 0) {
        // ページ切り替え前にシグナルをブロック（選択状態の誤削除を防ぐ）
        ui->listTableView->selectionModel()->blockSignals(true);
        m_appListModel->setPage(currentPage - 1);
        ui->listTableView->selectionModel()->blockSignals(false);
        updatePageControls();
        restoreSelectionOnPage();
    }
}

void MainWindow::onNextPageClicked()
{
    int currentPage = m_appListModel->currentPage();
    int totalPages = m_appListModel->totalPages();
    if (currentPage < totalPages - 1) {
        // ページ切り替え前にシグナルをブロック（選択状態の誤削除を防ぐ）
        ui->listTableView->selectionModel()->blockSignals(true);
        m_appListModel->setPage(currentPage + 1);
        ui->listTableView->selectionModel()->blockSignals(false);
        updatePageControls();
        restoreSelectionOnPage();
    }
}

void MainWindow::onLastPageClicked()
{
    int totalPages = m_appListModel->totalPages();
    if (totalPages > 0) {
        // ページ切り替え前にシグナルをブロック（選択状態の誤削除を防ぐ）
        ui->listTableView->selectionModel()->blockSignals(true);
        m_appListModel->setPage(totalPages - 1);
        ui->listTableView->selectionModel()->blockSignals(false);
        updatePageControls();
        restoreSelectionOnPage();
    }
}

void MainWindow::restoreSelectionOnPage()
{
    // 選択変更シグナルを一時的にブロック
    ui->listTableView->selectionModel()->blockSignals(true);

    // 現在の選択をクリア
    ui->listTableView->clearSelection();

    // 現在のページに表示されているアプリの中で、選択リストに含まれているものを選択
    int rowCount = m_appListModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QString appId = m_appListModel->getAppId(row);
        if (m_selectedAppIds.contains(appId)) {
            QModelIndex index = m_appListModel->index(row, 0);
            ui->listTableView->selectionModel()->select(index,
                QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    // シグナルのブロックを解除
    ui->listTableView->selectionModel()->blockSignals(false);

    // 削除ボタンの状態を更新
    ui->removeAppButton->setEnabled(!m_selectedAppIds.isEmpty());
}

// 列幅保存・復元
void MainWindow::onColumnResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    Q_UNUSED(logicalIndex)

    // 列幅変更時に保存（遅延保存で頻繁な書き込みを防ぐ）
    static QTimer *saveTimer = nullptr;
    if (!saveTimer) {
        saveTimer = new QTimer(this);
        saveTimer->setSingleShot(true);
        saveTimer->setInterval(500);  // 500ms後に保存
        connect(saveTimer, &QTimer::timeout, this, &MainWindow::saveColumnWidths);
    }
    saveTimer->start();
}

void MainWindow::saveColumnWidths()
{
    QHeaderView *header = ui->listTableView->horizontalHeader();
    if (!header || header->count() == 0) {
        qDebug() << "saveColumnWidths: No header or no columns";
        return;
    }

    QSettings settings("GameLauncher", "GameLauncher");
    settings.beginGroup("ColumnWidths");
    for (int i = 0; i < header->count(); ++i) {
        int width = header->sectionSize(i);
        settings.setValue(QString("column_%1").arg(i), width);
        qDebug() << "Saved column" << i << "width:" << width;
    }
    settings.endGroup();
    settings.sync();  // 即座に書き込み

    qDebug() << "Column widths saved to:" << settings.fileName();
}

void MainWindow::restoreColumnWidths()
{
    QHeaderView *header = ui->listTableView->horizontalHeader();
    if (!header || header->count() == 0) {
        qDebug() << "restoreColumnWidths: No header or no columns";
        return;
    }

    QSettings settings("GameLauncher", "GameLauncher");
    qDebug() << "Restoring column widths from:" << settings.fileName();

    settings.beginGroup("ColumnWidths");
    QStringList keys = settings.childKeys();
    qDebug() << "Found keys:" << keys;

    for (int i = 0; i < header->count(); ++i) {
        QString key = QString("column_%1").arg(i);
        if (settings.contains(key)) {
            int width = settings.value(key).toInt();
            if (width > 20) {  // 最小幅チェック
                header->resizeSection(i, width);
                qDebug() << "Restored column" << i << "width:" << width;
            }
        }
    }
    settings.endGroup();
}

QStringList MainWindow::getParentDirectories(const QStringList &paths)
{
    QStringList parentPaths;
    QSet<QString> uniqueParents;
    
    for (const QString &path : paths) {
        QFileInfo fileInfo(path);
        QString parentPath = fileInfo.dir().absolutePath();
        QString normalizedParentPath = QDir::fromNativeSeparators(parentPath.toLower());
        
        if (!uniqueParents.contains(normalizedParentPath)) {
            uniqueParents.insert(normalizedParentPath);
            parentPaths.append(normalizedParentPath);
        }
    }
    
    return parentPaths;
}

QStringList MainWindow::findAppsInDirectories(const QStringList &directories)
{
    QStringList appIds;
    QList<AppInfo> allApps = m_appManager->getApps();
    
    for (const AppInfo &app : allApps) {
        QFileInfo appFileInfo(app.path);
        QString appDirPath = QDir::fromNativeSeparators(appFileInfo.dir().absolutePath().toLower());
        
        for (const QString &directory : directories) {
            if (appDirPath == directory) {
                appIds.append(app.id);
                break;
            }
        }
    }
    
    return appIds;
}
