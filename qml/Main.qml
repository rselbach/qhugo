import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtWebEngine
import QHugo 1.0

ApplicationWindow {
    id: window
    width: 1400
    height: 800
    visible: true
    title: "QHugo"

    property string hugoRoot: ""  // The Hugo site root (server runs from here)
    property string currentDir: ""  // Current directory for file browser navigation
    property int hugoPort: 1313
    property bool configLoaded: false
    property bool hugoStarting: false

    // LSP Client for language server integration
    LspClient {
        id: lspClient
        onLogMessage: function(msg) {
            console.log("[LSP] " + msg)
        }
        onDiagnosticsChanged: function() {
            console.log("[LSP] Diagnostics updated")
        }
    }

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: fuzzyFinder.open()
    }
    Shortcut {
        sequence: "Meta+P"
        onActivated: fuzzyFinder.open()
    }
    Shortcut {
        sequence: "Ctrl+Shift+L"
        onActivated: {
            lspClient.enabled = !lspClient.enabled
            console.log("LSP " + (lspClient.enabled ? "enabled" : "disabled"))
        }
    }

function setHugoRoot(dir) {
    // Set the Hugo root and start the server (only called when opening a new site)
    if (dir === "" || dir === hugoRoot) {
        return
    }
    console.log("Setting Hugo root:", dir)
    hugoRoot = dir
    currentDir = dir
    hugoStarting = true
    
    // Set LSP workspace root
    console.log("[Main] Calling lspClient.setWorkspaceRoot with:", dir)
    lspClient.setWorkspaceRoot(dir)
    console.log("[Main] lspClient.setWorkspaceRoot returned")

    window.hugoPort = FileController.startHugoServer(dir)
    webView.url = ""
    previewTimer.start()
}

function navigateTo(dir) {
    // Just change the file browser directory, don't affect Hugo server
    if (dir !== "") {
        currentDir = dir
    }
}

    Timer {
        id: previewTimer
        interval: 1000
        onTriggered: {
            console.log("Loading Hugo Preview on Port:", window.hugoPort)
            webView.url = "http://localhost:" + window.hugoPort
            window.hugoStarting = false
        }
    }

    // Load config on startup
    Component.onCompleted: {
        // Initialize LSP client
        console.log("[Main] About to call lspClient.initialize()")
        lspClient.initialize()
        console.log("[Main] lspClient.initialize() returned, enabled:", lspClient.enabled)
        
        // Small delay to ensure everything is ready
        Qt.callLater(function() {
            var savedDir = FileController.loadConfigCurrent()
            if (savedDir && savedDir !== "") {
                // Remove file:// prefix if present
                if (savedDir.indexOf("file://") === 0) {
                    savedDir = savedDir.substring(7)
                }
                // Load saved directory without re-saving to config
                setHugoRoot(savedDir)
                configLoaded = true
            } else {
                // No saved config, show folder dialog
                initialFolderDialog.open()
            }
        })
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            ToolButton {
                text: "Open Hugo Repo"
                onClicked: folderDialog.open()
            }
            ToolButton {
                text: "New Post"
                onClicked: newPostDialog.open()
            }
            Item { Layout.fillWidth: true }
        }
    }

    FolderDialog {
        id: folderDialog
        onAccepted: {
            var path = selectedFolder.toString()
            // Remove file:// prefix if present
            if (path.indexOf("file://") === 0) {
                path = path.substring(7)
            }
            FileController.addSiteAndSetCurrent(path)
            setHugoRoot(path)
        }
    }

    FolderDialog {
        id: initialFolderDialog
        title: "Select Hugo Site Root Directory"
        onAccepted: {
            var path = selectedFolder.toString()
            if (path.indexOf("file://") === 0) {
                path = path.substring(7)
            }
            FileController.addSiteAndSetCurrent(path)
            setHugoRoot(path)
            configLoaded = true
        }
        onRejected: {
            // If user cancels, use documents location as fallback
            var fallbackDir = FileController.getDocumentsLocation()
            FileController.addSiteAndSetCurrent(fallbackDir)
            setHugoRoot(fallbackDir)
            configLoaded = true
        }
    }

    Dialog {
        id: newPostDialog
        title: "Create New Post"
        standardButtons: Dialog.Ok | Dialog.Cancel
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        ColumnLayout {
            Label { text: "Post Title:" }
            TextField {
                id: postTitleField
                Layout.fillWidth: true
                focus: true
            }
        }
        onAccepted: {
            var path = FileController.createPost(window.hugoRoot, postTitleField.text)
            editor.openFile(path)
            postTitleField.text = ""
        }
    }

    SplitView {
        anchors.fill: parent

    Sidebar {
        id: sidebar
        SplitView.preferredWidth: 250
        SplitView.minimumWidth: 150
        SplitView.maximumWidth: 400

        currentDirectory: window.currentDir
        rootDirectory: window.hugoRoot

        onFileSelected: function(path) {
            editor.openFile(path)
        }
        onDirectorySelected: function(path) {
            window.navigateTo(path)
        }
        onGoUpClicked: {
            window.navigateTo(FileController.getParentPath(window.currentDir))
        }
    }

        Editor {
            id: editor
            SplitView.fillWidth: true
            SplitView.preferredWidth: 500
            repoPath: window.hugoRoot
            lspClient: lspClient
            onContentSaved: {
                // Hugo handles live-reloading inside the webview via sockets.
            }
            onFileOpened: function(filePath) {
                // Navigate webview to the corresponding Hugo page
                if (window.hugoPort > 0 && window.hugoRoot !== "") {
                    var hugoURL = FileController.getHugoURL(filePath, window.hugoRoot)
                    if (hugoURL !== "") {
                        var fullURL = "http://localhost:" + window.hugoPort + hugoURL
                        console.log("Navigating to:", fullURL)
                        webView.url = fullURL
                    }
                }
            }
        }

        WebEngineView {
            id: webView
            SplitView.preferredWidth: 600
            SplitView.fillWidth: true
        }
    }

  FuzzyFinder {
    id: fuzzyFinder
    rootPath: window.hugoRoot
        onFileSelected: function(path) {
            editor.openFile(path)
        }
    }
}
