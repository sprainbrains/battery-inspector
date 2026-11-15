import QtQuick 2.9
import Lomiri.Components 1.3

Item {
    id: root

    property string label: ""
    property string value: "—"
    property bool monospace: false

    width: parent ? parent.width : implicitWidth
    height: Math.max(labelLabel.height, valueLabel.height) + units.gu(0.4)

    Label {
        id: labelLabel
        text: root.label
        fontSize: "small"
        color: theme.palette.normal.backgroundText
        opacity: 0.75
        width: Math.min(parent.width * 0.48, implicitWidth)
        elide: Text.ElideRight
    }

    Label {
        id: valueLabel
        text: root.value
        fontSize: "small"
        color: theme.palette.normal.backgroundText
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        width: Math.max(0, parent.width - labelLabel.width - units.gu(1))
        anchors.right: parent.right
        font.family: root.monospace ? "Ubuntu Mono" : "Ubuntu"
    }
}
