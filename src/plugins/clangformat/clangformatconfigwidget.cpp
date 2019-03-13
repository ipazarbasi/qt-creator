/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "clangformatconfigwidget.h"

#include "clangformatconstants.h"
#include "clangformatindenter.h"
#include "clangformatsettings.h"
#include "clangformatutils.h"
#include "ui_clangformatconfigwidget.h"

#include <clang/Format/Format.h>

#include <coreplugin/icore.h>
#include <cppeditor/cpphighlighter.h>
#include <cpptools/cppcodestylesnippets.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <texteditor/displaysettings.h>
#include <texteditor/snippets/snippeteditor.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditorsettings.h>

#include <QFile>
#include <QMessageBox>

#include <sstream>

using namespace ProjectExplorer;

namespace ClangFormat {

ClangFormatConfigWidget::ClangFormatConfigWidget(ProjectExplorer::Project *project, QWidget *parent)
    : CodeStyleEditorWidget(parent)
    , m_project(project)
    , m_ui(std::make_unique<Ui::ClangFormatConfigWidget>())
{
    m_ui->setupUi(this);

    initialize();
}

void ClangFormatConfigWidget::hideGlobalCheckboxes()
{
    m_ui->formatAlways->hide();
    m_ui->formatWhileTyping->hide();
    m_ui->formatOnSave->hide();
}

void ClangFormatConfigWidget::showGlobalCheckboxes()
{
    m_ui->formatAlways->setChecked(ClangFormatSettings::instance().formatCodeInsteadOfIndent());
    m_ui->formatAlways->show();

    m_ui->formatWhileTyping->setChecked(ClangFormatSettings::instance().formatWhileTyping());
    m_ui->formatWhileTyping->show();

    m_ui->formatOnSave->setChecked(ClangFormatSettings::instance().formatOnSave());
    m_ui->formatOnSave->show();
}

void ClangFormatConfigWidget::initialize()
{
    m_ui->projectHasClangFormat->show();
    m_ui->clangFormatOptionsTable->show();
    m_ui->applyButton->show();
    hideGlobalCheckboxes();

    m_preview = new TextEditor::SnippetEditorWidget(this);
    m_ui->horizontalLayout_2->addWidget(m_preview);
    m_preview->setPlainText(QLatin1String(CppTools::Constants::DEFAULT_CODE_STYLE_SNIPPETS[0]));
    m_preview->textDocument()->setIndenter(new ClangFormatIndenter(m_preview->document()));
    m_preview->textDocument()->setFontSettings(TextEditor::TextEditorSettings::fontSettings());
    m_preview->textDocument()->setSyntaxHighlighter(new CppEditor::CppHighlighter);

    TextEditor::DisplaySettings displaySettings = m_preview->displaySettings();
    displaySettings.m_visualizeWhitespace = true;
    m_preview->setDisplaySettings(displaySettings);

    QLayoutItem *lastItem = m_ui->verticalLayout->itemAt(m_ui->verticalLayout->count() - 1);
    if (lastItem->spacerItem())
        m_ui->verticalLayout->removeItem(lastItem);

    if (m_project
        && !m_project->projectDirectory().appendPath(Constants::SETTINGS_FILE_NAME).exists()) {
        m_ui->projectHasClangFormat->setText(tr("No .clang-format file for the project."));
        m_ui->clangFormatOptionsTable->hide();
        m_ui->applyButton->hide();
        m_ui->verticalLayout->addStretch(1);

        connect(m_ui->createFileButton, &QPushButton::clicked, this, [this]() {
            createStyleFileIfNeeded(false);
            initialize();
        });
        return;
    }

    m_ui->createFileButton->hide();

    Utils::FileName fileName;
    if (m_project) {
        m_ui->projectHasClangFormat->hide();
        connect(m_ui->applyButton, &QPushButton::clicked, this, &ClangFormatConfigWidget::apply);
        fileName = m_project->projectFilePath().appendPath("snippet.cpp");
    } else {
        const Project *currentProject = SessionManager::startupProject();
        if (!currentProject
            || !currentProject->projectDirectory()
                    .appendPath(Constants::SETTINGS_FILE_NAME)
                    .exists()) {
            m_ui->projectHasClangFormat->hide();
        } else {
            m_ui->projectHasClangFormat->setText(
                tr("Current project has its own .clang-format file "
                   "and can be configured in Projects > Code Style > C++."));
        }
        createStyleFileIfNeeded(true);
        showGlobalCheckboxes();
        m_ui->applyButton->hide();
        fileName = Utils::FileName::fromString(Core::ICore::userResourcePath())
                       .appendPath("snippet.cpp");
    }

    m_preview->textDocument()->indenter()->setFileName(fileName);
    fillTable();
    updatePreview();
}

void ClangFormatConfigWidget::updatePreview()
{
    QTextCursor cursor(m_preview->document());
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    m_preview->textDocument()->autoFormatOrIndent(cursor);
}

void ClangFormatConfigWidget::fillTable()
{
    clang::format::FormatStyle style = m_project ? currentProjectStyle() : currentGlobalStyle();

    std::string configText = clang::format::configurationAsText(style);
    m_ui->clangFormatOptionsTable->setPlainText(QString::fromStdString(configText));
}

ClangFormatConfigWidget::~ClangFormatConfigWidget() = default;

void ClangFormatConfigWidget::apply()
{
    if (!m_project) {
        ClangFormatSettings &settings = ClangFormatSettings::instance();
        settings.setFormatCodeInsteadOfIndent(m_ui->formatAlways->isChecked());
        settings.setFormatWhileTyping(m_ui->formatWhileTyping->isChecked());
        settings.setFormatOnSave(m_ui->formatOnSave->isChecked());
        settings.write();
    }

    const QString text = m_ui->clangFormatOptionsTable->toPlainText();
    clang::format::FormatStyle style;
    style.Language = clang::format::FormatStyle::LK_Cpp;
    const std::error_code error = clang::format::parseConfiguration(text.toStdString(), &style);
    if (error.value() != static_cast<int>(clang::format::ParseError::Success)) {
        QMessageBox::warning(this,
                             tr("Error in ClangFormat configuration"),
                             QString::fromStdString(error.message()));
        fillTable();
        updatePreview();
        return;
    }

    QString filePath;
    if (m_project)
        filePath = m_project->projectDirectory().appendPath(Constants::SETTINGS_FILE_NAME).toString();
    else
        filePath = Core::ICore::userResourcePath() + "/" + Constants::SETTINGS_FILE_NAME;
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
        return;

    file.write(text.toUtf8());
    file.close();

    updatePreview();
}

} // namespace ClangFormat
