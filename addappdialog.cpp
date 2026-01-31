#include "addappdialog.h"
#include "ui_addappdialog.h"
#include <QDebug>
#include <QApplication>
#include <QStyle>
#include <QDir>

const QSize AddAppDialog::ICON_PREVIEW_SIZE = QSize(64, 64);
const QStringList AddAppDialog::EXECUTABLE_FILTERS = {
    "実行ファイル (*.exe)",
    "すべてのファイル (*.*)"
};

AddAppDialog::AddAppDialog(CategoryManager *categoryManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAppDialog)
    , m_iconExtractor(new IconExtractor(this))
    , m_categoryManager(categoryManager)
    , m_editMode(false)
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
    setWindowTitle("アプリケーションの追加");
}

AddAppDialog::AddAppDialog(const AppInfo &app, CategoryManager *categoryManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAppDialog)
    , m_appInfo(app)
    , m_iconExtractor(new IconExtractor(this))
    , m_categoryManager(categoryManager)
    , m_editMode(true)
{
    ui->setupUi(this);
    setupUI();
    connectSignals();
    setAppInfo(app);
    setWindowTitle("アプリケーションの編集");
}

AddAppDialog::~AddAppDialog()
{
    delete ui;
}

void AddAppDialog::setupUI()
{
    // カテゴリコンボボックスの設定
    updateCategoryComboBox();
    
    // アイコンプレビューの初期設定
    ui->iconPreviewLabel->setAlignment(Qt::AlignCenter);
    ui->iconPreviewLabel->setMinimumSize(ICON_PREVIEW_SIZE);
    ui->iconPreviewLabel->setMaximumSize(ICON_PREVIEW_SIZE);
    
    // デフォルトアイコンを設定
    QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    ui->iconPreviewLabel->setPixmap(defaultIcon.pixmap(ICON_PREVIEW_SIZE));
    
    // バリデーション
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AddAppDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &AddAppDialog::reject);
}

void AddAppDialog::connectSignals()
{
    // ファイル選択
    connect(ui->browseButton, &QPushButton::clicked, this, &AddAppDialog::onBrowseButtonClicked);
    connect(ui->pathLineEdit, &QLineEdit::textChanged, this, &AddAppDialog::onExecutablePathChanged);
    
    // アイコン操作
    connect(ui->extractIconButton, &QPushButton::clicked, this, &AddAppDialog::onExtractIconClicked);
    connect(ui->browseIconButton, &QPushButton::clicked, this, &AddAppDialog::onBrowseIconClicked);
    connect(ui->clearIconButton, &QPushButton::clicked, this, &AddAppDialog::onClearIconClicked);
    
    // IconExtractor のシグナル
    connect(m_iconExtractor, &IconExtractor::iconExtracted,
            this, &AddAppDialog::onIconExtracted);
    connect(m_iconExtractor, &IconExtractor::iconExtractionFailed,
            this, &AddAppDialog::onIconExtractionFailed);
}

AppInfo AddAppDialog::getAppInfo() const
{
    AppInfo info = m_appInfo;
    info.name = ui->nameLineEdit->text().trimmed();
    info.path = ui->pathLineEdit->text().trimmed();
    info.description = ui->descriptionTextEdit->toPlainText().trimmed();
    
    // カテゴリの取得（アイコン付きテキストからカテゴリ名のみ抽出）
    QString categoryText = ui->categoryComboBox->currentText();
    QString categoryData = ui->categoryComboBox->currentData().toString();
    info.category = categoryData.isEmpty() ? categoryText : categoryData;
    
    if (!m_customIconPath.isEmpty()) {
        info.iconPath = m_customIconPath;
    }
    
    qDebug() << "GetAppInfo - name:" << info.name << "path:" << info.path 
             << "category:" << info.category << "iconPath:" << info.iconPath;
    
    return info;
}

void AddAppDialog::setAppInfo(const AppInfo &app)
{
    m_appInfo = app;
    
    ui->nameLineEdit->setText(app.name);
    ui->pathLineEdit->setText(app.path);
    ui->descriptionTextEdit->setPlainText(app.description);
    
    // カテゴリの設定
    int categoryIndex = ui->categoryComboBox->findData(app.category);
    if (categoryIndex >= 0) {
        ui->categoryComboBox->setCurrentIndex(categoryIndex);
    } else {
        // カテゴリが見つからない場合は "その他" を選択
        int otherIndex = ui->categoryComboBox->findData("その他");
        if (otherIndex >= 0) {
            ui->categoryComboBox->setCurrentIndex(otherIndex);
        }
    }
    
    // アイコンの設定
    if (!app.iconPath.isEmpty() && QFileInfo::exists(app.iconPath)) {
        QPixmap iconPixmap(app.iconPath);
        if (!iconPixmap.isNull()) {
            ui->iconPreviewLabel->setPixmap(iconPixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_customIconPath = app.iconPath;
        }
    }
}

bool AddAppDialog::validateInput() const
{
    QString name = ui->nameLineEdit->text().trimmed();
    QString path = ui->pathLineEdit->text().trimmed();
    
    if (name.isEmpty()) {
        showErrorMessage(tr("アプリケーション名を入力してください。"));
        return false;
    }
    
    if (path.isEmpty()) {
        showErrorMessage(tr("実行ファイルのパスを選択してください。"));
        return false;
    }
    
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        showErrorMessage(tr("指定されたファイルが見つかりません。"));
        return false;
    }
    
    if (!fileInfo.isExecutable()) {
        showErrorMessage(tr("指定されたファイルは実行可能ファイルではありません。"));
        return false;
    }
    
    return true;
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
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "実行ファイルを選択",
        QString(),
        EXECUTABLE_FILTERS.join(";;")
    );
    
    if (!fileName.isEmpty()) {
        ui->pathLineEdit->setText(QDir::toNativeSeparators(fileName));
        onExecutablePathChanged();
    }
}

