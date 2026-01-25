#ifndef APPWIDGET_H
#define APPWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QMouseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDateTime>
#include "appinfo.h"

class AppWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppWidget(const AppInfo &app, QWidget *parent = nullptr);
    ~AppWidget();
    
    // アプリ情報
    const AppInfo& getAppInfo() const;
    void setAppInfo(const AppInfo &app);
    void updateAppInfo(const AppInfo &app);
    
    // 表示設定
    void setIconSize(const QSize &size);
    QSize getIconSize() const;
    
    void setSelected(bool selected);
    bool isSelected() const;
    
    // サイズ設定
    void setFixedAppSize(const QSize &size);
    QSize sizeHint() const override;

signals:
    void clicked(const QString &appId);
    void doubleClicked(const QString &appId);
    void rightClicked(const QString &appId, const QPoint &globalPos);
    void editRequested(const QString &appId);
    void removeRequested(const QString &appId);
    void propertiesRequested(const QString &appId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onEditAction();
    void onRemoveAction();
    void onOpenFolderAction();
    void onPropertiesAction();

private:
    // Windows専用のフォルダオープン処理
    bool openFolderWithExplorer(const QString &filePath);
    bool openFolderWithDesktopServices(const QString &folderPath);

private:
    void setupUI();
    void setupContextMenu();
    void updateIcon();
    void updateLabels();
    void updateStyleSheet();
    
    // UI コンポーネント
    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_folderLabel;
    QVBoxLayout *m_layout;
    QMenu *m_contextMenu;
    
    // データ
    AppInfo m_appInfo;
    QSize m_iconSize;
    QSize m_fixedSize;
    bool m_selected;
    bool m_hovered;
    
    // フォルダオープンの制御
    QDateTime m_lastFolderOpenTime;
    static QDateTime s_globalLastFolderOpenTime; // 全AppWidgetで共有
    
    // アイコンキャッシュ（インスタンス単位）
    QHash<QString, QPixmap> m_iconCache;
    
    // 定数
    static const QSize DEFAULT_ICON_SIZE;
    static const QSize DEFAULT_WIDGET_SIZE;
    static const int MARGIN;
    static const int SPACING;
};

#endif // APPWIDGET_H