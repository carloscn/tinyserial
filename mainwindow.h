// Qt Cross-platform Project.                                             GitHub
/****************************************************************************/
/*                                                                          */
/*  @file       : mainwindow.h                                              */
/*  @Copyright  : MULTIBEANS ORG rights reserved.                          */
/*  @Revision   : Ver 1.5.                                                  */
/*  @Data       : 2025.12.11 Optimized.                                     */
/*  @Belong     : PROJECT.                                                  */
/*  @Git        : https://github.com/carloscn/tinyserial.git                */
/*  @Platform   : Cross-platform. Qt 5.15+ for Windows/Linux platform.   */
/*  @Encoding   : UTF-8                                                     */
/****************************************************************************/
/*  @Attention:                                                             */
/*  ---------------------------------------------------------------------   */
/*  |    Data    |  Behavior |     Offer      |          Content         |  */
/*  | 2017.09.16 |   create  | Carlos Wei (M) | create the document.     |  */
/*  | 2025.12.11 |   optimize| Carlos Wei (M) | Code refactoring & fix.  |  */
/*  ---------------------------------------------------------------------   */
/*  Email: carlos.wei.hk@gmail.com                              MULTIBEANS.*/
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
#include <QFileDialog>
#include <QRegExp>
#include <QValidator>
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

    void on_pushButton_pause_clicked();

    void on_actionAbout_TinySerialPort_triggered();

    void SoftAutoWriteUart( void );

    void on_checkBox_dispsend_clicked(bool checked);

    void on_checkBox_disptime_clicked(bool checked);

    void on_actionSave_Log_File_triggered();

    void on_comboBox_serialPort_currentIndexChanged(const QString &arg1);

    void on_comboBox_baudrate_editTextChanged(const QString &arg1);

private:
    static constexpr int DEFAULT_REPEAT_INTERVAL_MS = 1000;
    Ui::SerialPort *ui;
    AboutDialog *aboutDialog;
    QTimer *repeatSendTimer;
    QProcess *terminal;
    QSerialPort *serial;
    QString currentConnectCom;
    QByteArray globalRecvData;
    QValidator *validator_combox_baudrate;

    bool isRoot;
    bool isShowSend;
    bool isShowTime;
    bool recAsciiFormat;
    bool sendAsciiFormat;
    bool repeatSend;
    bool pauseComOutput;

    void RefreshTheUSBList();

    // Helper functions for code reuse
    void configureSerialPort();
    void setBaudRate(int index);
    void setStopBits(int index);
    void setParity(int index);
    void setDataBits(int index);
    void setFlowControl(int index);
    QString formatReceiveData(const QByteArray &data, bool isAscii);
    QString formatSendData(const QString &input, bool isAscii);
    void sendData(const QString &input, bool showInDisplay = true);

    QByteArray statusBarComInfo;
    quint64 recCount;
    quint64 sendCount;
};

#endif // SERIALPORT_H
