#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtWebEngineQuick>
#include "filecontroller.h"
#include "lspclient.h"
#include "markdownhighlighter.h" 

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    // Important: Initialize WebEngine before QGuiApp
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);

    // Register the Highlighter Class
    qmlRegisterType<MarkdownHighlighter>("QHugo", 1, 0, "MarkdownHighlighter");
    
    // Register the LSP Client
    qmlRegisterType<LspClient>("QHugo", 1, 0, "LspClient");

    QQmlApplicationEngine engine;

    FileController fileController;
    engine.rootContext()->setContextProperty("FileController", &fileController);

    // Using _s instead of _qs to fix the deprecation warning
    const QUrl url(u"qrc:/QHugo/qml/Main.qml"_s);
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
