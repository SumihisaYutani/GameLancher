#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QScrollArea>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_appManager(new AppManager(this))
    , m_appLauncher(new AppLauncher(this))
    , m_iconExtractor(new IconExtractor(this))
    , m_isGridView(true)
    , m_selectedAppId("")
    , m_gridLayout(nullptr)
    , m_statusTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupConnections();
    loadApplications();
    
    // 初期状態の設定
    switchToGridView();
    updateStatusBar();
    
    // ステータスバータイマーの設定
    m_statusTimer->setInterval(1000); // 1秒間隔
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // ツールバーボタン
    connect(ui->addAppButton, &QPushButton::clicked, this, &MainWindow::onAddAppButtonClicked);
    connect(ui->removeAppButton, &QPushButton::clicked, this, &MainWindow::onRemoveAppButtonClicked);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
    connect(ui->viewModeButton, &QToolButton::clicked, this, &MainWindow::onViewModeButtonClicked);
    
    // 検索
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    
    // リストビューイベント
    connect(ui->listTreeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onListItemClicked);
    connect(ui->listTreeWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onListItemDoubleClicked);
    
    // アプリケーション管理イベント
    connect(m_appManager, &AppManager::appAdded, this, &MainWindow::onAppAdded);
    connect(m_appManager, &AppManager::appRemoved, this, &MainWindow::onAppRemoved);
    connect(m_appManager, &AppManager::appUpdated, this, &MainWindow::onAppUpdated);
    
    // アプリケーション起動イベント
    connect(m_appLauncher, &AppLauncher::launched, this, &MainWindow::onAppLaunched);
    connect(m_appLauncher, &AppLauncher::finished, this, &MainWindow::onAppLaunchFinished);
    connect(m_appLauncher, &AppLauncher::errorOccurred, this, &MainWindow::onAppLaunchError);
    
    // メニューアクション
    connect(ui->actionAddApp, &QAction::triggered, this, &MainWindow::onActionAddApp);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit);
    connect(ui->actionGridView, &QAction::triggered, this, &MainWindow::onActionGridView);
    connect(ui->actionListView, &QAction::triggered, this, &MainWindow::onActionListView);
    connect(ui->actionRefresh, &QAction::triggered, this, &MainWindow::onActionRefresh);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout);
}

void MainWindow::loadApplications()
{
    m_appManager->loadApps();
    refreshViews();
    updateAppCount();
}

void MainWindow::refreshViews()
{
    if (m_isGridView) {
        updateGridView();
    } else {
        updateListView();
    }
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
    clearGridView();
    
    // グリッドレイアウトの準備
    QWidget *gridWidget = ui->gridScrollAreaWidgetContents;
    if (!m_gridLayout) {
        m_gridLayout = new QGridLayout(gridWidget);
        m_gridLayout->setSpacing(15);
        m_gridLayout->setContentsMargins(15, 15, 15, 15);
    }
    
    // フィルタリング
    QList<AppInfo> apps = m_appManager->getApps();
    if (!m_currentFilter.isEmpty()) {
        apps = m_appManager->searchApps(m_currentFilter);
    }
    
    // アプリウィジェットの作成と配置
    int row = 0;
    int col = 0;
    const int maxCols = 5; // 1行あたりの最大列数
    
    for (const AppInfo &app : std::as_const(apps)) {
        AppWidget *appWidget = new AppWidget(app, gridWidget);
        
        // シグナル接続
        connect(appWidget, &AppWidget::clicked, this, &MainWindow::onAppWidgetClicked);
        connect(appWidget, &AppWidget::doubleClicked, this, &MainWindow::onAppWidgetDoubleClicked);
        connect(appWidget, &AppWidget::rightClicked, this, &MainWindow::onAppWidgetRightClicked);
        connect(appWidget, &AppWidget::editRequested, this, &MainWindow::onAppEditRequested);
        connect(appWidget, &AppWidget::removeRequested, this, &MainWindow::onAppRemoveRequested);
        connect(appWidget, &AppWidget::propertiesRequested, this, &MainWindow::onAppPropertiesRequested);
        
        m_gridLayout->addWidget(appWidget, row, col);
        m_appWidgets.append(appWidget);
        
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    
    // レイアウトの調整
    for (int i = col; i < maxCols; ++i) {
        m_gridLayout->setColumnStretch(i, 1);
    }
    m_gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding), row + 1, 0);
}

