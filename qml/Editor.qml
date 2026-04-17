import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QHugo 1.0

Item {
    id: root
    
    property string repoPath
    property string currentFilePath: ""
    property LspClient lspClient: null

    signal contentSaved()
    signal fileOpened(string filePath)

    Shortcut {
        sequences: [StandardKey.Close]
        onActivated: closeTab(tabBar.currentIndex)
    }

    Shortcut {
        sequences: [StandardKey.Save]
        onActivated: root.saveCurrentTab()
    }

    Shortcut {
        sequence: "Meta+S"
        onActivated: root.saveCurrentTab()
    }

    function openFile(path) {
        var cleanPath = path.toString().replace("file://", "")
        
        for (var i = 0; i < tabModel.count; ++i) {
            if (tabModel.get(i).filePath === cleanPath) {
                tabBar.currentIndex = i
                root.currentFilePath = cleanPath
                root.fileOpened(cleanPath)
                return
            }
        }

        var content = FileController.readFile(cleanPath)
        tabModel.append({ "title": cleanPath.split('/').pop(), "filePath": cleanPath, "fileContent": content })
        tabBar.currentIndex = tabModel.count - 1
        root.currentFilePath = cleanPath
        root.fileOpened(cleanPath)
        
        // Notify LSP client about opened document
        if (lspClient && lspClient.enabled) {
            var ext = cleanPath.split('.').pop().toLowerCase()
            var langId = ext === "md" || ext === "markdown" ? "markdown" : ext
            lspClient.documentOpened(cleanPath, langId, content)
        }
    }
    
    function onEditorTextChanged(filePath, text) {
        // Notify LSP about document changes
        console.log("[Editor] onEditorTextChanged called for", filePath)
        if (!lspClient) {
            console.log("[Editor] lspClient is null!")
            return
        }
        console.log("[Editor] lspClient.enabled:", lspClient.enabled)
        if (lspClient.enabled && filePath !== "") {
            console.log("[Editor] Calling lspClient.documentChanged")
            lspClient.documentChanged(filePath, text)
        }
    }

    function onTabChanged() {
        if (tabBar.currentIndex >= 0 && tabBar.currentIndex < tabModel.count) {
            var newPath = tabModel.get(tabBar.currentIndex).filePath
            if (newPath !== root.currentFilePath) {
                root.currentFilePath = newPath
                root.fileOpened(newPath)
            }
        }
    }

    function closeTab(index) {
        if (index < 0 || index >= tabModel.count) return;
        
        // Notify LSP about document closure
        var item = tabModel.get(index)
        if (lspClient && lspClient.enabled && item && item.filePath) {
            lspClient.documentClosed(item.filePath)
        }
        
        tabModel.remove(index);
        if (tabBar.currentIndex >= tabModel.count) {
            tabBar.currentIndex = tabModel.count - 1;
        }
    }

    function saveCurrentTab() {
        if (tabBar.currentIndex >= 0 && tabBar.currentIndex < tabModel.count) {
            var currentItem = stackLayout.children[tabBar.currentIndex]
            if (currentItem && currentItem.editor) {
                var currentTab = tabModel.get(tabBar.currentIndex)
                FileController.saveFile(currentTab.filePath, currentItem.editor.text)
                root.contentSaved()
            }
        }
    }

    ListModel { id: tabModel }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            onCurrentIndexChanged: root.onTabChanged()
            
            Repeater {
                model: tabModel
                
                TabButton {
                    id: tabBtn
                    width: implicitWidth + 30 
                    
                    contentItem: RowLayout {
                        spacing: 5
                        Label {
                            text: title
                            font: tabBtn.font
                            color: tabBtn.checked ? (Qt.application.styleHints.colorScheme === Qt.Dark ? "white" : "black") : "#888"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        
                        ToolButton {
                            text: "×"
                            font.pixelSize: 16
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            background: Rectangle { color: "transparent" }
                            visible: tabBtn.hovered || tabBtn.checked
                            onClicked: closeTab(index)
                        }
                    }
                    onClicked: tabBar.currentIndex = index
                }
            }
        }

        StackLayout {
            id: stackLayout
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: tabModel
                
                Item {
                    id: tabItem
                    property string path: filePath
                    property var editor: textArea

                    ColumnLayout {
                        anchors.fill: parent
                        
                        // Toolbar
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 5
                            Button {
                                text: "Save"
                                onClicked: root.saveCurrentTab()
                            }
                            Item { Layout.fillWidth: true }
                        }

                        // Editor Area
                        ScrollView {
                            id: scrollView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            Row {
                                width: scrollView.availableWidth

        Column {
            id: lineNumbers
            width: 50
            property int diagVersion: 0

            Connections {
                target: root.lspClient
                function onDiagnosticsChanged() {
                    lineNumbers.diagVersion++
                }
            }

            Repeater {
                model: textArea.lineCount
                Row {
                    width: 50
                    height: textArea.cursorRectangle.height
                    spacing: 2

                    // Diagnostic indicator
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: {
                            if (!root.lspClient) return "transparent"
                            var _ = lineNumbers.diagVersion
                            var sev = root.lspClient.getDiagnosticSeverityColor(index)
                            if (sev === "error") return "#e06c75"
                            if (sev === "warning") return "#e5c07b"
                            if (sev === "info") return "#61afef"
                            if (sev === "hint") return "#98c379"
                            return "transparent"
                        }
                        visible: color !== "transparent"
                    }

                    // Line number
                    Label {
                        width: 40
                        height: textArea.cursorRectangle.height
                        horizontalAlignment: Text.AlignRight
                        padding: 5
                        text: index + 1
                        color: {
                            if (!root.lspClient) return "#888"
                            var _ = lineNumbers.diagVersion
                            var sev = root.lspClient.getDiagnosticSeverityColor(index)
                            if (sev === "error") return "#e06c75"
                            if (sev === "warning") return "#e5c07b"
                            return "#888"
                        }
                        font: textArea.font
                    }
                }
            }
        }

                    TextArea {
                        id: textArea
                        width: parent.width - lineNumbers.width
                        text: fileContent
                        textFormat: TextEdit.PlainText

                        font.family: "Courier New"
                        font.pixelSize: 14
                        padding: 0
                        leftPadding: 5

                        wrapMode: TextEdit.Wrap
                        selectByMouse: true

                        background: Rectangle {
                            color: "transparent"
                            border.width: 0
                        }

                        color: Qt.application.styleHints.colorScheme === Qt.Dark ? "white" : "black"

                        // Notify LSP on user edits (not programmatic changes)
                        onTextEdited: {
                            var currentPath = tabModel.get(tabBar.currentIndex) ? tabModel.get(tabBar.currentIndex).filePath : ""
                            if (currentPath !== "") {
                                root.onEditorTextChanged(currentPath, text)
                            }
                        }

                        // Hover handling for LSP
                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            propagateComposedEvents: true

                            property var hoverTimer: Timer {
                                interval: 300
                                property int pendingLine: -1
                                property int pendingCol: -1
                                onTriggered: {
                                    if (root.lspClient && root.lspClient.enabled && pendingLine >= 0) {
                                        root.lspClient.requestHover(root.currentFilePath, pendingLine, pendingCol)
                                    }
                                }
                            }

                            onPositionChanged: function(mouse) {
                                var pos = textArea.positionAt(mouse.x, mouse.y)
                                var line = 0
                                var lineStart = 0
                                var text = textArea.text
                                for (var i = 0; i < text.length; i++) {
                                    if (i === pos) break
                                    if (text[i] === '\n') {
                                        line++
                                        lineStart = i + 1
                                    }
                                }
                                var col = pos - lineStart

                                hoverArea.hoverTimer.pendingLine = line
                                hoverArea.hoverTimer.pendingCol = col
                                hoverArea.hoverTimer.restart()
                            }

                            onExited: {
                                hoverArea.hoverTimer.stop()
                                hoverTooltip.hide()
                            }
                        }

                        MarkdownHighlighter {
                            id: highlighter
                            document: textArea.textDocument
                        }

                        Connections {
                            target: root.lspClient
                            function onDiagnosticsChanged() {
                                highlighter.setDiagnostics(root.lspClient.currentDiagnostics)
                            }
                        }

                        // Hover Tooltip
                        Rectangle {
                            id: hoverTooltip
                            visible: false
                            color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#323232" : "#fafafa"
                            border.color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#555" : "#ccc"
                            border.width: 1
                            radius: 4
                            width: tooltipContent.implicitWidth + 16
                            height: tooltipContent.implicitHeight + 12
                            z: 100

                            property string content: ""

                            function show(text, x, y) {
                                content = text
                                tooltipContent.text = text
                                var newX = x + 10
                                var newY = y - height - 10
                                if (newX + width > textArea.width) newX = textArea.width - width - 5
                                if (newY < 0) newY = y + 20
                                x = newX
                                y = newY
                                visible = true
                            }

                            function hide() {
                                visible = false
                                content = ""
                            }

                            TextEdit {
                                id: tooltipContent
                                anchors.fill: parent
                                anchors.margins: 6
                                readOnly: true
                                wrapMode: TextEdit.WordWrap
                                font.pixelSize: 12
                                color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#ddd" : "#333"
                                textFormat: TextEdit.PlainText
                            }

                            Connections {
                                target: root.lspClient
                                function onHoverReceived(uri, line, character, contents) {
                                    if (contents && contents.length > 0) {
                                        // Calculate position for line and character
                                        var pos = 0
                                        var currentLine = 0
                                        for (var i = 0; i < textArea.text.length && currentLine < line; i++) {
                                            if (textArea.text[i] === '\n') {
                                                currentLine++
                                                if (currentLine === line) {
                                                    pos = i + 1 + character
                                                    break
                                                }
                                            }
                                        }
                                        if (currentLine < line) {
                                            pos = textArea.text.length
                                        } else if (currentLine === line && pos === 0) {
                                            pos = character
                                        }

                                        var rect = textArea.cursorRectangle
                                        var oldPos = textArea.cursorPosition
                                        textArea.cursorPosition = pos
                                        rect = textArea.cursorRectangle
                                        textArea.cursorPosition = oldPos
                                        hoverTooltip.show(contents, rect.x, rect.y)
                                    } else {
                                        hoverTooltip.hide()
                                    }
                                }
                            }
                        }
                    }
                            }

                            // Catch Image Drag and Drop - placed as sibling to content to avoid event conflicts
                            DropArea {
                                anchors.fill: parent
                                keys: ["text/uri-list"]

                                onEntered: function(drag) {
                                    // Accept the drag if it contains URLs with image extensions
                                    var hasImage = false
                                    if (drag.hasUrls) {
                                        for (var i = 0; i < drag.urls.length; i++) {
                                            var url = drag.urls[i].toString()
                                            if (url.match(/\.(jpg|jpeg|png|gif|webp|bmp|tiff?)$/i)) {
                                                hasImage = true
                                                break
                                            }
                                        }
                                    }
                                    drag.accepted = hasImage
                                }

                                onDropped: function(drop) {
                                    if (drop.hasUrls) {
                                        // Calculate text position from mouse coordinates
                                        // drop.x is relative to the DropArea (ScrollView)
                                        // Subtract line numbers width to get position within TextArea
                                        var textX = Math.max(0, drop.x - lineNumbers.width)
                                        var textY = drop.y
                                        var insertPosition = textArea.positionAt(textX, textY)

                                        for (var i = 0; i < drop.urls.length; i++) {
                                            var url = drop.urls[i].toString()
                                            if (url.match(/\.(jpg|jpeg|png|gif|webp|bmp|tiff?)$/i)) {
                                                var link = FileController.processImage(url, root.repoPath, tabItem.path)
                                                if (!link.startsWith("Error")) {
                                                    textArea.insert(insertPosition, link + "\n")
                                                    // Update position for next image (if multiple dropped)
                                                    insertPosition += link.length + 1
                                                } else {
                                                    console.error("Image processing error:", link)
                                                }
                                            }
                                        }
                                        drop.accept(Qt.CopyAction)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
