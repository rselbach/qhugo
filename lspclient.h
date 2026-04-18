#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QVector>

// Forward declarations for C types
extern "C" {
    typedef void (*lspDiagnosticCallback)(const char* uri, const char* jsonDiagnostics);
    typedef void (*lspHoverCallback)(const char* uri, int line, int character, const char* contents);
    typedef void (*lspLogCallback)(const char* message);
}

// Diagnostic structure for UI
struct LSPDiagnostic {
    int line;
    int character;
    int endLine;
    int endCharacter;
    int severity;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    QString code;
    QString source;
    QString message;
};

// LSP Server configuration
struct LSPServerConfig {
    QString name;
    QString command;
    QStringList args;
    QMap<QString, QString> environment;
    QStringList languages;
    bool enabled;
    QString rootUri;
};

/**
 * LspClient - C++ bridge to the Go LSP client backend
 * 
 * This class provides a QObject-based interface to the Go LSP client,
 * handling callbacks from Go and exposing LSP functionality to QML.
 */
class LspClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QJsonArray serverConfigs READ getServerConfigs NOTIFY serverConfigsChanged)
    Q_PROPERTY(QJsonArray currentDiagnostics READ getCurrentDiagnostics NOTIFY diagnosticsChanged)

public:
    explicit LspClient(QObject *parent = nullptr);
    ~LspClient();

    // Initialization
    Q_INVOKABLE void initialize();
    Q_INVOKABLE void cleanup();
    Q_INVOKABLE void setWorkspaceRoot(const QString& root);

    // Document lifecycle
    Q_INVOKABLE void documentOpened(const QString& uri, const QString& languageId, const QString& content);
    Q_INVOKABLE void documentChanged(const QString& uri, const QString& content);
    Q_INVOKABLE void documentClosed(const QString& uri);

    // Features
    Q_INVOKABLE void requestHover(const QString& uri, int line, int character);

    // Configuration
    Q_INVOKABLE bool isEnabled() const;
    Q_INVOKABLE void setEnabled(bool enabled);

    // Server management
    Q_INVOKABLE QJsonArray getServerConfigs() const;
    Q_INVOKABLE void addServer(const QJsonObject& config);
    Q_INVOKABLE void removeServer(const QString& name);
    Q_INVOKABLE void setServerEnabled(const QString& name, bool enabled);
    Q_INVOKABLE void startClients();
    Q_INVOKABLE void stopClients();

    // Diagnostics
    Q_INVOKABLE QJsonArray getCurrentDiagnostics() const;
    Q_INVOKABLE QJsonArray getDiagnosticsForLine(int line) const;
    Q_INVOKABLE QJsonArray getDiagnosticsAtPosition(int line, int character) const;
    Q_INVOKABLE bool hasDiagnosticsForLine(int line) const;
    Q_INVOKABLE QString getDiagnosticSeverityColor(int line) const; // Returns "error", "warning", "info", or empty

    // Callbacks from Go (called by C callbacks)
    void handleDiagnostics(const QString& uri, const QJsonArray& diagnostics);
    void handleHover(const QString& uri, int line, int character, const QString& contents);
    void handleLog(const QString& message);

signals:
    void enabledChanged();
    void serverConfigsChanged();
    void diagnosticsReceived(const QString& uri, const QJsonArray& diagnostics);
    void diagnosticsChanged();
    void hoverReceived(const QString& uri, int line, int character, const QString& contents);
    void logMessage(const QString& message);

private:
    bool m_initialized;
    QString m_workspaceRoot;
    QString m_currentDocumentUri;
    QMap<QString, QVector<LSPDiagnostic>> m_diagnostics;  // key: document URI

    // Static callback wrappers (called from C)
    static void diagnosticCallback(const char* uri, const char* jsonDiagnostics);
    static void hoverCallback(const char* uri, int line, int character, const char* contents);
    static void logCallback(const char* message);
    
    // Static instance for callbacks
    static LspClient* s_instance;
};

#endif // LSPCLIENT_H
