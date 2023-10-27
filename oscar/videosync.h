#pragma once

#include <QWidget>

class QPushButton;
class QProcess;

class VideoSync : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSync(QWidget *parent = nullptr);

signals:

private:
    QPushButton* m_button1;
    QPushButton* m_button2;
    QProcess* m_mpvProcess;

    void onOpenMpvClick();
};

