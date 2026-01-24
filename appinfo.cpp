#include "appinfo.h"
#include <QFileInfo>
#include <QDir>

AppInfo::AppInfo()
    : launchCount(0)
    , createdAt(QDateTime::currentDateTime())
    , category("その他")
{
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

AppInfo::AppInfo(const QString &name, const QString &path)
    : name(name)
    , path(path)
    , launchCount(0)
    , createdAt(QDateTime::currentDateTime())
    , category("その他")
{
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject AppInfo::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["path"] = path;
    obj["iconPath"] = iconPath;
    obj["lastLaunch"] = lastLaunch.toString(Qt::ISODate);
    obj["launchCount"] = launchCount;
    obj["description"] = description;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["category"] = category;
    return obj;
}

void AppInfo::fromJson(const QJsonObject &json)
{
    id = json["id"].toString();
    name = json["name"].toString();
    path = json["path"].toString();
    iconPath = json["iconPath"].toString();
    launchCount = json["launchCount"].toInt();
    description = json["description"].toString();
    category = json["category"].toString();
    
    // カテゴリが空の場合はデフォルト値を設定
    if (category.isEmpty()) {
        category = "その他";
    }
    
    // 日時の読み込み（空文字列チェック）
    QString lastLaunchStr = json["lastLaunch"].toString();
    if (!lastLaunchStr.isEmpty()) {
        lastLaunch = QDateTime::fromString(lastLaunchStr, Qt::ISODate);
    }
    
    QString createdAtStr = json["createdAt"].toString();
    if (!createdAtStr.isEmpty()) {
        createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
    } else {
        createdAt = QDateTime::currentDateTime();
    }
    
    // IDが空の場合は新しく生成
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
}

bool AppInfo::isValid() const
{
    return !name.isEmpty() && !path.isEmpty() && fileExists();
}

void AppInfo::updateLaunchInfo()
{
    lastLaunch = QDateTime::currentDateTime();
    launchCount++;
}

bool AppInfo::fileExists() const
{
    return QFileInfo::exists(path) && QFileInfo(path).isExecutable();
}