/**
 * \brief   This project about TinySerial Tool.
 *
 * \License  THIS FILE IS PART OF MULTIBEANS PROJECT;
 *           all of the files  - The core part of the project;
 *           THIS PROGRAM IS NOT FREE SOFTWARE, NEED MULTIBEANS ORG LIC;
 *
 *                ________________     ___           _________________
 *               |    __    __    |   |   |         |______     ______|
 *               |   |  |  |  |   |   |   |                |   |
 *               |   |  |__|  |   |   |   |________        |   |
 *               |___|        |___|   |____________|       |___|
 *
 *                               MULTIBEANS ORG.
 *                     Homepage: http://www.mlts.tech/
 *
 *           * You can download the license on our Github. ->
 *           * -> https://github.com/carloscn/tinyserial.git  <-
 *           * Copyright (c) 2017 Carlos Wei: carlos.wei.hk@gmail.com.
 *           * Copyright (c) 2013-2025 MULTIBEANS ORG. http://www.mlts.tech/
 *
 *  \note    void.
 ****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*  @file       : mainwindow.cpp                                            */
/*  @Copyright  : MULTIBEANS ORG rights reserved.                          */
/*  @Revision   : Ver 1.5.                                                  */
/*  @Data       : 2025.12.11 Optimized.                                     */
/*  @Belong     : PROJECT.                                                  */
/*  @Git        : https://github.com/carloscn/tinyserial.git                */
/*  @Platform   : Cross-platform. Qt 5.15+ for Windows/Linux platform.    */
/*  @Encoding   : UTF-8                                                     */
/****************************************************************************/
/*  @Attention:                                                             */
/*  ---------------------------------------------------------------------   */
/*  |    Data    |  Behavior |     Offer      |          Content         |  */
/*  | 2017.09.16 |   create  | Carlos Wei (M) | create the document.     |  */
/*  | 2020.09.25 |   modify  | Carlos Wei (M) | v2.0  the document.      |  */
/*  | 2025.12.11 |   optimize| Carlos Wei (M) | Code refactoring & fix.  |  */
/*  ---------------------------------------------------------------------   */
/*  Email: carlos.wei.hk@gmail.com                              MULTIBEANS. */
/****************************************************************************/

#include "mainwindow.h"
#include "ui_serialport.h"

#define VERISON tr("v1.4")

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SerialPort),
    aboutDialog(nullptr),
    repeatSendTimer(new QTimer),
    terminal(new QProcess),
    serial(new QSerialPort)
{
    ui->setupUi(this);
    setWindowTitle("tinySerial " + VERISON);

    // Send/Receive ASCII Format initialization
    sendAsciiFormat = true;
    recAsciiFormat = true;
    repeatSend = ui->checkBox_repeat->isChecked();
    pauseComOutput = false;
    recCount = 0;
    sendCount = 0;
    isRoot = false;
    validator_combox_baudrate = nullptr;

    // UI initialization
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_scan->setEnabled(true);
    ui->pushButton_send->setEnabled(false);
    ui->comboBox_baudrate->setCurrentIndex(CONFIG_BAUDRATE_115200_INDEX);
    ui->comboBox_checkdigit->setCurrentIndex(CONFIG_PARITY_NONE_INDEX);
    ui->comboBox_databits->setCurrentIndex(CONFIG_DATABITS_8_INDEX);
    ui->comboBox_stopbits->setCurrentText(CONFIG_STOPBIT_ONE_INDEX);
    ui->comboBox_flowctrl->setCurrentIndex(CONFIG_FLOWCTRL_NONE_INDEX);

    // Timer initialization for repeat send
    int repeatTime = ui->spinBox_repeat->value();
    if (repeatTime <= 0) {
        repeatTime = DEFAULT_REPEAT_INTERVAL_MS;
        ui->spinBox_repeat->setValue(repeatTime);
    }
    if (repeatSend) {
        repeatSendTimer->start(repeatTime);
    } else {
        repeatSendTimer->stop();
    }

    // Theme initialization
#ifdef ENABLE_THEME
    initQssStyleSheet();
#endif
    connect(repeatSendTimer, &QTimer::timeout, this, &MainWindow::SoftAutoWriteUart);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::serialRcvData);
    RefreshTheUSBList();
    on_checkBox_dispsend_clicked(false);
    on_checkBox_disptime_clicked(false);
}

