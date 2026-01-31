#ifndef APPICONDELEGATE_H
#define APPICONDELEGATE_H

#include <QStyledItemDelegate>
#include <QImage>
#include <QMap>
#include <functional>

class AppIconDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit AppIconDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    // アイコンパス取得用の関数を設定
    void setIconPathGetter(std::function<QString(const QString&)> getter);

    // キャッシュクリア
    void clearCache();
    void clearCacheFor(const QString &iconPath);

private:
    QImage loadIcon(const QString &appPath) const;
    QImage loadIconDirect(const QString &iconPath) const;

    mutable QMap<QString, QImage> m_imageCache;
    std::function<QString(const QString&)> m_iconPathGetter;
    QImage m_defaultIcon;
};

#endif // APPICONDELEGATE_H
