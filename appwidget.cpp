#include "appwidget.h"
#include "iconextractor.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QAction>
#include <QFontMetrics>
#include <QFileIconProvider>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QProcess>
#include <QElapsedTimer>
#include <QDebug>
#include <QTimer>

const QSize AppWidget::DEFAULT_ICON_SIZE(48, 48);
const QSize AppWidget::DEFAULT_WIDGET_SIZE(110, 130);
const int AppWidget::MARGIN = 6;
const int AppWidget::SPACING = 2;

// 静的変数の定義
QDateTime AppWidget::s_globalLastFolderOpenTime;

AppWidget::AppWidget(const AppInfo &app, QWidget *parent)
    : QWidget(parent)
    , m_appInfo(app)
    , m_iconSize(DEFAULT_ICON_SIZE)
    , m_fixedSize(DEFAULT_WIDGET_SIZE)
    , m_selected(false)
    , m_hovered(false)
{
    QElapsedTimer constructorTimer;
    constructorTimer.start();
    
    QElapsedTimer setupTimer;
    setupTimer.start();
    setupUI();
    // qDebug() << "AppWidget setupUI took:" << setupTimer.elapsed() << "ms for" << app.name; // DISABLED for performance
    
    setupTimer.restart();
    setupContextMenu();
    // qDebug() << "AppWidget setupContextMenu took:" << setupTimer.elapsed() << "ms for" << app.name; // DISABLED for performance
    
    setupTimer.restart();
    // 通常通りアイコンも含めて更新
    updateAppInfo(app);
    // qDebug() << "AppWidget updateAppInfo took:" << setupTimer.elapsed() << "ms for" << app.name; // DISABLED for performance
    
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    
    // qDebug() << "AppWidget constructor TOTAL took:" << constructorTimer.elapsed() << "ms for" << app.name; // DISABLED for performance
}

AppWidget::~AppWidget()
{
}

void AppWidget::setupUI()
{
    setFixedSize(m_fixedSize);
    setCursor(Qt::PointingHandCursor);
    
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    m_layout->setSpacing(SPACING);
    m_layout->setAlignment(Qt::AlignCenter);
    
    // フォルダ名ラベル（アイコンの上）
    m_folderLabel = new QLabel(this);
    m_folderLabel->setAlignment(Qt::AlignCenter);
    m_folderLabel->setWordWrap(false);
    m_folderLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    
    // フォルダ名フォント設定（小さく）
    QFont folderFont = m_folderLabel->font();
    folderFont.setPointSize(7);
    folderFont.setBold(false);
    m_folderLabel->setFont(folderFont);
    m_folderLabel->setStyleSheet("QLabel { color: #666; }");
    
    // アイコンラベル
    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(m_iconSize);
    m_iconLabel->setScaledContents(false);
    m_iconLabel->setStyleSheet("QLabel { border: none; background: transparent; }");
    
    // 名前ラベル
    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    
    // フォント設定
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(8);
    nameFont.setBold(false);
    m_nameLabel->setFont(nameFont);
    
    m_layout->addWidget(m_folderLabel);
    m_layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_nameLabel);
    
    updateStyleSheet();
}

void AppWidget::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    QAction *editAction = m_contextMenu->addAction("編集(&E)");
    editAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(editAction, &QAction::triggered, this, &AppWidget::onEditAction);
    
    QAction *openFolderAction = m_contextMenu->addAction("フォルダを開く(&F)");
    openFolderAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(openFolderAction, &QAction::triggered, this, &AppWidget::onOpenFolderAction);
    
    m_contextMenu->addSeparator();
    
    QAction *removeAction = m_contextMenu->addAction("削除(&D)");
    removeAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    connect(removeAction, &QAction::triggered, this, &AppWidget::onRemoveAction);
    
    m_contextMenu->addSeparator();
    
    QAction *propertiesAction = m_contextMenu->addAction("プロパティ(&P)");
    propertiesAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(propertiesAction, &QAction::triggered, this, &AppWidget::onPropertiesAction);
}

