#include "appdiscoverydialog.h"
#include "ui_appdiscoverydialog.h"
#include "iconextractor.h"
#include <QDebug>
#include <QLabel>
#include <QApplication>
#include <QStyle>
#include <QDir>
#include <QFileIconProvider>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QInputDialog>
#include <QRegularExpression>
#include <algorithm>

AppDiscoveryDialog::AppDiscoveryDialog(AppManager *appManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AppDiscoveryDialog)
    , m_appManager(appManager)
    , m_appDiscovery(new AppDiscovery(this))
    , m_scanInProgress(false)
{
    ui->setupUi(this);
    setupUI();
    
    // AppDiscoveryのシグナル接続
    connect(m_appDiscovery, &AppDiscovery::scanProgress, 
            this, &AppDiscoveryDialog::onScanProgress);
    connect(m_appDiscovery, &AppDiscovery::appDiscovered, 
            this, &AppDiscoveryDialog::onAppDiscovered);
    connect(m_appDiscovery, &AppDiscovery::scanStarted, 
            this, &AppDiscoveryDialog::onScanStarted);
    connect(m_appDiscovery, &AppDiscovery::scanFinished, 
            this, &AppDiscoveryDialog::onScanFinished);
    connect(m_appDiscovery, &AppDiscovery::scanCanceled, 
            this, &AppDiscoveryDialog::onScanCanceled);
}

AppDiscoveryDialog::~AppDiscoveryDialog()
{
    delete ui;
}

void AppDiscoveryDialog::setupUI()
{
    // テーブルの設定
    ui->resultsTable->horizontalHeader()->setStretchLastSection(false);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_SELECTED, QHeaderView::Fixed);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_ICON, QHeaderView::Fixed);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_PATH, QHeaderView::Stretch);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_CATEGORY, QHeaderView::Fixed);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(COL_SIZE, QHeaderView::Fixed);
    
    ui->resultsTable->setColumnWidth(COL_SELECTED, 60);
    ui->resultsTable->setColumnWidth(COL_ICON, 64);
    ui->resultsTable->setColumnWidth(COL_NAME, 200);
    ui->resultsTable->setColumnWidth(COL_CATEGORY, 100);
    ui->resultsTable->setColumnWidth(COL_SIZE, 80);
    
    // 行の高さを設定してアイコンが見やすくする
    ui->resultsTable->verticalHeader()->setDefaultSectionSize(56);
    
    // シグナル接続
    connect(ui->addPathButton, &QPushButton::clicked, this, &AppDiscoveryDialog::addPath);
    connect(ui->removePathButton, &QPushButton::clicked, this, &AppDiscoveryDialog::removePath);
    connect(ui->clearPathsButton, &QPushButton::clicked, this, &AppDiscoveryDialog::clearPaths);
    connect(ui->customPathsListWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        ui->removePathButton->setEnabled(!ui->customPathsListWidget->selectedItems().isEmpty());
    });
    connect(ui->startScanButton, &QPushButton::clicked, this, &AppDiscoveryDialog::startScan);
    connect(ui->stopScanButton, &QPushButton::clicked, this, &AppDiscoveryDialog::stopScan);
    
    connect(ui->selectAllButton, &QPushButton::clicked, this, &AppDiscoveryDialog::selectAllApps);
    connect(ui->selectNoneButton, &QPushButton::clicked, this, &AppDiscoveryDialog::selectNoneApps);
    connect(ui->addToExcludeButton, &QPushButton::clicked, this, &AppDiscoveryDialog::addToExcludeList);
    connect(ui->addPatternButton, &QPushButton::clicked, this, &AppDiscoveryDialog::addExcludePattern);
    connect(ui->clearPatternsButton, &QPushButton::clicked, this, &AppDiscoveryDialog::clearExcludePatterns);
    connect(ui->addSelectedButton, &QPushButton::clicked, this, &AppDiscoveryDialog::addSelectedApps);
    
    connect(ui->resultsTable, &QTableWidget::itemSelectionChanged, 
            this, &AppDiscoveryDialog::onItemSelectionChanged);
    connect(ui->resultsTable, &QTableWidget::cellDoubleClicked,
            this, &AppDiscoveryDialog::previewApp);
    
    // 初期状態では結果タブを無効化
    ui->tabWidget->setTabEnabled(1, false);
    
    // 初期ボタン状態設定
    updatePathButtonStates();
    
    // 除外リストとパターンを読み込み
    loadExcludeList();
}

