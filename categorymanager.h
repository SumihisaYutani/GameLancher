#ifndef CATEGORYMANAGER_H
#define CATEGORYMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

struct CategoryInfo {
    QString name;
    QColor color;
    QString icon;
    
    CategoryInfo() = default;
    CategoryInfo(const QString &n, const QColor &c = QColor(), const QString &i = QString())
        : name(n), color(c), icon(i) {}
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);
};

class CategoryManager : public QObject
{
    Q_OBJECT

public:
    explicit CategoryManager(QObject *parent = nullptr);
    
    // カテゴリ管理
    QStringList getCategories() const;
    QList<CategoryInfo> getCategoryInfoList() const;
    CategoryInfo getCategoryInfo(const QString &name) const;
    
    bool addCategory(const QString &name, const QColor &color = QColor(), const QString &icon = QString());
    bool removeCategory(const QString &name);
    bool updateCategory(const QString &name, const CategoryInfo &info);
    bool hasCategory(const QString &name) const;
    
    // カテゴリ色とアイコン
    QColor getCategoryColor(const QString &name) const;
    QString getCategoryIcon(const QString &name) const;
    void setCategoryColor(const QString &name, const QColor &color);
    void setCategoryIcon(const QString &name, const QString &icon);
    
    // デフォルトカテゴリ
    void initializeDefaultCategories();
    QStringList getDefaultCategories() const;
    
    // データの永続化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);

signals:
    void categoryAdded(const QString &name);
    void categoryRemoved(const QString &name);
    void categoryUpdated(const QString &name);

private:
    QMap<QString, CategoryInfo> m_categories;
    
    void setupDefaultCategories();
};

#endif // CATEGORYMANAGER_H