MainWindow::~MainWindow()
{
    on_pushButton_close_clicked();
    if (validator_combox_baudrate != nullptr) {
        delete validator_combox_baudrate;
        validator_combox_baudrate = nullptr;
        qDebug() << "delete validator before exit";
    }
    delete aboutDialog;  // Clean up about dialog
    delete ui;
}

void MainWindow::serialRcvData()
{
    if (pauseComOutput) {
        return;
    }

    QByteArray recvArray = serial->readAll();
    if (recvArray.isEmpty()) {
        return;
    }

    QString formattedStr = formatReceiveData(recvArray, recAsciiFormat);

    QScrollBar *scrollbar = ui->textBrowser_rec->verticalScrollBar();
    ui->textBrowser_rec->insertPlainText(formattedStr);
    scrollbar->setSliderPosition(scrollbar->maximum());

    recCount += (recAsciiFormat ? recvArray.length() : recvArray.toHex().length());
    ui->labelRBytes->setText(QString::number(recCount));
}

void MainWindow::SoftAutoWriteUart()
{
    QString input = ui->textEdit_send->toPlainText();

    if (input.isEmpty()) {
        QMessageBox::warning(this, "Warning", "The text is blank!\n Please input the data then send...");
        ui->checkBox_repeat->setChecked(false);
        repeatSendTimer->stop();
        return;
    }

    sendData(input, true);
}
/**
 * @brief Open serial port with configured parameters
 * Serial port configuration format:
 * 0: STATE | 1: NAME | 2: BAUD | 3: DATABITS | 4: STOP | 5: PARITY
 */
void MainWindow::on_pushButton_open_clicked()
{
    qDebug() << "COM Config Info: ";
    configureSerialPort();
    
    QString portInfo = ui->comboBox_serialPort->currentText();
    if (portInfo.isEmpty() || ui->comboBox_serialPort->currentIndex() == -1) {
        QMessageBox::warning(this, "Warning", 
            "Please click firstly the scan button to check your available devices.\n"
            "Then connect after selecting one device in the list.");
        return;
    }
    
    QList<QString> infoList = portInfo.split(',');
    if (infoList.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Invalid port information!");
        return;
    }
    
    currentConnectCom = infoList.at(0);
    qDebug() << currentConnectCom;
    qDebug() << tr("SYSTEM: Serial port ") + portInfo + tr(" ,system is connecting with it.....");
    serial->setPortName(currentConnectCom);
    
    if (!serial->open(QIODevice::ReadWrite)) {
        QString errorMsg = serial->errorString();
        QMessageBox::warning(this, "Warning", 
            QString("Open serial port failed!\n%1\n\nPlease check:\n"
                    "1. Is the serial port occupied by other software?\n"
                    "2. Is the serial port connection normal?\n"
                    "3. Do you have permission to access the port?")
            .arg(errorMsg));
        qDebug() << tr("SYSTEM: The serial port failed to open, error:") << errorMsg;
        ui->comboBox_serialPort->setEnabled(true);
        ui->statusBar->showMessage("Open: " + portInfo + " failed!");
    } else {
        qDebug() << tr("SYSTEM: The system has been connected with ") + portInfo;
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_open->setEnabled(false);
        ui->comboBox_serialPort->setEnabled(false);
        ui->pushButton_scan->setEnabled(false);
        ui->pushButton_send->setEnabled(true);
        QMessageBox::information(this, "Information", 
            "UART: " + portInfo + " has been connected!\nWait device signals.");
        ui->statusBar->showMessage("Open: " + portInfo + " OK!");
    }
    qDebug() << "The serial has been opened!!";
}

