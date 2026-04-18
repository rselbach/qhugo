#include "markdownhighlighter.h"
#include <QTextDocument>
#include <QTextLayout>
#include <QGuiApplication>
#include <QStyleHints>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

using namespace QSourceHighlite;

MarkdownHighlighter::MarkdownHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent), m_quickDocument(nullptr)
{
    // --- Determine Mode ---
    bool isDarkMode = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

    // --- 1. Initialize Markdown Rules ---
    headerFormat.setForeground(isDarkMode ? QColor("#61afef") : Qt::blue); // Light Blue in Dark Mode
    headerFormat.setFontWeight(QFont::Bold);
    
    HighlightingRule rule;
    rule.pattern = QRegularExpression("^(#{1,6})\\s.*");
    rule.format = headerFormat;
    markdownRules.append(rule);

    listFormat.setForeground(isDarkMode ? QColor("#e06c75") : Qt::red); // Soft Red in Dark Mode
    rule.pattern = QRegularExpression("^\\s*([*|-])\\s");
    rule.format = listFormat;
    markdownRules.append(rule);

    // --- 2. Initialize Code Highlighting Data ---
    initCodeFormats(isDarkMode);

    // Copy the base code block style for the fence lines (```)
    codeBlockFormat = _codeFormats[QSourceHighliter::Token::CodeBlock];
    // Make the fence lines slightly dimmer than the code text
    codeBlockFormat.setForeground(isDarkMode ? QColor("#666") : QColor("#999"));

    // Map strings "go", "cpp" to Enums
    _langStringToEnum["c"] = QSourceHighliter::CodeC;
    _langStringToEnum["cpp"] = QSourceHighliter::CodeCpp;
    _langStringToEnum["c++"] = QSourceHighliter::CodeCpp;
    _langStringToEnum["bash"] = QSourceHighliter::CodeBash;
    _langStringToEnum["sh"] = QSourceHighliter::CodeBash;
    _langStringToEnum["go"] = QSourceHighliter::CodeGo;
    _langStringToEnum["golang"] = QSourceHighliter::CodeGo;
    _langStringToEnum["js"] = QSourceHighliter::CodeJs;
    _langStringToEnum["javascript"] = QSourceHighliter::CodeJs;
    _langStringToEnum["json"] = QSourceHighliter::CodeJSON;
    _langStringToEnum["php"] = QSourceHighliter::CodePHP;
    _langStringToEnum["python"] = QSourceHighliter::CodePython;
    _langStringToEnum["py"] = QSourceHighliter::CodePython;
    _langStringToEnum["qml"] = QSourceHighliter::CodeQML;
    _langStringToEnum["rust"] = QSourceHighliter::CodeRust;
    _langStringToEnum["rs"] = QSourceHighliter::CodeRust;
    _langStringToEnum["sql"] = QSourceHighliter::CodeSQL;
    _langStringToEnum["xml"] = QSourceHighliter::CodeXML;
    _langStringToEnum["html"] = QSourceHighliter::CodeXML;
    _langStringToEnum["yaml"] = QSourceHighliter::CodeYAML;
    _langStringToEnum["yml"] = QSourceHighliter::CodeYAML;
    _langStringToEnum["cmake"] = QSourceHighliter::CodeCMake;
    _langStringToEnum["make"] = QSourceHighliter::CodeMake;
    _langStringToEnum["asm"] = QSourceHighliter::CodeAsm;
    _langStringToEnum["lua"] = QSourceHighliter::CodeLua;
}

