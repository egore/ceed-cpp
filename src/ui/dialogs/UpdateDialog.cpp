#include "UpdateDialog.h"
#include "ui_UpdateDialog.h"
#include "src/Application.h"
#include "src/util/Settings.h"
#include "src/util/Utils.h"
#include "src/util/descriptive_exception.h"
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <qevent.h>
#include <qmessagebox.h>
#include <qdesktopservices.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qscreen.h>
#include <qsettings.h>
#include <qdir.h>
#include <qprocess.h>

constexpr double MB = 1048576.0;

UpdateDialog::UpdateDialog(const QVersionNumber& currentVersion, const QVersionNumber& newVersion,
                           const QJsonObject& releaseInfo, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateDialog),
    _releaseVersion(newVersion.normalized())
{
    ui->setupUi(this);

#ifdef Q_OS_WIN
    const QString os = "Win";
#else
    const QString os = "";
#endif

#if QT_POINTER_SIZE == 4
    const QString arch = "x86";
#else
    const QString arch = "x64";
#endif

    _releaseAssetFileName = QString("CEED-v%1-%2-%3.zip").arg(newVersion.toString(), os, arch);
    const QString assetNameNoCase = _releaseAssetFileName.toLower();
    const auto assets = releaseInfo.value("assets").toArray();
    for (const QJsonValue& assetDesc : assets)
    {
        const auto assetObject = assetDesc.toObject();
        if (//assetObject.value("content_type").toString() == "application/zip" &&
            //assetObject.value("state").toString() == "uploaded" &&
            assetObject.value("name").toString().trimmed().toLower() == assetNameNoCase)
        {
            _releaseAsset = assetObject.value("browser_download_url").toString();
            _releaseAssetSize = static_cast<int64_t>(assetObject.value("size").toDouble());
            break;
        }
    }

    QString versionStr = tr("<b>%1 <img width=\"12\" height=\"12\" src=\"://icons/layout_editing/move_forward_in_parent_list.png\"/> %2</b>")
            .arg(currentVersion.toString(), newVersion.toString());

    if (_releaseAsset.isEmpty())
    {
        ui->btnUpdate->setEnabled(false);
        ui->btnUpdate->setText(tr("<No package>"));
        ui->btnUpdate->setToolTip(tr("No downloadable package was detected for your OS,\nplease visit a release web page and download manually"));
    }
    else
    {
#ifdef Q_OS_WIN
        versionStr += tr(" (%3 MB)").arg(static_cast<double>(_releaseAssetSize) / MB, 0, 'f', 1);
#else
        // TODO: support auto-update for Linux & Mac!
        ui->btnUpdate->setEnabled(false);
        ui->btnUpdate->setText(tr("<Not auto-updatable>"));
        ui->btnUpdate->setToolTip(tr("Auto-updates are not yet implemented for your OS,\nplease visit a release web page and download manually"));
#endif
    }

    _releaseWebPage = releaseInfo.value("html_url").toString();
    const QDateTime releaseDate = QDateTime::fromString(releaseInfo.value("published_at").toString(), Qt::ISODate);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (auto myScreen = screen())
    {
        const auto screenRect = myScreen->availableGeometry();
        const auto center = screenRect.center();
        const QSize size(std::min(screenRect.width() / 2, 640), std::min(screenRect.height() / 2, 480));
        setGeometry(center.x() - size.width() / 2, center.y() - size.height() / 2, size.width(), size.height());
    }

    const auto releaseFullName = releaseInfo.value("name").toString();
    if (!releaseFullName.isEmpty())
        setWindowTitle("Update available - " + releaseFullName);

    ui->lblVersions->setText(versionStr);
    ui->lblReleaseDate->setText("Published at " + releaseDate.toString(Qt::SystemLocaleLongDate) + " GMT");
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    ui->txtReleaseNotes->setMarkdown(releaseInfo.value("body").toString());
#else
    ui->txtReleaseNotes->setText(releaseInfo.value("body").toString());
#endif

    ui->lblStatus->setVisible(false);
    ui->progressBar->setVisible(false);
    ui->progressBar->setMaximum(1000);
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}

void UpdateDialog::keyPressEvent(QKeyEvent* event)
{
    // Prevent closing by Esc
    if (event->key() != Qt::Key_Escape)
        UpdateDialog::keyPressEvent(event);
}

void UpdateDialog::closeEvent(QCloseEvent* event)
{
    if (_blocked)
        event->ignore();
    else
        QDialog::closeEvent(event);
}

