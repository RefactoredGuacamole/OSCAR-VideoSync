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
    void onGraphPlayPauseReq();
    void onPlayheadChanged(bool visible, qint64 t);

private:
    QString m_mpvPath;
    QString m_mpvSocketName;

    QProcess *m_mpvProcess;
    QLocalSocket *m_mpvSocket;

    QPushButton *m_openMpvButton;
    QPushButton *m_syncButton;

    bool m_mpvPaused = true;
    float m_mpvPlaybackTime = 0;

    bool m_synced = false;
    qint64 m_syncedPlayheadTime = -1;
    float m_syncedVideoTime = -1;
    QString m_videoPath; // Not doing much with this right now

    void initMpvPaths();
    void createWidgets();
    void connectWidgets();
    void onOpenMpvClick();
    void onMpvSocketReadyRead();
    void sendMpvCommand(const QJsonArray &cmd);
    void update();
};