void MarkdownHighlighter::initCodeFormats(bool isDarkMode) {
    QTextCharFormat fmt;
    fmt.setFontFamilies(QStringList("Courier New"));

    if (isDarkMode) {
        // --- DARK THEME (Dracula-inspired) ---
        QColor bg("#282a36");
        
        // Base Block (MUST set foreground to override QML default)
        fmt.setBackground(bg);
        fmt.setForeground(QColor("#f8f8f2")); // Off-white text
        _codeFormats[QSourceHighliter::Token::CodeBlock] = fmt;

        fmt.setForeground(QColor("#ff79c6")); // Keyword: Pink
        fmt.setFontWeight(QFont::Bold);
        _codeFormats[QSourceHighliter::Token::CodeKeyWord] = fmt;
        fmt.setFontWeight(QFont::Normal);

        fmt.setForeground(QColor("#f1fa8c")); // String: Yellow
        _codeFormats[QSourceHighliter::Token::CodeString] = fmt;

        fmt.setForeground(QColor("#6272a4")); // Comment: Blue-Gray
        fmt.setFontItalic(true);
        _codeFormats[QSourceHighliter::Token::CodeComment] = fmt;
        fmt.setFontItalic(false);

        fmt.setForeground(QColor("#8be9fd")); // Type: Cyan
        _codeFormats[QSourceHighliter::Token::CodeType] = fmt;

        fmt.setForeground(QColor("#50fa7b")); // Builtin: Green
        _codeFormats[QSourceHighliter::Token::CodeBuiltIn] = fmt;

        fmt.setForeground(QColor("#bd93f9")); // Number: Purple
        _codeFormats[QSourceHighliter::Token::CodeNumLiteral] = fmt;

        fmt.setForeground(QColor("#ffb86c")); // Other: Orange
        _codeFormats[QSourceHighliter::Token::CodeOther] = fmt;

    } else {
        // --- LIGHT THEME (GitHub-inspired) ---
        QColor bg("#f0f0f0");

        // Base Block
        fmt.setBackground(bg);
        fmt.setForeground(QColor("#24292e")); // Dark text
        _codeFormats[QSourceHighliter::Token::CodeBlock] = fmt; 
        
        fmt.setForeground(QColor("#d73a49")); // Keyword: Red
        fmt.setFontWeight(QFont::Bold);
        _codeFormats[QSourceHighliter::Token::CodeKeyWord] = fmt;
        fmt.setFontWeight(QFont::Normal);

        fmt.setForeground(QColor("#032f62")); // String: Dark Blue
        _codeFormats[QSourceHighliter::Token::CodeString] = fmt;

        fmt.setForeground(QColor("#6a737d")); // Comment: Gray
        fmt.setFontItalic(true);
        _codeFormats[QSourceHighliter::Token::CodeComment] = fmt;
        fmt.setFontItalic(false);

        fmt.setForeground(QColor("#005cc5")); // Type: Blue
        _codeFormats[QSourceHighliter::Token::CodeType] = fmt;

        fmt.setForeground(QColor("#22863a")); // Builtin: Green
        _codeFormats[QSourceHighliter::Token::CodeBuiltIn] = fmt;

        fmt.setForeground(QColor("#005cc5")); // Number: Blue
        _codeFormats[QSourceHighliter::Token::CodeNumLiteral] = fmt;

        fmt.setForeground(QColor("#e36209")); // Other: Orange
        _codeFormats[QSourceHighliter::Token::CodeOther] = fmt;
    }
}