void MainWindow::updateListView()
{
    clearListView();
    
    // フィルタリング
    QList<AppInfo> apps = m_appManager->getApps();
    if (!m_currentFilter.isEmpty()) {
        apps = m_appManager->searchApps(m_currentFilter);
    }
    
    // リストアイテムの作成
    for (const AppInfo &app : std::as_const(apps)) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->listTreeWidget);
        item->setData(0, Qt::UserRole, app.id);
        item->setText(0, app.name);
        item->setText(1, app.path);
        item->setText(2, formatLastLaunch(app.lastLaunch));
        item->setText(3, formatLaunchCount(app.launchCount));
        
        // アイコンの設定
        if (!app.iconPath.isEmpty() && QFileInfo::exists(app.iconPath)) {
            item->setIcon(0, QIcon(app.iconPath));
        } else {
            item->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
        }
    }
    
    // カラムサイズの調整
    ui->listTreeWidget->header()->resizeSection(0, 200);
    ui->listTreeWidget->header()->resizeSection(1, 300);
    ui->listTreeWidget->header()->resizeSection(2, 150);
    ui->listTreeWidget->header()->resizeSection(3, 100);
}

void MainWindow::clearGridView()
{
    // 既存のウィジェットを削除
    for (AppWidget *widget : std::as_const(m_appWidgets)) {
        delete widget;
    }
    m_appWidgets.clear();
    
    // レイアウトをクリア
    if (m_gridLayout) {
        QLayoutItem *item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            delete item;
        }
    }
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
        // 起動情報を更新
        m_appManager->updateApp(appId, *app);
        
        // UI更新
        refreshViews();
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
    m_currentFilter = ui->searchLineEdit->text().trimmed();
    filterApplications();
}

// アプリウィジェットイベント
void MainWindow::onAppWidgetClicked(const QString &appId)
{
    m_selectedAppId = appId;
    ui->removeAppButton->setEnabled(!appId.isEmpty());
    
    // 他のウィジェットの選択を解除
    for (AppWidget *widget : std::as_const(m_appWidgets)) {
        widget->setSelected(widget->getAppInfo().id == appId);
    }
}

void MainWindow::onAppWidgetDoubleClicked(const QString &appId)
{
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

void MainWindow::onAppRemoved(const QString &appId)
{
    Q_UNUSED(appId)
    refreshViews();
    updateAppCount();
    if (m_selectedAppId == appId) {
        m_selectedAppId.clear();
        ui->removeAppButton->setEnabled(false);
    }
}

void MainWindow::onAppUpdated(const AppInfo &app)
{
    Q_UNUSED(app)
    refreshViews();
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
    AppInfo *app = m_appManager->findApp(appId);
    if (!app) {
        QMessageBox::warning(this, "エラー", "アプリケーションが見つかりません。");
        return;
    }
    
    int ret = QMessageBox::question(this, "確認",
                                   QString("'%1' を削除しますか？").arg(app->name),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        if (m_appManager->removeApp(appId)) {
            statusBar()->showMessage("アプリケーションを削除しました: " + app->name, 3000);
        } else {
            QMessageBox::warning(this, "エラー", "アプリケーションの削除に失敗しました。");
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
          formatLastLaunch(app->lastLaunch),
          app->description.isEmpty() ? "なし" : app->description);
    
    QMessageBox::information(this, "アプリケーションのプロパティ", properties);
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