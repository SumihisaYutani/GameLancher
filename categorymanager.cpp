#include "categorymanager.h"
#include <QDebug>

QJsonObject CategoryInfo::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["color"] = color.name();
    obj["icon"] = icon;
    return obj;
}

void CategoryInfo::fromJson(const QJsonObject &json)
{
    name = json["name"].toString();
    color = QColor(json["color"].toString());
    icon = json["icon"].toString();
}

CategoryManager::CategoryManager(QObject *parent)
    : QObject(parent)
{
    setupDefaultCategories();
}

QStringList CategoryManager::getCategories() const
{
    QStringList categories;
    categories << "すべて"; // 特別なカテゴリ
    for (auto it = m_categories.begin(); it != m_categories.end(); ++it) {
        categories << it.key();
    }
    return categories;
}

QList<CategoryInfo> CategoryManager::getCategoryInfoList() const
{
    return m_categories.values();
}

CategoryInfo CategoryManager::getCategoryInfo(const QString &name) const
{
    return m_categories.value(name, CategoryInfo());
}

bool CategoryManager::addCategory(const QString &name, const QColor &color, const QString &icon)
{
    if (name.isEmpty() || name == "すべて" || hasCategory(name)) {
        return false;
    }
    
    CategoryInfo info(name, color.isValid() ? color : QColor("#808080"), icon);
    m_categories[name] = info;
    
    emit categoryAdded(name);
    qDebug() << "Category added:" << name;
    return true;
}

bool CategoryManager::removeCategory(const QString &name)
{
    if (!hasCategory(name) || name == "その他") {
        return false; // "その他"は削除不可
    }
    
    m_categories.remove(name);
    emit categoryRemoved(name);
    qDebug() << "Category removed:" << name;
    return true;
}

bool CategoryManager::updateCategory(const QString &name, const CategoryInfo &info)
{
    if (!hasCategory(name)) {
        return false;
    }
    
    m_categories[name] = info;
    emit categoryUpdated(name);
    qDebug() << "Category updated:" << name;
    return true;
}

bool CategoryManager::hasCategory(const QString &name) const
{
    return m_categories.contains(name);
}

QColor CategoryManager::getCategoryColor(const QString &name) const
{
    return m_categories.value(name, CategoryInfo()).color;
}

QString CategoryManager::getCategoryIcon(const QString &name) const
{
    return m_categories.value(name, CategoryInfo()).icon;
}

void CategoryManager::setCategoryColor(const QString &name, const QColor &color)
{
    if (hasCategory(name)) {
        m_categories[name].color = color;
        emit categoryUpdated(name);
    }
}

void CategoryManager::setCategoryIcon(const QString &name, const QString &icon)
{
    if (hasCategory(name)) {
        m_categories[name].icon = icon;
        emit categoryUpdated(name);
    }
}

void CategoryManager::initializeDefaultCategories()
{
    setupDefaultCategories();
}

QStringList CategoryManager::getDefaultCategories() const
{
    return QStringList() << "ゲーム" << "ビジネス" << "ツール" << "メディア" << "開発" << "その他";
}

QJsonObject CategoryManager::toJson() const
{
    QJsonArray categoriesArray;
    for (auto it = m_categories.begin(); it != m_categories.end(); ++it) {
        categoriesArray.append(it.value().toJson());
    }
    
    QJsonObject obj;
    obj["categories"] = categoriesArray;
    return obj;
}

void CategoryManager::fromJson(const QJsonObject &json)
{
    m_categories.clear();
    
    QJsonArray categoriesArray = json["categories"].toArray();
    for (const auto &value : categoriesArray) {
        if (value.isObject()) {
            CategoryInfo info;
            info.fromJson(value.toObject());
            if (!info.name.isEmpty()) {
                m_categories[info.name] = info;
            }
        }
    }
    
    // デフォルトカテゴリが不足している場合は追加
    setupDefaultCategories();
    
    qDebug() << "Loaded" << m_categories.size() << "categories";
}

void CategoryManager::setupDefaultCategories()
{
    // デフォルトカテゴリの定義
    QList<CategoryInfo> defaults = {
        CategoryInfo("ゲーム", QColor("#FF6B6B"), "🎮"),
        CategoryInfo("ビジネス", QColor("#4ECDC4"), "💼"),
        CategoryInfo("ツール", QColor("#45B7D1"), "🛠️"),
        CategoryInfo("メディア", QColor("#96CEB4"), "🎵"),
        CategoryInfo("開発", QColor("#FECA57"), "💻"),
        CategoryInfo("その他", QColor("#95A5A6"), "📁")
    };
    
    for (const auto &defaultCategory : defaults) {
        if (!hasCategory(defaultCategory.name)) {
            m_categories[defaultCategory.name] = defaultCategory;
        }
    }
}