QSourceHighliter::Language MarkdownHighlighter::getLanguageFromFence(const QString &text) {
    QString clean = text.trimmed();
    if (!clean.startsWith("```")) return QSourceHighliter::CodeC;
    
    clean = clean.mid(3).trimmed().toLower();
    if (clean.isEmpty()) return QSourceHighliter::CodeC;
    
    return _langStringToEnum.value(clean, QSourceHighliter::CodeC);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    // --- STATE MANAGEMENT ---
    // State -1: Normal Markdown
    // State > 0: Inside Code Block (State value is the Language Enum)
    
    int currentState = previousBlockState();
    bool isFenceStart = text.trimmed().startsWith("```");
    
    if (isFenceStart) {
        if (currentState == -1) {
            // Opening Fence
            auto lang = getLanguageFromFence(text);
            setCurrentBlockState(lang);
            setFormat(0, text.length(), codeBlockFormat);
            applyDiagnostics(text, currentBlock().blockNumber(), currentBlock().position());
            return;
        } else {
            // Closing Fence
            setCurrentBlockState(-1);
            setFormat(0, text.length(), codeBlockFormat);
            applyDiagnostics(text, currentBlock().blockNumber(), currentBlock().position());
            return;
        }
    }

    if (currentState != -1) {
        // --- INSIDE CODE BLOCK ---
        setCurrentBlockState(currentState);

        // 1. Apply Base Format (Background + Base Text Color)
        // This is crucial. We must apply the base format to the WHOLE string first.
        // This ensures whitespace and non-highlighted tokens get the correct background
        // AND the correct foreground (overriding QML defaults).
        setFormat(0, text.length(), _codeFormats[QSourceHighliter::Token::CodeBlock]);

        // 2. Apply Syntax Highlighting (will overlay specific tokens)
        highlightSyntax(text);

        // 3. Apply LSP diagnostics (squiggly underlines)
        applyDiagnostics(text, currentBlock().blockNumber(), currentBlock().position());
        return;
    }

    // --- NORMAL MARKDOWN ---
    setCurrentBlockState(-1);

    for (const HighlightingRule &rule : markdownRules) {
        QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Apply LSP diagnostics (squiggly underlines) after syntax highlighting
    applyDiagnostics(text, currentBlock().blockNumber(), currentBlock().position());
}

// =============================================================================
// LOGIC PORTED FROM QSourceHighliter.cpp
// =============================================================================

void MarkdownHighlighter::highlightSyntax(const QString &text)
{
    if (text.isEmpty()) return;
    const auto textLen = text.length();

    QChar comment;
    bool isCSS = false;
    bool isYAML = false;
    bool isMake = false;
    bool isAsm = false;
    bool isSQL = false;

    QSourceHighliter::Language lang = (QSourceHighliter::Language)currentBlockState();

    LanguageData keywords{}, others{}, types{}, builtin{}, literals{};

    switch (lang) {
        case QSourceHighliter::CodeLua :
        case QSourceHighliter::CodeLuaComment :
            loadLuaData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeCpp :
        case QSourceHighliter::CodeCppComment :
            loadCppData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeJs :
        case QSourceHighliter::CodeJsComment :
            loadJSData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeC :
        case QSourceHighliter::CodeCComment :
            loadCppData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeBash :
            loadShellData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        case QSourceHighliter::CodePHP :
        case QSourceHighliter::CodePHPComment :
            loadPHPData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeQML :
        case QSourceHighliter::CodeQMLComment :
            loadQMLData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodePython :
            loadPythonData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        case QSourceHighliter::CodeRust :
        case QSourceHighliter::CodeRustComment :
            loadRustData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeJava :
        case QSourceHighliter::CodeJavaComment :
            loadJavaData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeCSharp :
        case QSourceHighliter::CodeCSharpComment :
            loadCSharpData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeGo :
        case QSourceHighliter::CodeGoComment :
            loadGoData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeV :
        case QSourceHighliter::CodeVComment :
            loadVData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeSQL :
            isSQL = true;
            loadSQLData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeJSON :
            loadJSONData(types, keywords, builtin, literals, others);
            break;
        case QSourceHighliter::CodeYAML:
            isYAML = true;
            loadYAMLData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        case QSourceHighliter::CodeCMake:
            loadCMakeData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        case QSourceHighliter::CodeMake:
            isMake = true;
            loadMakeData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        case QSourceHighliter::CodeAsm:
            isAsm = true;
            loadAsmData(types, keywords, builtin, literals, others);
            comment = QLatin1Char('#');
            break;
        default:
            break;
    }

    auto applyCodeFormat =
        [this](int i, const LanguageData &data,
               const QString &text, const QTextCharFormat &fmt) -> int {
        if (i == 0 || (!text.at(i - 1).isLetterOrNumber() &&
                       text.at(i-1) != QLatin1Char('_'))) {
            const auto wordList = data.values(text.at(i).toLatin1());
            for (const QLatin1String &word : wordList) {
                if (word == strMidRef(text, i, word.size()) &&
                    (i + word.size() == text.length() ||
                     (!text.at(i + word.size()).isLetterOrNumber() &&
                      text.at(i + word.size()) != QLatin1Char('_')))) {
                    setFormat(i, word.size(), fmt);
                    i += word.size();
                }
            }
        }
        return i;
    };

    const QTextCharFormat &formatType = _codeFormats[QSourceHighliter::Token::CodeType];
    const QTextCharFormat &formatKeyword = _codeFormats[QSourceHighliter::Token::CodeKeyWord];
    const QTextCharFormat &formatComment = _codeFormats[QSourceHighliter::Token::CodeComment];
    const QTextCharFormat &formatNumLit = _codeFormats[QSourceHighliter::Token::CodeNumLiteral];
    const QTextCharFormat &formatBuiltIn = _codeFormats[QSourceHighliter::Token::CodeBuiltIn];
    const QTextCharFormat &formatOther = _codeFormats[QSourceHighliter::Token::CodeOther];

    // The goto Comment below jumps into the middle of the inner while-loop.
    // Ported verbatim from pbek/qmarkdowntextedit; refactor with caution.
    for (int i = 0; i < textLen; ++i) {

        if (currentBlockState() % 2 != 0) goto Comment;

        while (i < textLen && !text[i].isLetter()) {
            if (text[i].isSpace()) {
                ++i;
                if (i == textLen) return;
                if (text[i].isLetter()) break;
                else continue;
            }
            
            if (comment.isNull() && text[i] == QLatin1Char('/')) {
                if((i+1) < textLen){
                    if(text[i+1] == QLatin1Char('/')) {
                        setFormat(i, textLen, formatComment);
                        return;
                    } else if(text[i+1] == QLatin1Char('*')) {
                        Comment:
                        int next = text.indexOf(QLatin1String("*/"),i);
                        if (next == -1) {
                            if (currentBlockState() % 2 == 0)
                                setCurrentBlockState(currentBlockState() + 1);
                            setFormat(i, textLen,  formatComment);
                            return;
                        } else {
                            if (currentBlockState() % 2 != 0) {
                                setCurrentBlockState(currentBlockState() - 1);
                            }
                            next += 2;
                            setFormat(i, next - i,  formatComment);
                            i = next;
                            if (i >= textLen) return;
                        }
                    }
                }
            } else if (isSQL && comment.isNull() && text[i] == QLatin1Char('-')) {
                if((i+1) < textLen){
                    if(text[i+1] == QLatin1Char('-')) {
                        setFormat(i, textLen, formatComment);
                        return;
                    }
                }
            } else if (text[i] == comment) {
                setFormat(i, textLen, formatComment);
                i = textLen;
            } else if (text[i].isNumber()) {
               i = highlightNumericLiterals(text, i);
            } else if (text[i] == QLatin1Char('\"')) {
               i = highlightStringLiterals('\"', text, i);
            }  else if (text[i] == QLatin1Char('\'')) {
               i = highlightStringLiterals('\'', text, i);
            }
            if (i >= textLen) {
                break;
            }
            ++i;
        }

        if (i == textLen || !text[i].isLetter()) continue;

        const int pos = i;

        i = applyCodeFormat(i, types, text, formatType);
        if (i == textLen || !text[i].isLetter()) continue;

        i = applyCodeFormat(i, keywords, text, formatKeyword);
        if (i == textLen || !text[i].isLetter()) continue;

        i = applyCodeFormat(i, literals, text, formatNumLit);
        if (i == textLen || !text[i].isLetter()) continue;

        i = applyCodeFormat(i, builtin, text, formatBuiltIn);
        if (i == textLen || !text[i].isLetter()) continue;

        if (( i == 0 || !text.at(i-1).isLetter()) && others.contains(text[i].toLatin1())) {
            const QList<QLatin1String> wordList = others.values(text[i].toLatin1());
            for(const QLatin1String &word : wordList) {
                if (word == strMidRef(text, i, word.size()) &&
                        (i + word.size() == text.length() || !text.at(i + word.size()).isLetter())) {
                    setFormat(i, word.size(), formatOther);
                    i += word.size();
                }
            }
        }

        if (pos == i) {
            int count = i;
            while (count < textLen) {
                if (!text[count].isLetter()) break;
                ++count;
            }
            i = count;
        }
    }
}

int MarkdownHighlighter::highlightNumericLiterals(const QString &text, int i)
{
    const int start = i;
    if ((i+1) >= text.length()) {
        setFormat(i, 1, _codeFormats[QSourceHighliter::Token::CodeNumLiteral]);
        return ++i;
    }
    ++i;
    if (text.at(i) == QChar('x') && text.at(i - 1) == QChar('0')) ++i;
    while (i < text.length()) {
        if (!text.at(i).isNumber() && text.at(i) != QChar('.') && text.at(i) != QChar('e')) break;
        ++i;
    }
    setFormat(start, i - start, _codeFormats[QSourceHighliter::Token::CodeNumLiteral]);
    return --i;
}

int MarkdownHighlighter::highlightStringLiterals(const QChar strType, const QString &text, int i) {
    setFormat(i, 1,  _codeFormats[QSourceHighliter::Token::CodeString]);
    ++i;
    while (i < text.length()) {
        if (text.at(i) == strType && text.at(i-1) != QLatin1Char('\\')) {
            setFormat(i, 1,  _codeFormats[QSourceHighliter::Token::CodeString]);
            ++i;
            break;
        }
        setFormat(i, 1,  _codeFormats[QSourceHighliter::Token::CodeString]);
        ++i;
    }
    return i;
}

void MarkdownHighlighter::setDiagnostics(const QJsonArray &diagnostics)
{
    m_diagnostics.clear();

    for (const auto &val : diagnostics) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        LSPDiagnosticRange diag;
        diag.startLine = obj["line"].toInt();
        diag.startColumn = obj["character"].toInt();
        diag.endLine = obj["endLine"].toInt();
        diag.endColumn = obj["endCharacter"].toInt();
        diag.severity = obj["severity"].toInt();
        diag.message = obj["message"].toString();

        m_diagnostics.append(diag);
    }

    rehighlight();
}

