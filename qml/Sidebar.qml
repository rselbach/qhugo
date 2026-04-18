import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt.labs.folderlistmodel

Item {
    id: root
    property string currentDirectory
    property string rootDirectory // Hugo root - can't go above this

    signal fileSelected(string path)
    signal imageSelected(string path)
    signal directorySelected(string path)
    signal goUpClicked()

    // Check if we're at the root directory
    readonly property bool atRoot: currentDirectory === rootDirectory ||
                                     currentDirectory === "" ||
                                     rootDirectory === ""

    // Check if current directory is a bundle (contains index.md)
    readonly property bool isBundle: {
        if (root.currentDirectory === "") return false
        // Check for index.md existence - simplified, actual check done in Go/C++
        return FileController.isBundleDirectory(root.currentDirectory)
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#1e1e1e" : "#ffffff"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 40
                color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#2a2a2a" : "#f0f0f0"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 5

                    Button {
                        id: goUpBtn
                        icon.name: "go-up"
                        text: ".."
                        Layout.preferredWidth: 40
                        Layout.fillHeight: true
                        display: AbstractButton.IconOnly
                        enabled: !root.atRoot
                        opacity: enabled ? 1.0 : 0.5
                        onClicked: root.goUpClicked()
                        ToolTip.text: "Up one level"
                        ToolTip.visible: hovered
                    }

                    Label {
                        text: "Files"
                        font.bold: true
                        Layout.fillWidth: true
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                        color: Qt.application.styleHints.colorScheme === Qt.Dark ? "#ddd" : "#333"
                    }
                }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                model: FolderListModel {
                    id: folderModel
                    folder: "file://" + root.currentDirectory
                    showDirsFirst: true
                    showDotAndDotDot: false
                    nameFilters: {
                        // In a bundle, also show images
                        if (root.isBundle) {
                            return ["*.md", "*.txt", "*.yaml", "*.toml", "*.json",
                                    "*.jpg", "*.jpeg", "*.png", "*.gif", "*.webp", "*.svg"]
                        }
                        return ["*.md", "*.txt", "*.yaml", "*.toml", "*.json"]
                    }
                }

                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: fileName
                    icon.name: {
                        if (fileIsDir) return "folder"
                        var ext = fileName.toLowerCase()
                        if (ext.endsWith(".jpg") || ext.endsWith(".jpeg") ||
                            ext.endsWith(".png") || ext.endsWith(".gif") ||
                            ext.endsWith(".webp") || ext.endsWith(".svg") ||
                            ext.endsWith(".bmp") || ext.endsWith(".tiff")) {
                            return "image-x-generic"
                        }
                        return "text-x-markdown"
                    }

                    readonly property bool isImage: {
                        var ext = fileName.toLowerCase()
                        return ext.endsWith(".jpg") || ext.endsWith(".jpeg") ||
                               ext.endsWith(".png") || ext.endsWith(".gif") ||
                               ext.endsWith(".webp") || ext.endsWith(".svg") ||
                               ext.endsWith(".bmp") || ext.endsWith(".tiff")
                    }

                    onClicked: {
                        if (fileIsDir) {
                            root.directorySelected(filePath)
                        } else if (isImage) {
                            root.imageSelected(filePath)
                        } else {
                            root.fileSelected(filePath)
                        }
                    }
                }
            }
        }
    }
}
