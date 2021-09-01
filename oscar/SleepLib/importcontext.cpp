/* SleepLib Import Context Implementation
*
* Copyright (c) 2021 The OSCAR Team
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#include <QApplication>
#include <QMessageBox>

#include "SleepLib/importcontext.h"


ImportContext::~ImportContext()
{
    FlushUnexpectedMessages();
}

void ImportContext::LogUnexpectedMessage(const QString & message)
{
    m_mutex.lock();
    m_unexpectedMessages += message;
    m_mutex.unlock();
}

void ImportContext::FlushUnexpectedMessages()
{
    if (m_unexpectedMessages.count() > 0 && m_machine) {
        // Compare this to the list of messages previously seen for this machine
        // and only alert if there are new ones.
        QSet<QString> newMessages = m_unexpectedMessages - m_machine->previouslySeenUnexpectedData();
        if (newMessages.count() > 0) {
            emit importEncounteredUnexpectedData(m_machine->getInfo(), newMessages);
            m_machine->previouslySeenUnexpectedData() += newMessages;
        }
    }
    m_unexpectedMessages.clear();
}


ProfileImportContext::ProfileImportContext(Profile* profile)
    : m_profile(profile)
{
    Q_ASSERT(m_profile);
}

bool ProfileImportContext::ShouldIgnoreOldSessions()
{
    return m_profile->session->ignoreOlderSessions();
}

QDateTime ProfileImportContext::IgnoreSessionsOlderThan()
{
    return m_profile->session->ignoreOlderSessionsDate();
}

Machine* ProfileImportContext::CreateMachineFromInfo(const MachineInfo & info)
{
    m_machineInfo = info;
    m_machine = m_profile->CreateMachine(m_machineInfo);
    return m_machine;
}


// MARK: -

ImportUI::ImportUI(Profile* profile)
    : m_profile(profile)
{
    Q_ASSERT(m_profile);
}

void ImportUI::onUnexpectedData(const MachineInfo & info, QSet<QString> & /*newMessages*/)
{
    QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Untested Data"),
                             QObject::tr("Your %1 %2 (%3) generated data that OSCAR has never seen before.").arg(info.brand).arg(info.model).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("The imported data may not be entirely accurate, so the developers would like a .zip copy of this machine's SD card and matching clinician .pdf reports to make sure OSCAR is handling the data correctly.")
                             ,QMessageBox::Ok);
}

void ImportUI::onDeviceReportsUsageOnly(const MachineInfo & info)
{
    if (m_profile->cpap->brickWarning()) {
        QApplication::processEvents();
        QMessageBox::information(QApplication::activeWindow(),
                                 QObject::tr("Non Data Capable Machine"),
                                 QString(QObject::tr("Your %1 CPAP machine (Model %2) is unfortunately not a data capable model.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                                         QObject::tr("I'm sorry to report that OSCAR can only track hours of use and very basic settings for this machine."))
                                 ,QMessageBox::Ok);
        m_profile->cpap->setBrickWarning(false);
    }
}

void ImportUI::onDeviceIsUntested(const MachineInfo & info)
{
    Machine* m = m_profile->CreateMachine(info);
    if (m_profile->session->warnOnUntestedMachine() && m->warnOnUntested()) {
        m->suppressWarnOnUntested();  // don't warn the user more than once
        QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Machine Untested"),
                             QObject::tr("Your %1 CPAP machine (Model %2) has not been tested yet.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("It seems similar enough to other machines that it might work, but the developers would like a .zip copy of this machine's SD card and matching clinician .pdf reports to make sure it works with OSCAR.")
                             ,QMessageBox::Ok);
    }
}

void ImportUI::onDeviceIsUnsupported(const MachineInfo & info)
{
    QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Machine Unsupported"),
                             QObject::tr("Sorry, your %1 CPAP machine (%2) is not supported yet.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("The developers need a .zip copy of this machine's SD card and matching clinician .pdf reports to make it work with OSCAR.")
                             ,QMessageBox::Ok);
}