void UpdateDialog::on_btnUpdate_clicked()
{
    auto app = qobject_cast<Application*>(qApp);
    QSettings* settings = app->getSettings()->getQSettings();

    // Check if we already have an update package downloaded
    QString packageName;
    if (settings->contains("update/version"))
    {
        // Version must match
        if (_releaseVersion == QVersionNumber::fromString(settings->value("update/version").toString()).normalized())
        {
            // Package zip must exist
            // TODO: verify checksum
            packageName = settings->value("update/package", QString()).toString();
            if (!packageName.isEmpty())
            {
                const QString fileName = QDir(app->getUpdatePath()).absoluteFilePath(packageName + ".zip");
                if (QFileInfo(fileName).size() != _releaseAssetSize)
                    packageName.clear();
            }
        }
    }

    if (!packageName.isEmpty())
    {
        ui->lblStatus->setVisible(true);
        ui->lblStatus->setText(tr("Update package '%1' is found in the cache").arg(packageName));
        installUpdate(packageName);
    }
    else
    {
        downloadUpdate();
    }
}

void UpdateDialog::on_btnWeb_clicked()
{
    QDesktopServices::openUrl(_releaseWebPage);
}

void UpdateDialog::downloadUpdate()
{
    // FIXME QTBUG: Qt 5.15.2 freezes in QMessageBox::question below
    //setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    auto app = qobject_cast<Application*>(qApp);

    // Our cache is verified to be invalid, let's erase it and start clean
    QDir updateDir(app->getUpdatePath());
    if (updateDir.exists()) updateDir.removeRecursively();
    updateDir.mkpath(".");

    QSettings* settings = app->getSettings()->getQSettings();
    settings->remove("update");

    // Reserve disk space for the download
    QFile file(updateDir.absoluteFilePath(_releaseAssetFileName));
    try
    {
        if (file.open(QFile::WriteOnly))
        {
            if (!file.seek(_releaseAssetSize - 1))
                throw descriptive_exception("Can't reserve enough space in the temporary file");
            file.write("\0", 1);
            file.close();
        }
        else
        {
            throw descriptive_exception("Can't create temporary file");
        }
    }
    catch (const std::exception& e)
    {
        if (file.exists())
            file.remove();
        onUpdateError(tr("Can't reserve disk space for downloading.\nCheck available space and access rights and then retry.\n\nError: %1").arg(e.what()));
        return;
    }

    _blocked = true;

    ui->btnUpdate->setEnabled(false);

    ui->progressBar->setValue(0);
    ui->progressBar->setVisible(true);

    ui->lblStatus->setVisible(true);
    ui->lblStatus->setText(tr("Preparing download..."));

    _downloadTimer.start();

    QNetworkRequest assetRequest(_releaseAsset);
    assetRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* assetReply = qobject_cast<Application*>(qApp)->getNetworkManager()->get(assetRequest);

    QObject::connect(assetReply, &QNetworkReply::downloadProgress, [this](qint64 bytesReceived, qint64 bytesTotal)
    {
        if (bytesTotal <= 0) bytesTotal = _releaseAssetSize;

        if (bytesReceived >= bytesTotal)
        {
            ui->progressBar->setValue(ui->progressBar->maximum());
            ui->lblStatus->setText(tr("Downloaded"));
            return;
        }

        if (bytesTotal <= 0) return;

        ui->progressBar->setValue(
                    static_cast<int>(ui->progressBar->maximum() * (static_cast<double>(bytesReceived) / static_cast<double>(bytesTotal))));

        const auto mbReceived = static_cast<double>(bytesReceived) / MB;
        const auto mbTotal = static_cast<double>(bytesTotal) / MB;

        double secondsElapsed = static_cast<double>(_downloadTimer.elapsed()) / 1000.0; // msec to sec

        double speedMbps = mbReceived / secondsElapsed;

        auto secondsRemaining = static_cast<uint64_t>((mbTotal - mbReceived) / speedMbps);

        if (secondsElapsed < 1.0)
        {
            ui->lblStatus->setText(tr("Downloading..."));
            return;
        }

        QString remainigTime;
        if (secondsRemaining > 3600)
        {
            remainigTime += QString("%1 h").arg(secondsRemaining / 3600);
            secondsRemaining %= 3600;
        }
        if (secondsRemaining > 60)
        {
            remainigTime += QString("%1 m").arg(secondsRemaining / 60);
            secondsRemaining %= 60;
        }
        remainigTime += QString("%1 s").arg(secondsRemaining);

        const bool mbps = (speedMbps > 1.0);

        ui->lblStatus->setText(
            tr("Downloading: %2 / %3 MB. %4 remaining (%5 %6)")
            .arg(mbReceived, 0, 'f', 1)
            .arg(mbTotal, 0, 'f', 1)
            .arg(remainigTime)
            .arg(mbps ? speedMbps : speedMbps * 1000.0, 0, 'f', mbps ? 2 : 0)
            .arg(mbps ? tr("MB/s") : tr("KB/s")));
    });

    QObject::connect(assetReply, &QNetworkReply::errorOccurred, [this, assetReply](QNetworkReply::NetworkError)
    {
        ui->lblStatus->setText(tr("Network error: %1").arg(assetReply->errorString()));
    });

    QObject::connect(assetReply, &QNetworkReply::finished, [this, assetReply, settings, filePath = file.fileName()]()
    {
        //setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);

        _blocked = false;

        ui->btnUpdate->setEnabled(true);

        QFileInfo fileInfo(filePath);

        if (assetReply->error() != QNetworkReply::NoError)
        {
            fileInfo.dir().removeRecursively();
            ui->progressBar->setVisible(false);
            ui->btnUpdate->setText(tr("Retry"));
            return;
        }

        QFile file(filePath);
        if (file.open(QFile::WriteOnly | QFile::Truncate))
        {
            file.write(assetReply->readAll());
            file.close();
        }

        const QString packageName = fileInfo.completeBaseName();

        // Update is successfully downloaded, remember its version and file name
        settings->setValue("update/version", _releaseVersion.toString());
        settings->setValue("update/package", packageName);

        installUpdate(packageName);
    });
}

