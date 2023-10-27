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
    QString m_mpvPath;
    QString m_mpvSocketName;

    QProcess *m_mpvProcess;
    QLocalSocket *m_mpvSocket;

    QPushButton *m_button1;

    void initMpvPaths();
    void createWidgets();
    void connectWidgets();
    void onOpenMpvClick();
};

