/****************************************************************************
**
** Copyright (C) 2016 Jochen Becher
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "connectionitem.h"

#include "qmt/diagram_controller/diagramcontroller.h"
#include "qmt/diagram/dconnection.h"
#include "qmt/diagram_scene/capabilities/intersectionable.h"
#include "qmt/diagram_scene/diagramscenemodel.h"
#include "qmt/diagram_scene/parts/arrowitem.h"
#include "qmt/infrastructure/geometryutilities.h"
#include "qmt/infrastructure/qmtassert.h"
#include "qmt/style/style.h"

#include <QGraphicsScene>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QVector2D>
#include <QPair>

namespace qmt {

ConnectionItem::ConnectionItem(DConnection *connection, DiagramSceneModel *diagramSceneModel, QGraphicsItem *parent)
    : RelationItem(connection, diagramSceneModel, parent),
      m_connection(connection)
{
}

ConnectionItem::~ConnectionItem()
{
}

void ConnectionItem::update(const Style *style)
{
    RelationItem::update(style);

    updateEndLabels(m_connection->endA(), m_connection->endB(), &m_endAName, &m_endACardinality, style);
    updateEndLabels(m_connection->endB(), m_connection->endA(), &m_endBName, &m_endBCardinality, style);

    QMT_CHECK(m_arrow);
    QGraphicsItem *endAItem = m_diagramSceneModel->graphicsItem(m_connection->endAUid());
    if (!endAItem)
        return;
    placeEndLabels(m_arrow->firstLineSegment(), m_endAName, m_endACardinality, endAItem, m_arrow->startHeadLength());
    QGraphicsItem *endBItem = m_diagramSceneModel->graphicsItem(m_connection->endBUid());
    if (!endBItem)
        return;
    placeEndLabels(m_arrow->lastLineSegment(), m_endBName, m_endBCardinality, endBItem, m_arrow->endHeadLength());
}

void ConnectionItem::updateEndLabels(const DConnectionEnd &end, const DConnectionEnd &otherEnd,
                                      QGraphicsSimpleTextItem **endName, QGraphicsSimpleTextItem **endCardinality,
                                      const Style *style)
{
    Q_UNUSED(end);

    if (!otherEnd.name().isEmpty()) {
        if (!*endName)
            *endName = new QGraphicsSimpleTextItem(this);
        (*endName)->setFont(style->smallFont());
        (*endName)->setBrush(style->textBrush());
        (*endName)->setText(otherEnd.name());
    } else if (*endName) {
        (*endName)->scene()->removeItem(*endName);
        delete *endName;
        *endName = 0;
    }

    if (!otherEnd.cardinality().isEmpty()) {
        if (!*endCardinality)
            *endCardinality = new QGraphicsSimpleTextItem(this);
        (*endCardinality)->setFont(style->smallFont());
        (*endCardinality)->setBrush(style->textBrush());
        (*endCardinality)->setText(otherEnd.cardinality());
    } else if (*endCardinality) {
        (*endCardinality)->scene()->removeItem(*endCardinality);
        delete *endCardinality;
        *endCardinality = 0;
    }
}

void ConnectionItem::placeEndLabels(const QLineF &lineSegment, QGraphicsItem *endName, QGraphicsItem *endCardinality,
                                     QGraphicsItem *endItem, double headLength)
{
    const double HEAD_OFFSET = headLength + 6.0;
    const double SIDE_OFFSET = 4.0;
    QPointF headOffset = QPointF(HEAD_OFFSET, 0);
    QPointF sideOffset = QPointF(0.0, SIDE_OFFSET);

    double angle = GeometryUtilities::calcAngle(lineSegment);
    if (angle >= -5 && angle <= 5) {
        if (endName)
            endName->setPos(lineSegment.p1() + headOffset + sideOffset);
        if (endCardinality)
            endCardinality->setPos(lineSegment.p1() + headOffset - sideOffset
                                   - endCardinality->boundingRect().bottomLeft());
    } else if (angle <= -175 || angle >= 175) {
        if (endName)
            endName->setPos(lineSegment.p1() - headOffset + sideOffset - endName->boundingRect().topRight());
        if (endCardinality)
            endCardinality->setPos(lineSegment.p1() - headOffset
                                   - sideOffset - endCardinality->boundingRect().bottomRight());
    } else {
        QRectF rect;
        if (endCardinality)
            rect = endCardinality->boundingRect();
        if (endName)
            rect = rect.united(endName->boundingRect().translated(rect.bottomLeft()));

        QPointF rectPlacement;
        GeometryUtilities::Side alignedSide = GeometryUtilities::SideUnspecified;

        if (auto objectItem = dynamic_cast<IIntersectionable *>(endItem)) {
            QPointF intersectionPoint;
            QLineF intersectionLine;

            if (objectItem->intersectShapeWithLine(GeometryUtilities::stretch(lineSegment.translated(pos()), 2.0, 0.0),
                                                   &intersectionPoint, &intersectionLine)) {
                if (!GeometryUtilities::placeRectAtLine(rect, lineSegment, HEAD_OFFSET, SIDE_OFFSET,
                                                        intersectionLine, &rectPlacement, &alignedSide)) {
                    rectPlacement = intersectionPoint;
                }
            } else {
                rectPlacement = lineSegment.p1();
            }
        } else {
            rectPlacement = endItem->pos();
        }

        if (endCardinality) {
            if (alignedSide == GeometryUtilities::SideRight)
                endCardinality->setPos(rectPlacement
                                       + QPointF(rect.width() - endCardinality->boundingRect().width(), 0.0));
            else
                endCardinality->setPos(rectPlacement);
            rectPlacement += endCardinality->boundingRect().bottomLeft();
        }
        if (endName) {
            if (alignedSide == GeometryUtilities::SideRight)
                endName->setPos(rectPlacement + QPointF(rect.width() - endName->boundingRect().width(), 0.0));
            else
                endName->setPos(rectPlacement);
        }
    }
}

} // namespace qmt
