#include "videosync.h"

#include <QDir>
#include <QLabel>
#include <QLocalSocket>
#include <QProcess>
#include <QPushButton>
#include <QStringBuilder>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtDebug>

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    initMpvPaths();
    createWidgets();
    connectWidgets();
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
    m_mpvSocketName = QString("oscar_mpv_socket_") % QUuid().toString();
#elif defined(Q_OS_UNIX)
    m_mpvSocketName = QString("/tmp/oscar_mpv_socket_") % QUuid().toString();
#endif
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
    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_mpvProcess, &QProcess::stateChanged, m_button1, [this] {
        m_button1->setText(tr("Open MPV"));
        if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
            m_button1->setText(tr("Focus MPV"));
            QTimer::singleShot(500, m_mpvSocket, [this] {
                m_mpvSocket->connectToServer(m_mpvSocketName);
            });
        }
    });
    connect(m_mpvProcess, &QProcess::errorOccurred, m_button1, [this] {
        qDebug() << m_mpvProcess->errorString();
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
                                QString("--input-ipc-server=") % m_mpvSocketName,
                                "--force-window",
                                "--idle",
                                "--no-config",
                                "--keep-open",
                            });
    } else if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
    }
}
