#ifndef ICONEXTRACTOR_H
#define ICONEXTRACTOR_H

#include <QObject>
#include <QString>
#include <QIcon>
#include <QPixmap>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#endif

class IconExtractor : public QObject
{
    Q_OBJECT

public:
    explicit IconExtractor(QObject *parent = nullptr);
    ~IconExtractor();
    
    // アイコン抽出
    QIcon extractIcon(const QString &executablePath);
    QPixmap extractIconPixmap(const QString &executablePath, const QSize &size = QSize(32, 32));
    
    // アイコン保存
    bool saveIcon(const QIcon &icon, const QString &savePath);
    bool saveIconPixmap(const QPixmap &pixmap, const QString &savePath);
    
    // アイコンファイル生成
    QString generateIconPath(const QString &executablePath, const QString &iconDir = QString());
    bool extractAndSaveIcon(const QString &executablePath, const QString &savePath);
    
    // ユーティリティ
    bool hasIcon(const QString &executablePath) const;
    QSize getIconSize(const QString &iconPath) const;
    
    // 設定
    void setDefaultIconSize(const QSize &size);
    QSize getDefaultIconSize() const;
    
    void setIconCacheDir(const QString &dir);
    QString getIconCacheDir() const;

signals:
    void iconExtracted(const QString &executablePath, const QString &iconPath);
    void iconExtractionFailed(const QString &executablePath, const QString &error);

private:
    QSize m_defaultIconSize;
    QString m_iconCacheDir;
    
    void initializeCacheDirectory();
    QString getDefaultCacheDir() const;
    
#ifdef Q_OS_WIN
    QIcon extractWin32Icon(const QString &executablePath);
    HICON extractWin32IconHandle(const QString &executablePath, int iconSize = 32);
    QPixmap convertHIconToPixmap(HICON hIcon);
#endif
    
    QIcon getDefaultApplicationIcon() const;
    QString generateUniqueFileName(const QString &executablePath) const;
};

#endif // ICONEXTRACTOR_H