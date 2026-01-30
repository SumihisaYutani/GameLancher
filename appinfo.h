#ifndef APPINFO_H
#define APPINFO_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUuid>

class AppInfo
{
public:
    AppInfo();
    AppInfo(const QString &name, const QString &path);
    
    // プロパティ
    QString id;             // 一意識別子
    QString name;           // アプリケーション名
    QString path;           // 実行ファイルパス
    QString iconPath;       // アイコンファイルパス
    QDateTime lastLaunch;   // 最終起動時刻
    int launchCount;        // 起動回数
    QString description;    // 説明（任意）
    QDateTime createdAt;    // 作成日時
    QString category;       // カテゴリ名
    
    // JSON変換
    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);
    
    // 有効性チェック
    bool isValid() const;              // 高速版（ファイルチェックなし）
    bool isValidWithFileCheck() const; // ファイル存在確認付き
    
    // 起動情報の更新
    void updateLaunchInfo();
    
    // ファイル存在確認
    bool fileExists() const;
};

#endif // APPINFO_H