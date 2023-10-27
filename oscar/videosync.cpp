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
    m_mpvSocketName = QString("oscar_mpv_socket");
#elif defined(Q_OS_UNIX)
    m_mpvSocketName = QString("/tmp/oscar_mpv_socket");
#endif
}

void VideoSync::createWidgets()
{
    m_mpvProcess = new QProcess(this);
    m_mpvSocket = new QLocalSocket(this);

    m_button1 = new QPushButton(tr("Open MPV"));
    m_button2 = new QPushButton(tr("Play/Pause"));

    auto *layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(m_button2);
    layout->setAlignment(Qt::AlignTop);
    setLayout(layout);
}

void VideoSync::connectWidgets()
{
    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_button2, &QPushButton::clicked, this, [this] {
        QJsonObject cmdJ;
        cmdJ["command"] = QJsonArray({"get_property", "volume"});
        QString cmdStr(QJsonDocument(cmdJ).toJson(QJsonDocument::Compact));
        m_mpvSocket->write(cmdStr.toUtf8());
        m_mpvSocket->write("\n");
    });

    connect(m_mpvProcess, &QProcess::stateChanged, m_button1, [this] {
#if defined(Q_OS_UNIX)
        // Attempt to disconnect lingering MPV instances on macOS/Linux. Not sure about Windows.
        QFile::remove(m_mpvSocketName);
#endif
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
    connect(m_mpvSocket, &QLocalSocket::readyRead, this, &VideoSync::onMpvSocketReadyRead);
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

void VideoSync::onMpvSocketReadyRead()
{
    while (m_mpvSocket->canReadLine()) {
        QString line(m_mpvSocket->readLine());
        qDebug() << "MPV socket >> " << line;
    }
}
