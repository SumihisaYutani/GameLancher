#include "applauncher.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>

AppLauncher::AppLauncher(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_lastExitCode(0)
{
}

AppLauncher::~AppLauncher()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        delete m_process;
    }
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
    
    // 既存のプロセスがあれば終了待ち
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qWarning() << "Previous process still running, terminating...";
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    
    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppLauncher::onProcessFinished);
        connect(m_process, &QProcess::errorOccurred,
                this, &AppLauncher::onProcessError);
    }
    
    m_currentAppId = app.id;
    m_lastError.clear();
    
    // 作業ディレクトリの設定
    QString workingDir = m_workingDirectory;
    if (workingDir.isEmpty()) {
        workingDir = getApplicationDirectory(app.path);
    }
    m_process->setWorkingDirectory(workingDir);
    
    qDebug() << "Launching app:" << app.name << "at" << app.path;
    qDebug() << "Working directory:" << workingDir;
    qDebug() << "Arguments:" << arguments;
    
    try {
        // プロセス開始
        m_process->start(app.path, arguments);
        
        if (!m_process->waitForStarted(5000)) {
            m_lastError = "アプリケーションの起動に失敗しました: " + m_process->errorString();
            qWarning() << m_lastError;
            return false;
        }
        
        // 起動情報の更新
        app.updateLaunchInfo();
        emit launched(app.id);
        
        qDebug() << "Successfully launched:" << app.name;
        return true;
        
    } catch (const std::exception &e) {
        m_lastError = "起動中に例外が発生しました: " + QString::fromStdString(e.what());
        qCritical() << m_lastError;
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
    return m_process && m_process->state() != QProcess::NotRunning;
}

void AppLauncher::terminate()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "Terminating process for app:" << m_currentAppId;
        m_process->terminate();
        
        if (!m_process->waitForFinished(5000)) {
            qWarning() << "Process did not terminate gracefully, killing...";
            m_process->kill();
            m_process->waitForFinished(3000);
        }
    }
}

void AppLauncher::kill()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "Killing process for app:" << m_currentAppId;
        m_process->kill();
        m_process->waitForFinished(3000);
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
    m_lastExitCode = exitCode;
    
    qDebug() << "Process finished for app:" << m_currentAppId
             << "Exit code:" << exitCode
             << "Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");
    
    if (exitStatus == QProcess::CrashExit) {
        m_lastError = "アプリケーションが異常終了しました (Exit Code: " + QString::number(exitCode) + ")";
        emit errorOccurred(m_currentAppId, m_lastError);
    }
    
    emit finished(m_currentAppId, exitCode);
    
    // リセット
    m_currentAppId.clear();
}

void AppLauncher::onProcessError(QProcess::ProcessError error)
{
    m_lastError = formatErrorMessage(error);
    qWarning() << "Process error for app:" << m_currentAppId << m_lastError;
    
    emit errorOccurred(m_currentAppId, m_lastError);
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