void AppDiscoveryDialog::startScan()
{
    if (m_scanInProgress) {
        return;
    }
    
    // 結果をクリア
    m_discoveredApps.clear();
    ui->resultsTable->setRowCount(0);
    
    ScanOptions options = getCurrentScanOptions();
    
    // UIの状態を更新
    setUIEnabled(false);
    m_scanInProgress = true;
    ui->tabWidget->setTabEnabled(1, true);
    ui->tabWidget->setCurrentIndex(1); // 結果タブに切り替え
    
    // 検索を開始
    QTimer::singleShot(100, [this, options]() {
        m_appDiscovery->discoverAllApps(options);
    });
}

void AppDiscoveryDialog::stopScan()
{
    if (!m_scanInProgress) {
        return;
    }
    
    m_appDiscovery->cancelScan();
}

void AppDiscoveryDialog::addPath()
{
    QString path = QFileDialog::getExistingDirectory(this, "検索フォルダを選択");
    if (!path.isEmpty()) {
        // 既に存在するかチェック
        for (int i = 0; i < ui->customPathsListWidget->count(); ++i) {
            if (ui->customPathsListWidget->item(i)->text() == path) {
                QMessageBox::information(this, "情報", "このフォルダは既に追加されています。");
                return;
            }
        }
        
        // リストに追加
        ui->customPathsListWidget->addItem(path);
        updatePathButtonStates();
        qDebug() << "Added custom path:" << path;
    }
}

void AppDiscoveryDialog::removePath()
{
    QList<QListWidgetItem*> selectedItems = ui->customPathsListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "情報", "削除するフォルダを選択してください。");
        return;
    }
    
    int ret = QMessageBox::question(this, "確認",
                                   QString("%1個のフォルダを削除しますか？").arg(selectedItems.size()),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        for (QListWidgetItem *item : selectedItems) {
            qDebug() << "Removing custom path:" << item->text();
            delete item;
        }
        updatePathButtonStates();
    }
}

void AppDiscoveryDialog::clearPaths()
{
    if (ui->customPathsListWidget->count() == 0) {
        return;
    }
    
    int ret = QMessageBox::question(this, "確認",
                                   "すべてのフォルダをクリアしますか？",
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        ui->customPathsListWidget->clear();
        updatePathButtonStates();
        qDebug() << "Cleared all custom paths";
    }
}

void AppDiscoveryDialog::addSelectedApps()
{
    QList<AppInfo> selectedApps = getSelectedApps();
    
    if (selectedApps.isEmpty()) {
        QMessageBox::information(this, "情報", "追加するアプリケーションを選択してください。");
        return;
    }
    
    // 一括追加を使用
    int addedCount = m_appManager->addApps(selectedApps);
    int duplicateCount = selectedApps.size() - addedCount;
    
    QString message = QString("%1個のアプリケーションを追加しました。").arg(addedCount);
    if (duplicateCount > 0) {
        message += QString("\n%1個は既に登録済みのため追加されませんでした。").arg(duplicateCount);
    }
    
    QMessageBox::information(this, "追加完了", message);
    
    if (addedCount > 0) {
        accept(); // ダイアログを閉じる
    }
}

void AppDiscoveryDialog::selectAllApps()
{
    for (int row = 0; row < ui->resultsTable->rowCount(); ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->resultsTable->cellWidget(row, COL_SELECTED));
        if (checkBox) {
            checkBox->setChecked(true);
        }
    }
    updateSelectedCount();
}

void AppDiscoveryDialog::selectNoneApps()
{
    for (int row = 0; row < ui->resultsTable->rowCount(); ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->resultsTable->cellWidget(row, COL_SELECTED));
        if (checkBox) {
            checkBox->setChecked(false);
        }
    }
    updateSelectedCount();
}

void AppDiscoveryDialog::onScanProgress(int current, int total, const QString &currentPath)
{
    ui->progressBar->setMaximum(total);
    ui->progressBar->setValue(current);
    ui->statusLabel->setText(QString("検索中: %1").arg(QFileInfo(currentPath).fileName()));
}

