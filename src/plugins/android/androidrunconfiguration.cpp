/****************************************************************************
**
** Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
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

#include "androidrunconfiguration.h"
#include "androidglobal.h"
#include "androidtoolchain.h"
#include "androidmanager.h"
#include "androidrunconfigurationwidget.h"

#include <projectexplorer/kitinformation.h>
#include <projectexplorer/target.h>
#include <qtsupport/qtoutputformatter.h>
#include <qtsupport/qtkitinformation.h>

#include <utils/qtcassert.h>

using namespace ProjectExplorer;

namespace Android {
using namespace Internal;
const char amStartArgsKey[] = "Android.AmStartArgsKey";

AndroidRunConfiguration::AndroidRunConfiguration(Target *parent, Core::Id id)
    : RunConfiguration(parent, id)
{
}

AndroidRunConfiguration::AndroidRunConfiguration(Target *parent, AndroidRunConfiguration *source)
    : RunConfiguration(parent, source)
{
}

void AndroidRunConfiguration::setAmStartExtraArgs(const QStringList &args)
{
    m_amStartExtraArgs = args;
}

QWidget *AndroidRunConfiguration::createConfigurationWidget()
{
    auto configWidget = new AndroidRunConfigurationWidget();
    configWidget->setAmStartArgs(m_amStartExtraArgs);
    connect(configWidget, &AndroidRunConfigurationWidget::amStartArgsChanged,
            this, &AndroidRunConfiguration::setAmStartExtraArgs);
    return configWidget;
}

Utils::OutputFormatter *AndroidRunConfiguration::createOutputFormatter() const
{
    return new QtSupport::QtOutputFormatter(target()->project());
}

bool AndroidRunConfiguration::fromMap(const QVariantMap &map)
{
    m_amStartExtraArgs = map.value(amStartArgsKey).toStringList();
    return RunConfiguration::fromMap(map);
}

QVariantMap AndroidRunConfiguration::toMap() const
{
    QVariantMap res = RunConfiguration::toMap();
    res[amStartArgsKey] = m_amStartExtraArgs;
    return res;
}

const QStringList &AndroidRunConfiguration::amStartExtraArgs() const
{
    return m_amStartExtraArgs;
}
} // namespace Android
