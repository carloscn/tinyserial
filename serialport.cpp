/***
 *
 *                        ,%%%%%%%%,
 *                      ,%%/\%%%%/\%%
 *                     ,%%%\c "" J/%%%
 *            %.       %%%%/ o  o \%%%
 *            `%%.     %%%%    _  |%%%
 *             `%%     `%%%%(__Y__)%%'
 *             //       ;%%%%`\-/%%%'
 *            ((       /  `%%%%%%%'
 *             \\    .'          |
 *              \\  /       \  | |
 *               \\/         ) | |
 *                \         /_ | |__
 *                (___________))))))) OFFER.
 *
// Qt for Linux   Project.                                             GitLab
/****************************************************************************/
/*                                                                          */
/*  @file       : serialport.cpp                  	                        */
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

#include "serialport.h"
#include "ui_serialport.h"
#include <QDebug>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QFont>

SerialPort::SerialPort(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SerialPort)
{
    ui->setupUi(this);
    serial = new QSerialPort();
    RefreshTheUSBList();
    connect(serial,SIGNAL(readyRead()),this,SLOT( serialRcvData() ) );

    // value init.
    sendAsciiFormat = true;
    recAsciiFormat = true;
    repeatSend = ui->checkBox_repeat->isChecked();
    pauseComOutput = false;

    // ui
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_scan->setEnabled(true);
    ui->pushButton_send->setEnabled(false);
    ui->comboBox_baudrate->setCurrentIndex( CONFIG_BAUDRATE_115200_INDEX );
    ui->comboBox_checkdigit->setCurrentIndex( CONFIG_PARITY_NONE_INDEX );
    ui->comboBox_databits->setCurrentIndex( CONFIG_DATABITS_8_INDEX );
    ui->comboBox_stopbits->setCurrentText( CONFIG_STOPBIT_ONE_INDEX );
    ui->comboBox_flowctrl->setCurrentIndex( CONFIG_FLOWCTRL_NONE_INDEX );

    // timer
    int repeatTime = ui->spinBox_repeat->text().toInt();
    repeatSendTimer = new QTimer(this);

    if( repeatSend == true ) {
        repeatSendTimer->start( repeatTime );
    }else{
        repeatSendTimer->stop();
    }
    connect( repeatSendTimer, SIGNAL(timeout()), this, SLOT(SoftAutoWriteUart()) );

    xcount = 0;
    xrange = 50;
    ui->plot->setLocale(QLocale(QLocale::English, QLocale::UnitedKingdom)); // period as decimal separator and comma as thousand separator
    ui->plot->legend->setVisible(true);
    QFont legendFont = font();  // start out with MainWindow's font..
    legendFont.setPointSize(9); // and make a bit smaller for legend
    ui->plot->legend->setFont(legendFont);
    ui->plot->legend->setBrush(QBrush(QColor(255,255,255,230)));
    // by default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement:
    ui->plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom|Qt::AlignRight);
    // 生成数据，画出的是抛物线
    ui->plot->addGraph();
    // 为坐标轴添加标签
    ui->plot->xAxis->setLabel("Num");
    ui->plot->yAxis->setLabel("Mag");
    // 设置坐标轴的范围，以看到所有数据
    ui->plot->xAxis->setRange(0, xrange);
    ui-> plot->yAxis->setRange(0, 50);


    //

}

SerialPort::~SerialPort()
{
    delete ui;
}

void SerialPort::serialRcvData( void )
{
    QByteArray recvArray;
    QString recvStr;
    recvArray = serial->readAll();
    recvStr = QString(recvArray);
    if( pauseComOutput == false ) {
        if( recAsciiFormat == true ) {
            // ASCII display.
            qDebug() << recvStr ;
            ui->textBrowser_rec->append( recvStr );
        }else {
            ui->textBrowser_rec->append( recvArray.toHex() );
        }

        if( enableDrawFunction == true ) {

            qDebug() << "draw data!";
#if false
            ui->plot->graph(0)->addData(xcount,mag);
            ui->plot->replot();
#endif
        }
    }
}


