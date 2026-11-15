import QtQuick 2.9
import Lomiri.Components 1.3

Rectangle {
    id: root

    property real percentage: 0
    property string statusKey: "unknown"
    property string statusLabel: ""
    property string timeText: ""

    height: units.gu(24)
    radius: units.gu(0.5)
    color: theme.palette.normal.foreground

    Canvas {
        id: ring
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) - units.gu(2)
        height: width

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var center = width / 2;
            var radius = width / 2 - units.gu(0.7);
            var lineWidth = units.gu(0.8);
            ctx.lineWidth = lineWidth;
            ctx.lineCap = "round";

            ctx.strokeStyle = theme.palette.normal.base;
            ctx.beginPath();
            ctx.arc(center, center, radius, 0, Math.PI * 2);
            ctx.stroke();

            var progress = Math.max(0, Math.min(1, root.percentage / 100));
            if (root.statusKey === "charging" || root.statusKey === "full")
                ctx.strokeStyle = theme.palette.normal.positive;
            else if (root.percentage < 15)
                ctx.strokeStyle = theme.palette.normal.negative;
            else
                ctx.strokeStyle = theme.palette.normal.backgroundText;

            ctx.beginPath();
            ctx.arc(center, center, radius, -Math.PI / 2,
                    -Math.PI / 2 + progress * Math.PI * 2);
            ctx.stroke();
        }

        Connections {
            target: root
            function onPercentageChanged() { ring.requestPaint() }
            function onStatusKeyChanged() { ring.requestPaint() }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Column {
        anchors.centerIn: parent
        width: Math.max(units.gu(8), ring.width * 0.68)
        spacing: units.gu(0.4)

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: Math.round(root.percentage) + "%"
            fontSize: "xx-large"
            color: theme.palette.normal.backgroundText
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.statusLabel
            fontSize: "medium"
            color: theme.palette.normal.backgroundText
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.timeText
            fontSize: "small"
            color: theme.palette.normal.backgroundText
            opacity: 0.8
            visible: text.length > 0
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
