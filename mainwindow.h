// Qt for Linux   Project.                                             GitLab
/****************************************************************************/
/*                                                                          */
/*  @file       : serialport.h                  	                        */
/*  @Copyright  : MULTIBEANS ORG rights reserved.                           */
/*  @Revision   : Ver 1.0.                                                  */
/*  @Data       : 2017.09.16 Realse.                                        */
/*  @Belong     : PROJECT.                                                  */
/*  @Git        : https://gitlab.com/coarlqq/serialPort.git                 */
/*  **code : (UTF-8) in Linux(Ubuntu16.04). Qt 5.7.1 for Linux platform.    */
/****************************************************************************/
/*  @Attention:                                                             */
/*  ---------------------------------------------------------------------   */
/*  |    Data    |  Behavior |     Offer      |          Content         |  */
/*  | 2017.09.16 |   create  |Carlos Lopez(M) | ceate the document.      |  */
/*  ---------------------------------------------------------------------   */
/*  Email: carlos@mltbns.top                                  MULTIBEANS.   */
/****************************************************************************/

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include "global.h"
#include <QDebug>
#include <QFile>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QFont>
#include <QProcess>
#include <QDateTime>
#include <QScrollBar>
#include "aboutdialog.h"

namespace Ui {
class SerialPort;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void StringToHex(QString str, QByteArray &senddata);
    char ConvertHexChar(char ch);
    void initQssStyleSheet();
    long xcount;
    long xrange;

private slots:
    void on_pushButton_open_clicked();

    void on_pushButton_scan_clicked();

    void serialRcvData();
    void on_pushButton_close_clicked();

    void on_comboBox_baudrate_currentIndexChanged(int index);


    void on_comboBox_stopbits_currentIndexChanged(int index);


    void on_comboBox_checkdigit_currentIndexChanged(int index);

    void on_comboBox_flowctrl_currentIndexChanged(int index);

    void on_radioButton_send_ascii_clicked();

    void on_radioButton_send_hex_clicked();

    void on_radioButton_rec_ascii_clicked();

    void on_radioButton_rec_hex_clicked();

    void on_pushButton_clear_clicked();

    void on_pushButton_send_clicked();

    void on_spinBox_repeat_valueChanged(int arg1);

    void on_checkBox_repeat_clicked(bool checked);

    void on_comboBox_databits_currentIndexChanged(int index);

    void on_checkBox_enableDraw_clicked(bool checked);


    void on_pushButton_pause_clicked();

    void on_actionAbout_TinySerialPort_triggered();

    void SoftAutoWriteUart( void );

    void on_checkBox_dispsend_clicked(bool checked);

    void on_checkBox_disptime_clicked(bool checked);

private:
    Ui::SerialPort *ui;
    AboutDialog *aboutDialog;
    QTimer *repeatSendTimer;
    QProcess *terminal;
    QSerialPort *serial;
    QString currentConnectCom;
    QByteArray globalRecvData;

    bool isRoot;
    bool isShowSend;
    bool isShowTime;
    bool recAsciiFormat;
    bool sendAsciiFormat;
    bool repeatSend;
    bool enableDrawFunction;
    bool pauseComOutput;

    void RefreshTheUSBList( void );


    QByteArray statusBarComInfo;
    quint64 recCount;
    quint64 sendCount;


};

#endif // SERIALPORT_H
