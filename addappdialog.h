#ifndef ADDAPPDIALOG_H
#define ADDAPPDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QComboBox>
#include "appinfo.h"
#include "iconextractor.h"
#include "categorymanager.h"

class AddAppDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddAppDialog(CategoryManager *categoryManager, QWidget *parent = nullptr);
    explicit AddAppDialog(const AppInfo &app, CategoryManager *categoryManager, QWidget *parent = nullptr);
    ~AddAppDialog();
    
    // アプリケーション情報の取得/設定
    AppInfo getAppInfo() const;
    void setAppInfo(const AppInfo &app);
    
    // バリデーション
    bool validateInput() const;
    
    // 編集モード
    void setEditMode(bool editMode);
    bool isEditMode() const;

private slots:
    void onBrowseButtonClicked();
    void onExecutablePathChanged();
    void onAcceptClicked();
    void onIconExtracted(const QString &executablePath, const QString &iconPath);
    void onIconExtractionFailed(const QString &executablePath, const QString &error);

private:
    void setupUI();
    void connectSignals();
    void updateIconPreview();
    void extractAndSetIcon();
    void setDefaultAppName();
    void updateCategoryComboBox();
    void showErrorMessage(const QString &message);
    
    // UI コンポーネント
    QLineEdit *m_nameLineEdit;
    QLineEdit *m_pathLineEdit;
    QPushButton *m_browseButton;
    QComboBox *m_categoryComboBox;
    QLabel *m_iconLabel;
    QPushButton *m_changeIconButton;
    QTextEdit *m_descriptionTextEdit;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    QGroupBox *m_basicInfoGroup;
    QGroupBox *m_iconGroup;
    QGroupBox *m_descriptionGroup;
    
    // データ
    AppInfo m_appInfo;
    IconExtractor *m_iconExtractor;
    CategoryManager *m_categoryManager;
    bool m_editMode;
    QString m_customIconPath;
    
    // 定数
    static const QSize ICON_PREVIEW_SIZE;
    static const QStringList EXECUTABLE_FILTERS;
};

#endif // ADDAPPDIALOG_H