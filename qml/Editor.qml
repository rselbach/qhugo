import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QHugo 1.0

Item {
    id: root
    
    property string repoPath

    signal contentSaved()

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
                return
            }
        }

        var content = FileController.readFile(cleanPath)
        tabModel.append({ "title": cleanPath.split('/').pop(), "filePath": cleanPath, "fileContent": content })
        tabBar.currentIndex = tabModel.count - 1
    }

    function closeTab(index) {
        if (index < 0 || index >= tabModel.count) return;
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
                                    width: 40
                                    Repeater {
                                        model: textArea.lineCount
                                        Label {
                                            width: 40
                                            height: textArea.cursorRectangle.height
                                            horizontalAlignment: Text.AlignRight
                                            padding: 5
                                            text: index + 1
                                            color: "#888"
                                            font: textArea.font
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

                                    MarkdownHighlighter {
                                        id: highlighter
                                        document: textArea.textDocument
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
