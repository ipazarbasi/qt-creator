/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://www.qt.io/contact-us
**
** This file is part of the Qt Enterprise ClangStaticAnalyzer Add-on.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://www.qt.io/contact-us
**
****************************************************************************/

#ifndef CLANGSTATICANALYZERRUNNER_H
#define CLANGSTATICANALYZERRUNNER_H

#include <QString>
#include <QProcess>

#include <utils/environment.h>
#include <utils/qtcassert.h>

namespace ClangStaticAnalyzer {
namespace Internal {

QString finishedWithBadExitCode(int exitCode); // exposed for tests

class ClangStaticAnalyzerRunner : public QObject
{
    Q_OBJECT

public:
    ClangStaticAnalyzerRunner(const QString &clangExecutable,
                              const QString &clangLogFileDir,
                              const Utils::Environment &environment,
                              QObject *parent = 0);
    ~ClangStaticAnalyzerRunner();

    // compilerOptions is expected to contain everything except:
    //   (1) filePath, that is the file to analyze
    //   (2) -o output-file
    bool run(const QString &filePath, const QStringList &compilerOptions = QStringList());

    QString filePath() const;

signals:
    void started();
    void finishedWithSuccess(const QString &logFilePath);
    void finishedWithFailure(const QString &errorMessage, const QString &errorDetails);

private:
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();

    QString createLogFile(const QString &filePath) const;
    QString processCommandlineAndOutput() const;
    QString actualLogFile() const;

private:
    QString m_clangExecutable;
    QString m_clangLogFileDir;
    QString m_filePath;
    QString m_logFile;
    QString m_commandLine;
    QProcess m_process;
    QByteArray m_processOutput;
};

} // namespace Internal
} // namespace ClangStaticAnalyzer

#endif // CLANGSTATICANALYZERRUNNER_H