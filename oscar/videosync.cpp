#include "videosync.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocalSocket>
#include <QProcess>
#include <QPushButton>
#include <QStringBuilder>
#include <QTimer>
#include <QVBoxLayout>
#include <QtDebug>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QFormLayout>

#include "SleepLib/profiles.h"

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    initMpvPaths();
    createWidgets();
    connectWidgets();
    update();
}

void VideoSync::onGraphPlayPauseReq() {
    if (m_synced) {
        m_videoPaused = !m_videoPaused;
        sendMpvCommand({"set_property", "pause", m_videoPaused});
    }
}

void VideoSync::onPlayheadChanged(qint64 t) {
    // Because mpv can control graphview and vice versa, check if changed to
    // avoid event loop
    if (t == m_playheadTime) {
        return;
    }
    m_playheadTime = t;
    if (m_synced) {
        updateMpv();
    }
    update();
}

void VideoSync::updateMpv() {
    double newVideoTime = m_syncedVideoTime + ((m_playheadTime - m_syncedPlayheadTime) / 1000.f) 
        * m_syncSkew;
    sendMpvCommand({"seek", newVideoTime, "absolute", "exact"});
}

void VideoSync::initMpvPaths()
{
    // Hunt MPV path
#if defined(Q_OS_WIN)
    const QStringList MPV_PATHS = {"mpv"};
#elif defined(Q_OS_MAC)
    const QStringList MPV_PATHS = {"mpv", "/opt/homebrew/bin/mpv", "/usr/local/bin/mpv"};
#elif defined(Q_OS_LINUX)
    const QStringList MPV_PATHS = {"mpv", QDir::homePath() % "/.local/bin/mpv"};
#endif
    // Hunt MPV path
    for (const auto &mpvPath : MPV_PATHS) {
        if (QProcess::startDetached(mpvPath, {"--version", "--really-quiet"})) {
            m_mpvPath = mpvPath;
            break;
        }
    }
    // TODO handle mpv command not found

    // Compute socket path
#if defined(Q_OS_WIN)
    m_mpvSocketName = QString("oscar_mpv_socket");
#elif defined(Q_OS_UNIX)
    m_mpvSocketName = QString("/tmp/oscar_mpv_socket");
#endif
}

void VideoSync::createWidgets()
{
    m_mpvProcess = new QProcess(this);
    m_mpvSocket = new QLocalSocket(this);

    m_openMpvButton = new QPushButton(tr("Open MPV"));
    m_syncButton = new QPushButton();

    QFormLayout *syncLayout = new QFormLayout();
    m_syncSkewEdit = new QLineEdit();
    m_syncSkewEdit->setText(QString::asprintf("%.10f", m_syncSkew));
    m_syncSkewEdit->setValidator(new QDoubleValidator(0.95, 1.05, 10));
    syncLayout->addRow(tr("&Sync Skew"), m_syncSkewEdit);

    m_debugBox = new QGroupBox(tr("Debug"));
    m_videoLabel = new QLabel();
    m_playheadLabel = new QLabel();
    m_videoSyncLabel = new QLabel();
    m_playheadSyncLabel = new QLabel();
    auto* debugBoxLayout = new QVBoxLayout();
    debugBoxLayout->addWidget(m_videoLabel);
    debugBoxLayout->addWidget(m_playheadLabel);
    debugBoxLayout->addWidget(m_videoSyncLabel);
    debugBoxLayout->addWidget(m_playheadSyncLabel);
    m_debugBox->setLayout(debugBoxLayout);

    auto *layout = new QVBoxLayout();
    layout->addWidget(m_openMpvButton);
    layout->addWidget(m_syncButton);
    layout->addLayout(syncLayout);
    layout->addWidget(m_debugBox);
    layout->setAlignment(Qt::AlignTop);
    setLayout(layout);
}