void MainWindow::RefreshTheUSBList()
{
    ui->comboBox_serialPort->clear();
    qDebug() << "Debug: Refresh the list...";

    const auto ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        qDebug() << "No serial ports found.";
        ui->statusBar->showMessage("No serial ports available", 2000);
        return;
    }

    foreach (const QSerialPortInfo &info, ports) {
        QString portName = info.portName();
        QString uartName = info.description();
        QString displayText = portName + ",(" + uartName + ")";

        ui->comboBox_serialPort->addItem(displayText);

        // Note: Permission handling for Linux - may need adjustment for Windows
#ifdef Q_OS_LINUX
        if (!isRoot) {
            qDebug() << "Attempting to set permissions for /dev/" + portName;
            // Use pkexec only if available, otherwise user needs to run with sudo
            terminal->start("pkexec", QStringList() << "chmod" << "666" << "/dev/" + portName);
            isRoot = true;  // Only try once
        }
#endif

        qDebug() << tr("SYSTEM: Scan the uart device: ") + uartName + "(" + portName + ")"
                 + tr(" has been added to the available list!");
    }

    ui->statusBar->showMessage(QString("Found %1 serial port(s)").arg(ports.size()), 2000);
}

void MainWindow::on_pushButton_scan_clicked()
{
    RefreshTheUSBList();
}

void MainWindow::on_pushButton_close_clicked()
{
    serial->close();
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_scan->setEnabled(true);
    ui->comboBox_serialPort->setEnabled(true);
    ui->statusBar->showMessage("No Port is Connected!");
    ui->checkBox_repeat->setChecked(false);
    repeatSendTimer->stop();
}

void MainWindow::on_comboBox_baudrate_currentIndexChanged(int index)
{
    ui->comboBox_baudrate->setEditable(false);
    if (validator_combox_baudrate != nullptr) {
        delete validator_combox_baudrate;
        validator_combox_baudrate = nullptr;
        qDebug() << "delete and clear validator";
    }

    if (index == CONFIG_BAUDRATE_CUSTOM_INDEX) {
        ui->comboBox_baudrate->setEditable(true);
        ui->comboBox_baudrate->setCurrentText("");
        QRegExp regx("[0-9]{1,7}");  // Allow 1-7 digits instead of exactly 7
        validator_combox_baudrate = new QRegExpValidator(regx, ui->comboBox_baudrate);
        ui->comboBox_baudrate->setValidator(validator_combox_baudrate);
        qDebug() << "validator set for custom baudrate";
    } else {
        setBaudRate(index);
    }
}

void MainWindow::on_comboBox_stopbits_currentIndexChanged(int index)
{
    setStopBits(index);
}

void MainWindow::on_comboBox_checkdigit_currentIndexChanged(int index)
{
    setParity(index);
}

void MainWindow::on_comboBox_flowctrl_currentIndexChanged(int index)
{
    setFlowControl(index);
}

void MainWindow::on_comboBox_databits_currentIndexChanged(int index)
{
    setDataBits(index);
}

void MainWindow::on_radioButton_send_ascii_clicked()
{
    sendAsciiFormat = true;
    qDebug() << "SYSTEM: Set send data by ASCII." ;
}

void MainWindow::on_radioButton_send_hex_clicked()
{
    sendAsciiFormat = false;
    qDebug() << "SYSTEM: Set send data by HEX." ;
}

void MainWindow::on_radioButton_rec_ascii_clicked()
{
    recAsciiFormat = true;
    qDebug() << "SYSTEM: Set recv data by ASCII." ;
}

void MainWindow::on_radioButton_rec_hex_clicked()
{
    recAsciiFormat = false;
    qDebug() << "SYSTEM: Set recv data by HEX." ;
}

void MainWindow::on_pushButton_clear_clicked()
{
    ui->textBrowser_rec->clear();
    recCount = 0;
    sendCount = 0;
    ui->labelRBytes->setText("0");
    ui->labelSBytes->setText("0");
}