void AppDiscoveryDialog::onAppDiscovered(const AppInfo &app)
{
    // 除外リストとパターンに含まれているかチェック
    if (!isAppExcluded(app) && !isAppExcludedByPattern(app)) {
        addAppToResults(app);
        m_discoveredApps.append(app);
        updateSelectedCount();
        qDebug() << "App discovered and added:" << app.name << "at" << app.path;
    } else {
        qDebug() << "App discovered but excluded:" << app.name << "at" << app.path;
    }
}

void AppDiscoveryDialog::onScanStarted()
{
    ui->statusLabel->setText("検索を開始しています...");
    ui->progressBar->setValue(0);
    
    // テキストエリアからパターン数を取得
    QString patterns = ui->excludePatternsTextEdit->toPlainText();
    QStringList patternList = patterns.split('\n', Qt::SkipEmptyParts);
    
    qDebug() << "Scan started. Exclude list contains" << m_excludeList.size() << "entries, patterns:" << patternList.size();
}

void AppDiscoveryDialog::onScanFinished(int totalFound)
{
    setUIEnabled(true);
    m_scanInProgress = false;
    ui->statusLabel->setText(QString("検索完了: %1個のアプリケーションを発見").arg(totalFound));
    ui->progressBar->setValue(ui->progressBar->maximum());
    
    qDebug() << "Scan finished signal received. Total found:" << totalFound << "Displayed in UI:" << m_discoveredApps.size();
    
    // 検索完了メッセージを表示
    QString message;
    int displayedCount = m_discoveredApps.size(); // 実際に表示されているアプリ数
    if (displayedCount > 0) {
        message = QString("アプリケーションの検索が完了しました。\n\n"
                         "発見されたアプリケーション: %1個\n"
                         "登録したいアプリケーションを選択してください。").arg(displayedCount);
    } else {
        QString excludeInfo = (totalFound > displayedCount) ? 
                             QString("\n\n注意: %1個のアプリケーションが除外リストに含まれているため表示されていません。").arg(totalFound - displayedCount) : 
                             "";
        message = "アプリケーションの検索が完了しました。\n\n"
                 "指定された条件でアプリケーションが見つかりませんでした。\n"
                 "検索パスや条件を確認して、再度お試しください。" + excludeInfo;
    }
    
    QMessageBox::information(this, "検索完了", message);
}

void AppDiscoveryDialog::onScanCanceled()
{
    setUIEnabled(true);
    m_scanInProgress = false;
    ui->statusLabel->setText("検索が中止されました");
}

void AppDiscoveryDialog::onItemSelectionChanged()
{
    updateSelectedCount();
}

void AppDiscoveryDialog::previewApp(int row, int column)
{
    Q_UNUSED(column)
    
    if (row < 0 || row >= m_discoveredApps.size()) {
        return;
    }
    
    const AppInfo &app = m_discoveredApps[row];
    
    QString info = QString(
        "アプリケーション: %1\n"
        "パス: %2\n"
        "カテゴリ: %3\n"
        "ファイルサイズ: %4 KB"
    ).arg(app.name, app.path, app.category, QString::number(QFileInfo(app.path).size() / 1024));
    
    QMessageBox::information(this, "アプリケーション情報", info);
}

