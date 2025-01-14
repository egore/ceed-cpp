#include "src/Application.h"
#include "src/util/Settings.h"
#include "src/util/SettingsCategory.h"
#include "src/util/SettingsSection.h"
#include "src/util/SettingsEntry.h"
#include "src/util/Utils.h"
#include "src/util/descriptive_exception.h"
#include "src/editors/imageset/ImagesetEditor.h"
#include "src/editors/layout/LayoutEditor.h"
#include "src/editors/looknfeel/LookNFeelEditor.h"
#include "src/ui/dialogs/UpdateDialog.h"
#include <qsplashscreen.h>
#include <qsettings.h>
#include <qdir.h>
#include <qcommandlineparser.h>
#include <qaction.h>
#include <QtNetwork/qnetworkaccessmanager.h>
#include <QtNetwork/qnetworkreply.h>
#include <qurl.h>
#include <qmessagebox.h>
#include <qdesktopservices.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qversionnumber.h>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setOrganizationName("CEGUI");
    setOrganizationDomain("cegui.org.uk");
    setApplicationName("CEED - CEGUI editor");
    setApplicationVersion("1.1.2");

    Utils::registerFileAssociation("ceed", "CEGUI Project file", "text/xml", "text", 0);

    // Create settings and load all values from the persistence store
    _settings = new Settings(new QSettings("CEGUI", "CEED", this));
    createSettingsEntries();
    ImagesetEditor::createActions(*this);
    LayoutEditor::createActions(*this);

    // Finally read stored values into our new setting entries
    _settings->load();

    QSplashScreen* splash = nullptr;
    if (_settings->getEntryValue("global/app/show_splash").toBool())
    {
        splash = new QSplashScreen(QPixmap(":/images/splashscreen.png"));
        splash->setWindowModality(Qt::ApplicationModal);
        splash->setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);
        splash->showMessage("version: " + applicationVersion(), Qt::AlignTop | Qt::AlignRight, Qt::GlobalColor::white);

        splash->show();

        // This ensures that the splash screen is shown on all platforms
        processEvents();
    }

    _cmdLine = new QCommandLineParser();
    _cmdLine->setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    _cmdLine->addOptions(
    {
        { "updateResult", tr("Update result code, 0 if succeeded."), tr("updateResult") },
        { "updateMessage", tr("Update results messaged by an updater."), tr("updateMessage") },
    });
    _cmdLine->process(*this);

    _network = new QNetworkAccessManager(this);

    _mainWindow = new MainWindow();

    ImagesetEditor::createToolbar(*this);
    LayoutEditor::createToolbar(*this);

    if (splash)
    {
        splash->finish(_mainWindow);
        delete splash;
    }

    // Bring our application to front before we show any message box
    // TODO: leave only necessary calls
    _mainWindow->show();
    _mainWindow->raise();
    _mainWindow->activateWindow();
    _mainWindow->setWindowState(_mainWindow->windowState() | Qt::WindowState::WindowActive);

    checkUpdateResults();

    // Checking for updates is async, initialization will continue after it finishes
    checkForUpdates(false, [this]()
    {
        _mainWindow->setStatusMessage("");

        // Now we can load requested project, if any
        if (_cmdLine->positionalArguments().size() > 0)
        {
            // Load project specified in a command line
            _mainWindow->loadProject(_cmdLine->positionalArguments().first());
        }
        else
        {
            // Perform a startup action
            const auto action = _settings->getEntryValue("global/app/startup_action").toInt();
            switch (action)
            {
                case 1:
                {
                    if (_settings->getQSettings()->contains("lastProject"))
                    {
                        const QString lastProject = _settings->getQSettings()->value("lastProject").toString();
                        if (QFileInfo::exists(lastProject))
                            _mainWindow->loadProject(lastProject);
                    }
                    break;
                }
                default: break; // 0: empty environment
            }
        }
    });
}

Application::~Application()
{
    delete _mainWindow;
    delete _settings;
    delete _cmdLine;
}