void VideoSync::connectWidgets()
{
    connect(m_openMpvButton, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_syncButton, &QPushButton::clicked, this, [this] {
        if (m_synced) {
            m_synced = false;
        } else {
            m_synced = true;
            m_syncedPlayheadTime = m_playheadTime;
            m_syncedVideoTime = m_videoTime;
        }
        saveSettings();
        update();
    });

    connect(m_mpvProcess, &QProcess::stateChanged, m_openMpvButton, [this] {
        if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
            QTimer::singleShot(500, m_mpvSocket, [this] {
                m_mpvSocket->connectToServer(m_mpvSocketName);
            });
        }
        update();
    });

    connect(m_mpvProcess, &QProcess::errorOccurred, m_openMpvButton, [this] {
        qDebug() << m_mpvProcess->errorString();
        update();
    });

    connect(m_mpvSocket,
            &QLocalSocket::stateChanged,
            this,
            [this](QLocalSocket::LocalSocketState state) {
                if (state == QLocalSocket::LocalSocketState::ConnectedState) {
                    qDebug() << "MPV socket connected";
                    QTimer::singleShot(100, m_mpvSocket, [this] { // WHY IS THIS NECESSARY
                        sendMpvCommand({"observe_property", 1, "playback-time"});
                        sendMpvCommand({"observe_property", 2, "path"});
                        sendMpvCommand({"observe_property", 3, "pause"});
                    });
                } else {
                    qDebug() << "MPV socket no connection";
                }
                update();
            });
    connect(m_mpvSocket, &QLocalSocket::errorOccurred, m_mpvSocket, [this]() {
        qDebug() << "MPV socket error:" << m_mpvSocket->errorString();
    });
    connect(m_mpvSocket, &QLocalSocket::readyRead, this, &VideoSync::onMpvSocketReadyRead);

    connect(m_syncSkewEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        m_syncSkew = text.toDouble();
        if (m_synced) {
            updateMpv();
        }
        update();
    });
    connect(m_syncSkewEdit, &QLineEdit::editingFinished, this, [this]() {
        saveSettings();
        m_syncSkewEdit->setText(QString::asprintf("%.10f", m_syncSkew));
    });
}

void VideoSync::onOpenMpvClick()
{
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        #if defined(Q_OS_UNIX)
                // Attempt to disconnect lingering MPV instances on macOS/Linux. Not sure about Windows.
                QFile::remove(m_mpvSocketName);
        #endif
        QStringList args({
                            QString("--input-ipc-server=") % m_mpvSocketName,
                            "--force-window",
                            "--idle",
                            "--no-config",
                            "--keep-open",
                            "--auto-window-resize=no", // Not implemented on macOS
                            "--ontop",
                            "--pause",
                        });
        if (m_videoPath != "") {
            args.push_back(m_videoPath);
        }
        m_mpvProcess->start(m_mpvPath, args);
    }
}

void VideoSync::onMpvSocketReadyRead()
{
    while (m_mpvSocket->canReadLine()) {
        QByteArray line(m_mpvSocket->readLine());
        qDebug() << "mpv >> " << line;
        QJsonDocument doc(QJsonDocument::fromJson(line));
        QJsonObject obj = doc.object();

        if (obj["event"] == "property-change") {
            if (obj["name"] == "playback-time") {
                m_videoTime = obj["data"].toDouble(0);
                if (m_synced) {
                    // Doing these as a one-liner does not work... am I being stupid about casts
                    double videoTimeDelta = m_videoTime - m_syncedVideoTime;
                    qint64 msDelta = qRound((videoTimeDelta * 1000) / m_syncSkew);
                    m_playheadTime = m_syncedPlayheadTime + msDelta;

                    emit playheadChanged(m_playheadTime);
                }
            } else if (obj["name"] == "path") {
                m_videoPath = obj["data"].toString("");
            } else if (obj["name"] == "pause") {
                m_videoPaused = obj["data"].toBool(true);
            }
        }
    }
    update();
}

void VideoSync::sendMpvCommand(const QJsonArray &cmd)
{
    QJsonObject cmdJ;
    cmdJ["command"] = cmd;
    QString cmdStr(QJsonDocument(cmdJ).toJson(QJsonDocument::Compact));
    qDebug() << "MPV socket << " << cmdStr;
    m_mpvSocket->write(cmdStr.toUtf8());
    m_mpvSocket->write("\n");
    m_mpvSocket->flush();
}