const AppInfo& AppWidget::getAppInfo() const
{
    return m_appInfo;
}

void AppWidget::setAppInfo(const AppInfo &app)
{
    m_appInfo = app;
    updateIcon();
    updateLabels();
}

void AppWidget::updateAppInfo(const AppInfo &app)
{
    setAppInfo(app);
}

void AppWidget::setIconSize(const QSize &size)
{
    if (size.isValid() && size != m_iconSize) {
        m_iconSize = size;
        m_iconLabel->setFixedSize(size);
        updateIcon();
    }
}

QSize AppWidget::getIconSize() const
{
    return m_iconSize;
}

void AppWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        updateStyleSheet();
        // 最適化: 必要な部分のみ再描画
        update();
    }
}

bool AppWidget::isSelected() const
{
    return m_selected;
}

void AppWidget::setFixedAppSize(const QSize &size)
{
    if (size.isValid() && size != m_fixedSize) {
        m_fixedSize = size;
        setFixedSize(size);
    }
}

QSize AppWidget::sizeHint() const
{
    return m_fixedSize;
}

void AppWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        qDebug() << "AppWidget::mousePressEvent - Left click on" << m_appInfo.name;
        emit clicked(m_appInfo.id);
    } else if (event->button() == Qt::RightButton) {
        qDebug() << "AppWidget::mousePressEvent - Right click on" << m_appInfo.name;
    }
    QWidget::mousePressEvent(event);
}

void AppWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        qDebug() << "AppWidget::mouseDoubleClickEvent - Double-click on" << m_appInfo.name << "- launching app";
        emit doubleClicked(m_appInfo.id);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AppWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // AppWidget独自のコンテキストメニューを表示
    // MainWindowのrightClickedシグナルは発行しない（重複処理を避けるため）
    qDebug() << "AppWidget::contextMenuEvent - Showing context menu for" << m_appInfo.name;
    m_contextMenu->exec(event->globalPos());
}

void AppWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    rect.adjust(1, 1, -1, -1); // マージンを少し取る
    
    // 背景の描画
    if (m_selected) {
        // 選択時の背景
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, QColor(33, 150, 243, 100)); // #2196f3 with alpha
        gradient.setColorAt(1, QColor(25, 118, 210, 120)); // #1976d2 with alpha
        
        painter.setBrush(QBrush(gradient));
        painter.setPen(QPen(QColor(25, 118, 210), 2));
        painter.drawRoundedRect(rect, 6, 6);
        
    } else if (m_hovered) {
        // ホバー時の背景
        QLinearGradient gradient(0, 0, 0, rect.height());
        gradient.setColorAt(0, QColor(227, 242, 253, 80)); // #e3f2fd with alpha
        gradient.setColorAt(1, QColor(187, 222, 251, 100)); // #bbdefb with alpha
        
        painter.setBrush(QBrush(gradient));
        painter.setPen(QPen(QColor(179, 217, 255, 150), 1));
        painter.drawRoundedRect(rect, 6, 6);
    }
    
    QWidget::paintEvent(event);
}

void AppWidget::enterEvent(QEnterEvent *event)
{
    if (!m_hovered) {
        m_hovered = true;
        updateStyleSheet();
        update();
    }
    QWidget::enterEvent(event);
}

void AppWidget::leaveEvent(QEvent *event)
{
    if (m_hovered) {
        m_hovered = false;
        updateStyleSheet();
        update();
    }
    QWidget::leaveEvent(event);
}

void AppWidget::onEditAction()
{
    emit editRequested(m_appInfo.id);
}

void AppWidget::onRemoveAction()
{
    qDebug() << "AppWidget::onRemoveAction - Requesting removal of app:" << m_appInfo.name << "ID:" << m_appInfo.id;
    emit removeRequested(m_appInfo.id);
}

