#include "addappdialog.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QApplication>
#include <QStyle>
#include <QSizePolicy>

const QSize AddAppDialog::ICON_PREVIEW_SIZE(64, 64);
const QStringList AddAppDialog::EXECUTABLE_FILTERS = {
    "実行ファイル (*.exe)",
    "アプリケーション (*.app)",
    "すべてのファイル (*.*)"
};

AddAppDialog::AddAppDialog(CategoryManager *categoryManager, QWidget *parent)
    : QDialog(parent)
    , m_iconExtractor(new IconExtractor(this))
    , m_categoryManager(categoryManager)
    , m_editMode(false)
{
    setupUI();
    connectSignals();
    setWindowTitle("アプリケーションの追加");
}

AddAppDialog::AddAppDialog(const AppInfo &app, CategoryManager *categoryManager, QWidget *parent)
    : QDialog(parent)
    , m_appInfo(app)
    , m_iconExtractor(new IconExtractor(this))
    , m_categoryManager(categoryManager)
    , m_editMode(true)
{
    setupUI();
    connectSignals();
    setAppInfo(app);
    setWindowTitle("アプリケーションの編集");
}

AddAppDialog::~AddAppDialog()
{
}

void AddAppDialog::setupUI()
{
    setModal(true);
    setMinimumSize(500, 400);
    resize(550, 450);
    
    // メインレイアウト
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 基本情報グループ
    m_basicInfoGroup = new QGroupBox("基本情報", this);
    QFormLayout *basicLayout = new QFormLayout(m_basicInfoGroup);
    basicLayout->setSpacing(10);
    
    // アプリケーション名
    m_nameLineEdit = new QLineEdit(this);
    m_nameLineEdit->setMaxLength(50);
    m_nameLineEdit->setPlaceholderText("アプリケーション名を入力してください");
    basicLayout->addRow("名前(&N):", m_nameLineEdit);
    
    // 実行ファイルパス
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setPlaceholderText("実行ファイルのパスを選択してください");
    m_pathLineEdit->setReadOnly(true);
    
    m_browseButton = new QPushButton("参照(&B)...", this);
    m_browseButton->setMaximumWidth(80);
    
    pathLayout->addWidget(m_pathLineEdit);
    pathLayout->addWidget(m_browseButton);
    basicLayout->addRow("パス(&P):", pathLayout);
    
    // カテゴリ選択
    m_categoryComboBox = new QComboBox(this);
    m_categoryComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateCategoryComboBox();
    basicLayout->addRow("カテゴリ(&C):", m_categoryComboBox);
    
    mainLayout->addWidget(m_basicInfoGroup);
    
    // アイコングループ
    m_iconGroup = new QGroupBox("アイコン", this);
    QHBoxLayout *iconLayout = new QHBoxLayout(m_iconGroup);
    iconLayout->setSpacing(15);
    
    // アイコンプレビュー
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(ICON_PREVIEW_SIZE);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("QLabel { border: 2px dashed #cccccc; background-color: #f9f9f9; }");
    m_iconLabel->setText("アイコン\nプレビュー");
    
    // アイコン変更ボタン
    QVBoxLayout *iconButtonLayout = new QVBoxLayout();
    m_changeIconButton = new QPushButton("アイコンを変更(&I)...", this);
    m_changeIconButton->setEnabled(false);
    
    iconButtonLayout->addWidget(m_changeIconButton);
    iconButtonLayout->addStretch();
    
    iconLayout->addWidget(m_iconLabel);
    iconLayout->addLayout(iconButtonLayout);
    iconLayout->addStretch();
    
    mainLayout->addWidget(m_iconGroup);
    
    // 説明グループ
    m_descriptionGroup = new QGroupBox("説明（任意）", this);
    QVBoxLayout *descLayout = new QVBoxLayout(m_descriptionGroup);
    
    m_descriptionTextEdit = new QTextEdit(this);
    m_descriptionTextEdit->setMaximumHeight(80);
    m_descriptionTextEdit->setPlaceholderText("アプリケーションの説明を入力してください（任意）");
    descLayout->addWidget(m_descriptionTextEdit);
    
    mainLayout->addWidget(m_descriptionGroup);
    
    mainLayout->addStretch();
    
    // ボタンレイアウト
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton("キャンセル(&C)", this);
    m_okButton = new QPushButton("OK(&O)", this);
    m_okButton->setDefault(true);
    m_okButton->setEnabled(false);
    
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_okButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // スタイル適用
    setStyleSheet(
        "QGroupBox { "
        "    font-weight: bold; "
        "    border: 2px solid #b3d9ff; "
        "    border-radius: 6px; "
        "    margin-top: 8px; "
        "    padding-top: 10px; "
        "} "
        "QGroupBox::title { "
        "    subcontrol-origin: margin; "
        "    left: 10px; "
        "    padding: 0 5px 0 5px; "
        "    color: #1565c0; "
        "} "
        "QLineEdit { "
        "    padding: 6px; "
        "    border: 1px solid #b3d9ff; "
        "    border-radius: 4px; "
        "} "
        "QLineEdit:focus { "
        "    border-color: #2196f3; "
        "} "
        "QPushButton { "
        "    padding: 8px 16px; "
        "    border: 1px solid #b3d9ff; "
        "    border-radius: 4px; "
        "    background-color: #f8fbff; "
        "} "
        "QPushButton:hover { "
        "    background-color: #e3f2fd; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #bbdefb; "
        "} "
        "QPushButton:disabled { "
        "    color: #999999; "
        "    background-color: #f5f5f5; "
        "    border-color: #d5d5d5; "
        "} "
        "QTextEdit { "
        "    padding: 6px; "
        "    border: 1px solid #b3d9ff; "
        "    border-radius: 4px; "
        "} "
        "QTextEdit:focus { "
        "    border-color: #2196f3; "
        "}"
    );
}