void MainWindow::on_pushButton_send_clicked()
{
    QString input = ui->textEdit_send->toPlainText();
    
    if (input.isEmpty()) {
        if (!ui->checkBox_repeat->isChecked()) {
            QMessageBox::warning(this, "Warning", "The send text is empty!\n Please input the data...");
        }
        return;
    }
    
    sendData(input, true);
}

void MainWindow::StringToHex(QString str, QByteArray &senddata)
{
    int hexdata,lowhexdata;
    int hexdatalen = 0;
    int len = str.length();
    senddata.resize(len/2);
    char lstr,hstr;

    for(int i=0; i<len;) {
        //char lstr,
        hstr = str[i].toLatin1();
        if (hstr == ' ') {
            i++;
            continue;
        }
        i++ ;
        if (i >= len)
            break;
        lstr = str[i].toLatin1();
        hexdata = ConvertHexChar(hstr);
        lowhexdata = ConvertHexChar(lstr);
        if ((hexdata == 16) || (lowhexdata == 16))
            break;
        else
            hexdata = hexdata*16+lowhexdata;
        i++;
        senddata[hexdatalen] = (char)hexdata;
        hexdatalen++;
    }
    senddata.resize(hexdatalen);
}

char MainWindow::ConvertHexChar(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return ch-0x30;
    else if ((ch >= 'A') && (ch <= 'F'))
        return ch-'A'+10;
    else if ((ch >= 'a') && (ch <= 'f'))
        return ch-'a'+10;
    else return (-1);
}

void MainWindow::on_spinBox_repeat_valueChanged(int arg1)
{
    if (ui->checkBox_repeat->isChecked() && arg1 > 0) {
        repeatSendTimer->start(arg1);
        qDebug() << "Repeat interval changed:" << arg1 << "ms";
    }
}

void MainWindow::on_checkBox_repeat_clicked(bool checked)
{
    repeatSend = checked;
    if (repeatSend) {
        int interval = ui->spinBox_repeat->value();
        if (interval <= 0) {
            interval = DEFAULT_REPEAT_INTERVAL_MS;
            ui->spinBox_repeat->setValue(interval);
        }
        repeatSendTimer->start(interval);
    } else {
        repeatSendTimer->stop();
    }
}

// Note: on_checkBox_enableDraw_clicked function removed
// The corresponding UI control (checkBox_enableDraw) no longer exists in the UI file

void MainWindow::initQssStyleSheet()
{
    QFile qssFile(":/qss/Aqua.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString qss = QLatin1String(qssFile.readAll());
        qApp->setStyleSheet(qss);
        qssFile.close();
        qDebug() << "QSS stylesheet loaded successfully";
    } else {
        qDebug() << "Failed to load QSS stylesheet:" << qssFile.errorString();
    }
}

void MainWindow::on_pushButton_pause_clicked()
{
    if (pauseComOutput == false) {
        pauseComOutput = true;
        ui->pushButton_pause->setText("start");
    } else {
        pauseComOutput = false;
        ui->pushButton_pause->setText("pause");
    }
}

void MainWindow::on_actionAbout_TinySerialPort_triggered()
{
    if (!aboutDialog) {
        aboutDialog = new AboutDialog(this);
        aboutDialog->setWindowTitle("About");
    }
    aboutDialog->setModal(true);
    aboutDialog->show();
    aboutDialog->raise();
    aboutDialog->activateWindow();
}

void MainWindow::on_checkBox_dispsend_clicked(bool checked)
{
    isShowSend = checked;
}

void MainWindow::on_checkBox_disptime_clicked(bool checked)
{
    isShowTime = checked;
}

void MainWindow::on_actionSave_Log_File_triggered()
{
    if (ui->textBrowser_rec->toPlainText().isEmpty()) {
        QMessageBox::warning(this, "Warning", "The text browser is blank! Can't be saved.\n");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(
                this,
                tr("Save as a log file"),
                QString(),
                tr("Text files (*.txt);;All files (*.*)"));
    if (fileName.isEmpty()) {
        return;  // User cancelled
    }
    
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "Warning", 
            QString("Failed to create log file!\n%1").arg(file.errorString()));
        return;
    }
    
    QTextStream stream(&file);
    stream << ui->textBrowser_rec->toPlainText();
    file.close();
    QMessageBox::information(this, "Info", "Saved log file:\n" + fileName);
}

