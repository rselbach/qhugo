import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtWebEngine

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

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: fuzzyFinder.open()
    }
    Shortcut {
        sequence: "Meta+P"
        onActivated: fuzzyFinder.open()
    }

function setHugoRoot(dir) {
    // Set the Hugo root and start the server (only called when opening a new site)
    if (dir === "" || dir === hugoRoot) {
        return
    }
    console.log("Setting Hugo root:", dir)
    hugoRoot = dir
    currentDir = dir

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
            onContentSaved: {
                // Hugo handles live-reloading inside the webview via sockets.
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
        rootPath: window.currentDir
        onFileSelected: function(path) {
            editor.openFile(path)
        }
    }
}
