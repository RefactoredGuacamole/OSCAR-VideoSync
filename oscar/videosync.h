#pragma once

#include <QWidget>
#include <QDate>

class QPushButton;
class QProcess;
class QLocalSocket;
class QJsonArray;
class QGroupBox;
class QLabel;
class QLineEdit;

class VideoSync : public QWidget
{
    Q_OBJECT
public:
    explicit VideoSync(QWidget *parent = nullptr);
    void loadDate(QDate date);

signals:
    void playheadChanged(qint64 t);

public slots:
    void onGraphPlayPauseReq();
    void onPlayheadChanged(qint64 t);

private:
    QString m_mpvPath;
    QString m_mpvSocketName;

    QProcess *m_mpvProcess;
    QLocalSocket *m_mpvSocket;

    QPushButton *m_openMpvButton;
    QPushButton *m_syncButton;
    QLineEdit *m_syncSkewEdit;

    // Debug
    QGroupBox* m_debugBox;
    QLabel* m_videoSyncLabel;
    QLabel* m_playheadSyncLabel;
    QLabel* m_videoLabel;
    QLabel* m_playheadLabel;

    // Cached properties from MPV
    bool m_videoPaused = true;
    double m_videoTime = 0;
    QString m_videoPath; // Not doing much with this right now

    qint64 m_playheadTime = 0;

    QDate m_date;
    bool m_synced = false;
    qint64 m_syncedPlayheadTime = 0;
    double m_syncedVideoTime = 0;
    double m_syncSkew = 1.0;

    void initMpvPaths();
    void createWidgets();
    void connectWidgets();
    void onOpenMpvClick();
    void onMpvSocketReadyRead();
    void sendMpvCommand(const QJsonArray &cmd);
    void update();
    void saveSettings();
    QString buildSettingsFilePath();
    void updateMpv();
};
