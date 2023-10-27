#include "videosync.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QProcess>

VideoSync::VideoSync(QWidget *parent)
    : QWidget{parent}
{
    m_button1 = new QPushButton("Open MPV");
    m_button2 = new QPushButton("Butts 2");

    auto* layout = new QVBoxLayout();
    layout->addWidget(m_button1);
    layout->addWidget(m_button2);
    setLayout(layout);

    m_mpvProcess = new QProcess(this);

    connect(m_button1, &QPushButton::clicked, this, &VideoSync::onOpenMpvClick);
}

void VideoSync::onOpenMpvClick() {
    if (m_mpvProcess == nullptr) {
        m_button2->setText(std::to_string((uintptr_t)m_mpvProcess).c_str());
    } else {
        m_button2->setText(std::to_string((uintptr_t)m_mpvProcess).c_str());
    }
}