void SerialPort::SoftAutoWriteUart( void )
{
    QString input = ui->textEdit_send->toPlainText();
    QByteArray temp;
    qDebug() << "Hello!!!! Timer!";
    if( input.isEmpty() == true ) {
        QMessageBox::warning(this,"Warring","The text is blank!\n Please input the data then send...");
        return;
    }else {

        if( sendAsciiFormat == true ) {
            serial->write( input.toLatin1() );
            qDebug() << "UART SendAscii : " << input.toLatin1();
        }else{
            StringToHex(input, temp);
            serial->write( temp.toHex() );
            qDebug() << "UART SendHex : " << temp.toHex();
        }
    }

}
// 0       1      2      3           4      5
// STATE | NAME | BAUD | DATABAYTE | STOP | PARITY |
void SerialPort::on_pushButton_open_clicked()
{

    qint8 comboxIndex = 0xff;
    // set baud rate.
    comboxIndex = ui->comboBox_baudrate->currentIndex();
    qDebug() << "COM Config Info: ";
    switch( comboxIndex ) {
    case CONFIG_BAUDRATE_1200_INDEX:
        serial->setBaudRate(QSerialPort::Baud1200);

        qDebug() << "Baud Rate: 1200; ";
        break;
    case CONFIG_BAUDRATE_2400_INDEX:
        serial->setBaudRate(QSerialPort::Baud2400);

        qDebug() << "Baud Rate: 2400; ";
        break;
    case CONFIG_BAUDRATE_4800_INDEX:
        serial->setBaudRate(QSerialPort::Baud4800);
        qDebug() << "Baud Rate: 4800; ";

        break;
    case CONFIG_BAUDRATE_9600_INDEX:
        serial->setBaudRate(QSerialPort::Baud9600);
        qDebug() << "Baud Rate: 9600; ";

        break;
    case CONFIG_BAUDRATE_19200_INDEX:
        serial->setBaudRate(QSerialPort::Baud19200);
        qDebug() << "Baud Rate: 19200; ";

        break;
    case CONFIG_BAUDRATE_38400_INDEX:
        serial->setBaudRate(QSerialPort::Baud38400);
        qDebug() << "Baud Rate: 38400; ";

        break;
    case CONFIG_BAUDRATE_57600_INDEX:
        serial->setBaudRate(QSerialPort::Baud57600);
        qDebug() << "Baud Rate: 57600; ";

        break;
    case CONFIG_BAUDRATE_115200_INDEX:
        serial->setBaudRate(QSerialPort::Baud115200);
        qDebug() << "Baud Rate: 115200; ";

        break;
    }
    // set stop bits.
    comboxIndex = ui->comboBox_stopbits->currentIndex();

    switch (comboxIndex) {
    case CONFIG_STOPBIT_ONE_INDEX:
        serial->setStopBits(QSerialPort::OneStop);
        qDebug() << "stop bits: 1 bit; ";
        //statusBarComInfo.at(4) = ""
        break;
    case CONFIG_STOPBIT_ONEANDHALF_INDEX:
        serial->setStopBits(QSerialPort::OneAndHalfStop);
        qDebug() << "stop bits: 1.5 bits;";
        break;
    case CONFIG_STOPBIT_TWO_INDEX:
        serial->setStopBits(QSerialPort::TwoStop);
        qDebug() << "stop bits: 2 bits; ";
        break;
    }

    // set parity.
    comboxIndex = ui->comboBox_checkdigit->currentIndex();
    switch( comboxIndex ) {

    case CONFIG_PARITY_NONE_INDEX:
        serial->setParity( QSerialPort::NoParity );
        qDebug() << "partiy set: noParity.";
        break;

    case CONFIG_PARITY_EVEN_INDEX:
        serial->setParity( QSerialPort::EvenParity );
        qDebug() << "partiy set: EvenParity.";
        break;

    case CONFIG_PARITY_ODD_INDEX:
        serial->setParity( QSerialPort::OddParity );
        qDebug() << "partiy set: OddParity.";
        break;

    case CONFIG_PARITY_SPACE_INDEX:
        serial->setParity( QSerialPort::SpaceParity );
        qDebug() << "partiy set: SpaceParity.";
        break;

    case CONFIG_PARITY_MARK_INDEX:
        serial->setParity( QSerialPort::MarkParity );
        qDebug() << "partiy set: MarkParity.";
        break;

    }

    // set databytes.
    comboxIndex = ui->comboBox_databits->currentIndex();
    switch (comboxIndex) {
    case CONFIG_DATABITS_5_INDEX:
        serial->setDataBits(QSerialPort::Data5);
        qDebug() << "Data bits: 5 bits; ";
        break;

    case CONFIG_DATABITS_6_INDEX:
        serial->setDataBits(QSerialPort::Data6);
        qDebug() << "Data bits: 6 bits; ";
        break;

    case CONFIG_DATABITS_7_INDEX:
        serial->setDataBits(QSerialPort::Data7);
        qDebug() << "Data bits: 7 bits; ";
        break;

    case CONFIG_DATABITS_8_INDEX:
        serial->setDataBits(QSerialPort::Data8);
        qDebug() << "Data bits: 8 bits; ";
        break;
    }
    // set flowctrl
    comboxIndex = ui->comboBox_flowctrl->currentIndex();
    switch (comboxIndex) {
    case CONFIG_FLOWCTRL_NONE_INDEX:
        serial->setFlowControl(QSerialPort::NoFlowControl);
        qDebug() << "flow ctrl: no flow ctrl; ";
        break;
    case CONFIG_FLOWCTRL_HARD_INDEX:
        serial->setFlowControl(QSerialPort::HardwareControl);
        qDebug() << "flow ctrl: hardware flow ctrl; ";
        break;
    case CONFIG_FLOWCTRL_SOFT_INDEX:
        serial->setFlowControl(QSerialPort::SoftwareControl);
        qDebug() << "flow ctrl: software flow ctrl; ";
        break;
    }
    qDebug() << "--------------------------------; \n";

    QString portInfo = ui->comboBox_serialPort->currentText();
    QList<QString> infoList = portInfo.split(',');
    currentConnectCom = infoList.at(0);
    qDebug() << currentConnectCom;
    qDebug() << tr("SYSTEM: Serial port ")+portInfo+tr(" ,system is connecting with it.....");
    serial->setPortName(currentConnectCom);
    //serial.close();
    if( ui->comboBox_serialPort->currentIndex() == -1 ) {
        QMessageBox::warning(this,"Warring","Please click firstly the scan button to check your available devices.\nthan connect after selecting one device in the list.");
        return;
    }
    if (!serial->open(QIODevice::ReadWrite)) {
        QMessageBox::warning(this,"Warring","Open serial port fail!\n Please see the the information window to solve problem.");
        qDebug() << tr("SYSTEM: The serial port failed to open,Please check as follows: ");
        qDebug() << tr("        1> if the serial port is occupied by other software? ");
        qDebug() << tr("        2> if the serial port connection is normal?");
        qDebug() << tr("        3> if the program is run at root user? You can use the cmd sudo ./(programname) and type your password to be done.");

        ui->comboBox_serialPort->setEnabled(true);
    } else {
        qDebug() << tr("SYSTEM: The system has been connected with ")+portInfo+" " ;
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_open->setEnabled(false);
        ui->comboBox_serialPort->setEnabled(false);
        ui->pushButton_scan->setEnabled(false);
        ui->pushButton_send->setEnabled(true);
        QMessageBox::information(this,"Information", "UART: "+ portInfo+" has been connected! \n"+"Wait device signals.");
    }

    qDebug() << "The serial has been openned!! \n";

}

