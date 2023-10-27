#pragma once

#include <QWidget>

class QPushButton;
class QProcess;
class QLocalSocket;

class VideoSync : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSync(QWidget *parent = nullptr);

signals:

private:
    void createWidgets();
    void connectWidgets();

    QPushButton *m_button1;
    QProcess* m_mpvProcess;
    QString m_mpvPath;
    QLocalSocket *m_mpvSocket;

    void onOpenMpvClick();
};

