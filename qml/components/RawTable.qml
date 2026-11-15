import QtQuick 2.9
import Lomiri.Components 1.3

Item {
    id: root

    property var rows: []
    property string filterText: ""
    property var filteredRows: []
    property int totalCount: 0
    property int limit: 300

    implicitHeight: column.height

    function refilter() {
        var result = [];
        var needle = filterText.toLowerCase();

        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i];
            if (needle.length === 0
                    || row.supply.toLowerCase().indexOf(needle) !== -1
                    || row.name.toLowerCase().indexOf(needle) !== -1) {
                result.push(row);
            }
        }

        totalCount = result.length;
        filteredRows = result.slice(0, limit);
    }

    onRowsChanged: refilter()
    onFilterTextChanged: refilter()
    Component.onCompleted: refilter()

    Column {
        id: column
        width: parent.width
        spacing: units.gu(0.4)

        TextField {
            id: filterField
            width: parent.width
            placeholderText: i18n.tr("Filter by supply or attribute")
            onTextChanged: root.filterText = text
        }

        Row {
            width: parent.width
            spacing: units.gu(0.5)

            Label {
                width: parent.width * 0.18
                text: i18n.tr("Supply")
                fontSize: "x-small"
                font.weight: Font.DemiBold
                color: theme.palette.normal.backgroundText
                opacity: 0.7
            }
            Label {
                width: parent.width * 0.37
                text: i18n.tr("Attribute")
                fontSize: "x-small"
                font.weight: Font.DemiBold
                color: theme.palette.normal.backgroundText
                opacity: 0.7
            }
            Label {
                width: parent.width * 0.42
                text: i18n.tr("Value")
                fontSize: "x-small"
                font.weight: Font.DemiBold
                color: theme.palette.normal.backgroundText
                opacity: 0.7
            }
        }

        Repeater {
            model: root.filteredRows

            Row {
                width: column.width
                spacing: units.gu(0.5)
                height: Math.max(supplyLabel.height, nameLabel.height, valueEdit.height) + units.gu(0.2)

                Label {
                    id: supplyLabel
                    width: column.width * 0.18
                    text: modelData.supply
                    fontSize: "x-small"
                    color: theme.palette.normal.backgroundText
                    opacity: 0.75
                    elide: Text.ElideRight
                }

                Label {
                    id: nameLabel
                    width: column.width * 0.37
                    text: modelData.name
                    fontSize: "x-small"
                    font.family: "Ubuntu Mono"
                    color: theme.palette.normal.backgroundText
                    elide: Text.ElideRight
                }

                TextEdit {
                    id: valueEdit
                    width: column.width * 0.42
                    height: Math.max(contentHeight, units.gu(1.6))
                    text: modelData.value
                    readOnly: true
                    selectByMouse: true
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    font.family: "Ubuntu Mono"
                    font.pixelSize: units.dp(12)
                    color: theme.palette.normal.backgroundText
                }
            }
        }

        Label {
            width: parent.width
            visible: root.totalCount > root.filteredRows.length
            text: i18n.tr("Showing %1 of %2 rows").arg(root.filteredRows.length).arg(root.totalCount)
            fontSize: "x-small"
            color: theme.palette.normal.backgroundText
            opacity: 0.7
        }
    }
}
