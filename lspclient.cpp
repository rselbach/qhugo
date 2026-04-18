#include "lspclient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QFileInfo>
#include <QDir>

// Include the Go backend header
extern "C" {
#include "backend/backend.h"

// Global callback pointers - defined here, declared in Go preamble as extern
lspDiagnosticCallback g_diagnosticCallback = NULL;
lspHoverCallback g_hoverCallback = NULL;
lspLogCallback g_logCallback = NULL;

// C helper functions for callbacks - defined here, declared in Go preamble as extern
void setDiagnosticCallback(lspDiagnosticCallback cb) {
    g_diagnosticCallback = cb;
}

void setHoverCallback(lspHoverCallback cb) {
    g_hoverCallback = cb;
}

void setLogCallback(lspLogCallback cb) {
    g_logCallback = cb;
}

void callDiagnosticCallback(const char* uri, const char* jsonDiagnostics) {
    if (g_diagnosticCallback != NULL) {
        g_diagnosticCallback(uri, jsonDiagnostics);
    }
}

void callHoverCallback(const char* uri, int line, int character, const char* contents) {
    if (g_hoverCallback != NULL) {
        g_hoverCallback(uri, line, character, contents);
    }
}

void callLogCallback(const char* message) {
    if (g_logCallback != NULL) {
        g_logCallback(message);
    }
}
}

// Static instance for callbacks
LspClient* LspClient::s_instance = nullptr;

LspClient::LspClient(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
    s_instance = this;
}

LspClient::~LspClient()
{
    cleanup();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void LspClient::initialize()
{
    if (m_initialized) {
        return;
    }

    // Set up callbacks
    LSPSetCallbacks(
        (void*)diagnosticCallback,
        (void*)hoverCallback,
        (void*)logCallback
    );

    // Initialize LSP manager
    if (LSPInitialize() == 1) {
        m_initialized = true;
    }
}

void LspClient::cleanup()
{
    if (m_initialized) {
        LSPCleanup();
        m_initialized = false;
    }
}

void LspClient::setWorkspaceRoot(const QString& root)
{
    m_workspaceRoot = root;
    if (m_initialized) {
        QByteArray rootUtf8 = root.toUtf8();
        LSPSetWorkspaceRoot(rootUtf8.data());
    }
}

void LspClient::documentOpened(const QString& uri, const QString& languageId, const QString& content)
{
    if (!m_initialized || !isEnabled()) {
        return;
    }

    m_currentDocumentUri = uri;
    
    QByteArray uriUtf8 = uri.toUtf8();
    QByteArray langUtf8 = languageId.toUtf8();
    QByteArray contentUtf8 = content.toUtf8();
    
    LSPDocumentOpened(uriUtf8.data(), langUtf8.data(), contentUtf8.data());
}

void LspClient::documentChanged(const QString& uri, const QString& content)
{
    if (!m_initialized || !isEnabled()) {
        return;
    }

    QByteArray uriUtf8 = uri.toUtf8();
    QByteArray contentUtf8 = content.toUtf8();
    
    LSPDocumentChanged(uriUtf8.data(), contentUtf8.data());
}

void LspClient::documentClosed(const QString& uri)
{
    if (!m_initialized || !isEnabled()) {
        return;
    }

    if (m_currentDocumentUri == uri) {
        m_currentDocumentUri.clear();
    }
    
    // Clear diagnostics for this document
    m_diagnostics.remove(uri);
    emit diagnosticsChanged();
    
    QByteArray uriUtf8 = uri.toUtf8();
    LSPDocumentClosed(uriUtf8.data());
}

void LspClient::requestHover(const QString& uri, int line, int character)
{
    if (!m_initialized || !isEnabled()) {
        return;
    }

    QByteArray uriUtf8 = uri.toUtf8();
    LSPRequestHover(uriUtf8.data(), line, character);
}

bool LspClient::isEnabled() const
{
    return LSPIsEnabled() == 1;
}

void LspClient::setEnabled(bool enabled)
{
    if (LSPSetEnabled(enabled ? 1 : 0) == 1) {
        emit enabledChanged();
        
        if (enabled) {
            startClients();
        } else {
            stopClients();
        }
    }
}

QJsonArray LspClient::getServerConfigs() const
{
    char* serversJson = LSPGetServers();
    if (!serversJson) {
        return QJsonArray();
    }
    
    QByteArray data(serversJson);
    FreeString(serversJson);
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
        return doc.array();
    }
    
    return QJsonArray();
}

void LspClient::addServer(const QJsonObject& config)
{
    QJsonDocument doc(config);
    QByteArray json = doc.toJson(QJsonDocument::Compact);

    if (LSPAddServer(json.data()) == 1) {
        emit serverConfigsChanged();
    }
}

void LspClient::removeServer(const QString& name)
{
    QByteArray nameUtf8 = name.toUtf8();

    if (LSPRemoveServer(nameUtf8.data()) == 1) {
        emit serverConfigsChanged();
    }
}

void LspClient::setServerEnabled(const QString& name, bool enabled)
{
    QByteArray nameUtf8 = name.toUtf8();
    
    LSPSetServerEnabled(nameUtf8.data(), enabled ? 1 : 0);
    emit serverConfigsChanged();
}

void LspClient::startClients()
{
    LSPStartClients();
}

void LspClient::stopClients()
{
    LSPStopClients();
}