SettingsSection* Application::getOrCreateShortcutSettingsSection(const QString& groupId, const QString& label)
{
    auto category = _settings->getCategory("shortcuts");
    if (!category) category = _settings->createCategory("shortcuts", "Shortcuts");
    auto section = category->getSection(groupId);
    return section ? section : category->createSection(groupId, label);
}

QAction* Application::registerAction(const QString& groupId, const QString& id, const QString& label,
                                     const QString& help, QIcon icon, QKeySequence defaultShortcut, bool checkable)
{
    QString actualLabel = label.isEmpty() ? id : label;
    QString settingsLabel = actualLabel.replace("&&", "%amp%").replace("&", "").replace("%amp%", "&&");

    QAction* action = new QAction(this);
    action->setObjectName(id);
    action->setText(actualLabel);
    action->setIcon(icon);
    action->setToolTip(settingsLabel);
    action->setStatusTip(help);
    //action->setMenuRole(menuRole);
    action->setShortcutContext(Qt::WindowShortcut);
    action->setShortcut(defaultShortcut);
    action->setCheckable(checkable);

    _globalActions.emplace(groupId + "/" + id, action);

    if (!_settings) return action;
    auto category = _settings->getCategory("shortcuts");
    if (!category) return action;
    auto section = category->getSection(groupId);
    if (!section) return action;

    SettingsEntryPtr entryPtr(new SettingsEntry(*section, id, defaultShortcut, settingsLabel, help, "keySequence", false, 1));
    auto entry = section->addEntry(std::move(entryPtr));

    // When the entry changes, we want to change our shortcut too!
    connect(entry, &SettingsEntry::valueChanged, [action](const QVariant& value)
    {
        action->setShortcut(value.value<QKeySequence>());
    });

    return action;
}

QAction* Application::getAction(const QString& fullId) const
{
    auto it = _globalActions.find(fullId);
    return (it == _globalActions.end()) ? nullptr : it->second;
}

void Application::setActionsEnabled(const QString& groupId, bool enabled)
{
    const QString prefix = groupId + "/";
    for (auto& pair : _globalActions)
        if (pair.first.startsWith(prefix))
            pair.second->setEnabled(enabled);
}

// The absolute path to the doc directory
QString Application::getDocumentationPath() const
{
    /*
    # Potential system doc dir, we check it's existence and set
    # DOC_DIR as system_data_dir if it exists
    SYSTEM_DOC_DIR = "/usr/share/doc/ceed-%s" % (version.CEED)
    SYSTEM_DOC_DIR_EXISTS = False
    try:
        if os.path.exists(SYSTEM_DOC_DIR):
            DOC_DIR = SYSTEM_DOC_DIR
            SYSTEM_DOC_DIR_EXISTS = True

        else:
            SYSTEM_DOC_DIR = "/usr/share/doc/ceed"
            if os.path.exists(SYSTEM_DOC_DIR):
                DOC_DIR = SYSTEM_DOC_DIR
                SYSTEM_DOC_DIR_EXISTS = True
    */

    return QDir::current().absoluteFilePath("doc");
}

QString Application::getUpdatePath() const
{
    return QDir::temp().absoluteFilePath("CEEDUpdate");
}