void AddAppDialog::connectSignals()
{
    connect(m_browseButton, &QPushButton::clicked, this, &AddAppDialog::onBrowseButtonClicked);
    connect(m_pathLineEdit, &QLineEdit::textChanged, this, &AddAppDialog::onExecutablePathChanged);
    connect(m_nameLineEdit, &QLineEdit::textChanged, this, [this]() {
        m_okButton->setEnabled(validateInput());
    });
    
    connect(m_okButton, &QPushButton::clicked, this, &AddAppDialog::onAcceptClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // アイコン抽出シグナル
    connect(m_iconExtractor, &IconExtractor::iconExtracted, 
            this, &AddAppDialog::onIconExtracted);
    connect(m_iconExtractor, &IconExtractor::iconExtractionFailed, 
            this, &AddAppDialog::onIconExtractionFailed);
}

AppInfo AddAppDialog::getAppInfo() const
{
    AppInfo info = m_appInfo;
    info.name = m_nameLineEdit->text().trimmed();
    info.path = m_pathLineEdit->text().trimmed();
    info.description = m_descriptionTextEdit->toPlainText().trimmed();
    info.category = m_categoryComboBox->currentText();
    
    if (!m_customIconPath.isEmpty()) {
        info.iconPath = m_customIconPath;
    }
    
    qDebug() << "GetAppInfo - name:" << info.name << "path:" << info.path << "category:" << info.category << "iconPath:" << info.iconPath;
    
    return info;
}

void AddAppDialog::setAppInfo(const AppInfo &app)
{
    m_appInfo = app;
    m_nameLineEdit->setText(app.name);
    m_pathLineEdit->setText(app.path);
    m_descriptionTextEdit->setPlainText(app.description);
    
    // カテゴリの設定
    int categoryIndex = m_categoryComboBox->findText(app.category);
    if (categoryIndex >= 0) {
        m_categoryComboBox->setCurrentIndex(categoryIndex);
    } else {
        // カテゴリが見つからない場合は "その他" を選択
        int otherIndex = m_categoryComboBox->findText("その他");
        if (otherIndex >= 0) {
            m_categoryComboBox->setCurrentIndex(otherIndex);
        }
    }
    
    if (!app.iconPath.isEmpty() && QFileInfo::exists(app.iconPath)) {
        QPixmap iconPixmap(app.iconPath);
        if (!iconPixmap.isNull()) {
            m_iconLabel->setPixmap(iconPixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_customIconPath = app.iconPath;
        }
    }
    
    updateIconPreview();
    m_okButton->setEnabled(validateInput());
}

bool AddAppDialog::validateInput() const
{
    QString name = m_nameLineEdit->text().trimmed();
    QString path = m_pathLineEdit->text().trimmed();
    
    qDebug() << "Validating input - name:" << name << "path:" << path;
    
    if (name.isEmpty() || path.isEmpty()) {
        qDebug() << "Name or path is empty";
        return false;
    }
    
    QFileInfo fileInfo(path);
    bool exists = fileInfo.exists();
    bool executable = fileInfo.isExecutable();
    
    qDebug() << "File exists:" << exists << "Is executable:" << executable;
    
    return exists && executable;
}

void AddAppDialog::setEditMode(bool editMode)
{
    m_editMode = editMode;
    setWindowTitle(editMode ? "アプリケーションの編集" : "アプリケーションの追加");
}

bool AddAppDialog::isEditMode() const
{
    return m_editMode;
}

void AddAppDialog::onBrowseButtonClicked()
{
    QString filter = EXECUTABLE_FILTERS.join(";;");
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "実行ファイルを選択",
        QString(),
        filter
    );
    
    if (!fileName.isEmpty()) {
        m_pathLineEdit->setText(fileName);
    }
}

void AddAppDialog::onExecutablePathChanged()
{
    QString path = m_pathLineEdit->text().trimmed();
    
    if (QFileInfo::exists(path)) {
        setDefaultAppName();
        extractAndSetIcon();
        m_changeIconButton->setEnabled(true);
    } else {
        m_iconLabel->clear();
        m_iconLabel->setText("アイコン\nプレビュー");
        m_changeIconButton->setEnabled(false);
        m_customIconPath.clear();
    }
    
    m_okButton->setEnabled(validateInput());
}

void AddAppDialog::onAcceptClicked()
{
    qDebug() << "Accept button clicked";
    if (!validateInput()) {
        qDebug() << "Validation failed";
        showErrorMessage("入力内容に誤りがあります。\nアプリケーション名とパスを正しく入力してください。");
        return;
    }
    
    qDebug() << "Validation passed";
    
    // 重複チェック（編集モードでない場合）
    if (!m_editMode) {
        QString path = m_pathLineEdit->text().trimmed();
        // TODO: AppManagerで重複チェック
    }
    
    qDebug() << "Calling accept()";
    accept();
}

void AddAppDialog::onIconExtracted(const QString &executablePath, const QString &iconPath)
{
    if (executablePath == m_pathLineEdit->text().trimmed()) {
        m_customIconPath = iconPath;
        updateIconPreview();
    }
}

void AddAppDialog::onIconExtractionFailed(const QString &executablePath, const QString &error)
{
    if (executablePath == m_pathLineEdit->text().trimmed()) {
        qWarning() << "Icon extraction failed for" << executablePath << ":" << error;
        // デフォルトアイコンを表示
        QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        m_iconLabel->setPixmap(defaultIcon.pixmap(ICON_PREVIEW_SIZE));
    }
}

void AddAppDialog::updateIconPreview()
{
    if (!m_customIconPath.isEmpty() && QFileInfo::exists(m_customIconPath)) {
        QPixmap pixmap(m_customIconPath);
        if (!pixmap.isNull()) {
            m_iconLabel->setPixmap(pixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    
    // フォールバック
    QString path = m_pathLineEdit->text().trimmed();
    if (!path.isEmpty()) {
        QIcon icon = m_iconExtractor->extractIcon(path);
        m_iconLabel->setPixmap(icon.pixmap(ICON_PREVIEW_SIZE));
    }
}

void AddAppDialog::extractAndSetIcon()
{
    QString path = m_pathLineEdit->text().trimmed();
    if (!path.isEmpty()) {
        QString iconPath = m_iconExtractor->generateIconPath(path);
        m_iconExtractor->extractAndSaveIcon(path, iconPath);
    }
}

void AddAppDialog::setDefaultAppName()
{
    if (m_nameLineEdit->text().trimmed().isEmpty()) {
        QString path = m_pathLineEdit->text().trimmed();
        if (!path.isEmpty()) {
            QFileInfo fileInfo(path);
            QString baseName = fileInfo.completeBaseName();
            m_nameLineEdit->setText(baseName);
        }
    }
}

void AddAppDialog::updateCategoryComboBox()
{
    if (!m_categoryManager) {
        m_categoryComboBox->addItem("その他");
        return;
    }
    
    m_categoryComboBox->clear();
    QStringList categories = m_categoryManager->getCategories();
    
    // "すべて"を除外
    categories.removeAll("すべて");
    
    for (const QString &category : categories) {
        CategoryInfo info = m_categoryManager->getCategoryInfo(category);
        QString displayText = category;
        if (!info.icon.isEmpty()) {
            displayText = info.icon + " " + category;
        }
        m_categoryComboBox->addItem(displayText, category);
    }
    
    // デフォルトで "その他" を選択
    int otherIndex = m_categoryComboBox->findData("その他");
    if (otherIndex >= 0) {
        m_categoryComboBox->setCurrentIndex(otherIndex);
    }
}

void AddAppDialog::showErrorMessage(const QString &message)
{
    QMessageBox::warning(this, "入力エラー", message);
}