void AppWidget::onOpenFolderAction()
{
    qDebug() << "=== AppWidget::onOpenFolderAction START ===";
    qDebug() << "App name:" << m_appInfo.name;
    qDebug() << "App path:" << m_appInfo.path;
    qDebug() << "App ID:" << m_appInfo.id;
    qDebug() << "App iconPath:" << m_appInfo.iconPath;
    qDebug() << "App category:" << m_appInfo.category;
    qDebug() << "App description:" << m_appInfo.description;
    qDebug() << "Current time:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // パスが空でないかの基本チェック
    if (m_appInfo.path.isEmpty()) {
        qCritical() << "*** CRITICAL ERROR: App path is completely empty! ***";
        qDebug() << "App info dump:";
        qDebug() << "  Name:" << m_appInfo.name;
        qDebug() << "  ID:" << m_appInfo.id;
        qDebug() << "  All other fields:" << m_appInfo.description << m_appInfo.category << m_appInfo.iconPath;
        return;
    }
    
    // パスの基本情報を表示
    qDebug() << "Raw app path length:" << m_appInfo.path.length();
    qDebug() << "Raw app path contains spaces:" << m_appInfo.path.contains(' ');
    qDebug() << "Raw app path first 50 chars:" << m_appInfo.path.left(50);
    if (m_appInfo.path.length() > 50) {
        qDebug() << "Raw app path last 50 chars:" << m_appInfo.path.right(50);
    }
    
    // 連続実行制御: グローバルとローカル両方をチェック
    QDateTime currentTime = QDateTime::currentDateTime();
    
    // グローバル制御チェック (全AppWidgetを通じて)
    if (s_globalLastFolderOpenTime.isValid()) {
        qint64 msSinceGlobalOpen = s_globalLastFolderOpenTime.msecsTo(currentTime);
        qDebug() << "Global last folder open time:" << s_globalLastFolderOpenTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
        qDebug() << "Global time difference:" << msSinceGlobalOpen << "ms";
        if (msSinceGlobalOpen < 2000) { // グローバルは2秒制御
            qWarning() << "*** IGNORING rapid consecutive folder open request (global within 2 seconds) ***";
            return;
        }
    }
    
    // ローカル制御チェック (このAppWidget)
    if (m_lastFolderOpenTime.isValid()) {
        qint64 msSinceLocalOpen = m_lastFolderOpenTime.msecsTo(currentTime);
        qDebug() << "Local last folder open time:" << m_lastFolderOpenTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
        qDebug() << "Local time difference:" << msSinceLocalOpen << "ms";
        if (msSinceLocalOpen < 1500) { // ローカルは1.5秒制御
            qWarning() << "*** IGNORING rapid consecutive folder open request (local within 1.5 seconds) ***";
            return;
        }
    }
    
    qDebug() << "Time checks passed - proceeding with folder open";
    m_lastFolderOpenTime = currentTime;
    s_globalLastFolderOpenTime = currentTime;
    
    if (m_appInfo.path.isEmpty()) {
        qWarning() << "*** ERROR: App path is empty for" << m_appInfo.name << "***";
        return;
    }

    QFileInfo fileInfo(m_appInfo.path);
    QString folderPath = fileInfo.absoluteDir().absolutePath();
    
    qDebug() << "--- Path Analysis ---";
    qDebug() << "Original app path:" << m_appInfo.path;
    qDebug() << "Absolute file path:" << fileInfo.absoluteFilePath();
    qDebug() << "Canonical file path:" << fileInfo.canonicalFilePath();
    qDebug() << "Calculated folder path:" << folderPath;
    qDebug() << "Absolute folder path:" << fileInfo.absoluteDir().absolutePath();
    qDebug() << "Canonical folder path:" << fileInfo.absoluteDir().canonicalPath();
    qDebug() << "File exists:" << fileInfo.exists();
    qDebug() << "File is readable:" << fileInfo.isReadable();
    qDebug() << "Folder exists:" << QDir(folderPath).exists();

#ifdef Q_OS_WIN
    qDebug() << "--- Windows Environment Detected ---";
    
    // DEBUGGING: 一旦最もシンプルな方法だけをテスト
    qDebug() << "*** DEBUGGING MODE: Testing simplest folder opening method ***";
    
    // 方法1: フォルダが存在するかもう一度確認
    if (!QDir(folderPath).exists()) {
        qWarning() << "*** CRITICAL: Target folder does not exist:" << folderPath << "***";
        qDebug() << "=== AppWidget::onOpenFolderAction END (FOLDER NOT EXIST) ===";
        return;
    }
    
    qDebug() << "Folder confirmed to exist - trying direct explorer command";
    
    // 方法2: 直接的なexplorerコマンドテスト
    QString directCommand = QString("explorer.exe \"%1\"").arg(QDir::toNativeSeparators(folderPath));
    qDebug() << "DIRECT COMMAND:" << directCommand;
    
    bool directResult = QProcess::startDetached(directCommand);
    qDebug() << "Direct explorer result:" << directResult;
    
    if (directResult) {
        qDebug() << "*** DIRECT EXPLORER SUCCESS ***";
        qDebug() << "=== AppWidget::onOpenFolderAction END (SUCCESS direct explorer) ===";
        return;
    }
    
    // 方法3: QDesktopServicesをテスト
    qDebug() << "Direct explorer failed, testing QDesktopServices";
    bool desktopResult = openFolderWithDesktopServices(folderPath);
    qDebug() << "QDesktopServices result:" << desktopResult;
    
    if (desktopResult) {
        qDebug() << "*** QDESKTOPSERVICES SUCCESS ***";
        qDebug() << "=== AppWidget::onOpenFolderAction END (SUCCESS QDesktopServices) ===";
        return;
    }
    
    // 方法4: 最後の手段 - cmd.exe経由
    qDebug() << "QDesktopServices failed, trying cmd.exe";
    QString cmdCommand = QString("cmd.exe /C start \"\" \"%1\"").arg(QDir::toNativeSeparators(folderPath));
    qDebug() << "CMD COMMAND:" << cmdCommand;
    
    bool cmdResult = QProcess::startDetached(cmdCommand);
    qDebug() << "CMD result:" << cmdResult;
    
    if (cmdResult) {
        qDebug() << "*** CMD SUCCESS ***";
        qDebug() << "=== AppWidget::onOpenFolderAction END (SUCCESS cmd) ===";
        return;
    }
    
    qWarning() << "*** ALL METHODS FAILED ***";
    qDebug() << "Failed folder path:" << folderPath;
    qDebug() << "Failed normalized path:" << QDir::toNativeSeparators(folderPath);
    qDebug() << "=== AppWidget::onOpenFolderAction END (ALL FAILED) ===";
    
#else
    qDebug() << "--- Non-Windows Environment ---";
    if (QDir(folderPath).exists()) {
        openFolderWithDesktopServices(folderPath);
    } else {
        qWarning() << "Target folder does not exist:" << folderPath;
    }
    qDebug() << "=== AppWidget::onOpenFolderAction END (Non-Windows) ===";
#endif
}

