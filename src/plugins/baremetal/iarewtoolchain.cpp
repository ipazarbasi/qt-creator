/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "baremetalconstants.h"
#include "iarewtoolchain.h"

#include <projectexplorer/abiwidget.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmacro.h>
#include <projectexplorer/toolchainmanager.h>

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/synchronousprocess.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTemporaryFile>

using namespace ProjectExplorer;
using namespace Utils;

namespace BareMetal {
namespace Internal {

// Helpers:

static const char compilerCommandKeyC[] = "BareMetal.IarToolChain.CompilerPath";
static const char targetAbiKeyC[] = "BareMetal.IarToolChain.TargetAbi";

static bool isCompilerExists(const FileName &compilerPath)
{
    const QFileInfo fi = compilerPath.toFileInfo();
    return fi.exists() && fi.isExecutable() && fi.isFile();
}

static Macros dumpPredefinedMacros(const FileName &compiler, const QStringList &env)
{
    if (compiler.isEmpty() || !compiler.toFileInfo().isExecutable())
        return {};

    // IAR compiler requires an input and output files.

    QTemporaryFile fakeIn;
    if (!fakeIn.open())
        return {};
    fakeIn.close();

    const QString outpath = fakeIn.fileName() + ".tmp";

    SynchronousProcess cpp;
    cpp.setEnvironment(env);
    cpp.setTimeoutS(10);

    QStringList arguments;
    arguments.push_back(fakeIn.fileName());
    arguments.push_back("--predef_macros");
    arguments.push_back(outpath);

    const SynchronousProcessResponse response = cpp.runBlocking(compiler.toString(), arguments);
    if (response.result != SynchronousProcessResponse::Finished
            || response.exitCode != 0) {
        qWarning() << response.exitMessage(compiler.toString(), 10);
        return {};
    }

    QByteArray output;
    QFile fakeOut(outpath);
    if (fakeOut.open(QIODevice::ReadOnly))
        output = fakeOut.readAll();
    fakeOut.remove();

    return Macro::toMacros(output);
}

static Abi::Architecture guessArchitecture(const Macros &macros)
{
    for (const Macro &macro : macros) {
        if (macro.key == "__ICCARM__")
            return Abi::Architecture::ArmArchitecture;
        if (macro.key == "__ICC8051__")
            return Abi::Architecture::Mcs51Architecture;
        if (macro.key == "__ICCAVR__")
            return Abi::Architecture::AvrArchitecture;
    }
    return Abi::Architecture::UnknownArchitecture;
}

static unsigned char guessWordWidth(const Macros &macros)
{
    const Macro sizeMacro = Utils::findOrDefault(macros, [](const Macro &m) {
        return m.key == "__INT_SIZE__";
    });
    if (sizeMacro.isValid() && sizeMacro.type == MacroType::Define)
        return sizeMacro.value.toInt() * 8;
    return 0;
}

static Abi::BinaryFormat guessFormat(Abi::Architecture arch)
{
    if (arch == Abi::Architecture::ArmArchitecture)
        return Abi::BinaryFormat::ElfFormat;
    if (arch == Abi::Architecture::Mcs51Architecture
            || arch == Abi::Architecture::AvrArchitecture) {
        return Abi::BinaryFormat::UbrofFormat;
    }
    return Abi::BinaryFormat::UnknownFormat;
}

static Abi guessAbi(const Macros &macros)
{
    const auto arch = guessArchitecture(macros);
    return {arch, Abi::OS::BareMetalOS, Abi::OSFlavor::GenericFlavor,
            guessFormat(arch), guessWordWidth(macros)};
}

static QString buildDisplayName(Abi::Architecture arch, Core::Id language,
                                const QString &version)
{
    return IarToolChain::tr("IAREW %1 (%2, %3)")
            .arg(version, language.toString(), Abi::toString(arch));
}

// IarToolChain

IarToolChain::IarToolChain(Detection d) :
    ToolChain(Constants::IAREW_TOOLCHAIN_TYPEID, d),
    m_predefinedMacrosCache(std::make_shared<Cache<MacroInspectionReport, 64>>())
{ }

IarToolChain::IarToolChain(Core::Id language, Detection d) :
    IarToolChain(d)
{
    setLanguage(language);
}

QString IarToolChain::typeDisplayName() const
{
    return Internal::IarToolChainFactory::tr("IAREW");
}

void IarToolChain::setTargetAbi(const Abi &abi)
{
    if (abi == m_targetAbi)
        return;
    m_targetAbi = abi;
    toolChainUpdated();
}

Abi IarToolChain::targetAbi() const
{
    return m_targetAbi;
}

bool IarToolChain::isValid() const
{
    return true;
}

ToolChain::MacroInspectionRunner IarToolChain::createMacroInspectionRunner() const
{
    Environment env = Environment::systemEnvironment();
    addToEnvironment(env);

    const Utils::FileName compilerCommand = m_compilerCommand;
    const Core::Id lang = language();

    MacrosCache macrosCache = m_predefinedMacrosCache;

    return [env, compilerCommand, macrosCache, lang]
            (const QStringList &flags) {
        Q_UNUSED(flags)

        const Macros macros = dumpPredefinedMacros(compilerCommand, env.toStringList());
        const auto report = MacroInspectionReport{macros, languageVersion(lang, macros)};
        macrosCache->insert({}, report);

        return report;
    };
}

Macros IarToolChain::predefinedMacros(const QStringList &cxxflags) const
{
    return createMacroInspectionRunner()(cxxflags).macros;
}

Utils::LanguageExtensions IarToolChain::languageExtensions(const QStringList &) const
{
    return LanguageExtension::None;
}

WarningFlags IarToolChain::warningFlags(const QStringList &cxxflags) const
{
    Q_UNUSED(cxxflags);
    return WarningFlags::Default;
}

ToolChain::BuiltInHeaderPathsRunner IarToolChain::createBuiltInHeaderPathsRunner() const
{
    return {};
}

HeaderPaths IarToolChain::builtInHeaderPaths(const QStringList &cxxFlags,
                                             const FileName &fileName) const
{
    Q_UNUSED(cxxFlags)
    Q_UNUSED(fileName)
    return {};
}

void IarToolChain::addToEnvironment(Environment &env) const
{
    if (!m_compilerCommand.isEmpty()) {
        const FileName path = m_compilerCommand.parentDir();
        env.prependOrSetPath(path.toString());
    }
}

IOutputParser *IarToolChain::outputParser() const
{
    return nullptr;
}

QVariantMap IarToolChain::toMap() const
{
    QVariantMap data = ToolChain::toMap();
    data.insert(compilerCommandKeyC, m_compilerCommand.toString());
    data.insert(targetAbiKeyC, m_targetAbi.toString());
    return data;
}

bool IarToolChain::fromMap(const QVariantMap &data)
{
    if (!ToolChain::fromMap(data))
        return false;
    m_compilerCommand = FileName::fromString(data.value(compilerCommandKeyC).toString());
    m_targetAbi = Abi::fromString(data.value(targetAbiKeyC).toString());
    return true;
}

std::unique_ptr<ToolChainConfigWidget> IarToolChain::createConfigurationWidget()
{
    return std::make_unique<IarToolChainConfigWidget>(this);
}

bool IarToolChain::operator==(const ToolChain &other) const
{
    if (!ToolChain::operator==(other))
        return false;

    const auto customTc = static_cast<const IarToolChain *>(&other);
    return m_compilerCommand == customTc->m_compilerCommand
            && m_targetAbi == customTc->m_targetAbi
            ;
}

void IarToolChain::setCompilerCommand(const FileName &file)
{
    if (file == m_compilerCommand)
        return;
    m_compilerCommand = file;
    toolChainUpdated();
}

FileName IarToolChain::compilerCommand() const
{
    return m_compilerCommand;
}

QString IarToolChain::makeCommand(const Environment &env) const
{
    Q_UNUSED(env)
    return {};
}

ToolChain *IarToolChain::clone() const
{
    return new IarToolChain(*this);
}

void IarToolChain::toolChainUpdated()
{
    m_predefinedMacrosCache->invalidate();
    ToolChain::toolChainUpdated();
}

// IarToolChainFactory

IarToolChainFactory::IarToolChainFactory()
{
    setDisplayName(tr("IAREW"));
}

QSet<Core::Id> IarToolChainFactory::supportedLanguages() const
{
    return {ProjectExplorer::Constants::C_LANGUAGE_ID,
            ProjectExplorer::Constants::CXX_LANGUAGE_ID};
}

QList<ToolChain *> IarToolChainFactory::autoDetect(const QList<ToolChain *> &alreadyKnown)
{
    Candidates candidates;

#ifdef Q_OS_WIN

#ifdef Q_OS_WIN64
    static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\IAR Systems\\Embedded Workbench";
#else
    static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\IAR Systems\\Embedded Workbench";
#endif

    // Dictionary for know toolchains.
    static const struct Entry {
        QString registryKey;
        QString subExePath;
    } knowToolchains[] = {
        {"EWARM", "\\arm\\bin\\iccarm.exe"},
        {"EWAVR", "\\avr\\bin\\iccavr.exe"},
        {"EW8051", "\\8051\\bin\\icc8051.exe"},
    };

    QSettings registry(kRegistryNode, QSettings::NativeFormat);
    const auto oneLevelGroups = registry.childGroups();
    for (const QString &oneLevelKey : oneLevelGroups) {
        registry.beginGroup(oneLevelKey);
        const auto twoLevelGroups = registry.childGroups();
        for (const Entry &entry : knowToolchains) {
            if (twoLevelGroups.contains(entry.registryKey)) {
                registry.beginGroup(entry.registryKey);
                const auto threeLevelGroups = registry.childGroups();
                for (const QString &threeLevelKey : threeLevelGroups) {
                    registry.beginGroup(threeLevelKey);
                    QString compilerPath = registry.value("InstallPath").toString();
                    if (!compilerPath.isEmpty()) {
                        // Build full compiler path.
                        compilerPath += entry.subExePath;
                        const FileName fn = FileName::fromString(compilerPath);
                        if (isCompilerExists(fn)) {
                            // Note: threeLevelKey is a guessed toolchain version.
                            candidates.push_back({fn, threeLevelKey});
                        }
                    }
                    registry.endGroup();
                }
                registry.endGroup();
            }
        }
        registry.endGroup();
    }

#endif // Q_OS_WIN

    return autoDetectToolchains(candidates, alreadyKnown);
}

bool IarToolChainFactory::canCreate()
{
    return true;
}

ToolChain *IarToolChainFactory::create(Core::Id language)
{
    return new IarToolChain(language, ToolChain::ManualDetection);
}

bool IarToolChainFactory::canRestore(const QVariantMap &data)
{
    return typeIdFromMap(data) == Constants::IAREW_TOOLCHAIN_TYPEID;
}

ToolChain *IarToolChainFactory::restore(const QVariantMap &data)
{
    const auto tc = new IarToolChain(ToolChain::ManualDetection);
    if (tc->fromMap(data))
        return tc;

    delete tc;
    return nullptr;
}

QList<ToolChain *> IarToolChainFactory::autoDetectToolchains(
        const Candidates &candidates, const QList<ToolChain *> &alreadyKnown) const
{
    QList<ToolChain *> result;

    for (const Candidate &candidate : qAsConst(candidates)) {
        const QList<ToolChain *> filtered = Utils::filtered(
                    alreadyKnown, [candidate](ToolChain *tc) {
            return tc->typeId() == Constants::IAREW_TOOLCHAIN_TYPEID
                && tc->compilerCommand() == candidate.first
                && (tc->language() == ProjectExplorer::Constants::C_LANGUAGE_ID
                    || tc->language() == ProjectExplorer::Constants::CXX_LANGUAGE_ID);
        });

        if (!filtered.isEmpty()) {
            result << filtered;
            continue;
        }

        // Create toolchains for both C and C++ languages.
        result << autoDetectToolchain(candidate, ProjectExplorer::Constants::C_LANGUAGE_ID);
        result << autoDetectToolchain(candidate, ProjectExplorer::Constants::CXX_LANGUAGE_ID);
    }

    return result;
}

QList<ToolChain *> IarToolChainFactory::autoDetectToolchain(
        const Candidate &candidate, Core::Id language) const
{
    const auto env = Environment::systemEnvironment();
    const Macros macros = dumpPredefinedMacros(candidate.first, env.toStringList());
    if (macros.isEmpty())
        return {};
    const Abi abi = guessAbi(macros);

    const auto tc = new IarToolChain(ToolChain::AutoDetection);
    tc->setLanguage(language);
    tc->setCompilerCommand(candidate.first);
    tc->setTargetAbi(abi);
    tc->setDisplayName(buildDisplayName(abi.architecture(), language, candidate.second));

    const auto languageVersion = ToolChain::languageVersion(language, macros);
    tc->m_predefinedMacrosCache->insert({}, {macros, languageVersion});
    return {tc};
}

// IarToolChainConfigWidget

IarToolChainConfigWidget::IarToolChainConfigWidget(IarToolChain *tc) :
    ToolChainConfigWidget(tc),
    m_compilerCommand(new PathChooser),
    m_abiWidget(new AbiWidget)
{
    m_compilerCommand->setExpectedKind(PathChooser::ExistingCommand);
    m_compilerCommand->setHistoryCompleter("PE.IAREW.Command.History");
    m_mainLayout->addRow(tr("&Compiler path:"), m_compilerCommand);
    m_mainLayout->addRow(tr("&ABI:"), m_abiWidget);

    m_abiWidget->setEnabled(false);

    addErrorLabel();
    setFromToolchain();

    connect(m_compilerCommand, &PathChooser::rawPathChanged,
            this, &IarToolChainConfigWidget::handleCompilerCommandChange);
    connect(m_abiWidget, &AbiWidget::abiChanged,
            this, &ToolChainConfigWidget::dirty);
}

void IarToolChainConfigWidget::applyImpl()
{
    if (toolChain()->isAutoDetected())
        return;

    const auto tc = static_cast<IarToolChain *>(toolChain());
    const QString displayName = tc->displayName();
    tc->setCompilerCommand(m_compilerCommand->fileName());
    tc->setTargetAbi(m_abiWidget->currentAbi());
    tc->setDisplayName(displayName);

    if (m_macros.isEmpty())
        return;

    const auto languageVersion = ToolChain::languageVersion(tc->language(), m_macros);
    tc->m_predefinedMacrosCache->insert({}, {m_macros, languageVersion});

    setFromToolchain();
}

bool IarToolChainConfigWidget::isDirtyImpl() const
{
    const auto tc = static_cast<IarToolChain *>(toolChain());
    return m_compilerCommand->fileName() != tc->compilerCommand()
            || m_abiWidget->currentAbi() != tc->targetAbi()
            ;
}

void IarToolChainConfigWidget::makeReadOnlyImpl()
{
    m_compilerCommand->setReadOnly(true);
    m_abiWidget->setEnabled(false);
}

void IarToolChainConfigWidget::setFromToolchain()
{
    const QSignalBlocker blocker(this);
    const auto tc = static_cast<IarToolChain *>(toolChain());
    m_compilerCommand->setFileName(tc->compilerCommand());
    m_abiWidget->setAbis({}, tc->targetAbi());
    const bool haveCompiler = isCompilerExists(m_compilerCommand->fileName());
    m_abiWidget->setEnabled(haveCompiler && !tc->isAutoDetected());
}

void IarToolChainConfigWidget::handleCompilerCommandChange()
{
    const FileName compilerPath = m_compilerCommand->fileName();
    const bool haveCompiler = isCompilerExists(compilerPath);
    if (haveCompiler) {
        const auto env = Environment::systemEnvironment();
        m_macros = dumpPredefinedMacros(compilerPath, env.toStringList());
        const Abi guessed = guessAbi(m_macros);
        m_abiWidget->setAbis({}, guessed);
    }

    m_abiWidget->setEnabled(haveCompiler);
    emit dirty();
}

} // namespace Internal
} // namespace BareMetal
