#include "videosync.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QProcess>
#include <QLabel>

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    m_button1 = new QPushButton("Open MPV");
    auto* label = new QLabel();

    auto* layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(label);
    setLayout(layout);

    m_mpvProcess = new QProcess(this);

    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
    connect(m_mpvProcess, &QProcess::stateChanged, label, [=] {
        if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
            label->setText("NotRunning");
        } else if (m_mpvProcess->state() == QProcess::ProcessState::Starting) {
            label->setText("Starting");
        } else if (m_mpvProcess->state() == QProcess::ProcessState::Running) {
            label->setText("Running");
        }
    });
}

void VideoSync::onOpenMpvClick() {
    if (m_mpvProcess->state() == QProcess::ProcessState::NotRunning) {
        m_mpvProcess->start("/opt/homebrew/bin/mpv", {"--input-ipc-server=/tmp/mpvsocket", "--force-window", "--idle"});
    }
}