void MainWindow::on_comboBox_serialPort_currentIndexChanged(const QString &arg1)
{
    ui->comboBox_serialPort->setToolTip(arg1);
}


void MainWindow::on_comboBox_baudrate_editTextChanged(const QString &arg1)
{
    qint32 baud_rate = arg1.toInt();
    serial->setBaudRate(baud_rate);
    qDebug() << "Baud Rate: " << baud_rate;
}

// Helper function implementations
void MainWindow::setBaudRate(int index)
{
    switch(index) {
    case CONFIG_BAUDRATE_1200_INDEX:
        serial->setBaudRate(QSerialPort::Baud1200);
        qDebug() << "Baud Rate: 1200;";
        break;
    case CONFIG_BAUDRATE_2400_INDEX:
        serial->setBaudRate(QSerialPort::Baud2400);
        qDebug() << "Baud Rate: 2400;";
        break;
    case CONFIG_BAUDRATE_4800_INDEX:
        serial->setBaudRate(QSerialPort::Baud4800);
        qDebug() << "Baud Rate: 4800;";
        break;
    case CONFIG_BAUDRATE_9600_INDEX:
        serial->setBaudRate(QSerialPort::Baud9600);
        qDebug() << "Baud Rate: 9600;";
        break;
    case CONFIG_BAUDRATE_19200_INDEX:
        serial->setBaudRate(QSerialPort::Baud19200);
        qDebug() << "Baud Rate: 19200;";
        break;
    case CONFIG_BAUDRATE_38400_INDEX:
        serial->setBaudRate(QSerialPort::Baud38400);
        qDebug() << "Baud Rate: 38400;";
        break;
    case CONFIG_BAUDRATE_57600_INDEX:
        serial->setBaudRate(QSerialPort::Baud57600);
        qDebug() << "Baud Rate: 57600;";
        break;
    case CONFIG_BAUDRATE_115200_INDEX:
        serial->setBaudRate(QSerialPort::Baud115200);
        qDebug() << "Baud Rate: 115200;";
        break;
    }
}

void MainWindow::setStopBits(int index)
{
    switch (index) {
    case CONFIG_STOPBIT_ONE_INDEX:
        serial->setStopBits(QSerialPort::OneStop);
        qDebug() << "stop bits: 1 bit;";
        break;
    case CONFIG_STOPBIT_ONEANDHALF_INDEX:
        serial->setStopBits(QSerialPort::OneAndHalfStop);
        qDebug() << "stop bits: 1.5 bits;";
        break;
    case CONFIG_STOPBIT_TWO_INDEX:
        serial->setStopBits(QSerialPort::TwoStop);
        qDebug() << "stop bits: 2 bits;";
        break;
    }
}

void MainWindow::setParity(int index)
{
    switch (index) {
    case CONFIG_PARITY_NONE_INDEX:
        serial->setParity(QSerialPort::NoParity);
        qDebug() << "parity set: noParity.";
        break;
    case CONFIG_PARITY_EVEN_INDEX:
        serial->setParity(QSerialPort::EvenParity);
        qDebug() << "parity set: EvenParity.";
        break;
    case CONFIG_PARITY_ODD_INDEX:
        serial->setParity(QSerialPort::OddParity);
        qDebug() << "parity set: OddParity.";
        break;
    case CONFIG_PARITY_SPACE_INDEX:
        serial->setParity(QSerialPort::SpaceParity);
        qDebug() << "parity set: SpaceParity.";
        break;
    case CONFIG_PARITY_MARK_INDEX:
        serial->setParity(QSerialPort::MarkParity);
        qDebug() << "parity set: MarkParity.";
        break;
    }
}