void MarkdownHighlighter::clearDiagnostics()
{
    m_diagnostics.clear();
    rehighlight();
}

void MarkdownHighlighter::applyDiagnostics(const QString &text, int blockNumber, int blockStart)
{
    Q_UNUSED(blockStart);

    if (m_diagnostics.isEmpty())
        return;

    for (const auto &diag : m_diagnostics) {
        int diagStartLine = diag.startLine;
        int diagEndLine = diag.endLine;

        if (blockNumber < diagStartLine || blockNumber > diagEndLine)
            continue;

        int startCol = (blockNumber == diagStartLine) ? diag.startColumn : 0;
        int endCol = (blockNumber == diagEndLine) ? diag.endColumn : text.length();

        if (startCol < 0) startCol = 0;
        if (endCol > text.length()) endCol = text.length();

        if (startCol == endCol && startCol < text.length()) {
            endCol = startCol + 1;
        }

        if (startCol < endCol) {
            QColor underlineColor;
            switch (diag.severity) {
            case 1: underlineColor = QColor("#e06c75"); break;
            case 2: underlineColor = QColor("#e5c07b"); break;
            case 3: underlineColor = QColor("#61afef"); break;
            default: underlineColor = QColor("#98c379"); break;
            }

            for (int i = startCol; i < endCol; ++i) {
                QTextCharFormat existingFormat = format(i);
                existingFormat.setBackground(QColor(underlineColor.red(), underlineColor.green(), underlineColor.blue(), 40));
                setFormat(i, 1, existingFormat);
            }
        }
    }
}

QQuickTextDocument *MarkdownHighlighter::document() const
{
    return m_quickDocument;
}

void MarkdownHighlighter::setDocument(QQuickTextDocument *document)
{
    if (m_quickDocument == document)
        return;

    m_quickDocument = document;
    if (m_quickDocument) {
        QSyntaxHighlighter::setDocument(m_quickDocument->textDocument());
    } else {
        QSyntaxHighlighter::setDocument(nullptr);
    }
    emit documentChanged();
}