void Application::checkForUpdates(bool manual, const std::function<void()>& cb)
{
    if (!Utils::isInternetConnected())
    {
        qCritical() << "No Internet connection, update check skipped";
        if (cb) cb();
        return;
    }

    const auto currTime = QDateTime::currentSecsSinceEpoch();

    // Automatic update checks should honor their settings
    if (!manual)
    {
        const auto updateCheckFrequencySec = _settings->getEntryValue("global/app/update_check_frequency").toInt();
        if (updateCheckFrequencySec < 0)
        {
            if (cb) cb();
            return;
        }

        const auto lastUpdateCheckTime = _settings->getQSettings()->value("update/lastTimestamp", 0).toLongLong();
        if (currTime - lastUpdateCheckTime < updateCheckFrequencySec)
        {
            if (cb) cb();
            return;
        }
    }

    _settings->getQSettings()->setValue("update/lastTimestamp", currTime);

    const QUrl infoUrl = _settings->getQSettings()->value("update/url", "https://api.github.com/repos/cegui/ceed-cpp/releases/latest").toUrl();

    _mainWindow->setStatusMessage("Checking for updates...");

    QNetworkReply* infoReply = _network->get(QNetworkRequest(infoUrl));
    QObject::connect(infoReply, &QNetworkReply::errorOccurred, [this, cb, infoReply](QNetworkReply::NetworkError)
    {
        onUpdateError(infoReply->url(), infoReply->errorString());
        if (cb) cb();
    });

    QObject::connect(infoReply, &QNetworkReply::finished, [this, cb, infoReply, manual]()
    {
        // Already processed by QNetworkReply::errorOccurred handler
        if (infoReply->error() != QNetworkReply::NoError)
        {
            if (cb) cb();
            return;
        }

        try
        {
            auto releaseInfo = QJsonDocument::fromJson(infoReply->readAll()).object();
            QString latestVersionStr = releaseInfo.value("tag_name").toString();
            if (!latestVersionStr.isEmpty() && latestVersionStr[0] == 'v')
                latestVersionStr = latestVersionStr.mid(1);

            if (latestVersionStr.isEmpty()) throw descriptive_exception("Latest release version string is empty");

            QVersionNumber latestVersion = QVersionNumber::fromString(latestVersionStr);
            QVersionNumber currentVersion = QVersionNumber::fromString(applicationVersion());
            if (latestVersion.normalized() > currentVersion.normalized())
            {
                if (_settings->getQSettings()->value("update/failed").toBool())
                {
                    auto savedVersion = QVersionNumber::fromString(_settings->getQSettings()->value("update/version").toString());
                    if (latestVersion.normalized() == savedVersion.normalized())
                    {
                        const QString msg = tr("Auto-update to %1 is blocked because it failed before. "
                                               "Use Help->Check For Updates to try again.").arg(latestVersionStr);
                        _mainWindow->setStatusMessage(msg);
                        if (manual)
                            QMessageBox::warning(_mainWindow, tr("Auto-update blocked"), msg);
                        if (cb) cb();
                        return;
                    }
                    else
                    {
                        _settings->getQSettings()->remove("update");

                        QDir updateDir(getUpdatePath());
                        if (updateDir.exists()) updateDir.removeRecursively();
                    }
                }

                UpdateDialog dlg(currentVersion, latestVersion, releaseInfo);
                dlg.exec();
            }
            else
            {
                const QString msg = tr("CEED is already at the latest version: %1").arg(applicationVersion());
                _mainWindow->setStatusMessage("No update found");
                if (manual)
                    QMessageBox::information(_mainWindow, tr("Already updated"), msg);
            }
        }
        catch (const std::exception& e)
        {
            onUpdateError(infoReply->url(), e.what());
        }

        if (cb) cb();
    });
}