void MainWindow::setDataBits(int index)
{
    switch (index) {
    case CONFIG_DATABITS_5_INDEX:
        serial->setDataBits(QSerialPort::Data5);
        qDebug() << "Data bits: 5 bits;";
        break;
    case CONFIG_DATABITS_6_INDEX:
        serial->setDataBits(QSerialPort::Data6);
        qDebug() << "Data bits: 6 bits;";
        break;
    case CONFIG_DATABITS_7_INDEX:
        serial->setDataBits(QSerialPort::Data7);
        qDebug() << "Data bits: 7 bits;";
        break;
    case CONFIG_DATABITS_8_INDEX:
        serial->setDataBits(QSerialPort::Data8);
        qDebug() << "Data bits: 8 bits;";
        break;
    }
}

void MainWindow::setFlowControl(int index)
{
    switch (index) {
    case CONFIG_FLOWCTRL_NONE_INDEX:
        serial->setFlowControl(QSerialPort::NoFlowControl);
        qDebug() << "flow ctrl: no flow ctrl;";
        break;
    case CONFIG_FLOWCTRL_HARD_INDEX:
        serial->setFlowControl(QSerialPort::HardwareControl);
        qDebug() << "flow ctrl: hardware flow ctrl;";
        break;
    case CONFIG_FLOWCTRL_SOFT_INDEX:
        serial->setFlowControl(QSerialPort::SoftwareControl);
        qDebug() << "flow ctrl: software flow ctrl;";
        break;
    }
}

void MainWindow::configureSerialPort()
{
    setBaudRate(ui->comboBox_baudrate->currentIndex());
    setStopBits(ui->comboBox_stopbits->currentIndex());
    setParity(ui->comboBox_checkdigit->currentIndex());
    setDataBits(ui->comboBox_databits->currentIndex());
    setFlowControl(ui->comboBox_flowctrl->currentIndex());
    qDebug() << "--------------------------------;";
}

QString MainWindow::formatReceiveData(const QByteArray &data, bool isAscii)
{
    QString str;
    if (isAscii) {
        str = QString::fromLatin1(data);
    } else {
        str = data.toHex(' ').toUpper();  // Add space between hex bytes and uppercase
    }

    if (isShowSend) {
        str = "rx -> " + str;
    }

    if (isShowTime) {
        QDateTime local = QDateTime::currentDateTime();
        QString localTime = "<" + local.toString("hh:mm:ss.zzz") + ">";
        str = localTime + str;
    }

    return str;
}

QString MainWindow::formatSendData(const QString &input, bool isAscii)
{
    QString str;
    if (isAscii) {
        str = input;
    } else {
        QByteArray temp;
        StringToHex(input, temp);
        str = temp.toHex(' ').toUpper();  // Add space between hex bytes and uppercase
    }

    if (isShowSend) {
        str = "tx -> " + str;
    }

    if (isShowTime) {
        QDateTime local = QDateTime::currentDateTime();
        QString localTime = "<" + local.toString("hh:mm:ss.zzz") + ">";
        str = localTime + str;
    }

    return str;
}

void MainWindow::sendData(const QString &input, bool showInDisplay)
{
    QByteArray dataToSend;

    if (sendAsciiFormat) {
        dataToSend = input.toLatin1();
        qDebug() << "UART SendAscii :" << dataToSend;
    } else {
        StringToHex(input, dataToSend);
        qDebug() << "UART SendHex :" << dataToSend.toHex(' ');
    }

    if (serial->isOpen()) {
        qint64 bytesWritten = serial->write(dataToSend);
        if (bytesWritten == -1) {
            QMessageBox::warning(this, "Warning", "Failed to write data to serial port!");
            return;
        }

        if (showInDisplay) {
            QString formattedStr = formatSendData(input, sendAsciiFormat);
            ui->textBrowser_rec->insertPlainText(formattedStr);
        }

        sendCount += (sendAsciiFormat ? input.length() : dataToSend.length());
        ui->labelSBytes->setText(QString::number(sendCount));
    }
}