void AppDiscoveryDialog::addAppToResults(const AppInfo &app)
{
    int row = ui->resultsTable->rowCount();
    ui->resultsTable->insertRow(row);
    
    // チェックボックス
    QCheckBox *checkBox = new QCheckBox();
    checkBox->setChecked(true);
    connect(checkBox, &QCheckBox::toggled, this, &AppDiscoveryDialog::updateSelectedCount);
    ui->resultsTable->setCellWidget(row, COL_SELECTED, checkBox);
    
    // アイコン
    QLabel *iconLabel = new QLabel();
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setStyleSheet("border: 1px solid gray; background-color: #f0f0f0;");
    
    QPixmap iconPixmap;
    bool iconLoaded = false;
    
    // 1. 既存のアイコンパスをチェック
    if (!app.iconPath.isEmpty() && QFileInfo::exists(app.iconPath)) {
        if (iconPixmap.load(app.iconPath)) {
            qDebug() << "Loaded icon from path:" << app.iconPath;
            iconLoaded = true;
        } else {
            qDebug() << "Failed to load icon from path:" << app.iconPath;
        }
    }
    
    // 2. キャッシュからアイコンをチェック
    if (!iconLoaded && m_iconCacheForPath.contains(app.path)) {
        iconPixmap = m_iconCacheForPath[app.path];
        if (!iconPixmap.isNull()) {
            qDebug() << "Using cached icon for:" << app.path;
            iconLoaded = true;
        }
    }

    // 3. QFileIconProviderを使用してファイル固有アイコンを取得
    if (!iconLoaded && QFileInfo::exists(app.path)) {
        QFileIconProvider iconProvider;
        QFileInfo fileInfo(app.path);
        
        // ファイル固有のアイコンを取得
        QIcon fileIcon = iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            iconPixmap = fileIcon.pixmap(QSize(48, 48));
            if (!iconPixmap.isNull()) {
                qDebug() << "Extracted file-specific icon using QFileIconProvider from:" << app.path;
                m_iconCacheForPath[app.path] = iconPixmap; // キャッシュに保存
                iconLoaded = true;
            }
        }
        
        // フォールバック: IconExtractorを使用して実際にアイコンファイルを保存
        if (!iconLoaded) {
            IconExtractor iconExtractor;
            
            // アイコンファイルのパスを生成
            QString iconSavePath = iconExtractor.generateIconPath(app.path);
            
            // アイコンを抽出してファイルに保存
            if (iconExtractor.extractAndSaveIcon(app.path, iconSavePath)) {
                // 保存されたアイコンファイルを読み込み
                iconPixmap.load(iconSavePath);
                if (!iconPixmap.isNull()) {
                    qDebug() << "Extracted and saved icon to:" << iconSavePath;
                    app.iconPath = iconSavePath; // 実際のアイコンファイルパスを設定
                    m_iconCacheForPath[app.path] = iconPixmap; // キャッシュに保存
                    iconLoaded = true;
                }
            } else {
                qDebug() << "IconExtractor failed for:" << app.path;
            }
        }
        
        // さらなるフォールバック: 拡張子ベースのアイコン
        if (!iconLoaded) {
            QString extension = fileInfo.suffix().toLower();
            QIcon extensionIcon;
            
            if (extension == "exe") {
                extensionIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
            } else if (extension == "txt") {
                extensionIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
            } else {
                extensionIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
            }
            
            if (!extensionIcon.isNull()) {
                iconPixmap = extensionIcon.pixmap(QSize(48, 48));
                if (!iconPixmap.isNull()) {
                    qDebug() << "Using extension-based icon for:" << app.path << "ext:" << extension;
                    iconLoaded = true;
                }
            }
        }
    }
    
    // 3. デフォルトアイコンを使用
    if (!iconLoaded) {
        // 複数のデフォルトアイコンを試す
        QStringList iconTypes = {
            "SP_ComputerIcon", "SP_DesktopIcon", "SP_FileIcon", 
            "SP_DirIcon", "SP_DriveHDIcon"
        };
        
        QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        iconPixmap = defaultIcon.pixmap(QSize(48, 48));
        
        // フォールバック: 単色の四角形を作成
        if (iconPixmap.isNull()) {
            iconPixmap = QPixmap(48, 48);
            iconPixmap.fill(QColor(100, 150, 200)); // 青色
            qDebug() << "Created fallback colored icon for:" << app.name;
        } else {
            qDebug() << "Using default system icon for:" << app.name;
        }
    }
    
    // アイコンを設定
    if (!iconPixmap.isNull()) {
        QPixmap scaledIcon = iconPixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        iconLabel->setPixmap(scaledIcon);
        iconLabel->setStyleSheet("border: 1px solid gray; background-color: white;");
        qDebug() << "Icon set successfully for:" << app.name << "Size:" << scaledIcon.size();
    } else {
        // 最終フォールバック: テキスト表示
        iconLabel->setText("EXE");
        iconLabel->setStyleSheet("border: 1px solid gray; background-color: #e0e0e0; color: #666; font-weight: bold;");
        qDebug() << "Using text fallback for:" << app.name;
    }
    
    ui->resultsTable->setCellWidget(row, COL_ICON, iconLabel);
    
    // アプリケーション名
    ui->resultsTable->setItem(row, COL_NAME, new QTableWidgetItem(app.name));
    
    // パス
    ui->resultsTable->setItem(row, COL_PATH, new QTableWidgetItem(app.path));
    
    // カテゴリ
    ui->resultsTable->setItem(row, COL_CATEGORY, new QTableWidgetItem(app.category));
    
    // サイズ
    QFileInfo fileInfo(app.path);
    QString sizeText = QString("%1 KB").arg(fileInfo.size() / 1024);
    ui->resultsTable->setItem(row, COL_SIZE, new QTableWidgetItem(sizeText));
}