void SerialPort::RefreshTheUSBList( void )
{
    QString portName;
    QString uartName;
    QSerialPortInfo info;
    ui->comboBox_serialPort->clear();

    qDebug() << "Debug: Refresh the list...";
    foreach ( info , QSerialPortInfo::availablePorts()) {
        serial->setPort(info);
        portName = info.portName();
        uartName = info.description();
        ui->comboBox_serialPort->addItem(  portName +",(" +uartName+") "   );
        qDebug() << tr("SYSTEM: Scan the uart device: ")+uartName + "("+portName+")"+tr(" has been added to the available list! ");
    }
}

void SerialPort::on_pushButton_scan_clicked()
{
    RefreshTheUSBList();
}


void SerialPort::on_pushButton_close_clicked()
{
    serial->close();
    ui->pushButton_open->setEnabled( true );
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_scan->setEnabled(true);
    ui->comboBox_serialPort->setEnabled(true);
}

void SerialPort::on_comboBox_baudrate_currentIndexChanged(int index)
{
    switch( index ) {
    case CONFIG_BAUDRATE_1200_INDEX:
        serial->setBaudRate(QSerialPort::Baud1200);
        qDebug() << "Baud Rate: 1200; ";
        break;
    case CONFIG_BAUDRATE_2400_INDEX:
        serial->setBaudRate(QSerialPort::Baud2400);
        qDebug() << "Baud Rate: 2400; ";
        break;
    case CONFIG_BAUDRATE_4800_INDEX:
        serial->setBaudRate(QSerialPort::Baud4800);
        qDebug() << "Baud Rate: 4800; ";
        break;
    case CONFIG_BAUDRATE_9600_INDEX:
        serial->setBaudRate(QSerialPort::Baud9600);
        qDebug() << "Baud Rate: 9600; ";
        break;
    case CONFIG_BAUDRATE_19200_INDEX:
        serial->setBaudRate(QSerialPort::Baud19200);
        qDebug() << "Baud Rate: 19200; ";
        break;
    case CONFIG_BAUDRATE_38400_INDEX:
        serial->setBaudRate(QSerialPort::Baud38400);
        qDebug() << "Baud Rate: 38400; ";
        break;
    case CONFIG_BAUDRATE_57600_INDEX:
        serial->setBaudRate(QSerialPort::Baud57600);
        qDebug() << "Baud Rate: 57600; ";
        break;
    case CONFIG_BAUDRATE_115200_INDEX:
        serial->setBaudRate(QSerialPort::Baud115200);
        qDebug() << "Baud Rate: 115200; ";
        break;
    }

}

