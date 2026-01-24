#include "appwidget.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QAction>
#include <QFontMetrics>

const QSize AppWidget::DEFAULT_ICON_SIZE(48, 48);
const QSize AppWidget::DEFAULT_WIDGET_SIZE(120, 120);
const int AppWidget::MARGIN = 8;
const int AppWidget::SPACING = 4;

AppWidget::AppWidget(const AppInfo &app, QWidget *parent)
    : QWidget(parent)
    , m_appInfo(app)
    , m_iconSize(DEFAULT_ICON_SIZE)
    , m_fixedSize(DEFAULT_WIDGET_SIZE)
    , m_selected(false)
    , m_hovered(false)
{
    setupUI();
    setupContextMenu();
    updateAppInfo(app);
    
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
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
    nameFont.setPointSize(9);
    nameFont.setBold(false);
    m_nameLabel->setFont(nameFont);
    
    m_layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_nameLabel);
    
    updateStyleSheet();
}

void AppWidget::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    QAction *editAction = m_contextMenu->addAction("編集(&E)");
    editAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    connect(editAction, &QAction::triggered, this, &AppWidget::onEditAction);
    
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
        emit clicked(m_appInfo.id);
    }
    QWidget::mousePressEvent(event);
}

void AppWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(m_appInfo.id);
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AppWidget::contextMenuEvent(QContextMenuEvent *event)
{
    emit rightClicked(m_appInfo.id, event->globalPos());
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
    m_hovered = true;
    updateStyleSheet();
    update();
    QWidget::enterEvent(event);
}

void AppWidget::leaveEvent(QEvent *event)
{
    m_hovered = false;
    updateStyleSheet();
    update();
    QWidget::leaveEvent(event);
}

void AppWidget::onEditAction()
{
    emit editRequested(m_appInfo.id);
}

void AppWidget::onRemoveAction()
{
    emit removeRequested(m_appInfo.id);
}

void AppWidget::onPropertiesAction()
{
    emit propertiesRequested(m_appInfo.id);
}

void AppWidget::updateIcon()
{
    QPixmap iconPixmap;
    
    // アイコンファイルが存在する場合
    if (!m_appInfo.iconPath.isEmpty() && QFileInfo::exists(m_appInfo.iconPath)) {
        iconPixmap = QPixmap(m_appInfo.iconPath);
    }
    
    // アイコンが取得できない場合はデフォルトアイコン
    if (iconPixmap.isNull()) {
        QIcon defaultIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        iconPixmap = defaultIcon.pixmap(m_iconSize);
    }
    
    // アイコンサイズに合わせてスケール
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
}

void AppWidget::updateLabels()
{
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
    
    setStyleSheet(styleSheet);
}