ScanOptions AppDiscoveryDialog::getCurrentScanOptions()
{
    ScanOptions options;
    
    options.scanDesktop = ui->scanDesktopCheck->isChecked();
    options.scanStartMenu = ui->scanStartMenuCheck->isChecked();
    options.scanProgramFiles = ui->scanProgramFilesCheck->isChecked();
    options.scanSteam = ui->scanSteamCheck->isChecked();
    options.maxDepth = ui->maxDepthSpinBox->value();
    
    // 追加パス（複数）
    for (int i = 0; i < ui->customPathsListWidget->count(); ++i) {
        QString path = ui->customPathsListWidget->item(i)->text().trimmed();
        if (!path.isEmpty()) {
            options.includePaths << path;
        }
    }
    
    // 除外パターン
    QString patterns = ui->excludePatternsTextEdit->toPlainText();
    QStringList patternList = patterns.split('\n', Qt::SkipEmptyParts);
    for (QString &pattern : patternList) {
        pattern = pattern.trimmed();
        if (!pattern.isEmpty()) {
            options.excludePatterns << pattern;
        }
    }
    
    return options;
}

QList<AppInfo> AppDiscoveryDialog::getSelectedApps()
{
    QList<AppInfo> selectedApps;
    
    for (int row = 0; row < ui->resultsTable->rowCount(); ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->resultsTable->cellWidget(row, COL_SELECTED));
        if (checkBox && checkBox->isChecked() && row < m_discoveredApps.size()) {
            selectedApps.append(m_discoveredApps[row]);
        }
    }
    
    return selectedApps;
}

void AppDiscoveryDialog::setUIEnabled(bool enabled)
{
    ui->startScanButton->setEnabled(enabled);
    ui->stopScanButton->setEnabled(!enabled);
    ui->tabWidget->setTabEnabled(0, enabled);
}

void AppDiscoveryDialog::updateSelectedCount()
{
    int selectedCount = 0;
    int totalCount = ui->resultsTable->rowCount();
    
    for (int row = 0; row < totalCount; ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->resultsTable->cellWidget(row, COL_SELECTED));
        if (checkBox && checkBox->isChecked()) {
            selectedCount++;
        }
    }
    
    ui->selectedCountLabel->setText(QString("選択: %1 / %2").arg(selectedCount).arg(totalCount));
    ui->addSelectedButton->setEnabled(selectedCount > 0);
    ui->addToExcludeButton->setEnabled(selectedCount > 0);
}

void AppDiscoveryDialog::addToExcludeList()
{
    QList<AppInfo> selectedApps = getSelectedApps();
    
    if (selectedApps.isEmpty()) {
        QMessageBox::information(this, "情報", "除外リストに追加するアプリケーションを選択してください。");
        return;
    }
    
    int addedCount = 0;
    
    for (const AppInfo &app : selectedApps) {
        // パスを除外リストに追加
        QString normalizedPath = QDir::fromNativeSeparators(app.path.toLower());
        if (!m_excludeList.contains(normalizedPath)) {
            m_excludeList.append(normalizedPath);
            addedCount++;
        }
    }
    
    if (addedCount > 0) {
        // 除外リストを保存
        saveExcludeList();
        
        // 選択されたアプリを結果から除去
        removeSelectedFromResults();
        
        QString message = QString("%1個のアプリケーションを除外リストに追加しました。").arg(addedCount);
        QMessageBox::information(this, "除外リスト追加", message);
    } else {
        QMessageBox::information(this, "情報", "選択されたアプリケーションは既に除外リストに登録されています。");
    }
}

