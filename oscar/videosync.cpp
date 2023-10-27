#include "videosync.h"

#include <QDir>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QStringBuilder>
#include <QVBoxLayout>
#include <QtDebug>

namespace {
// MPV search paths
#if defined(Q_OS_WIN)
const char *SOCKET_NAME = const QStringList MPV_PATHS = {"mpv"};
#elif defined(Q_OS_MAC)
const char *SOCKET_NAME = "/tmp/mpvsocket";
const QStringList MPV_PATHS = {"mpv", "/opt/homebrew/bin/mpv", "/usr/local/bin/mpv"};
#elif defined(Q_OS_LINUX)
const char *SOCKET_NAME = "/tmp/mpvsocket";
const QStringList MPV_PATHS = {"mpv", QDir::homePath() % "/.local/bin/mpv"};
#endif
} // namespace

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    m_button1 = new QPushButton(tr("Open MPV"));
    auto *label = new QLabel();

    auto *layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(label);
    setLayout(layout);

    m_mpvProcess = new QProcess(this);

    // Hunt MPV path
    for (const auto &mpvPath : MPV_PATHS) {
        if (QProcess::startDetached(mpvPath, {"--version", "--really-quiet"})) {
            m_mpvPath = mpvPath;
            break;
        }
    }
    if (m_mpvPath.length() == 0) {
        m_button1->setText(tr("mpv: command not found"));
        m_button1->setEnabled(false);
    } else {
        connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
        connect(m_mpvProcess, &QProcess::stateChanged, m_button1, [this] {
            if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
                m_button1->setText(tr("Open MPV"));
            } else {
                m_button1->setText(tr("Focus MPV"));
            }
        });
    }
}

void VideoSync::onOpenMpvClick()
{
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        m_mpvProcess->start(m_mpvPath,
                            {QString("--input-ipc-server=") % SOCKET_NAME,
                             "--force-window",
                             "--idle",
                             "--no-config"});
    } else if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
        // TODO focus mpv
    }
}
