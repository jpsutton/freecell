#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;

// Checks the latest GitHub release's latest.json feed (written by release.yml) and
// offers to update. Release builds only (FREECELL_UPDATER); a build from source
// never checks.
//
// How an update installs depends on how FreeCell runs:
//   - macOS app: the new FreeCell.app is downloaded, checked against the feed's
//     SHA-256 and against this app's code signature (it must satisfy the running
//     app's designated requirement, so only builds signed with the same release
//     certificate are accepted), and swapped in place.
//   - Linux AppImage: the new AppImage is checked against the SHA-256 and replaces
//     the running file.
//   - Anything else (Windows, other Linux builds): the release page opens in the
//     browser.
class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QWidget* window);

    static bool supported();
    // "Check for updates at startup" (Options); on by default.
    static bool checkAtStartupEnabled();
    static void setCheckAtStartup(bool on);

    // Quiet: says nothing if there is no update, the check fails, or the user
    // skipped this version.
    void checkAtStartup();
    // Help > Check for Updates: reports every outcome.
    void checkNow();

signals:
    // An update is installed and the user wants to restart: close the window
    // (which saves the game) and start this program.
    void restartRequested(const QString& program, const QStringList& args);

private:
    void check(bool interactive);
    void offer(const QJsonObject& feed, bool interactive);
    void install(const QString& version, const QJsonObject& asset, const QString& page);
    bool installMac(const QString& file, QString& error);
    bool installAppImage(const QString& file, QString& error);
    QString platformKey() const;
    void fail(const QString& message, bool interactive);

    QWidget* m_window;
    QNetworkAccessManager m_net;
    bool m_busy = false;
    QString m_restartProgram;
    QStringList m_restartArgs;
};