void SerialPort::on_comboBox_stopbits_currentIndexChanged(int index)
{
    switch (index) {
    case CONFIG_STOPBIT_ONE_INDEX:
        serial->setStopBits(QSerialPort::OneStop);
        qDebug() << "stop bits: 1 bit; ";
        break;
    case CONFIG_STOPBIT_ONEANDHALF_INDEX:
        serial->setStopBits(QSerialPort::OneAndHalfStop);
        qDebug() << "stop bits: 1.5 bits;";
        break;
    case CONFIG_STOPBIT_TWO_INDEX:
        serial->setStopBits(QSerialPort::TwoStop);
        qDebug() << "stop bits: 2 bits; ";
        break;
    }
}

void SerialPort::on_comboBox_checkdigit_currentIndexChanged(int index)
{

    switch( index ) {

    case CONFIG_PARITY_NONE_INDEX:
        serial->setParity( QSerialPort::NoParity );
        qDebug() << "partiy set: noParity.";
        break;

    case CONFIG_PARITY_EVEN_INDEX:
        serial->setParity( QSerialPort::EvenParity );
        qDebug() << "partiy set: EvenParity.";
        break;

    case CONFIG_PARITY_ODD_INDEX:
        serial->setParity( QSerialPort::OddParity );
        qDebug() << "partiy set: OddParity.";
        break;

    case CONFIG_PARITY_SPACE_INDEX:
        serial->setParity( QSerialPort::SpaceParity );
        qDebug() << "partiy set: SpaceParity.";
        break;

    case CONFIG_PARITY_MARK_INDEX:
        serial->setParity( QSerialPort::MarkParity );
        qDebug() << "partiy set: MarkParity.";
        break;

    }
}

void SerialPort::on_comboBox_flowctrl_currentIndexChanged(int index)
{
    switch (index) {
    case CONFIG_FLOWCTRL_NONE_INDEX:
        serial->setFlowControl(QSerialPort::NoFlowControl);
        qDebug() << "flow ctrl: no flow ctrl; ";
        break;
    case CONFIG_FLOWCTRL_HARD_INDEX:
        serial->setFlowControl(QSerialPort::HardwareControl);
        qDebug() << "flow ctrl: hardware flow ctrl; ";
        break;
    case CONFIG_FLOWCTRL_SOFT_INDEX:
        serial->setFlowControl(QSerialPort::SoftwareControl);
        qDebug() << "flow ctrl: software flow ctrl; ";
        break;
    }
}

void SerialPort::on_comboBox_databits_currentIndexChanged(int index)
{
    switch (index) {
    case CONFIG_DATABITS_5_INDEX:
        serial->setDataBits(QSerialPort::Data5);
        qDebug() << "Data bits: 5 bits; ";
        break;

    case CONFIG_DATABITS_6_INDEX:
        serial->setDataBits(QSerialPort::Data6);
        qDebug() << "Data bits: 6 bits; ";
        break;

    case CONFIG_DATABITS_7_INDEX:
        serial->setDataBits(QSerialPort::Data7);
        qDebug() << "Data bits: 7 bits; ";
        break;

    case CONFIG_DATABITS_8_INDEX:
        serial->setDataBits(QSerialPort::Data8);
        qDebug() << "Data bits: 8 bits; ";
        break;
    }
}
void SerialPort::on_radioButton_send_ascii_clicked()
{
    sendAsciiFormat = true;
    qDebug() << "SYSTEM: Set send data by ASCII." ;
}