void AppDiscoveryDialog::loadExcludeList()
{
    QString appDir = QApplication::applicationDirPath();
    
    // 除外パスリストの読み込み
    QString excludeFilePath = QDir(appDir).filePath("exclude_list.txt");
    QFile file(excludeFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Exclude list file not found, starting with empty list:" << excludeFilePath;
    } else {
        QTextStream in(&file);
        m_excludeList.clear();
        
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                m_excludeList.append(line);
            }
        }
        file.close();
        qDebug() << "Loaded exclude list with" << m_excludeList.size() << "entries";
    }
    
    // 除外パターンリストの読み込み
    // テキストエリアの内容を優先し、ファイルからも補完
    QString textAreaPatterns = ui->excludePatternsTextEdit->toPlainText();
    QStringList uiPatterns = textAreaPatterns.split('\n', Qt::SkipEmptyParts);
    
    QString patternFilePath = QDir(appDir).filePath("exclude_patterns.txt");
    QFile patternFile(patternFilePath);
    QStringList filePatterns;
    
    if (patternFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&patternFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                filePatterns.append(line);
            }
        }
        patternFile.close();
    }
    
    // UIパターンとファイルパターンを結合（重複を除去）
    m_excludePatterns.clear();
    QStringList allPatterns = uiPatterns + filePatterns;
    allPatterns.removeDuplicates();
    m_excludePatterns = allPatterns;
    
    // テキストエリアを更新（ファイルから読み込んだパターンも含める）
    if (!allPatterns.isEmpty()) {
        ui->excludePatternsTextEdit->setPlainText(allPatterns.join('\n'));
    }
    
    qDebug() << "Loaded exclude patterns with" << m_excludePatterns.size() << "entries";
}

void AppDiscoveryDialog::saveExcludeList()
{
    QString appDir = QApplication::applicationDirPath();
    QString excludeFilePath = QDir(appDir).filePath("exclude_list.txt");
    
    QFile file(excludeFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open exclude list file for writing:" << excludeFilePath;
        return;
    }
    
    QTextStream out(&file);
    for (const QString &path : m_excludeList) {
        out << path << "\n";
    }
    
    file.close();
    qDebug() << "Saved exclude list with" << m_excludeList.size() << "entries";
}

bool AppDiscoveryDialog::isAppExcluded(const AppInfo &app)
{
    QString normalizedPath = QDir::fromNativeSeparators(app.path.toLower());
    return m_excludeList.contains(normalizedPath);
}

void AppDiscoveryDialog::removeSelectedFromResults()
{
    // 選択されている行を特定
    QList<int> rowsToRemove;
    
    for (int row = 0; row < ui->resultsTable->rowCount(); ++row) {
        QCheckBox *checkBox = qobject_cast<QCheckBox*>(ui->resultsTable->cellWidget(row, COL_SELECTED));
        if (checkBox && checkBox->isChecked()) {
            rowsToRemove.append(row);
        }
    }
    
    // 行を後ろから削除（インデックスがずれないように）
    std::sort(rowsToRemove.rbegin(), rowsToRemove.rend());
    
    for (int row : rowsToRemove) {
        // m_discoveredAppsからも削除
        if (row < m_discoveredApps.size()) {
            m_discoveredApps.removeAt(row);
        }
        
        // テーブルから行を削除
        ui->resultsTable->removeRow(row);
    }
    
    updateSelectedCount();
}

void AppDiscoveryDialog::addExcludePattern()
{
    bool ok;
    QString pattern = QInputDialog::getText(this, "除外パターン追加", 
                                          "除外パターンを入力してください:\n"
                                          "例: setup, uninstall, launcher\n"
                                          "注意: 前後にワイルドカード(*)が自動で追加されます", 
                                          QLineEdit::Normal, "", &ok);
    
    if (ok && !pattern.trimmed().isEmpty()) {
        pattern = pattern.trimmed();
        
        // 前後にワイルドカードを自動追加
        QString fullPattern = "*" + pattern + "*";
        
        // テキストエリアの現在の内容を取得
        QString currentText = ui->excludePatternsTextEdit->toPlainText();
        QStringList existingPatterns = currentText.split('\n', Qt::SkipEmptyParts);
        
        // パターンが既に存在するかチェック
        if (existingPatterns.contains(fullPattern)) {
            QMessageBox::information(this, "情報", "このパターンは既に登録されています。");
            return;
        }
        
        // テキストエリアに追加
        if (!currentText.isEmpty() && !currentText.endsWith('\n')) {
            currentText += "\n";
        }
        currentText += fullPattern;
        ui->excludePatternsTextEdit->setPlainText(currentText);
        
        // ファイルに保存
        addPatternToExcludeFile(fullPattern);
        
        QMessageBox::information(this, "パターン追加", 
                               QString("除外パターン「%1」を追加しました。").arg(fullPattern));
    }
}

