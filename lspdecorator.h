#ifndef LSPDECORATOR_H
#define LSPDECORATOR_H

#include <QObject>
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextEdit>
#include <QJsonArray>

struct LSPDiagnosticDecoration {
    int start;
    int end;
    int severity;
    QString message;
};

class LspDecorator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QJsonArray diagnostics READ diagnostics WRITE setDiagnostics NOTIFY diagnosticsChanged)

public:
    explicit LspDecorator(QObject *parent = nullptr);
    ~LspDecorator();

    QQuickTextDocument* document() const;
    void setDocument(QQuickTextDocument *doc);

    QJsonArray diagnostics() const;
    void setDiagnostics(const QJsonArray &diagnostics);

signals:
    void documentChanged();
    void diagnosticsChanged();

private:
    void applyDecorations();
    void clearDecorations();

    QQuickTextDocument *m_document = nullptr;
    QTextDocument *m_textDocument = nullptr;
    QList<LSPDiagnosticDecoration> m_decorations;
};

#endif // LSPDECORATOR_H
