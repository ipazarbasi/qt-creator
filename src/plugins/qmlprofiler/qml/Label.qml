/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

import QtQuick 1.0

Item {
    property alias text: txt.text
    property bool expanded: false
    property int typeIndex: index

    property variant descriptions: [text]

    height: root.singleRowHeight
    width: 150

    onExpandedChanged: {
        var rE = labels.rowExpanded;
        rE[typeIndex] = expanded;
        labels.rowExpanded = rE;
        backgroundMarks.requestRedraw();
        view.rowExpanded(typeIndex, expanded);
        updateHeight();
    }

    Component.onCompleted: {
        updateHeight();
    }

    function updateHeight() {
        height = root.singleRowHeight *
            (expanded ? qmlEventList.uniqueEventsOfType(typeIndex) : qmlEventList.maxNestingForType(typeIndex));
    }

    Connections {
        target: qmlEventList
        onDataReady: {
            var desc=[];
            for (var i=0; i<qmlEventList.uniqueEventsOfType(typeIndex); i++)
                desc[i] = qmlEventList.eventTextForType(typeIndex, i);
            // special case: empty
            if (desc.length == 1 && desc[0]=="")
                desc[0] = text;
            descriptions = desc;
            updateHeight();
        }
        onDataClear: {
            descriptions = [text];
            updateHeight();
        }
    }

    Text {
        id: txt
        visible: !expanded
        x: 5
        font.pixelSize: 12
        color: "#232323"
        anchors.verticalCenter: parent.verticalCenter
    }

    Rectangle {
        height: 1
        width: parent.width
        color: "#cccccc"
        anchors.bottom: parent.bottom
    }

    Column {
        visible: expanded
        Repeater {
            model: descriptions.length
            Text {
                height: root.singleRowHeight
                x: 5
                width: 140
                text: descriptions[index]
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Image {
        source: expanded ? "arrow_down.png" : "arrow_right.png"
        x: parent.width - 12
        y: 2
        MouseArea {
            anchors.fill: parent
            onClicked: {
                expanded = !expanded;
            }
        }
    }
}
