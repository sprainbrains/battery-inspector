import QtQuick 2.9
import Lomiri.Components 1.3

Rectangle {
    id: root

    property string title: ""
    default property alias content: inner.data

    width: parent ? parent.width : implicitWidth
    height: inner.height + units.gu(2)
    radius: units.gu(0.5)
    color: theme.palette.normal.foreground

    Column {
        id: inner
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: units.gu(1)
        }
        spacing: units.gu(0.8)

        Label {
            text: root.title
            fontSize: "medium"
            font.weight: Font.DemiBold
            color: theme.palette.normal.backgroundText
        }
    }
}
