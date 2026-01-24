#ifndef APPLAUNCHER_H
#define APPLAUNCHER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include "appinfo.h"

class AppLauncher : public QObject
{
    Q_OBJECT

public:
    explicit AppLauncher(QObject *parent = nullptr);
    ~AppLauncher();
    
    // 起動機能
    bool launch(AppInfo &app);
    bool launchWithArguments(AppInfo &app, const QStringList &arguments);
    
    // 起動可能性チェック
    bool canLaunch(const AppInfo &app) const;
    
    // プロセス管理
    bool isRunning() const;
    void terminate();
    void kill();
    
    // 起動オプション
    void setWorkingDirectory(const QString &dir);
    QString getWorkingDirectory() const;
    
    // 起動情報
    QString getLastError() const;
    int getExitCode() const;

signals:
    void launched(const QString &appId);
    void finished(const QString &appId, int exitCode);
    void errorOccurred(const QString &appId, const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    QString m_currentAppId;
    QString m_workingDirectory;
    QString m_lastError;
    int m_lastExitCode;
    
    QString getApplicationDirectory(const QString &appPath) const;
    QString formatErrorMessage(QProcess::ProcessError error) const;
};

#endif // APPLAUNCHER_H