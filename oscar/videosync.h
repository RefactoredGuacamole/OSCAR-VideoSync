#pragma once

#include <QWidget>

class QPushButton;
class QProcess;
class QLocalSocket;
class QJsonArray;
class QGroupBox;
class QLabel;

class VideoSync : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSync(QWidget *parent = nullptr);

signals:
    void playheadChanged(bool visible, qint64 t);

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

    // Debug
    QGroupBox* m_debugBox;
    QLabel* m_videoSyncLabel;
    QLabel* m_playheadSyncLabel;
    QLabel* m_videoLabel;
    QLabel* m_playheadLabel;

    // Cached properties from MPV
    bool m_videoPaused = true;
    float m_videoTime = 0;
    QString m_videoPath; // Not doing much with this right now

    bool m_playheadVisible = false;
    qint64 m_playheadTime = -1;

    bool m_synced = false;
    qint64 m_syncedPlayheadTime = -1;
    float m_syncedVideoTime = -1;

    void initMpvPaths();
    void createWidgets();
    void connectWidgets();
    void onOpenMpvClick();
    void onMpvSocketReadyRead();
    void sendMpvCommand(const QJsonArray &cmd);
    void update();
};