void AppWidget::onPropertiesAction()
{
    emit propertiesRequested(m_appInfo.id);
}

bool AppWidget::openFolderWithExplorer(const QString &filePath)
{
#ifdef Q_OS_WIN
    qDebug() << "--- openFolderWithExplorer START ---";
    qDebug() << "Input file path:" << filePath;
    
    QString normalizedPath = QDir::toNativeSeparators(filePath);
    qDebug() << "Normalized path:" << normalizedPath;
    
    // パスの検証
    QFileInfo fileInfo(filePath);
    qDebug() << "File exists check:" << fileInfo.exists();
    qDebug() << "File absolute path:" << fileInfo.absoluteFilePath();
    qDebug() << "File canonical path:" << fileInfo.canonicalFilePath();
    
    if (!fileInfo.exists()) {
        qWarning() << "*** File does not exist, cannot use explorer /select ***";
        qDebug() << "--- openFolderWithExplorer END (file not exist) ---";
        return false;
    }
    
    qDebug() << "File exists - testing simpler approaches";
    
    // TEST: 最もシンプルで確実な方法をテスト
    // まず、フォルダだけを開く（ファイル選択なし）
    QString folderPath = fileInfo.absoluteDir().absolutePath();
    QString folderNormalized = QDir::toNativeSeparators(folderPath);
    
    qDebug() << "SIMPLE TEST - Opening folder only (no file selection)";
    qDebug() << "Folder path:" << folderPath;
    qDebug() << "Folder normalized:" << folderNormalized;
    
    // Method 1: シンプルなexplorer フォルダパス
    QString simpleCommand = QString("explorer.exe \"%1\"").arg(folderNormalized);
    qDebug() << "SIMPLE METHOD - Command:" << simpleCommand;
    
    bool simpleSuccess = QProcess::startDetached(simpleCommand);
    qDebug() << "Simple explorer result:" << simpleSuccess;
    
    if (simpleSuccess) {
        qDebug() << "*** SIMPLE METHOD SUCCESS - Folder opened ***";
        qDebug() << "--- openFolderWithExplorer END (simple success) ---";
        return true;
    }
    
    // Method 2: QProcessで別々の引数
    qDebug() << "Simple method failed, trying QProcess with separate args";
    QStringList simpleArgs;
    simpleArgs << folderNormalized;
    
    qint64 pid = 0;
    bool qprocessSuccess = QProcess::startDetached("explorer.exe", simpleArgs, QString(), &pid);
    qDebug() << "QProcess separate args result:" << qprocessSuccess << "PID:" << pid;
    
    if (qprocessSuccess && pid > 0) {
        qDebug() << "*** QPROCESS METHOD SUCCESS - Folder opened with PID:" << pid << "***";
        qDebug() << "--- openFolderWithExplorer END (qprocess success) ---";
        return true;
    }
    
    qWarning() << "*** ALL SIMPLE METHODS FAILED ***";
    qDebug() << "--- openFolderWithExplorer END (all failed) ---";
    return false;
#else
    Q_UNUSED(filePath)
    qDebug() << "Not Windows environment - returning false";
    return false;
#endif
}

