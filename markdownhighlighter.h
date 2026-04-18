#ifndef MARKDOWNHIGHLIGHTER_H
#define MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QHash>
#include <QJsonArray>

// Include library definitions
#include "highlighter/languagedata.h"
#include "highlighter/qsourcehighliterthemes.h"

struct LSPDiagnosticRange {
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    int severity;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    QString message;
};

class MarkdownHighlighter : public QSyntaxHighlighter
{
Q_OBJECT
Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument NOTIFY documentChanged)

public:
    explicit MarkdownHighlighter(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);

    Q_INVOKABLE void setDiagnostics(const QJsonArray &diagnostics);
    Q_INVOKABLE void clearDiagnostics();

signals:
    void documentChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    QQuickTextDocument *m_quickDocument;

    // --- Markdown Rules ---
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> markdownRules;
    QTextCharFormat headerFormat;
    QTextCharFormat listFormat;
    QTextCharFormat codeBlockFormat;

    // --- Code Highlighting Logic (Adapted from QSourceHighliter) ---
    void initCodeFormats(bool isDarkMode);

    void highlightSyntax(const QString &text);
    int highlightNumericLiterals(const QString &text, int i);
    int highlightStringLiterals(const QChar strType, const QString &text, int i);

    // Helpers
    QSourceHighlite::QSourceHighliter::Language getLanguageFromFence(const QString &text);

    // Data structures from QSourceHighlite
    QHash<QSourceHighlite::QSourceHighliter::Token, QTextCharFormat> _codeFormats;

    // Language String map
    QHash<QString, QSourceHighlite::QSourceHighliter::Language> _langStringToEnum;

    // --- LSP Diagnostics ---
    QList<LSPDiagnosticRange> m_diagnostics;
    void applyDiagnostics(const QString &text, int blockNumber, int blockStart);

    // Helpers for the library logic
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    static inline QStringView strMidRef(const QString& str, qsizetype position, qsizetype n = -1) { return QStringView(str).mid(position, n); }
#else
    static inline QStringRef strMidRef(const QString& str, int position, int n = -1) { return str.midRef(position, n); }
#endif
};

#endif // MARKDOWNHIGHLIGHTER_H