void AddAppDialog::onExecutablePathChanged()
{
    QString path = ui->pathLineEdit->text().trimmed();
    
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        setDefaultAppName();
        extractAndSetIcon();
    }
}

void AddAppDialog::onExtractIconClicked()
{
    QString path = ui->pathLineEdit->text().trimmed();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        extractAndSetIcon();
    } else {
        showErrorMessage(tr("まず実行ファイルのパスを選択してください。"));
    }
}

void AddAppDialog::onBrowseIconClicked()
{
    QString iconPath = QFileDialog::getOpenFileName(
        this,
        "アイコンファイルを選択",
        QString(),
        "画像ファイル (*.png *.jpg *.jpeg *.bmp *.gif *.ico);;すべてのファイル (*.*)"
    );
    
    if (!iconPath.isEmpty()) {
        QPixmap iconPixmap(iconPath);
        if (!iconPixmap.isNull()) {
            ui->iconPreviewLabel->setPixmap(iconPixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_customIconPath = iconPath;
        } else {
            showErrorMessage(tr("選択されたファイルは有効な画像ファイルではありません。"));
        }
    }
}

void AddAppDialog::onClearIconClicked()
{
    QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    ui->iconPreviewLabel->setPixmap(defaultIcon.pixmap(ICON_PREVIEW_SIZE));
    m_customIconPath.clear();
}

void AddAppDialog::onIconExtracted(const QString &executablePath, const QString &iconPath)
{
    Q_UNUSED(executablePath)
    
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        QPixmap iconPixmap(iconPath);
        if (!iconPixmap.isNull()) {
            ui->iconPreviewLabel->setPixmap(iconPixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_customIconPath = iconPath;
            return;
        }
    }
    
    // アイコン抽出に失敗した場合はデフォルトアイコンを設定
    onClearIconClicked();
}

void AddAppDialog::onIconExtractionFailed(const QString &executablePath, const QString &error)
{
    Q_UNUSED(executablePath)
    Q_UNUSED(error)
    
    // エラー時はデフォルトアイコンを設定
    onClearIconClicked();
}

void AddAppDialog::updateIconPreview()
{
    if (!m_customIconPath.isEmpty() && QFileInfo::exists(m_customIconPath)) {
        QPixmap pixmap(m_customIconPath);
        if (!pixmap.isNull()) {
            ui->iconPreviewLabel->setPixmap(pixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    
    // デフォルトアイコンを表示
    QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    ui->iconPreviewLabel->setPixmap(defaultIcon.pixmap(ICON_PREVIEW_SIZE));
}

void AddAppDialog::extractAndSetIcon()
{
    QString path = ui->pathLineEdit->text().trimmed();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        // アイコン保存先フォルダを設定（アプリケーションディレクトリ/icons）
        QString iconDir = QApplication::applicationDirPath() + "/icons";
        QDir dir(iconDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // アイコン保存先パスを生成
        QString savePath = m_iconExtractor->generateIconPath(path, iconDir);

        // アイコンを抽出してPNGとして保存
        if (m_iconExtractor->extractAndSaveIcon(path, savePath)) {
            // 成功した場合、直接パスを設定
            m_customIconPath = savePath;
            QPixmap iconPixmap(savePath);
            if (!iconPixmap.isNull()) {
                ui->iconPreviewLabel->setPixmap(iconPixmap.scaled(ICON_PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            qDebug() << "Icon saved successfully:" << savePath;
        } else {
            qDebug() << "Failed to save icon for:" << path;
        }
    }
}

void AddAppDialog::setDefaultAppName()
{
    QString path = ui->pathLineEdit->text().trimmed();
    if (path.isEmpty()) {
        return;
    }
    
    QFileInfo fileInfo(path);
    QString baseName = fileInfo.baseName();
    
    // 名前フィールドが空の場合のみ設定
    if (ui->nameLineEdit->text().trimmed().isEmpty()) {
        ui->nameLineEdit->setText(baseName);
    }
}

void AddAppDialog::updateCategoryComboBox()
{
    if (!m_categoryManager) {
        ui->categoryComboBox->addItem("その他", "その他");
        return;
    }
    
    ui->categoryComboBox->clear();
    QStringList categories = m_categoryManager->getCategories();
    
    // "すべて"を除外
    categories.removeAll("すべて");
    
    for (const QString &category : categories) {
        CategoryInfo info = m_categoryManager->getCategoryInfo(category);
        QString displayText = category;
        if (!info.icon.isEmpty()) {
            displayText = info.icon + " " + category;
        }
        ui->categoryComboBox->addItem(displayText, category);
    }
    
    // デフォルトで "その他" を選択
    int otherIndex = ui->categoryComboBox->findData("その他");
    if (otherIndex >= 0) {
        ui->categoryComboBox->setCurrentIndex(otherIndex);
    }
}

void AddAppDialog::showErrorMessage(const QString &message) const
{
    QMessageBox::warning(const_cast<AddAppDialog*>(this), tr("入力エラー"), message);
}

void AddAppDialog::accept()
{
    if (validateInput()) {
        QDialog::accept();
    }
}