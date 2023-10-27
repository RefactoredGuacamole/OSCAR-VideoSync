#include "videosync.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QProcess>
#include <QLabel>

namespace {
const char *SOCKET_NAME = "/tmp/mpvsocket";
}

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    m_button1 = new QPushButton(tr("Open MPV"));
    auto* label = new QLabel();

    auto* layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(label);
    setLayout(layout);

    m_mpvProcess = new QProcess(this);

    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_mpvProcess, &QProcess::stateChanged, m_button1, [this] {
        if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
            m_button1->setText(tr("Open MPV"));
        } else {
            m_button1->setText(tr("Focus MPV"));
        }
    });
}

void VideoSync::onOpenMpvClick() {
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        m_mpvProcess->start("/opt/homebrew/bin/mpv", {QString("--input-ipc-server=%s").arg(SOCKET_NAME), "--force-window", "--idle"});
    } else {
        // TODO focus mpv
    }
}
