#include "updater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <QWidget>
#include <cstdio>

namespace {
const char* kFeed = "https://github.com/jpsutton/freecell/releases/latest/download/latest.json";

QNetworkRequest request(const QUrl& url) {
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("FreeCell/%1").arg(QCoreApplication::applicationVersion()));
    r.setTransferTimeout(30000);
    return r;
}

// Runs a helper program to completion; returns its exit code (-1 if it didn't run)
// and its combined output.
int run(const QString& program, const QStringList& args, QString* output = nullptr) {
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForFinished(120000)) return -1;
    if (output) *output = QString::fromUtf8(p.readAll());
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

QString sha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}
}  // namespace

Updater::Updater(QWidget* window) : QObject(window), m_window(window) {}

bool Updater::supported() {
#ifdef FREECELL_UPDATER
    return true;
#else
    return false;
#endif
}

bool Updater::checkAtStartupEnabled() { return QSettings().value("updates/check", true).toBool(); }

void Updater::setCheckAtStartup(bool on) { QSettings().setValue("updates/check", on); }

void Updater::checkAtStartup() {
    if (!supported() || !checkAtStartupEnabled()) return;
    // Let the window come up first.
    QTimer::singleShot(3000, this, [this] { check(false); });
}

void Updater::checkNow() { check(true); }

QString Updater::platformKey() const {
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("linux-x86_64");
#elif defined(Q_OS_WIN) && defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("windows-x64");
#else
    return {};
#endif
}

void Updater::fail(const QString& message, bool interactive) {
    std::fprintf(stderr, "FreeCell update: %s\n", qPrintable(message));
    if (interactive) QMessageBox::warning(m_window, tr("FreeCell"), message);
}

void Updater::check(bool interactive) {
    if (m_busy) return;
    m_busy = true;
    // FREECELL_UPDATE_FEED points the check at another feed, for testing.
    const QString feed = qEnvironmentVariable("FREECELL_UPDATE_FEED", QString::fromLatin1(kFeed));
    QNetworkReply* reply = m_net.get(request(QUrl(feed)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] {
        reply->deleteLater();
        m_busy = false;
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Could not check for updates:\n%1").arg(reply->errorString()), interactive);
            return;
        }
        QJsonObject feed = QJsonDocument::fromJson(reply->readAll()).object();
        if (QVersionNumber::fromString(feed.value("version").toString()).isNull()) {
            fail(tr("Could not check for updates: the release feed is not valid."), interactive);
            return;
        }
        offer(feed, interactive);
    });
}

void Updater::offer(const QJsonObject& feed, bool interactive) {
    const QString version = feed.value("version").toString();
    const QString current = QCoreApplication::applicationVersion();
    if (QVersionNumber::fromString(version) <= QVersionNumber::fromString(current)) {
        if (interactive)
            QMessageBox::information(m_window, tr("FreeCell"),
                                     tr("FreeCell %1 is the latest version.").arg(current));
        return;
    }
    QSettings s;
    if (!interactive && s.value("updates/skip").toString() == version) return;

    QMessageBox box(QMessageBox::Question, tr("FreeCell"),
                    tr("FreeCell %1 is available. You have %2.").arg(version, current),
                    QMessageBox::NoButton, m_window);
    QPushButton* update = box.addButton(tr("&Update"), QMessageBox::AcceptRole);
    QPushButton* skip = box.addButton(tr("&Skip This Version"), QMessageBox::DestructiveRole);
    box.addButton(tr("&Later"), QMessageBox::RejectRole);
    box.setDefaultButton(update);
    box.exec();
    if (box.clickedButton() == skip) {
        s.setValue("updates/skip", version);
        return;
    }
    if (box.clickedButton() != update) return;
    const QString page = feed.value("url").toString();
    install(version, feed.value("assets").toObject().value(platformKey()).toObject(), page);
}

void Updater::install(const QString& version, const QJsonObject& asset, const QString& page) {
    // In-place updates only where FreeCell can replace itself; otherwise the download page.
    bool inPlace = false;
#if defined(Q_OS_MACOS)
    inPlace = true;
#elif defined(Q_OS_LINUX)
    inPlace = !qEnvironmentVariable("APPIMAGE").isEmpty();
#endif
    const QString url = asset.value("url").toString();
    const QString expected = asset.value("sha256").toString().toLower();
    if (!inPlace || url.isEmpty() || expected.size() != 64) {
        QDesktopServices::openUrl(QUrl(page));
        return;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        fail(tr("Could not create a temporary folder for the update."), true);
        return;
    }
    const QString file = tmp.filePath(asset.value("name").toString(QStringLiteral("update")));
    QFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        fail(tr("Could not write the update to %1.").arg(file), true);
        return;
    }

    QProgressDialog progress(tr("Downloading FreeCell %1...").arg(version), tr("Cancel"), 0, 0, m_window);
    progress.setWindowTitle(tr("FreeCell"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QNetworkReply* reply = m_net.get(request(QUrl(url)));
    QEventLoop loop;
    connect(reply, &QNetworkReply::readyRead, &loop, [&] { out.write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 got, qint64 total) {
        if (total > 0) {
            progress.setMaximum(int(total / 1024));
            progress.setValue(int(got / 1024));
        }
    });
    connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    out.write(reply->readAll());
    out.close();
    progress.reset();
    reply->deleteLater();
    if (reply->error() == QNetworkReply::OperationCanceledError) return;
    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("Could not download the update:\n%1").arg(reply->errorString()), true);
        return;
    }
    if (sha256(file) != expected) {
        fail(tr("The downloaded update is damaged (its checksum does not match). Nothing was changed."), true);
        return;
    }

    QString error;