void UpdateDialog::installUpdate(const QString& packageName)
{
    // Confirm restart

    auto response = QMessageBox::question(this, tr("Confirm restart"),
                                          tr("Application will be closed and all unsaved work will be lost.\nContinue?"),
                                          QMessageBox::Yes | QMessageBox::No);
    if (response != QMessageBox::Yes)
    {
        ui->lblStatus->setVisible(false);
        ui->progressBar->setVisible(false);
        return;
    }

    // Unpack a zip package

    auto app = qobject_cast<Application*>(qApp);

    QDir updateDir = QDir(app->getUpdatePath());
    QString packageDirPath = updateDir.absoluteFilePath(packageName);
    QDir packageDir(packageDirPath);
    const QString packageZipPath = packageDirPath + ".zip";

    if (packageDir.exists()) packageDir.removeRecursively();

    if (!Utils::unzip(packageZipPath, packageDirPath) || !packageDir.exists())
    {
        onUpdateError(tr("Failed to unpack '%1'").arg(packageZipPath));
        return;
    }

    packageDir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    const auto itemCount = packageDir.count();
    if (itemCount == 0)
    {
        onUpdateError(tr("A package '%1' was empty").arg(packageZipPath));
        return;
    }
    else if (itemCount == 1)
    {
        // If there is a single root directory in the package, we skip it
        const auto dirList = packageDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (!dirList.isEmpty())
            packageDir.cd(dirList[0]);
    }

    // Launch an updater and exit

    const QString appFile = qApp->applicationFilePath();
    const QString installPath = qApp->applicationDirPath();
    const QString updatePath = packageDir.absolutePath();

#ifdef Q_OS_WIN
    // 'findstr' requires slashes to be escaped
    QString appFileSlashesEsc = appFile;
    appFileSlashesEsc.replace("\\", "\\\\");
    appFileSlashesEsc.replace("/", "\\\\");

    // Copy update script to the update folder because the current installation will be removed.
    // NB: working directory is changed accordingly!
    const QString cmdFileSrc = QDir(installPath).absoluteFilePath("data/misc/update.cmd");
    const QString cmdFileDst = updateDir.absoluteFilePath("update.cmd");
    QFile::copy(cmdFileSrc, cmdFileDst);

    QStringList cmdArgs;
    cmdArgs.push_back(appFileSlashesEsc);
    cmdArgs.push_back(installPath);
    cmdArgs.push_back(updatePath);
    if (!QProcess::startDetached(cmdFileDst, cmdArgs, QFileInfo(cmdFileDst).absolutePath()))
    {
        onUpdateError(tr("Failed to launch an updater script"));
        packageDir.removeRecursively();
        return;
    }
#else
    // TODO: Linux, Mac
#endif

    // Remember that we started an update to check results on the next CEED launch
    QSettings* settings = app->getSettings()->getQSettings();
    settings->setValue("update/launched", true);
    settings->setValue("update/webPage", _releaseWebPage.toString());

    exit(0);
}

void UpdateDialog::onUpdateError(const QString& error)
{
    QMessageBox::critical(this, tr("Error"), error);
    ui->lblStatus->setText(error);
    ui->lblStatus->setVisible(true);
    ui->progressBar->setVisible(false);
}