void Application::onUpdateError(const QUrl& url, const QString& errorString)
{
    _mainWindow->setStatusMessage("Failed to check for updates");
    qCritical() << "Update error: '" << errorString << "' accessing " << url;

    const auto response = QMessageBox::question(_mainWindow, tr("Update check failed"),
            tr("Update failed with error:\n%1\n\nOpen releases web page?").arg(errorString),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

    if (response == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl("https://github.com/cegui/ceed-cpp/releases"));
}

void Application::checkUpdateResults()
{
    const bool updateLaunched = _settings->getQSettings()->value("update/launched").toBool();
    const bool startedByUpdater = _cmdLine->isSet("updateResult");
    if (!updateLaunched && !startedByUpdater) return;

    if (!updateLaunched)
        QMessageBox::warning(_mainWindow, tr("Warning"), tr("An application was launched by an updater script but no update was scheduled!"));

    int updateResult = -1;
    QString updateMessage = "<Not launched by updater>";
    if (startedByUpdater)
    {
        updateResult = _cmdLine->value("updateResult").toInt();
        updateMessage = tr(_cmdLine->value("updateMessage").toUtf8());
    }

    auto currentVersion = QVersionNumber::fromString(applicationVersion());
    auto savedVersion = QVersionNumber::fromString(_settings->getQSettings()->value("update/version").toString());
    if (currentVersion.normalized() >= savedVersion.normalized())
    {
        // Updated but couldn't remove a tmp folder, let's try again
        if (updateResult == 30)
        {
            QDir backupDir(applicationDirPath() + "_old");
            if (!backupDir.exists() || backupDir.removeRecursively())
            {
                updateResult = 0;
                updateMessage = tr("Updated successfully");
            }
        }

        QDir updateDir(getUpdatePath());
        if (updateDir.exists()) updateDir.removeRecursively();

        auto releaseWebPage = _settings->getQSettings()->value("update/webPage").toString();
        if (releaseWebPage.isEmpty())
            releaseWebPage= "https://github.com/cegui/ceed-cpp/releases/tag/v" + currentVersion.toString();

        _settings->getQSettings()->remove("update");

        if (updateResult == 0 && startedByUpdater)
        {
            // NB: \n is replaced with <br/> to indicate Qt that this is a rich text
            QMessageBox::information(_mainWindow, tr("Updated"),
                                     tr("Updated to %1.<br/>"
                                        "Visit <a href=\"%2\">release page</a> for the full release description.<br/><br/>"
                                        "Updater result code: %3<br/>"
                                        "Updater message: %4")
                                     .arg(currentVersion.toString())
                                     .arg(releaseWebPage)
                                     .arg(updateResult)
                                     .arg(updateMessage));
        }
        else
        {
            QMessageBox::warning(_mainWindow, tr("Updated with problems"),
                                 tr("Application executable is updated to %1, yet something gone wrong. "
                                    "Please check updater results and reinstall manually if you encounter problems.\n\n"
                                    "Updater result code: %2\n"
                                    "Updater message: %3")
                                 .arg(currentVersion.toString())
                                 .arg(updateResult)
                                 .arg(updateMessage));
        }
    }
    else
    {
        QMessageBox::critical(_mainWindow, tr("Update failed"),
                              tr("Failed to update to %1.\n"
                                 "Automatic update will be blocked for this version.\n"
                                 "Use Help->Check For Updates to remove the block.\n\n"
                                 "Updater result code: %2\n"
                                 "Updater message: %3")
                              .arg(savedVersion.toString())
                              .arg(updateResult)
                              .arg(updateMessage));
        _settings->getQSettings()->setValue("update/failed", true);
    }
}

// Creates general application settings plus some subsystem settings
// TODO: subsystem settings to subsystems? load from storage on subsystem init?
void Application::createSettingsEntries()
{
    // General settings

    auto catGlobal = _settings->createCategory("global", "Global");
    auto secApp = catGlobal->createSection("app", "Application");

    SettingsEntryPtr entry(new SettingsEntry(*secApp, "startup_action", 1, "On startup, load",
                                "What to show when an application started",
                                "combobox", false, 1, { {0, "Empty environment"}, {1, "Most recent project"} }));
    secApp->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secApp, "update_check_frequency", 0, "Check for updates",
                                "How frequently an update should be checked",
                                "combobox", false, 1, { {7 * 86400, "Once a week"}, {86400, "Once a day"}, {0, "Every launch"}, {-1, "Never"} }));
    secApp->addEntry(std::move(entry));


    // By default we limit the undo stack to 500 undo commands, should be enough and should
    // avoid memory drainage. Keep in mind that every tabbed editor has it's own undo stack,
    // so the overall command limit is number_of_tabs * 500!
    entry.reset(new SettingsEntry(*secApp, "undo_limit", 500, "Undo history size",
                                 "Puts a limit on every tabbed editor's undo stack. You can undo at most the number of times specified here.",
                                 "int", true, 1));
    secApp->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secApp, "copy_path_os_separators", true, "Copy path with OS-specific separators",
                                  "When copy a file path to clipboard, will convert forward slashes (/) to OS-specific separators",
                                  "checkbox", false, 1));
    secApp->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secApp, "show_splash", true, "Show splash screen",
                                  "Show the splash screen on startup",
                                  "checkbox", false, 1));
    secApp->addEntry(std::move(entry));

    auto secUI = catGlobal->createSection("ui", "User Interface");
    entry.reset(new SettingsEntry(*secUI, "toolbar_icon_size", 32, "Toolbar icon size",
                                  "Sets the size of the toolbar icons",
                                  "combobox", false, 1, { {32, "Normal"}, {24, "Small"}, {16, "Smaller"} }));
    secUI->addEntry(std::move(entry));

    auto secCEGUIDebug = catGlobal->createSection("cegui_debug_info", "CEGUI debug info");
    entry.reset(new SettingsEntry(*secCEGUIDebug, "log_limit", 20000, "Log messages limit",
                                  "Limits number of remembered log messages to given amount. This is there to prevent endless growth of memory consumed by CEED.",
                                  "int", true, 1));
    secCEGUIDebug->addEntry(std::move(entry));

    auto secNavigation = catGlobal->createSection("navigation", "Navigation");
    entry.reset(new SettingsEntry(*secNavigation, "ctrl_zoom", true, "Only zoom when CTRL is pressed",
                                  "Mouse wheel zoom is ignored unless the Control key is pressed when it happens.",
                                  "checkbox", false, 1));
    secNavigation->addEntry(std::move(entry));

    // CEGUI settings

    auto catCEGUI = _settings->createCategory("cegui", "Embedded CEGUI");
    auto secBG = catCEGUI->createSection("background", "Rendering background (checkerboard)");

    entry.reset(new SettingsEntry(*secBG, "checker_width", 10, "Width of the checkers",
                                  "Width of one checker element in pixels.",
                                  "int", false, 1));
    secBG->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secBG, "checker_height", 10, "Height of the checkers",
                                  "Height of one checker element in pixels.",
                                  "int", false, 2));
    secBG->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secBG, "first_colour", QColor(Qt::darkGray), "First colour",
                                  "First of the alternating colours to use.",
                                  "colour", false, 3));
    secBG->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secBG, "second_colour", QColor(Qt::lightGray), "Second colour",
                                  "Second of the alternating colours to use. (use the same as first to get a solid background)",
                                  "colour", false, 4));
    secBG->addEntry(std::move(entry));

    auto secScreenshots = catCEGUI->createSection("screenshots", "Screenshots");

    entry.reset(new SettingsEntry(*secScreenshots, "save", true, "Save to file",
                                  "Save screenshot to file (otherwise it is only copied to the clipboard)",
                                  "checkbox", false, 1));
    secScreenshots->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secScreenshots, "after_save_action", 0, "After save",
                                  "What to do after saving a screenshot to the file",
                                  "combobox", false, 2, { {0, "Open folder"}, {1, "Open file"}, {2, "Do nothing"} }));
    secScreenshots->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secScreenshots, "bg_checker", false, "Checkered background in clipboard",
                                  "Fill screenshot background with a checkerboard (otherwise\n"
                                  "transparency is kept when pasting to applications that\n"
                                  "support transparent images)",
                                  "checkbox", false, 3));
    secScreenshots->addEntry(std::move(entry));

    entry.reset(new SettingsEntry(*secScreenshots, "use_qt_setimage", true, "Add Qt image to clipboard",
                                  "Adds Qt's 'application/x-qt-image' to a clipboard, which\n"
                                  "expands to multiple platform and cross-platform formats.\n"
                                  "On Windows it's known to enable pasting to Paint & Slack,\n"
                                  "but to break pasting to Word.",
                                  "checkbox", false, 4));

    secScreenshots->addEntry(std::move(entry));

    ImagesetEditor::createSettings(*_settings);
    LayoutEditor::createSettings(*_settings);
    LookNFeelEditor::createSettings(*_settings);
}
