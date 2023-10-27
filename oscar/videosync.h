#pragma once

#include <QWidget>

class QPushButton;
class QProcess;
class QLocalSocket;
class QJsonArray;

class VideoSync : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSync(QWidget *parent = nullptr);

signals:

public slots:
    void onSpacePressed();

private:
    QString m_mpvPath;
    QString m_mpvSocketName;

    QProcess *m_mpvProcess;
    QLocalSocket *m_mpvSocket;

    QPushButton *m_button1;
    QPushButton *m_button2;

    bool m_mpvPaused = true;
    float m_mpvPlaybackTime = 0;

    void initMpvPaths();
    void createWidgets();
    void connectWidgets();
    void onOpenMpvClick();
    void onMpvSocketReadyRead();
    void sendMpvCommand(const QJsonArray &cmd);
};

