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

#include "refactoringengine.h"

#include "projectpartutilities.h"


#include <refactoringserverinterface.h>
#include <requestsourcelocationforrenamingmessage.h>

#include <cpptools/clangcompileroptionsbuilder.h>
#include <cpptools/cpptoolsreuse.h>

#include <texteditor/textdocument.h>

#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

namespace ClangRefactoring {

using ClangBackEnd::RequestSourceLocationsForRenamingMessage;

RefactoringEngine::RefactoringEngine(ClangBackEnd::RefactoringServerInterface &server,
                                     ClangBackEnd::RefactoringClientInterface &client,
                                     ClangBackEnd::FilePathCachingInterface &filePathCache)
    : m_server(server),
      m_client(client),
      m_filePathCache(filePathCache)
{
}

void RefactoringEngine::startLocalRenaming(const CppTools::CursorInEditor &data,
                                           CppTools::ProjectPart *projectPart,
                                           RenameCallback &&renameSymbolsCallback)
{
    using CppTools::ClangCompilerOptionsBuilder;

    setUsable(false);

    m_client.setLocalRenamingCallback(std::move(renameSymbolsCallback));

    QString filePath = data.filePath().toString();
    QTextCursor textCursor = data.cursor();
    ClangCompilerOptionsBuilder clangCOBuilder{*projectPart, CLANG_VERSION, CLANG_RESOURCE_DIR};
    Utils::SmallStringVector commandLine{clangCOBuilder.build(
                    fileKindInProjectPart(projectPart, filePath),
                    CppTools::getPchUsage())};

    commandLine.push_back(filePath);

    RequestSourceLocationsForRenamingMessage message(ClangBackEnd::FilePath(filePath),
                                                     uint(textCursor.blockNumber() + 1),
                                                     uint(textCursor.positionInBlock() + 1),
                                                     textCursor.document()->toPlainText(),
                                                     std::move(commandLine),
                                                     textCursor.document()->revision());


    m_server.requestSourceLocationsForRenamingMessage(std::move(message));
}

void RefactoringEngine::startGlobalRenaming(const CppTools::CursorInEditor &)
{
    // TODO: implement
}

bool RefactoringEngine::isUsable() const
{
    return m_server.isUsable();
}

void RefactoringEngine::setUsable(bool isUsable)
{
    m_server.setUsable(isUsable);
}

} // namespace ClangRefactoring
