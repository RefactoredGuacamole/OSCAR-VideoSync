#include "videosync.h"

#include <QDir>
#include <QLabel>
#include <QLocalSocket>
#include <QProcess>
#include <QPushButton>
#include <QStringBuilder>
#include <QTimer>
#include <QVBoxLayout>
#include <QtDebug>

namespace {
// MPV search paths
#if defined(Q_OS_WIN)
const QStringList MPV_PATHS = {"mpv"};
#elif defined(Q_OS_MAC)
const char *MPV_SOCKET = "/tmp/oscar_mpv_socket";
const QStringList MPV_PATHS = {"mpv", "/opt/homebrew/bin/mpv", "/usr/local/bin/mpv"};
#elif defined(Q_OS_LINUX)
const char *MPV_SOCKET = "/tmp/oscar_mpv_socket";
const QStringList MPV_PATHS = {"mpv", QDir::homePath() % "/.local/bin/mpv"};
#endif
} // namespace

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    createWidgets();
    connectWidgets();
}

void VideoSync::createWidgets()
{
    m_mpvProcess = new QProcess(this);
    m_mpvSocket = new QLocalSocket(this);

    m_button1 = new QPushButton(tr("Open MPV"));
    auto *button2 = new QPushButton(tr("Play/Pause"));

    auto *layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(button2);
    layout->setAlignment(Qt::AlignTop);
    setLayout(layout);
}

void VideoSync::connectWidgets()
{
    // Hunt MPV path
    for (const auto &mpvPath : MPV_PATHS) {
        if (QProcess::startDetached(mpvPath, {"--version", "--really-quiet"})) {
            m_mpvPath = mpvPath;
            break;
        }
    }
    // TODO handle mpv command not found

    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_mpvProcess, &QProcess::stateChanged, m_button1, [this] {
        m_button1->setText(tr("Open MPV"));
        if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
            m_button1->setText(tr("Focus MPV"));
            QTimer::singleShot(500, m_mpvSocket, [this] {
                m_mpvSocket->connectToServer(MPV_SOCKET);
            });
        }
    });

    connect(m_mpvSocket, &QLocalSocket::stateChanged, [](QLocalSocket::LocalSocketState state) {
        if (state == QLocalSocket::LocalSocketState::ConnectedState) {
            qDebug() << "Connected to MPV socket";
        } else {
            qDebug() << "MPV socket no connection";
        }
    });
    connect(m_mpvSocket, &QLocalSocket::errorOccurred, m_mpvSocket, [this]() {
        qDebug() << "MPV socket error:" << m_mpvSocket->errorString();
    });
}

void VideoSync::onOpenMpvClick()
{
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        m_mpvProcess->start(m_mpvPath,
                            {
                                QString("--input-ipc-server=") % MPV_SOCKET,
                                "--force-window",
                                "--idle",
                                "--no-config",
                                "--keep-open",
                            });
    } else if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
        // TODO focus mpv
        //        QTimer::singleShot(0.5, m_mpvSocket, [this] { m_mpvSocket->connectToServer(MPV_SOCKET); });
    }
}