bool AppWidget::openFolderWithDesktopServices(const QString &folderPath)
{
    qDebug() << "--- openFolderWithDesktopServices START ---";
    qDebug() << "Input folder path:" << folderPath;
    
    // パスの検証
    QDir dir(folderPath);
    qDebug() << "Directory exists check:" << dir.exists();
    qDebug() << "Directory absolute path:" << dir.absolutePath();
    qDebug() << "Directory canonical path:" << dir.canonicalPath();
    
    if (!dir.exists()) {
        qWarning() << "*** Folder does not exist:" << folderPath << "***";
        qDebug() << "--- openFolderWithDesktopServices END (folder not exist) ---";
        return false;
    }
    
    // 正規化されたパスを使用
    QString canonicalPath = dir.canonicalPath();
    qDebug() << "Using canonical path:" << canonicalPath;
    
    // URLの作成と検証
    QUrl folderUrl = QUrl::fromLocalFile(canonicalPath);
    qDebug() << "Created URL:" << folderUrl.toString();
    qDebug() << "URL scheme:" << folderUrl.scheme();
    qDebug() << "URL path:" << folderUrl.path();
    qDebug() << "URL to local file:" << folderUrl.toLocalFile();
    qDebug() << "URL is valid:" << folderUrl.isValid();
    qDebug() << "URL is local file:" << folderUrl.isLocalFile();
    
    qDebug() << "Calling QDesktopServices::openUrl...";
    bool result = QDesktopServices::openUrl(folderUrl);
    qDebug() << "QDesktopServices::openUrl result:" << result;
    
    if (result) {
        qDebug() << "*** SUCCESS - QDesktopServices opened folder successfully ***";
        qDebug() << "--- openFolderWithDesktopServices END (success) ---";
    } else {
        qWarning() << "*** FAILED - QDesktopServices::openUrl failed ***";
        qDebug() << "Failed URL:" << folderUrl.toString();
        qDebug() << "Failed canonical path:" << canonicalPath;
        qDebug() << "--- openFolderWithDesktopServices END (failed) ---";
    }
    
    return result;
}