void VideoSync::update() {
    if (m_mpvProcess->state() == QProcess::NotRunning) {
        m_openMpvButton->setEnabled(true);
    } else {
        m_openMpvButton->setEnabled(false);
    }

    // Sync enabled
    if (m_videoPath == "") {
        m_syncButton->setEnabled(false);
    } else {
        m_syncButton->setEnabled(true);
    }

    // Sync button text
    if (m_videoPath == "") {
        m_syncButton->setText(tr("Not Synced - No Video Open"));
    } else if (!m_synced) {
        m_syncButton->setText(tr("Not Synced - Click to Set Sync"));
    } else {
        m_syncButton->setText(tr("Synced - Click to Unsync"));
    }

    // Debug
    m_videoLabel->setText(QString::asprintf("Video time: %.3f", m_videoTime));
    m_playheadLabel->setText(QString::asprintf("Playhead time: %lld", m_playheadTime));
    if (m_synced) {
        m_videoSyncLabel->setText(QString::asprintf("Video sync: %.3f", m_syncedVideoTime));
        m_playheadSyncLabel->setText(QString::asprintf("Playhead sync: %lld", m_syncedPlayheadTime));
    } else {
        m_videoSyncLabel->setText(QString("Video sync: NONE"));
        m_playheadSyncLabel->setText(QString("Playhead sync: NONE"));
    }
}

void VideoSync::loadDate(QDate date) {
    m_date = date;

    // Clear all settings
    m_videoPath = "";
    m_synced = false;

    QFile settingsFile(buildSettingsFilePath());
    if (!settingsFile.exists()) {
        return;
    }
    if (!settingsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open " << settingsFile;
        return;
    }
    // QJsonDocument assumes utf8
    QJsonDocument doc(QJsonDocument::fromJson(settingsFile.readAll()));
    QJsonObject obj = doc.object();

    QJsonArray version = obj["version"].toArray();
    if (version[0].toInt() > 1) {
        qWarning() << "VideoSync save version too new to understand" << version;
        return;
    }
    for (auto videoVal : obj["videos"].toArray()) {
        QJsonObject videoObj = videoVal.toObject();
        m_synced = true;
        m_videoPath = videoObj["filePath"].toString();
        m_syncedVideoTime = videoObj["syncVideoTime"].toDouble();
        m_syncedPlayheadTime = videoObj["syncCpapTime"].toString().toLongLong();
        m_syncSkew = videoObj["syncSkew"].toDouble();
        break; // Assume only one video for now
    }

    update();

    // TODO refresh mpv
}

void VideoSync::saveSettings() {
    QJsonObject settingsJ;
    settingsJ["version"] = QJsonArray({1, 0, 0});
    if (m_synced) {
        settingsJ["videos"] = QJsonArray({
            QJsonObject({
                {"filePath", m_videoPath},
                {"syncVideoTime", m_syncedVideoTime},
                {"syncCpapTime", QString::number(m_syncedPlayheadTime)},
                {"syncSkew", m_syncSkew},
            })}
        );
    } else {
        settingsJ["videos"] = QJsonArray();
    }

    // I think this always emits utf8
    QByteArray settingsStr = QJsonDocument(settingsJ).toJson(QJsonDocument::Indented);
    QFile settingsFile(buildSettingsFilePath());
    settingsFile.open(QIODevice::WriteOnly | QIODevice::Text);
    settingsFile.write(settingsStr);
}

QString VideoSync::buildSettingsFilePath() {
    QDir settingsDir(p_profile->dataFolder());
    settingsDir.mkdir("VideoSync");
    settingsDir.cd("VideoSync");
    QString yearDir(QString::asprintf("%04d", m_date.year()));
    settingsDir.mkdir(yearDir);
    settingsDir.cd(yearDir);
    QString fileName(QString::asprintf("%02d%02d.json", m_date.month(), m_date.day()));
    return settingsDir.filePath(fileName);
}