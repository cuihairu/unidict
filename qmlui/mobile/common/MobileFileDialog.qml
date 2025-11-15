import QtQuick 2.15
import QtQuick.Controls 2.15

// 移动端优化的文件选择器封装
Item {
    id: root

    property string fileUrl: ""
    property string currentFolder: ""
    property var nameFilters: ["All files (*)"]
    property string title: "Select File"
    property bool selectExisting: true

    signal accepted()
    signal rejected()

    function open() {
        if (MobileUtils.isAndroid()) {
            // Android使用原生文档选择器
            MobileUtils.openDocumentPicker(title, nameFilters, selectExisting)
        } else if (MobileUtils.isIOS()) {
            // iOS使用原生文档选择器
            MobileUtils.openIOSDocumentPicker(title, nameFilters, selectExisting)
        } else {
            // 桌面平台显示提示信息
            console.log("Desktop file dialog not available, using mobile utils fallback")
            root.rejected()
        }
    }

    function close() {
        // 移动平台没有需要关闭的对话框
    }

    // 移动平台响应
    Connections {
        target: MobileUtils
        enabled: MobileUtils.isMobile()

        function onDocumentSelected(url) {
            root.fileUrl = url
            root.accepted()
        }

        function onDocumentSelectionCancelled() {
            root.rejected()
        }
    }
}