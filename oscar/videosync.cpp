#include "videosync.h"
#include "QtWidgets/qboxlayout.h"

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

void VideoSync::onPlayheadChanged(bool visible, qint64 t) {
    // Because mpv can control graphview and vice versa, check if changed to
    // avoid event loop
    if (visible == m_playheadVisible && t == m_playheadTime) {
        return;
    }

    m_playheadVisible = visible;
    m_playheadTime = t;
    if (m_synced) {
        float newVideoTime = m_syncedVideoTime + ((m_playheadTime - m_syncedPlayheadTime) / 1000.f);
        sendMpvCommand({"seek", newVideoTime, "absolute", "exact"});
    }
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
}

void VideoSync::onOpenMpvClick()
{
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        #if defined(Q_OS_UNIX)
                // Attempt to disconnect lingering MPV instances on macOS/Linux. Not sure about Windows.
                QFile::remove(m_mpvSocketName);
        #endif
        m_mpvProcess->start(m_mpvPath,
                            {
                                QString("--input-ipc-server=") % m_mpvSocketName,
                                "--force-window",
                                "--idle",
                                "--no-config",
                                "--keep-open",
                                "--auto-window-resize=no", // Not implemented on macOS
                                "--ontop",
                                "--pause",
                            });
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
                m_videoTime = obj["data"].toDouble(-1);
                if (m_synced) {
                    m_playheadTime = m_syncedPlayheadTime + (m_videoTime - m_syncedVideoTime) * 1000;
                    emit playheadChanged(m_playheadVisible, m_playheadTime);
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

    if (m_mpvSocket->state() != QLocalSocket::ConnectedState) {
        m_synced = false;
        m_videoPath = "";
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
