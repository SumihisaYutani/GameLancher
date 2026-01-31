#include "appicondelegate.h"
#include "applistmodel.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

AppIconDelegate::AppIconDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    // デフォルトアイコン（48x48のグレー四角）
    m_defaultIcon = QImage(48, 48, QImage::Format_ARGB32);
    m_defaultIcon.fill(QColor(200, 200, 200));
}

void AppIconDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    // 背景とセレクションの描画
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // 背景を描画
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    // アイコン列（0列目）のみアイコンを描画
    if (index.column() == 0) {
        // アイコンパスを直接取得
        QString iconPath = index.data(AppListModel::IconPathRole).toString();

        // アイコン画像を取得
        QImage icon = loadIconDirect(iconPath);

        // アイコン描画位置（48x48）
        QRect iconRect = opt.rect;
        iconRect.setWidth(48);
        iconRect.setHeight(48);
        iconRect.moveTop(opt.rect.top() + (opt.rect.height() - 48) / 2);
        iconRect.moveLeft(opt.rect.left() + 4);

        // PNG画像を直接描画（QPixmap/QIconを経由しない）
        painter->drawImage(iconRect, icon);

        // テキスト描画位置を調整
        QRect textRect = opt.rect;
        textRect.setLeft(iconRect.right() + 8);

        // テキストを描画
        QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(opt.state & QStyle::State_Selected ? opt.palette.highlightedText().color() : opt.palette.text().color());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    } else {
        // 他の列は通常描画
        QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(opt.state & QStyle::State_Selected ? opt.palette.highlightedText().color() : opt.palette.text().color());
        painter->drawText(opt.rect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}

QSize AppIconDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(200, 56);
}

void AppIconDelegate::setIconPathGetter(std::function<QString(const QString&)> getter)
{
    m_iconPathGetter = getter;
}

void AppIconDelegate::clearCache()
{
    m_imageCache.clear();
}

void AppIconDelegate::clearCacheFor(const QString &iconPath)
{
    m_imageCache.remove(iconPath);
}

QImage AppIconDelegate::loadIcon(const QString &appPath) const
{
    Q_UNUSED(appPath)
    return m_defaultIcon;
}

QImage AppIconDelegate::loadIconDirect(const QString &iconPath) const
{
    // 空のパスはデフォルト
    if (iconPath.isEmpty()) {
        return m_defaultIcon;
    }

    // パスを正規化
    QString normalizedPath = QDir::toNativeSeparators(iconPath);

    // キャッシュをチェック
    auto it = m_imageCache.constFind(normalizedPath);
    if (it != m_imageCache.constEnd()) {
        return *it;
    }

    QImage image;

    // PNG画像を直接読み込み
    if (QFileInfo::exists(normalizedPath)) {
        if (!image.load(normalizedPath)) {
            qDebug() << "Failed to load image:" << normalizedPath;
        }
    } else {
        qDebug() << "Icon file not found:" << normalizedPath;
    }

    // 読み込めなかった場合はデフォルト
    if (image.isNull()) {
        image = m_defaultIcon;
    } else {
        // 48x48にリサイズ
        image = image.scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    // キャッシュに保存
    m_imageCache.insert(normalizedPath, image);

    return image;
}
