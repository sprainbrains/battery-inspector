import QtQuick 2.9
import Lomiri.Components 1.3

Rectangle {
    id: root

    property string label: ""
    property string value: "—"
    property string unit: ""

    height: units.gu(9)
    radius: units.gu(0.5)
    color: theme.palette.normal.foreground

    Column {
        anchors {
            fill: parent
            margins: units.gu(1)
        }
        spacing: units.gu(0.4)

        Label {
            text: root.label
            fontSize: "small"
            color: theme.palette.normal.backgroundText
            opacity: 0.75
            elide: Text.ElideRight
            width: parent.width
        }

        Row {
            spacing: units.gu(0.3)

            Label {
                id: valueLabel
                text: root.value
                fontSize: "x-large"
                color: theme.palette.normal.backgroundText
            }

            Label {
                text: root.unit
                fontSize: "small"
                color: theme.palette.normal.backgroundText
                opacity: 0.75
                height: valueLabel.height
                verticalAlignment: Text.AlignBottom
            }
        }
    }
}