#if defined(Q_OS_MACOS)
    bool ok = installMac(file, error);
#else
    bool ok = installAppImage(file, error);
#endif
    if (!ok) {
        QMessageBox box(QMessageBox::Warning, tr("FreeCell"),
                        tr("FreeCell could not update itself.\n\n%1").arg(error),
                        QMessageBox::NoButton, m_window);
        QPushButton* open = box.addButton(tr("Open Download Page"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Close);
        box.exec();
        if (box.clickedButton() == open) QDesktopServices::openUrl(QUrl(page));
        return;
    }
    if (QMessageBox::question(m_window, tr("FreeCell"),
                              tr("FreeCell %1 is installed. Restart now?\n\n"
                                 "Your game in progress is saved and resumes after the restart.")
                                  .arg(version)) == QMessageBox::Yes)
        emit restartRequested(m_restartProgram, m_restartArgs);
}

bool Updater::installMac(const QString& file, QString& error) {
    // .../FreeCell.app/Contents/MacOS/FreeCell -> .../FreeCell.app
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    const QString bundle = dir.canonicalPath();
    const QString parent = QFileInfo(bundle).absolutePath();
    if (!bundle.endsWith(QLatin1String(".app"))) {
        error = tr("FreeCell is not running from an app bundle.");
        return false;
    }
    if (!QFileInfo(parent).isWritable()) {
        error = tr("FreeCell cannot write to %1. Move FreeCell.app to Applications, or update "
                   "it with the install command in the README.").arg(parent);
        return false;
    }

    // Only accept an app signed like this one: the new bundle must satisfy the running
    // app's designated requirement, which names the release certificate.
    QString info;
    run("/usr/bin/codesign", {"-d", "-r-", bundle}, &info);
    QString requirement;
    for (const QString& line : info.split('\n'))
        if (line.startsWith(QLatin1String("designated => "))) requirement = line.mid(14).trimmed();
    if (requirement.isEmpty() || requirement.contains(QLatin1String("cdhash"))) {
        error = tr("This copy of FreeCell is not signed with the release certificate, so it "
                   "cannot check the update's signature.");
        return false;
    }

    const QString unpack = QFileInfo(file).absolutePath() + "/unpacked";
    QDir().mkpath(unpack);
    if (run("/usr/bin/tar", {"-xzf", file, "-C", unpack}) != 0 || !QFileInfo::exists(unpack + "/FreeCell.app")) {
        error = tr("The update could not be unpacked.");
        return false;
    }
    const QString fresh = unpack + "/FreeCell.app";
    if (run("/usr/bin/codesign", {"--verify", "--deep", "--strict", "-R", "=" + requirement, fresh}) != 0) {
        error = tr("The update is not signed with FreeCell's release certificate. Nothing was changed.");
        return false;
    }

    // Copy next to the app first (same volume), then swap with two renames.
    const QString staged = parent + "/.FreeCell-update.app";
    const QString old = parent + "/.FreeCell-old.app";
    QDir(staged).removeRecursively();
    QDir(old).removeRecursively();
    if (run("/usr/bin/ditto", {fresh, staged}) != 0) {
        QDir(staged).removeRecursively();
        error = tr("The update could not be copied to %1.").arg(parent);
        return false;
    }
    if (!QDir().rename(bundle, old)) {
        QDir(staged).removeRecursively();
        error = tr("%1 could not be replaced.").arg(bundle);
        return false;
    }
    if (!QDir().rename(staged, bundle)) {
        QDir().rename(old, bundle);
        QDir(staged).removeRecursively();
        error = tr("%1 could not be replaced.").arg(bundle);
        return false;
    }
    QDir(old).removeRecursively();
    m_restartProgram = QStringLiteral("/usr/bin/open");
    m_restartArgs = {QStringLiteral("-n"), bundle};
    return true;
}

bool Updater::installAppImage(const QString& file, QString& error) {
    const QString target = qEnvironmentVariable("APPIMAGE");
    const QString staged = target + ".new";
    QFile::remove(staged);
    if (!QFile::copy(file, staged)) {
        error = tr("FreeCell cannot write next to %1.").arg(target);
        return false;
    }
    QFile::setPermissions(staged, QFile::permissions(target) | QFile::ExeOwner | QFile::ReadOwner);
    // rename() replaces the file atomically; the running copy keeps its open inode.
    if (std::rename(QFile::encodeName(staged).constData(), QFile::encodeName(target).constData()) != 0) {
        QFile::remove(staged);
        error = tr("%1 could not be replaced.").arg(target);
        return false;
    }
    m_restartProgram = target;
    m_restartArgs = {};
    return true;
}
