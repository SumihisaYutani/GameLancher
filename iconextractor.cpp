#include "iconextractor.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QApplication>
#include <QDebug>
#include <QImageWriter>
#include <QCryptographicHash>
#include <QStyle>

#ifdef Q_OS_WIN
// Windows specific includes for icon extraction
#endif

IconExtractor::IconExtractor(QObject *parent)
    : QObject(parent)
    , m_defaultIconSize(32, 32)
{
    m_iconCacheDir = getDefaultCacheDir();
    initializeCacheDirectory();
}

IconExtractor::~IconExtractor()
{
}

QIcon IconExtractor::extractIcon(const QString &executablePath)
{
    if (!QFileInfo::exists(executablePath)) {
        qWarning() << "Executable file does not exist:" << executablePath;
        emit iconExtractionFailed(executablePath, "ファイルが存在しません");
        return getDefaultApplicationIcon();
    }

#ifdef Q_OS_WIN
    QIcon icon = extractWin32Icon(executablePath);
    if (!icon.isNull()) {
        return icon;
    }
#endif
    
    // フォールバック: Qt標準のファイルアイコン
    QFileInfo fileInfo(executablePath);
    QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    
    if (fileIcon.isNull()) {
        fileIcon = getDefaultApplicationIcon();
    }
    
    qDebug() << "Using fallback icon for:" << executablePath;
    return fileIcon;
}

QPixmap IconExtractor::extractIconPixmap(const QString &executablePath, const QSize &size)
{
    QIcon icon = extractIcon(executablePath);
    return icon.pixmap(size.isValid() ? size : m_defaultIconSize);
}

bool IconExtractor::saveIcon(const QIcon &icon, const QString &savePath)
{
    if (icon.isNull()) {
        qWarning() << "Cannot save null icon to:" << savePath;
        return false;
    }
    
    QPixmap pixmap = icon.pixmap(m_defaultIconSize);
    return saveIconPixmap(pixmap, savePath);
}

bool IconExtractor::saveIconPixmap(const QPixmap &pixmap, const QString &savePath)
{
    if (pixmap.isNull()) {
        qWarning() << "Cannot save null pixmap to:" << savePath;
        return false;
    }
    
    QFileInfo fileInfo(savePath);
    QDir dir = fileInfo.absoluteDir();
    
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Cannot create directory for icon:" << dir.absolutePath();
        return false;
    }
    
    bool success = pixmap.save(savePath, "PNG");
    if (!success) {
        qWarning() << "Failed to save icon to:" << savePath;
    } else {
        qDebug() << "Icon saved successfully to:" << savePath;
    }
    
    return success;
}

QString IconExtractor::generateIconPath(const QString &executablePath, const QString &iconDir)
{
    QString baseDir = iconDir.isEmpty() ? m_iconCacheDir : iconDir;
    QString fileName = generateUniqueFileName(executablePath) + ".png";
    return QDir(baseDir).filePath(fileName);
}

bool IconExtractor::extractAndSaveIcon(const QString &executablePath, const QString &savePath)
{
    try {
        QIcon icon = extractIcon(executablePath);
        bool success = saveIcon(icon, savePath);
        
        if (success) {
            emit iconExtracted(executablePath, savePath);
        } else {
            emit iconExtractionFailed(executablePath, "アイコンの保存に失敗しました");
        }
        
        return success;
        
    } catch (const std::exception &e) {
        QString error = "アイコン抽出中に例外が発生しました: " + QString::fromStdString(e.what());
        qCritical() << error;
        emit iconExtractionFailed(executablePath, error);
        return false;
    }
}

bool IconExtractor::hasIcon(const QString &executablePath) const
{
    QIcon icon = const_cast<IconExtractor*>(this)->extractIcon(executablePath);
    return !icon.isNull();
}

QSize IconExtractor::getIconSize(const QString &iconPath) const
{
    QPixmap pixmap(iconPath);
    return pixmap.size();
}

void IconExtractor::setDefaultIconSize(const QSize &size)
{
    if (size.isValid()) {
        m_defaultIconSize = size;
    }
}

QSize IconExtractor::getDefaultIconSize() const
{
    return m_defaultIconSize;
}

void IconExtractor::setIconCacheDir(const QString &dir)
{
    m_iconCacheDir = dir;
    initializeCacheDirectory();
}

QString IconExtractor::getIconCacheDir() const
{
    return m_iconCacheDir;
}

void IconExtractor::initializeCacheDirectory()
{
    QDir dir(m_iconCacheDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Cannot create icon cache directory:" << m_iconCacheDir;
        // フォールバック
        m_iconCacheDir = QApplication::applicationDirPath() + "/icons/extracted";
        QDir(m_iconCacheDir).mkpath(".");
    }
    qDebug() << "Icon cache directory:" << m_iconCacheDir;
}

