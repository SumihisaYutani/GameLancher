#include "applauncher.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>

AppLauncher::AppLauncher(QObject *parent)
    : QObject(parent)
    , m_lastExitCode(0)
{
}

AppLauncher::~AppLauncher()
{
    // 全ての実行中プロセスを終了
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        QProcess *process = it.value();
        if (process && process->state() != QProcess::NotRunning) {
            process->terminate();
            if (!process->waitForFinished(3000)) {
                process->kill();
            }
        }
        delete process;
    }
    m_processes.clear();
}

bool AppLauncher::launch(AppInfo &app)
{
    return launchWithArguments(app, QStringList());
}

bool AppLauncher::launchWithArguments(AppInfo &app, const QStringList &arguments)
{
    if (!canLaunch(app)) {
        m_lastError = "アプリケーションを起動できません: " + app.name;
        qWarning() << m_lastError << app.path;
        return false;
    }
    
    // 既存のプロセスがあるかチェック（複数起動を許可）
    if (m_processes.contains(app.id)) {
        QProcess *existingProcess = m_processes[app.id];
        if (existingProcess && existingProcess->state() != QProcess::NotRunning) {
            qDebug() << "App" << app.name << "is already running. Allowing multiple instances.";
            // 複数起動を許可する場合は、新しいプロセスIDを生成
        }
    }
    
    // 新しいプロセスを作成
    QProcess *process = createProcess(app.id);
    m_lastError.clear();
    
    // 作業ディレクトリの設定
    QString workingDir = m_workingDirectory;
    if (workingDir.isEmpty()) {
        workingDir = getApplicationDirectory(app.path);
    }
    process->setWorkingDirectory(workingDir);
    
    qDebug() << "Launching app:" << app.name << "at" << app.path;
    qDebug() << "Working directory:" << workingDir;
    qDebug() << "Arguments:" << arguments;
    
    try {
        // プロセス開始（非同期・独立実行）
        process->start(app.path, arguments);
        
        if (!process->waitForStarted(5000)) {
            m_lastError = "アプリケーションの起動に失敗しました: " + process->errorString();
            qWarning() << m_lastError;
            // プロセスを削除
            m_processes.remove(app.id);
            delete process;
            return false;
        }
        
        // 起動情報の更新
        app.updateLaunchInfo();
        emit launched(app.id);
        
        qDebug() << "Successfully launched:" << app.name << "(PID:" << process->processId() << ")";
        return true;
        
    } catch (const std::exception &e) {
        m_lastError = "起動中に例外が発生しました: " + QString::fromStdString(e.what());
        qCritical() << m_lastError;
        // プロセスを削除
        m_processes.remove(app.id);
        delete process;
        return false;
    }
}

bool AppLauncher::canLaunch(const AppInfo &app) const
{
    if (!app.isValid()) {
        return false;
    }
    
    QFileInfo fileInfo(app.path);
    if (!fileInfo.exists()) {
        qWarning() << "File does not exist:" << app.path;
        return false;
    }
    
    if (!fileInfo.isExecutable()) {
        qWarning() << "File is not executable:" << app.path;
        return false;
    }
    
    return true;
}

bool AppLauncher::isRunning() const
{
    // 実行中のプロセスがあるかチェック
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        QProcess *process = it.value();
        if (process && process->state() != QProcess::NotRunning) {
            return true;
        }
    }
    return false;
}

void AppLauncher::terminate()
{
    // 全ての実行中プロセスを終了
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        QProcess *process = it.value();
        if (process && process->state() != QProcess::NotRunning) {
            QString appId = it.key();
            qDebug() << "Terminating process for app:" << appId;
            process->terminate();
            
            if (!process->waitForFinished(5000)) {
                qWarning() << "Process" << appId << "did not terminate gracefully, killing...";
                process->kill();
                process->waitForFinished(3000);
            }
        }
    }
}

void AppLauncher::kill()
{
    // 全ての実行中プロセスを強制終了
    for (auto it = m_processes.begin(); it != m_processes.end(); ++it) {
        QProcess *process = it.value();
        if (process && process->state() != QProcess::NotRunning) {
            QString appId = it.key();
            qDebug() << "Killing process for app:" << appId;
            process->kill();
            process->waitForFinished(3000);
        }
    }
}

void AppLauncher::setWorkingDirectory(const QString &dir)
{
    m_workingDirectory = dir;
}

QString AppLauncher::getWorkingDirectory() const
{
    return m_workingDirectory;
}

QString AppLauncher::getLastError() const
{
    return m_lastError;
}

int AppLauncher::getExitCode() const
{
    return m_lastExitCode;
}

void AppLauncher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 送信者のプロセスを特定
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (!process) return;
    
    QString appId = process->property("appId").toString();
    m_lastExitCode = exitCode;
    
    qDebug() << "Process finished for app:" << appId
             << "Exit code:" << exitCode
             << "Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed")
             << "(PID:" << process->processId() << ")";
    
    if (exitStatus == QProcess::CrashExit) {
        m_lastError = "アプリケーションが異常終了しました (Exit Code: " + QString::number(exitCode) + ")";
        emit errorOccurred(appId, m_lastError);
    }
    
    emit finished(appId, exitCode);
    
    // プロセスをクリーンアップ
    m_processes.remove(appId);
    process->deleteLater();
}

void AppLauncher::onProcessError(QProcess::ProcessError error)
{
    // 送信者のプロセスを特定
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (!process) return;
    
    QString appId = process->property("appId").toString();
    m_lastError = formatErrorMessage(error);
    
    qWarning() << "Process error for app:" << appId << m_lastError;
    
    emit errorOccurred(appId, m_lastError);
    
    // エラーの場合もプロセスをクリーンアップ
    m_processes.remove(appId);
    process->deleteLater();
}

QString AppLauncher::getApplicationDirectory(const QString &appPath) const
{
    QFileInfo fileInfo(appPath);
    return fileInfo.absoluteDir().absolutePath();
}

QString AppLauncher::formatErrorMessage(QProcess::ProcessError error) const
{
    switch (error) {
    case QProcess::FailedToStart:
        return "アプリケーションの起動に失敗しました。ファイルが見つからないか、権限がありません。";
    case QProcess::Crashed:
        return "アプリケーションが起動後にクラッシュしました。";
    case QProcess::Timedout:
        return "アプリケーションの起動がタイムアウトしました。";
    case QProcess::WriteError:
        return "アプリケーションへの書き込みエラーが発生しました。";
    case QProcess::ReadError:
        return "アプリケーションからの読み込みエラーが発生しました。";
    case QProcess::UnknownError:
    default:
        return "不明なエラーが発生しました。";
    }
}

QProcess* AppLauncher::createProcess(const QString &appId)
{
    QProcess *process = new QProcess(this);
    
    // プロセスにappIdを関連付けるためのプロパティを設定
    process->setProperty("appId", appId);
    
    // シグナル接続
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AppLauncher::onProcessFinished);
    connect(process, &QProcess::errorOccurred,
            this, &AppLauncher::onProcessError);
    
    // プロセスをマップに追加
    m_processes[appId] = process;
    
    return process;
}