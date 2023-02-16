/* search  GUI Headers
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SEARCHDAILY_H
#define SEARCHDAILY_H

#include <QDate>
#include <QTextDocument>
#include <QList>
#include <QFrame>
#include <QWidget>
#include <QTabWidget>
#include <QMap>
#include "SleepLib/common.h"

class QWidget ;
class QHBoxLayout ;
class QVBoxLayout ;
class QPushButton ;
class QLabel ;
class QComboBox ;
class QDoubleSpinBox ;
class QSpinBox ;
class QLineEdit ;
class QTableWidget ;
class QTableWidgetItem ;

class Day;  //forward declaration.
class Daily;  //forward declaration.

class DailySearchTab : public QWidget
{
	Q_OBJECT
public:
    DailySearchTab ( Daily* daily , QWidget* , QTabWidget* ) ;
    ~DailySearchTab();

private:

    // these values are hard coded. because dynamic translation might not do the proper assignment.
    // Dynamic code is commented out using c preprocess #if #endif
    const int          TW_NONE       = -1;
    const int          TW_DETAILED   =  0;
    const int          TW_EVENTS     =  1;
    const int          TW_NOTES      =  2;
    const int          TW_BOOKMARK   =  3;
    const int          TW_SEARCH     =  4;

    const int     dateRole = Qt::UserRole;
    const int     valueRole = 1+Qt::UserRole;
    const int     passDisplayLimit = 30;

    Daily*        daily;
    QWidget*      parent;
    QWidget*      searchTabWidget;
    QTabWidget*   dailyTabWidget;

    QVBoxLayout*  searchTabLayout;
    QHBoxLayout*  criteriaLayout;
    QFrame  *     innerCriteriaFrame;
    QHBoxLayout*  innerCriteriaLayout;

    QHBoxLayout*  searchLayout;
    QHBoxLayout*  summaryLayout;

    QPushButton*  helpInfo;

    QComboBox*    selectOperationCombo;
    QPushButton*  selectOperationButton;
    QComboBox*    selectCommandCombo;
    QPushButton*  selectCommandButton;
    QPushButton*  selectMatch;
    QLabel*       selectUnits;

    QLabel*       statusProgress;

    QLabel*       summaryProgress;
    QLabel*       summaryFound;
    QLabel*       summaryMinMax;

    QDoubleSpinBox* selectDouble;
    QSpinBox*     selectInteger;
    QLineEdit*    selectString;
    QPushButton*  startButton;

    QTableWidget* guiDisplayTable;
    QTableWidgetItem* horizontalHeader0;
    QTableWidgetItem* horizontalHeader1;


    QIcon*        m_icon_selected;
    QIcon*        m_icon_notSelected;
    QIcon*        m_icon_configure;

    QMap <QString,qint32> opCodeMap;
    QString     opCodeStr(int);
    int         selectOperationOpCode = 0;


    bool        helpMode=false;
    void        createUi();
    void        delayedCreateUi();

    void        search(QDate date);
    bool        find(QDate& , Day* day);
    void        criteriaChanged();
    void        endOfPass();
    void        displayStatistics();


    void        addItem(QDate date, QString value);
    void        setCommandPopupEnabled(bool );
    void        setOperationPopupEnabled(bool );
    void        setOperation( );

    QString     helpStr();
    QString     centerLine(QString line);
    QString     formatTime (qint32) ;
    QString     convertRichText2Plain (QString rich);
    EventDataType calculateAhi(Day* day);
    bool        compare(int,int );
    
    bool        createUiFinished=false;
    bool        startButtonMode=true;
    int         searchType;
    int         nextTab;

    QDate       firstDate ;
    QDate       lastDate ;
    QDate       nextDate;

    // 
    int         daysTotal;
    int         daysSkipped;
    int         daysSearched;
    int         daysFound;
    int         passFound;

    enum ValueMode { notUsed , minutesToMs ,hoursToMs, hundredths , whole , string};

    ValueMode   valueMode;
    qint32     selectValue=0;

    bool        minMaxValid;
    qint32     minInteger;
    qint32     maxInteger;
    void        updateValues(qint32);

    QString     valueToString(int value, QString empty = "");
    qint32     foundValue;
    QString     foundString;

    double      maxDouble;
    double      minDouble;

    QTextDocument richText;


public slots:
private slots:
    void on_dateItemClicked(QTableWidgetItem *item);
    void on_startButton_clicked();
    void on_selectMatch_clicked();
    void on_selectCommandButton_clicked();
    void on_selectCommandCombo_activated(int);
    void on_selectOperationButton_clicked();
    void on_selectOperationCombo_activated(int);
    void on_helpInfo_clicked();
    void on_dailyTabWidgetCurrentChanged(int);
    void on_intValueChanged(int);
    void on_doubleValueChanged(double);
    void on_textEdited(QString);
};

#endif // SEARCHDAILY_H

