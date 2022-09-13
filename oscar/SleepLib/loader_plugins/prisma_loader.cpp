/* SleepLib Prisma Loader Implementation
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QDataStream>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include <cmath>

#include "SleepLib/schema.h"
#include "SleepLib/importcontext.h"
#include "prisma_loader.h"
#include "SleepLib/session.h"
#include "SleepLib/calcs.h"
#include "rawdata.h"

#define CONFIG_FILE "config.pscfg"

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the prisma_data_version in prisma_loader.h when making changes to this
// loader that change loader behaviour or modify channels.
//********************************************************************************************

// parameters
ChannelID Prisma_Mode = 0, Prisma_SoftPAP = 0, Prisma_PSoft = 0, Prisma_PSoft_Min = 0, Prisma_AutoStart = 0, Prisma_Softstart_Time = 0, Prisma_Softstart_TimeMax = 0, Prisma_TubeType = 0, Prisma_PMaxOA = 0;
// waveforms
ChannelID Prisma_ObstructLevel = 0, Prisma_rMVFluctuation = 0, Prisma_rRMV= 0, Prisma_PressureMeasured = 0, Prisma_FlowFull = 0, Prisma_SPRStatus = 0;
// events
ChannelID Prisma_Artifact = 0, Prisma_CriticalLeak = 0, Prisma_eSO = 0, Prisma_eMO = 0, Prisma_eS = 0, Prisma_eF = 0, Prisma_DeepSleep = 0;

QString PrismaLoader::PresReliefLabel() { return QString("SoftPAP: "); }
ChannelID PrismaLoader::PresReliefMode() { return Prisma_SoftPAP; }
ChannelID PrismaLoader::CPAPModeChannel() { return Prisma_Mode; }

//********************************************************************************************

bool WMEDFInfo::ParseSignalData() {
    // Now check the file isn't truncated before allocating space for the values
    long allocsize = 0;
    for (auto & sig : edfsignals) {
        if (edfHdr.num_data_records > 0) {
            allocsize += sig.sampleCnt * edfHdr.num_data_records * 2;
        }
    }
    // allocate the arrays for the signal values
    for (auto & sig : edfsignals) {
        long samples = sig.sampleCnt * edfHdr.num_data_records;
        if (edfHdr.num_data_records <= 0) {
            sig.dataArray = nullptr;
            continue;
        }
        sig.dataArray = new qint16 [samples];
    }
    for (int recNo = 0; recNo < edfHdr.num_data_records; recNo++) {
        for (auto & sig : edfsignals) {
            for (int j=0;j<sig.sampleCnt;j++) {
                if (sig.reserved == "#1") {
                    if (sig.digital_minimum >= 0)
                    {
                        sig.dataArray[recNo*sig.sampleCnt+j]=(qint16)Read8U();
                    }
                    else
                    {
                        sig.dataArray[recNo*sig.sampleCnt+j]=(qint16)Read8S();
                    }
                } else if (sig.reserved == "#2") {
                    qint16 t=Read16();
                    sig.dataArray[recNo*sig.sampleCnt+j]=t;
                }
            }
        }
    }
    return true;
}

qint8 WMEDFInfo::Read8S()
{
    if ((pos + 1) > datasize) {
        eof = true;
        return 0;
    }
    qint8 res = *(qint8 *)&signalPtr[pos];
    pos += 1;
    return res;
}

quint8 WMEDFInfo::Read8U()
{
    if ((pos + 1) > datasize) {
        eof = true;
        return 0;
    }
    quint8 res = *(quint8 *)&signalPtr[pos];
    pos += 1;
    return res;
}

//********************************************************************************************

void PrismaImport::run()
{
    qDebug() << "PRISMA IMPORT" << eventFileName << " " << signalFileName;

    if (!wmedf.Open(signalFileName)) {
        qWarning() << "Signal file open failed" << signalFileName;
        return;
    }

    if (!wmedf.Parse()) {
        qWarning() << "Signal file parsing failed" << signalFileName;
        return;
    }

    eventFile = new PrismaEventFile(eventFileName);

    startdate = qint64(wmedf.edfHdr.startdate_orig.toTime_t()) * 1000L;
    enddate = startdate + wmedf.GetDuration() * qint64(wmedf.GetNumDataRecords()) * 1000;

    session = loader->context()->CreateSession(sessionid);
    session->really_set_first(startdate);
    session->really_set_last(enddate);

    // TODO AXT: set physical limits from a config file
    session->setPhysMax(CPAP_Pressure, 20);
    session->setPhysMin(CPAP_Pressure, 4);
    session->setPhysMax(CPAP_IPAP, 20);
    session->setPhysMin(CPAP_IPAP, 4);
    session->setPhysMax(CPAP_EPAP, 20);
    session->setPhysMin(CPAP_EPAP, 4);

    // set session parameters
    auto parameters = eventFile->getParameters();
    // TODO AXT: extract
    switch(parameters[PRISMA_MODE]) {
        case PRISMA_MODE_CPAP:
            session->settings[CPAP_Mode] = (int)MODE_CPAP;
            session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_CPAP;
        break;
        case PRISMA_MODE_APAP:
            session->settings[CPAP_Mode] = (int)MODE_APAP;
            switch (parameters[PRISMA_APAP_DYNAMIC])
            {
                case PRISMA_APAP_MODE_STANDARD:
                    session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_STD;
                break;
                case PRISMA_APAP_MODE_DYNAMIC:
                    session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_DYN;
                break;
            }

        break;
    }
    session->settings[CPAP_PressureMin] = parameters[PRISMA_PRESSURE] / 100;
    session->settings[CPAP_PressureMax] = parameters[PRISMA_PRESSURE_MAX] / 100;
    session->settings[Prisma_SoftPAP] = parameters[PRISMA_SOFTPAP];
    session->settings[Prisma_PSoft] = parameters[PRISMA_PSOFT] / 100.0;
    session->settings[Prisma_PSoft_Min] = parameters[PRISMA_PSOFT_MIN] / 100;
    session->settings[Prisma_AutoStart] = parameters[PRISMA_AUTOSTART] / 100;
    session->settings[Prisma_Softstart_Time] = parameters[PRISMA_SOFTSTART_TIME];
    session->settings[Prisma_Softstart_TimeMax] = parameters[PRISMA_SOFTSTART_TIME_MAX];
    session->settings[Prisma_TubeType] = parameters[PRISMA_TUBE_TYPE];
    session->settings[Prisma_PMaxOA] = parameters[PRISMA_PMAXOA] / 100;

    // add waveforms
    AddWaveform(CPAP_Pressure, QString("CPAPPressure"));
    AddWaveform(CPAP_EPAP, QString("EPAP"));
    AddWaveform(CPAP_MaskPressure, QString("Pressure"));
    AddWaveform(CPAP_FlowRate, QString("RespFlow"));
    AddWaveform(CPAP_Leak, QString("LeakFlowBreath"));
    AddWaveform(Prisma_ObstructLevel, QString("ObstructLevel"));
    AddWaveform(Prisma_rMVFluctuation, QString("rMVFluctuation"));
    AddWaveform(Prisma_rRMV, QString("rRMV"));
    AddWaveform(Prisma_PressureMeasured, QString("PressureMeasured"));
    AddWaveform(Prisma_FlowFull, QString("FlowFull"));
    AddWaveform(Prisma_SPRStatus, QString("SPRStatus"));

    // add signals
    AddEvents(CPAP_Obstructive, PRISMA_EVENT_OBSTRUCTIVE_APNEA);
    AddEvents(CPAP_ClearAirway, PRISMA_EVENT_CENTRAL_APNEA);
    AddEvents(CPAP_Apnea, { PRISMA_EVENT_APNEA_LEAKAGE, PRISMA_EVENT_APNEA_HIGH_PRESSURE, PRISMA_EVENT_APNEA_MOVEMENT});
    AddEvents(CPAP_Hypopnea, { PRISMA_EVENT_OBSTRUCTIVE_HYPOPNEA, PRISMA_EVENT_CENTRAL_HYPOPNEA});
    AddEvents(CPAP_RERA, PRISMA_EVENT_RERA);
    AddEvents(CPAP_Snore, PRISMA_EVENT_SNORE);
    AddEvents(CPAP_CSR, PRISMA_EVENT_CS_RESPIRATION);
    AddEvents(CPAP_FlowLimit, PRISMA_EVENT_FLOW_LIMITATION);

    AddEvents(Prisma_Artifact, PRISMA_EVENT_ARTIFACT);
    AddEvents(Prisma_CriticalLeak, PRISMA_EVENT_CRITICAL_LEAKAGE);
    AddEvents(Prisma_eSO, PRISMA_EVENT_EPOCH_SEVERE_OBSTRUCTION);
    AddEvents(Prisma_eMO, PRISMA_EVENT_EPOCH_MILD_OBSTRUCTION);
    AddEvents(Prisma_eF, PRISMA_EVENT_EPOCH_FLOW_LIMITATION);
    AddEvents(Prisma_eS, PRISMA_EVENT_EPOCH_SNORE);
    AddEvents(Prisma_DeepSleep, PRISMA_EVENT_EPOCH_DEEPSLEEP);

    session->SetChanged(true);
    loader->context()->AddSession(session);
}

void PrismaImport::AddWaveform(ChannelID code, QString edfLabel)
{
    EDFSignal * es = wmedf.lookupLabel(edfLabel);
    if (es != nullptr) {
        qint64 duration = wmedf.GetNumDataRecords() * wmedf.GetDuration() * 1000L;
        long recs = es->sampleCnt * wmedf.GetNumDataRecords();

        double rate = double(duration) / double(recs);
        EventList *a = session->AddEventList(code, EVL_Waveform, es->gain, es->offset, 0, 0, rate);
        a->setDimension(es->physical_dimension);
        a->AddWaveform(startdate, es->dataArray, recs, duration);

        session->setPhysMin(code, es->physical_minimum);
        session->setPhysMax(code, es->physical_maximum);
    }
}

void PrismaImport::AddEvents(ChannelID channel, QList<Prisma_Event_Type> eventTypes)
{
    EventList *eventList = nullptr;
    for (auto eventType : eventTypes) {
        QList<PrismaEvent> events = eventFile->getEvents(eventType);
        for (auto event: events) {
            if (eventList == nullptr) {
                eventList = session->AddEventList(channel, EVL_Event, 1.0, 0.0, 0.0, 0.0, 0.0, true);
            }
            eventList ->AddEvent(startdate + event.endTime(), event.duration(), event.strength());
        }
    }
    session->AddEventList(channel, EVL_Event);
}

//********************************************************************************************
PrismaEventFile::PrismaEventFile(QString fname)
{
    QFile file(fname);
    QDomDocument dom;

    if(file.open(QIODevice::ReadOnly)) {
        dom.setContent(&file);
        file.close();

        QDomElement root = dom.documentElement();

        QDomNodeList  deviceEventNodelist = root.elementsByTagName("DeviceEvent");
        for(int i=0; i < deviceEventNodelist.count(); i++)
        {
            QDomElement node=deviceEventNodelist.item(i).toElement();
            int eventId = node.attribute("DeviceEventID").toInt();
            if (eventId == 0) {
                int parameterId = node.attribute("ParameterID").toInt();
                int value = node.attribute("NewValue").toInt();
                m_parameters[parameterId] = value;
            }
        }

        QDomNodeList  respEventNodelist = root.elementsByTagName("RespEvent");
        for(int i=0; i < respEventNodelist.count(); i++)
        {
            QDomElement node=respEventNodelist.item(i).toElement();
            int eventId = node.attribute("RespEventID").toInt();
            const int time_quantum = 10;
            int endTime  = node.attribute("EndTime").toInt() * 1000 / time_quantum;
            int duration = node.attribute("Duration").toInt() / time_quantum;
            int pressure = node.attribute("Pressure").toInt();
            int strength = node.attribute("Strength").toInt();
            m_events[eventId].append(PrismaEvent(endTime, duration, pressure, strength));
        }
    }
};

//********************************************************************************************

struct PrismaTestedModel
{
    QString deviceId;
    const char* name;
};

static const PrismaTestedModel s_PrismaTestedModels[] = {
    { "0x92", "Prisma Smart" },
    { "0x91", "Prisma Soft" },
    { "", ""}
};

PrismaModelInfo s_PrismaModelInfo;

PrismaModelInfo::PrismaModelInfo ()
{
    for (int i = 0; !s_PrismaTestedModels[i].deviceId.isEmpty(); i++) {
        const PrismaTestedModel & model = s_PrismaTestedModels[i];
        m_modelNames[model.deviceId] = model.name;
    }
}

bool PrismaModelInfo::IsTested(const QString &  deviceId) const
{
    return m_modelNames.contains(deviceId);
};

const char* PrismaModelInfo::Name(const QString & deviceId) const
{
    const char* name;
    if (m_modelNames.contains(deviceId)) {
        name = m_modelNames[deviceId];
    } else {
        name = "Unknown Model";
    }
    return name;
};

//********************************************************************************************

PrismaLoader::PrismaLoader()
{
    m_type = MT_CPAP;
}

PrismaLoader::~PrismaLoader()
{
}

bool PrismaLoader::Detect(const QString & selectedPath)
{
    QFile configFile(selectedPath + QDir::separator() + CONFIG_FILE);
    return configFile.exists();
}

int PrismaLoader::Open(const QString & selectedPath)
{
    if (m_ctx == nullptr) {
        qWarning() << "PrismaLoader::Open() called without a valid m_ctx object present";
        return 0;
    }
    Q_ASSERT(m_ctx);

    qDebug() << "Prisma opening" << selectedPath;

    QString configFilePath = selectedPath + QDir::separator() + CONFIG_FILE;
    QFile configFile(configFilePath);
    if (!configFile.exists()) // TODO AXT || !configFile.isReadable() fails
    {
        qDebug() << "Prisma config file error" << configFile << " " << configFile.exists() << " " << configFile.isReadable();
        return 0;
    }

    m_abort = false;
    emit setProgressValue(0);
    emit updateMessage(QObject::tr("Getting Ready..."));
    QCoreApplication::processEvents();

    MachineInfo info = PeekInfoFromConfig(configFilePath);
    qDebug() << "Prisma machine info" << info.serial;

    if (info.type == MT_UNKNOWN) {
        emit deviceIsUnsupported(info);
        return -1;
    }

    m_ctx->CreateMachineFromInfo(info);

    if (!s_PrismaModelInfo.IsTested(info.modelnumber)) {
        qDebug() << info.modelnumber << "untested";
        emit deviceIsUntested(info);
    }

    emit updateMessage(QObject::tr("Backing Up Files..."));
    QCoreApplication::processEvents();


    QString backupPath = context()->GetBackupPath() + selectedPath.section("/", -1);
    if (QDir::cleanPath(selectedPath).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(selectedPath, backupPath);
    }

    emit updateMessage(QObject::tr("Scanning Files..."));
    QCoreApplication::processEvents();

    // TODO AXT extract
    char out[12];
    int serialInDecimal;
    sscanf(info.serial.toLocal8Bit().data() , "%x", &serialInDecimal);
    snprintf(out, 12, "%010d", serialInDecimal);

    ScanFiles(info, selectedPath + QDir::separator() + out);

    int tasks = countTasks();

    emit updateMessage(QObject::tr("Importing Sessions..."));
    QCoreApplication::processEvents();

    runTasks(AppSetting->multithreading());

    m_ctx->FlushUnexpectedMessages();

    return tasks;
}

MachineInfo PrismaLoader::PeekInfo(const QString & selectedPath)
{
    qDebug() << "PeekInfo " << selectedPath;
    if (!Detect(selectedPath))
        return MachineInfo();

    return PeekInfoFromConfig(selectedPath + QDir::separator() + CONFIG_FILE);
}

MachineInfo PrismaLoader::PeekInfoFromConfig(const QString & configFilePath)
{
    QFile configFile(configFilePath);

    if (configFile.exists()) {
        if (!configFile.open(QIODevice::ReadOnly)) {
            return MachineInfo();
        }
        MachineInfo info = newInfo();
        QByteArray configData = configFile.readAll();
        configFile.close();

        QJsonDocument configDoc(QJsonDocument::fromJson(configData));
        QJsonObject  configObj = configDoc.object();
        QJsonObject  devObj = configObj["dev"].toObject();
        info.modelnumber=configObj["devid"].toString();
        info.model = s_PrismaModelInfo.Name(info.modelnumber);
        info.serial = devObj["sn"].toString();
        // TODO AXT load props
        info.properties["cica"] = "mica";
        return info;
    }
    return MachineInfo();
}

void PrismaLoader::ScanFiles(const MachineInfo& info, const QString & machinePath)
{
    Q_ASSERT(m_ctx);
    qDebug() << "SCANFILES" << machinePath;

    QDir machineDir(machinePath);
    machineDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks);
    machineDir.setSorting(QDir::Name);
    QFileInfoList dayListing = machineDir.entryInfoList();

    QSet<SessionID> sessions;
    QHash<SessionID, QString> eventFiles;
    QHash<SessionID, QString> signalFiles;

    qint64 ignoreBefore = m_ctx->IgnoreSessionsOlderThan().toMSecsSinceEpoch()/1000;
    bool ignoreOldSessions = m_ctx->ShouldIgnoreOldSessions();

    qDebug() << "INFO  " << ignoreBefore << " " << ignoreOldSessions;

    for (auto & dayDirInfo : dayListing) {
        QDir dayDir(dayDirInfo.canonicalFilePath());
        dayDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks);
        dayDir.setSorting(QDir::Name);
        QFileInfoList subDirs = dayDir.entryInfoList();
        QDir dataDir;

        if (subDirs.size() == 0) {
            dataDir = dayDir;
        } else if (subDirs.size() == 1) {
            dataDir = QDir(subDirs.at(0).canonicalFilePath());
        } else {
            qWarning() << "PrismaLoader: Directory structure not recognized!";
            continue;
        }

        dataDir.setFilter(QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks);
        dataDir.setSorting(QDir::Name);

        if (dataDir.exists()) {
            for (auto & inputFile : dataDir.entryInfoList()) {
                QString fileName = inputFile.fileName().toLower();
                if (fileName.startsWith("event_") && fileName.endsWith(".xml")) {
                    SessionID sid = fileName.mid(6,fileName.size()-4-6).toLong();
                    sessions += sid;
                    eventFiles[sid] = inputFile.canonicalFilePath();
                }
                if (inputFile.fileName().toLower().startsWith("signal_") && inputFile.fileName().toLower().endsWith(".wmedf")) {
                    SessionID sid = fileName.mid(7,fileName.size()-6-7).toLong();
                    sessions += sid;
                    signalFiles[sid] = inputFile.canonicalFilePath();
                }
            }
        }
    }

    for(auto & sid : sessions) {
        queTask(new PrismaImport(this, info,  sid, eventFiles[sid], signalFiles[sid]));
    }
}

using namespace schema;

void PrismaLoader::initChannels()
{
    Channel * chan = nullptr;
    channel.add(GRP_CPAP, chan = new Channel(Prisma_Mode=0xe400, SETTING,  MT_CPAP,  SESSION,
        "PrismaMode", QObject::tr("Mode"),
        QObject::tr("PAP Mode"),
        QObject::tr("PAP Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(PRISMA_COMBINED_MODE_CPAP, QObject::tr("CPAP"));
    chan->addOption(PRISMA_COMBINED_MODE_APAP_STD, QObject::tr("APAP (std)"));
    chan->addOption(PRISMA_COMBINED_MODE_APAP_DYN, QObject::tr("APAP (dyn)"));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_SoftPAP=0xe401, SETTING,  MT_CPAP,  SESSION,
        "Prisma_SoftPAP",
        QObject::tr("SoftPAP Mode"),
        QObject::tr("SoftPAP Mode"),
        QObject::tr("SoftPAP Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(Prisma_SoftPAP_OFF, QObject::tr("Off"));
    chan->addOption(Prisma_SoftPAP_SLIGHT, QObject::tr("Slight"));
    chan->addOption(Prisma_SoftPAP_STANDARD, QObject::tr("Standard"));

    channel.add(GRP_CPAP, new Channel(Prisma_PSoft=0xe402, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PSoft", QObject::tr("PSoft"),
        QObject::tr("PSoft"),
        QObject::tr("PSoft"),
        STR_UNIT_CMH2O, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_PSoft_Min=0xe403, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PSoft_Min", QObject::tr("PSoftMin"),
        QObject::tr("PSoftMin"),
        QObject::tr("PSoftMin"),
        STR_UNIT_CMH2O, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_AutoStart=0xe404, SETTING,  MT_CPAP,  SESSION,
        "Prisma_AutoStart", QObject::tr("AutoStart"),
        QObject::tr("AutoStart"),
        QObject::tr("AutoStart"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, new Channel(Prisma_Softstart_Time=0xe405, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Softstart_Time", QObject::tr("Softstart_Time"),
        QObject::tr("Softstart_Time"),
        QObject::tr("Softstart_Time"),
        STR_UNIT_Minutes, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_Softstart_TimeMax=0xe406, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Softstart_TimeMax", QObject::tr("Softstart_TimeMax"),
        QObject::tr("Softstart_TimeMax"),
        QObject::tr("Softstart_TimeMax"),
        STR_UNIT_Minutes, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_TubeType=0xe407, SETTING,  MT_CPAP,  SESSION,
        "Prisma_TubeType", QObject::tr("TubeType"),
        QObject::tr("TubeType"),
        QObject::tr("TubeType"),
        STR_UNIT_CM, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_PMaxOA=0xe408, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PMaxOA", QObject::tr("PMaxOA"),
        QObject::tr("PMaxOA"),
        QObject::tr("PMaxOA"),
        STR_UNIT_CMH2O, LOOKUP, Qt::green));


    channel.add(GRP_CPAP, chan = new Channel(Prisma_ObstructLevel=0xe440, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_ObstructLevel",
        QObject::tr("ObstructLevel"),
        // TODO AXT add desc
        QObject::tr("Obstruction Level"),
        QObject::tr("ObstructLevel"),
        STR_UNIT_Percentage, DEFAULT, QColor("light purple")));
    chan->setUpperThreshold(100);
    chan->setLowerThreshold(0);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_rMVFluctuation=0xe441, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_rMVFluctuation",
        QObject::tr("rMVFluctuation"),
        // TODO AXT add desc
        QObject::tr("rMVFluctuation"),
        QObject::tr("rMVFluctuation"),
        STR_UNIT_Unknown, DEFAULT, QColor("light purple")));
    chan->setUpperThreshold(16);
    chan->setLowerThreshold(0);

    channel.add(GRP_CPAP, new Channel(Prisma_rRMV=0xe442, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_rRMV",
        QObject::tr("rRMV"),
        // TODO AXT add desc
        QObject::tr("rRMV"),
        QObject::tr("rRMV"),
        STR_UNIT_Unknown, DEFAULT, QColor("light purple")));

    channel.add(GRP_CPAP, new Channel(Prisma_PressureMeasured=0xe443, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_PressureMeasured",
        QObject::tr("PressureMeasured"),
        // TODO AXT add desc
        QObject::tr("PressureMeasured"),
        QObject::tr("PressureMeasured"),
        STR_UNIT_CMH2O, DEFAULT, QColor("black")));

    channel.add(GRP_CPAP, new Channel(Prisma_FlowFull=0xe444, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_FlowFull",
        QObject::tr("FlowFull"),
        // TODO AXT add desc
        QObject::tr("FlowFull"),
        QObject::tr("FlowFull"),
        STR_UNIT_Unknown, DEFAULT, QColor("black")));

    channel.add(GRP_CPAP, new Channel(Prisma_SPRStatus=0xe445, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_SPRStatus",
        QObject::tr("SPRStatus"),
        // TODO AXT add desc
        QObject::tr("SPRStatus"),
        QObject::tr("SPRStatus"),
        STR_UNIT_Unknown, DEFAULT, QColor("black")));


    channel.add(GRP_CPAP, new Channel(Prisma_Artifact=0xe446, SPAN,  MT_CPAP,   SESSION,
        "Prisma_Artifact",
        QObject::tr("Artifact"),
        // TODO AXT add desc
        QObject::tr("Artifact"),
        QObject::tr("ART"),
        STR_UNIT_Percentage, DEFAULT, QColor("salmon")));

    channel.add(GRP_CPAP, new Channel(Prisma_CriticalLeak = 0xe447, SPAN,  MT_CPAP,   SESSION,
        "Prisma_CriticalLeak",
        QObject::tr("CriticalLeak"),
        // TODO AXT add desc
        QObject::tr("CriticalLeak"),
        QObject::tr("CL"),
        STR_UNIT_EventsPerHour, DEFAULT, QColor("orchid")));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eMO = 0xe448, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eMO",
        QObject::tr("eMO"),
        // TODO AXT add desc
        QObject::tr("eMO"),
        QObject::tr("eMO"),
        STR_UNIT_Percentage, DEFAULT, QColor("red")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eSO = 0xe449, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eSO",
        QObject::tr("eSO"),
        // TODO AXT add desc
        QObject::tr("eSO"),
        QObject::tr("eSO"),
        STR_UNIT_Percentage, DEFAULT, QColor("orange")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eS = 0xe44a, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eS",
        QObject::tr("eS"),
        // TODO AXT add desc
        QObject::tr("eS"),
        QObject::tr("eS"),
        STR_UNIT_Percentage, DEFAULT, QColor("light green")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eF = 0xe44b, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eFL",
        QObject::tr("eFL"),
        // TODO AXT add desc
        QObject::tr("eFL"),
        QObject::tr("eFL"),
        STR_UNIT_Percentage, DEFAULT, QColor("yellow")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_DeepSleep = 0xe44c, SPAN,  MT_CPAP,   SESSION,
        "Prisma_DS",
        QObject::tr("DeepSleep"),
        // TODO AXT add desc
        QObject::tr("DeepSleep"),
        QObject::tr("DS"),
        STR_UNIT_Percentage, DEFAULT, QColor("light blue")));
    chan->setEnabled(false);

}

bool PrismaLoader::initialized = false;

void PrismaLoader::Register()
{
    if (initialized) { return; }

    qDebug() << "Registering PrismaLoader";
    RegisterLoader(new PrismaLoader());
    initialized = true;
}