QString IconExtractor::getDefaultCacheDir() const
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QApplication::applicationDirPath() + "/cache";
    }
    return QDir(cacheDir).filePath("icons");
}

#ifdef Q_OS_WIN
QIcon IconExtractor::extractWin32Icon(const QString &executablePath)
{
    HICON hIcon = extractWin32IconHandle(executablePath, m_defaultIconSize.width());
    
    if (hIcon) {
        // Qt 6では QPixmap::fromWinHICON は削除されたため、代替実装を使用
        QPixmap pixmap = convertHIconToPixmap(hIcon);
        DestroyIcon(hIcon);
        
        if (!pixmap.isNull()) {
            qDebug() << "Successfully extracted Win32 icon from:" << executablePath;
            return QIcon(pixmap);
        }
    }
    
    qDebug() << "Failed to extract Win32 icon from:" << executablePath;
    return QIcon();
}

QPixmap IconExtractor::convertHIconToPixmap(HICON hIcon)
{
    if (!hIcon) {
        return QPixmap();
    }
    
    // アイコン情報を取得
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) {
        return QPixmap();
    }
    
    // ビットマップ情報を取得
    BITMAP bitmap;
    if (!GetObject(iconInfo.hbmColor, sizeof(bitmap), &bitmap)) {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return QPixmap();
    }
    
    int width = bitmap.bmWidth;
    int height = bitmap.bmHeight;
    
    // デバイスコンテキストの作成
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    
    // ビットマップの作成
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // トップダウン
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    QPixmap result;
    
    if (hbm && bits) {
        HBITMAP oldBm = (HBITMAP)SelectObject(memDC, hbm);
        
        // アイコンを描画
        DrawIconEx(memDC, 0, 0, hIcon, width, height, 0, nullptr, DI_NORMAL);
        
        // QPixmapに変換
        QImage image((uchar*)bits, width, height, QImage::Format_ARGB32);
        result = QPixmap::fromImage(image.rgbSwapped());
        
        SelectObject(memDC, oldBm);
        DeleteObject(hbm);
    }
    
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    
    return result;
}

HICON IconExtractor::extractWin32IconHandle(const QString &executablePath, int iconSize)
{
    std::wstring wPath = executablePath.toStdWString();
    
    // 大きいアイコンと小さいアイコンを取得
    HICON hIconLarge = nullptr;
    HICON hIconSmall = nullptr;
    
    // ExtractIconExを使用してアイコンを抽出
    UINT iconCount = ExtractIconExW(wPath.c_str(), 0, &hIconLarge, &hIconSmall, 1);
    
    if (iconCount > 0) {
        // サイズに応じてアイコンを選択
        HICON hIcon = (iconSize > 16) ? hIconLarge : hIconSmall;
        
        // 使わない方のアイコンを解放
        if (hIconLarge && hIcon != hIconLarge) {
            DestroyIcon(hIconLarge);
        }
        if (hIconSmall && hIcon != hIconSmall) {
            DestroyIcon(hIconSmall);
        }
        
        return hIcon;
    }
    
    // フォールバック: SHGetFileInfoを使用
    SHFILEINFOW fileInfo;
    ZeroMemory(&fileInfo, sizeof(fileInfo));
    
    DWORD flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
    if (iconSize <= 16) {
        flags |= SHGFI_SMALLICON;
    } else {
        flags |= SHGFI_LARGEICON;
    }
    
    DWORD_PTR result = SHGetFileInfoW(wPath.c_str(), FILE_ATTRIBUTE_NORMAL, &fileInfo, 
                                      sizeof(fileInfo), flags);
    
    if (result && fileInfo.hIcon) {
        return fileInfo.hIcon;
    }
    
    return nullptr;
}
#endif

QIcon IconExtractor::getDefaultApplicationIcon() const
{
    // Qt標準のアプリケーションアイコンを返す
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    
    if (icon.isNull()) {
        // 最後の手段：空のアイコンではなく何かしらのアイコンを作成
        QPixmap pixmap(m_defaultIconSize);
        pixmap.fill(Qt::lightGray);
        icon = QIcon(pixmap);
    }
    
    return icon;
}

QString IconExtractor::generateUniqueFileName(const QString &executablePath) const
{
    // ファイルパスのハッシュ値を使用してユニークなファイル名を生成
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(executablePath.toUtf8());
    QString hashString = QString(hash.result().toHex());
    
    QFileInfo fileInfo(executablePath);
    QString baseName = fileInfo.completeBaseName();
    
    // 日本語文字などを含む可能性があるため、ハッシュを使用
    return QString("%1_%2").arg(baseName.left(20), hashString.left(8));
}