void AppWidget::updateIcon()
{
    QElapsedTimer iconTimer;
    iconTimer.start();
    
    QPixmap iconPixmap;
    
    // 1. アイコンファイルが存在する場合（保存済みアイコンを確認）
    QElapsedTimer step1Timer;
    step1Timer.start();
    if (!m_appInfo.iconPath.isEmpty() && QFileInfo::exists(m_appInfo.iconPath)) {
        iconPixmap = QPixmap(m_appInfo.iconPath);
    } else if (m_appInfo.iconPath.isEmpty() && !m_appInfo.path.isEmpty()) {
        // iconPathが空の場合、保存済みアイコンファイルがあるか確認
        IconExtractor iconExtractor;
        QString possibleIconPath = iconExtractor.generateIconPath(m_appInfo.path);
        if (QFileInfo::exists(possibleIconPath)) {
            iconPixmap = QPixmap(possibleIconPath);
            qDebug() << "Found saved icon:" << possibleIconPath << "for" << m_appInfo.name;
        }
    }
    // qDebug() << "Icon step 1 took:" << step1Timer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
    
    // 2. QFileIconProviderを使用してファイル固有アイコンを取得
    QElapsedTimer step2Timer;
    step2Timer.start();
    if (iconPixmap.isNull() && !m_appInfo.path.isEmpty() && QFileInfo::exists(m_appInfo.path)) {
        QFileIconProvider iconProvider;
        QFileInfo fileInfo(m_appInfo.path);
        QIcon fileIcon = iconProvider.icon(fileInfo);
        if (!fileIcon.isNull()) {
            iconPixmap = fileIcon.pixmap(m_iconSize);
            // アイコン抽出成功
        }
    }
    // qDebug() << "Icon step 2 (FileIconProvider) took:" << step2Timer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
    
    // 3. IconExtractorを使用してアイコンを抽出（プロセス内キャッシュ使用）
    QElapsedTimer step3Timer;
    step3Timer.start();
    if (iconPixmap.isNull() && !m_appInfo.path.isEmpty() && QFileInfo::exists(m_appInfo.path)) {
        // インスタンスキャッシュでアイコン抽出の重複を防ぐ
        QString cachePath = m_appInfo.path + "_" + QString::number(m_iconSize.width());
        
        if (m_iconCache.contains(cachePath)) {
            iconPixmap = m_iconCache[cachePath];
        } else {
            IconExtractor iconExtractor;
            iconPixmap = iconExtractor.extractIconPixmap(m_appInfo.path, m_iconSize);
            if (!iconPixmap.isNull()) {
                m_iconCache[cachePath] = iconPixmap; // インスタンスキャッシュに保存
            }
        }
    }
    // qDebug() << "Icon step 3 (IconExtractor) took:" << step3Timer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
    
    // 4. デフォルトアイコン
    QElapsedTimer step4Timer;
    step4Timer.start();
    if (iconPixmap.isNull()) {
        QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        iconPixmap = defaultIcon.pixmap(m_iconSize);
        // デフォルトアイコン使用
    }
    // qDebug() << "Icon step 4 (Default) took:" << step4Timer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
    
    // アイコンサイズに合わせてスケール
    QElapsedTimer scaleTimer;
    scaleTimer.start();
    if (!iconPixmap.isNull()) {
        QPixmap scaledPixmap = iconPixmap.scaled(m_iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // 中央寄せのために、ラベルサイズに合わせたPixmapを作成
        QPixmap centeredPixmap(m_iconSize);
        centeredPixmap.fill(Qt::transparent);
        
        QPainter painter(&centeredPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 中央に配置するための計算
        int x = (m_iconSize.width() - scaledPixmap.width()) / 2;
        int y = (m_iconSize.height() - scaledPixmap.height()) / 2;
        
        painter.drawPixmap(x, y, scaledPixmap);
        painter.end();
        
        m_iconLabel->setPixmap(centeredPixmap);
    }
    // qDebug() << "Icon scaling took:" << scaleTimer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
    
    // qDebug() << "updateIcon TOTAL took:" << iconTimer.elapsed() << "ms for" << m_appInfo.name; // DISABLED for performance
}


void AppWidget::updateLabels()
{
    // フォルダ名の設定
    QFileInfo fileInfo(m_appInfo.path);
    QString folderName = fileInfo.dir().dirName();
    if (folderName.isEmpty()) {
        folderName = fileInfo.dir().absolutePath();
    }
    
    // フォルダ名が長い場合は省略
    QFontMetrics folderFm(m_folderLabel->font());
    int folderMaxWidth = m_fixedSize.width() - (MARGIN * 2);
    QString elidedFolderName = folderFm.elidedText(folderName, Qt::ElideMiddle, folderMaxWidth);
    m_folderLabel->setText(elidedFolderName);
    
    // 名前の設定（長い名前は省略）
    QString displayName = m_appInfo.name;
    QFontMetrics fm(m_nameLabel->font());
    int maxWidth = m_fixedSize.width() - (MARGIN * 2);
    
    // 2行以内に収まるように調整
    QStringList words = displayName.split(' ');
    QString line1, line2;
    
    for (const QString &word : words) {
        QString testLine1 = line1.isEmpty() ? word : line1 + " " + word;
        if (fm.horizontalAdvance(testLine1) <= maxWidth) {
            line1 = testLine1;
        } else {
            QString testLine2 = line2.isEmpty() ? word : line2 + " " + word;
            if (fm.horizontalAdvance(testLine2) <= maxWidth) {
                line2 = testLine2;
            } else {
                // 2行目も収まらない場合は省略
                line2 = fm.elidedText(line2 + " " + word, Qt::ElideRight, maxWidth);
                break;
            }
        }
    }
    
    QString finalName = line2.isEmpty() ? line1 : line1 + "\n" + line2;
    m_nameLabel->setText(finalName);
    
    // ツールチップの設定
    QString tooltip = QString("<b>%1</b><br>パス: %2").arg(m_appInfo.name, m_appInfo.path);
    if (m_appInfo.launchCount > 0) {
        tooltip += QString("<br>起動回数: %1回").arg(m_appInfo.launchCount);
    }
    if (m_appInfo.lastLaunch.isValid()) {
        tooltip += QString("<br>最終起動: %1").arg(m_appInfo.lastLaunch.toString("yyyy/MM/dd hh:mm"));
    }
    setToolTip(tooltip);
}

void AppWidget::updateStyleSheet()
{
    QString styleSheet;
    
    if (m_selected) {
        styleSheet = QString(
            "QLabel { color: #0d47a1; font-weight: 500; } "
        );
    } else if (m_hovered) {
        styleSheet = QString(
            "QLabel { color: #1565c0; } "
        );
    } else {
        styleSheet = QString(
            "QLabel { color: #333333; } "
        );
    }
    
    // スタイルが変更された場合のみ適用
    if (this->styleSheet() != styleSheet) {
        setStyleSheet(styleSheet);
    }
}