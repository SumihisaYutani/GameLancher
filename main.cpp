#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTime>

// ログファイルに出力するためのハンドラー
void messageOutput(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static QFile debugFile;
    static QTextStream debugStream;
    static bool initialized = false;

    if (!initialized) {
        // アプリケーション実行ディレクトリにログファイルを作成
        QString logFileName = QApplication::applicationDirPath() + "/debug_log.txt";
        debugFile.setFileName(logFileName);
        
        if (debugFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            debugStream.setDevice(&debugFile);
        }
        initialized = true;
        
        // ログ開始マーク
        debugStream << "\n=== LOG SESSION START " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===" << Qt::endl;
    }

    QString txt;
    switch (type) {
    case QtDebugMsg:    txt = QString("Debug: %1").arg(msg); break;
    case QtWarningMsg:  txt = QString("Warning: %1").arg(msg); break;
    case QtCriticalMsg: txt = QString("Critical: %1").arg(msg); break;
    case QtFatalMsg:    txt = QString("Fatal: %1").arg(msg); break;
    case QtInfoMsg:     txt = QString("Info: %1").arg(msg); break;
    }

    // タイムスタンプ付きでログ出力
    QString timeStamped = QString("[%1] %2").arg(QTime::currentTime().toString("hh:mm:ss.zzz")).arg(txt);
    
    // ファイルに書き込み
    if (debugStream.device()) {
        debugStream << timeStamped << Qt::endl;
        debugStream.flush();
    }
    
    // コンソールにも出力
    QTextStream(stderr) << timeStamped << Qt::endl;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // カスタムメッセージハンドラーを設定
    qInstallMessageHandler(messageOutput);
    
    qDebug() << "=== GameLancher Application Starting ===";
    qDebug() << "Application dir:" << QApplication::applicationDirPath();
    qDebug() << "Working directory:" << QDir::currentPath();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "GameLancher_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    
    qDebug() << "Creating MainWindow...";
    MainWindow w;
    qDebug() << "Showing MainWindow...";
    w.show();
    
    qDebug() << "Starting event loop...";
    int result = a.exec();
    qDebug() << "=== GameLancher Application Ending ===";
    return result;
}