void SerialPort::on_radioButton_send_hex_clicked()
{
    sendAsciiFormat = false;
    qDebug() << "SYSTEM: Set send data by HEX." ;
}

void SerialPort::on_radioButton_rec_ascii_clicked()
{
    recAsciiFormat = true;

    qDebug() << "SYSTEM: Set recv data by ASCII." ;
}

void SerialPort::on_radioButton_rec_hex_clicked()
{
    recAsciiFormat = false;
    if( ui->checkBox_enableDraw->isChecked() ) {
        ui->checkBox_enableDraw->setChecked(false);
        enableDrawFunction = false;
    }
    qDebug() << "SYSTEM: Set recv data by HEX." ;
}

void SerialPort::on_pushButton_clear_clicked()
{
    ui->textBrowser_rec->clear();
}

void SerialPort::on_pushButton_send_clicked()
{
    QString input = ui->textEdit_send->toPlainText();
    QByteArray temp;


    if( input.isEmpty() == true ) {
        QMessageBox::warning(this,"Warring","The text is blank!\n Please input the data then send...");
        return;
    }else {

        if( sendAsciiFormat == true ) {
            serial->write( input.toLatin1() );
            qDebug() << "UART SendAscii : " << input.toLatin1();
        }else{
            StringToHex(input, temp);
            serial->write( temp.toHex() );
            qDebug() << "UART SendHex : " << temp.toHex();
        }
    }
}


void SerialPort::StringToHex(QString str, QByteArray &senddata)
{
    int hexdata,lowhexdata;
    int hexdatalen = 0;
    int len = str.length();
    senddata.resize(len/2);
    char lstr,hstr;

    for(int i=0; i<len; ) {
        //char lstr,
        hstr = str[i].toLatin1();
        if(hstr == ' ') {
            i++;
            continue;
        }
        i++ ;
        if(i >= len)
            break;
        lstr = str[i].toLatin1();
        hexdata = ConvertHexChar(hstr);
        lowhexdata = ConvertHexChar(lstr);
        if((hexdata == 16) || (lowhexdata == 16))
            break;
        else
            hexdata = hexdata*16+lowhexdata;
        i++;
        senddata[hexdatalen] = (char)hexdata;
        hexdatalen++;
    }
    senddata.resize(hexdatalen);
}

char SerialPort::ConvertHexChar(char ch)
{
    if((ch >= '0') && (ch <= '9'))
        return ch-0x30;
    else if((ch >= 'A') && (ch <= 'F'))
        return ch-'A'+10;
    else if((ch >= 'a') && (ch <= 'f'))
        return ch-'a'+10;
    else return (-1);
}

void SerialPort::on_spinBox_repeat_valueChanged(int arg1)
{
    if( ui->checkBox_repeat->isChecked() ) {

        repeatSendTimer->start( arg1 );
        qDebug() << "CheckBox value changed:" << arg1;

    }else {
        return;
    }

}

void SerialPort::on_checkBox_repeat_clicked(bool checked)
{
    repeatSend = checked;
    if( repeatSend == true ) {
        repeatSendTimer->start( ui->spinBox_repeat->text().toInt()  );

    }else{
        repeatSendTimer->stop();
    }
}



void SerialPort::on_checkBox_enableDraw_clicked(bool checked)
{
    enableDrawFunction = checked;

    if( enableDrawFunction == true ) {
        if( ui->radioButton_rec_hex->isChecked() ) {
            ui->checkBox_enableDraw->setChecked(false);
            QMessageBox::warning(this,"Warring","Enable draw function is failed!\n Please select the recieve mode by ASCII.");
            enableDrawFunction = false;
        }else{
            qDebug() << "Enable draw function.";
        }
    }else{
        enableDrawFunction = false;
        qDebug() << "Disable draw function.";
    }

}


void SerialPort::on_pushButton_pause_clicked()
{
    if( pauseComOutput == false ) {
        pauseComOutput = true;
        ui->pushButton_pause->setText("start");
    } else{
        pauseComOutput = false;
        ui->pushButton_pause->setText("pause");
    }
}