QJsonArray LspClient::getCurrentDiagnostics() const
{
    QJsonArray result;
    
    if (m_currentDocumentUri.isEmpty()) {
        return result;
    }
    
    auto it = m_diagnostics.find(m_currentDocumentUri);
    if (it != m_diagnostics.end()) {
        for (const auto& diag : it.value()) {
            QJsonObject obj;
            obj["line"] = diag.line;
            obj["character"] = diag.character;
            obj["endLine"] = diag.endLine;
            obj["endCharacter"] = diag.endCharacter;
            obj["severity"] = diag.severity;
            obj["code"] = diag.code;
            obj["source"] = diag.source;
            obj["message"] = diag.message;
            result.append(obj);
        }
    }
    
    return result;
}

QJsonArray LspClient::getDiagnosticsForLine(int line) const
{
    QJsonArray result;
    
    if (m_currentDocumentUri.isEmpty()) {
        return result;
    }
    
    auto it = m_diagnostics.find(m_currentDocumentUri);
    if (it != m_diagnostics.end()) {
        for (const auto& diag : it.value()) {
            if (diag.line == line) {
                QJsonObject obj;
                obj["line"] = diag.line;
                obj["character"] = diag.character;
                obj["endLine"] = diag.endLine;
                obj["endCharacter"] = diag.endCharacter;
                obj["severity"] = diag.severity;
                obj["code"] = diag.code;
                obj["source"] = diag.source;
                obj["message"] = diag.message;
                result.append(obj);
            }
        }
    }
    
    return result;
}

QJsonArray LspClient::getDiagnosticsAtPosition(int line, int character) const
{
    QJsonArray result;

    if (m_currentDocumentUri.isEmpty()) {
        return result;
    }

    auto it = m_diagnostics.find(m_currentDocumentUri);
    if (it != m_diagnostics.end()) {
        for (const auto& diag : it.value()) {
            if (diag.line == line && character >= diag.character && character < diag.endCharacter) {
                QJsonObject obj;
                obj["line"] = diag.line;
                obj["character"] = diag.character;
                obj["endLine"] = diag.endLine;
                obj["endCharacter"] = diag.endCharacter;
                obj["severity"] = diag.severity;
                obj["code"] = diag.code;
                obj["source"] = diag.source;
                obj["message"] = diag.message;
                result.append(obj);
            }
        }
    }

    return result;
}

bool LspClient::hasDiagnosticsForLine(int line) const
{
    if (m_currentDocumentUri.isEmpty()) {
        return false;
    }
    
    auto it = m_diagnostics.find(m_currentDocumentUri);
    if (it != m_diagnostics.end()) {
        for (const auto& diag : it.value()) {
            if (diag.line == line) {
                return true;
            }
        }
    }
    
    return false;
}

QString LspClient::getDiagnosticSeverityColor(int line) const
{
    if (m_currentDocumentUri.isEmpty()) {
        return QString();
    }
    
    int worstSeverity = 5;  // Higher is better (1=error, 2=warning, 3=info, 4=hint)
    
    auto it = m_diagnostics.find(m_currentDocumentUri);
    if (it != m_diagnostics.end()) {
        for (const auto& diag : it.value()) {
            if (diag.line == line && diag.severity < worstSeverity) {
                worstSeverity = diag.severity;
            }
        }
    }
    
    switch (worstSeverity) {
        case 1: return QStringLiteral("error");
        case 2: return QStringLiteral("warning");
        case 3: return QStringLiteral("info");
        case 4: return QStringLiteral("hint");
        default: return QString();
    }
}

// Callback handlers from Go
void LspClient::handleDiagnostics(const QString& uri, const QJsonArray& diagnostics)
{
    QVector<LSPDiagnostic> diagList;

    for (const auto& val : diagnostics) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        LSPDiagnostic diag;
        diag.line = obj["line"].toInt();
        diag.character = obj["character"].toInt();
        diag.endLine = obj["endLine"].toInt();
        diag.endCharacter = obj["endCharacter"].toInt();
        diag.severity = obj["severity"].toInt();
        diag.code = obj["code"].toString();
        diag.source = obj["source"].toString();
        diag.message = obj["message"].toString();

        diagList.append(diag);
    }

    m_diagnostics[uri] = diagList;

    if (uri == m_currentDocumentUri) {
        emit diagnosticsChanged();
    }

    emit diagnosticsReceived(uri, diagnostics);
}

void LspClient::handleHover(const QString& uri, int line, int character, const QString& contents)
{
    emit hoverReceived(uri, line, character, contents);
}

void LspClient::handleLog(const QString& message)
{
    emit logMessage(message);
}

// Static C callbacks
void LspClient::diagnosticCallback(const char* uri, const char* jsonDiagnostics)
{
    if (s_instance) {
        QString uriStr = QString::fromUtf8(uri);
        QByteArray data(jsonDiagnostics);

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            s_instance->handleDiagnostics(uriStr, doc.array());
        }
    }
}

void LspClient::hoverCallback(const char* uri, int line, int character, const char* contents)
{
    if (s_instance) {
        QString uriStr = QString::fromUtf8(uri);
        QString contentsStr = QString::fromUtf8(contents);
        s_instance->handleHover(uriStr, line, character, contentsStr);
    }
}

void LspClient::logCallback(const char* message)
{
    if (s_instance) {
        QString msgStr = QString::fromUtf8(message);
        s_instance->handleLog(msgStr);
    }
}