bool AppDiscoveryDialog::isAppExcludedByPattern(const AppInfo &app)
{
    QString appName = app.name.toLower();
    QString appPath = app.path.toLower();
    QString fileName = QFileInfo(app.path).fileName().toLower();
    
    // テキストエリアから現在のパターンを読み込み
    QString patterns = ui->excludePatternsTextEdit->toPlainText();
    QStringList patternList = patterns.split('\n', Qt::SkipEmptyParts);
    
    for (QString pattern : patternList) {
        pattern = pattern.trimmed().toLower();
        if (pattern.isEmpty()) continue;
        
        // QRegularExpressionを使用してワイルドカードパターンをチェック
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern));
        
        // アプリ名、ファイル名、パスのいずれかがパターンにマッチするかチェック
        if (regex.match(appName).hasMatch() || 
            regex.match(fileName).hasMatch() || 
            regex.match(appPath).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

void AppDiscoveryDialog::addPatternToExcludeFile(const QString &pattern)
{
    QString appDir = QApplication::applicationDirPath();
    QString patternFilePath = QDir(appDir).filePath("exclude_patterns.txt");
    
    QFile file(patternFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Cannot open exclude pattern file for writing:" << patternFilePath;
        return;
    }
    
    QTextStream out(&file);
    out << pattern << "\n";
    
    file.close();
    qDebug() << "Added pattern to exclude file:" << pattern;
}

QStringList AppDiscoveryDialog::getExcludePatterns()
{
    return m_excludePatterns;
}

void AppDiscoveryDialog::removeAppsMatchingPattern(const QString &pattern)
{
    QList<int> rowsToRemove;
    
    for (int row = 0; row < ui->resultsTable->rowCount(); ++row) {
        if (row < m_discoveredApps.size()) {
            const AppInfo &app = m_discoveredApps[row];
            
            QString appName = app.name.toLower();
            QString appPath = app.path.toLower();
            QString fileName = QFileInfo(app.path).fileName().toLower();
            QString lowerPattern = pattern.toLower();
            
            QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(lowerPattern));
            
            if (regex.match(appName).hasMatch() || 
                regex.match(fileName).hasMatch() || 
                regex.match(appPath).hasMatch()) {
                rowsToRemove.append(row);
            }
        }
    }
    
    // 行を後ろから削除（インデックスがずれないように）
    std::sort(rowsToRemove.rbegin(), rowsToRemove.rend());
    
    for (int row : rowsToRemove) {
        // m_discoveredAppsからも削除
        if (row < m_discoveredApps.size()) {
            m_discoveredApps.removeAt(row);
        }
        
        // テーブルから行を削除
        ui->resultsTable->removeRow(row);
    }
    
    if (!rowsToRemove.isEmpty()) {
        updateSelectedCount();
        qDebug() << "Removed" << rowsToRemove.size() << "apps matching pattern:" << pattern;
    }
}

void AppDiscoveryDialog::updatePathButtonStates()
{
    bool hasItems = ui->customPathsListWidget->count() > 0;
    bool hasSelection = !ui->customPathsListWidget->selectedItems().isEmpty();
    
    ui->removePathButton->setEnabled(hasSelection);
    ui->clearPathsButton->setEnabled(hasItems);
}

void AppDiscoveryDialog::clearExcludePatterns()
{
    int ret = QMessageBox::question(this, "確認",
                                   "除外パターンをすべてクリアしますか？",
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // テキストエリアをクリア
        ui->excludePatternsTextEdit->clear();
        
        // ファイルもクリア
        QString appDir = QApplication::applicationDirPath();
        QString patternFilePath = QDir(appDir).filePath("exclude_patterns.txt");
        QFile file(patternFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.close();
        }
        
        QMessageBox::information(this, "パターンクリア", "除外パターンをすべてクリアしました。");
    }
}