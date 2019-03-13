/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#pragma once

#include "internalproperty.h"


namespace QmlDesigner {
namespace Internal {

class InternalNodeAbstractProperty : public InternalProperty
{
    friend class InternalNode;

public:
    using Pointer = QSharedPointer<InternalNodeAbstractProperty>;
    using WeakPointer = QWeakPointer<InternalNodeAbstractProperty>;

    bool isNodeAbstractProperty() const override;

    virtual QList<InternalNodePointer> allSubNodes() const = 0;
    virtual QList<InternalNodePointer> directSubNodes() const = 0;

    virtual bool isEmpty() const = 0;
    virtual int count() const = 0;
    virtual int indexOf(const InternalNodePointer &node) const = 0;

    bool isValid() const override;

    using InternalProperty::remove; // keep the virtual remove(...) function around

protected:
    InternalNodeAbstractProperty(const PropertyName &name, const InternalNodePointer &propertyOwner);
    virtual void remove(const InternalNodePointer &node) = 0;
    virtual void add(const InternalNodePointer &node) = 0;
};

} // namespace Internal
} // namespace QmlDesigner
