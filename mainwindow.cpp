// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2017-2025 Carlos Wei and MULTIBEANS ORG.

#include "appversion.h"
#include "mainwindow.h"
#include "serialcodec.h"
#include "ui_serialport.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SerialPort),
    aboutDialog(nullptr),
    repeatSendTimer(new QTimer(this)),
    portWatchTimer(new QTimer(this)),
    displayFlushTimer(new QTimer(this)),
    serial(new QSerialPort(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("tinySerial v") + QLatin1String(APP_VERSION));

    // Send/Receive ASCII Format initialization
    sendAsciiFormat = true;
    recAsciiFormat = true;
    repeatSend = ui->checkBox_repeat->isChecked();
    pauseComOutput = false;
    recCount = 0;
    sendCount = 0;
    validator_combox_baudrate = nullptr;

    // UI initialization
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_scan->setEnabled(true);
    ui->pushButton_send->setEnabled(false);
    ui->comboBox_baudrate->setCurrentIndex(CONFIG_BAUDRATE_115200_INDEX);
    ui->comboBox_checkdigit->setCurrentIndex(CONFIG_PARITY_NONE_INDEX);
    ui->comboBox_databits->setCurrentIndex(CONFIG_DATABITS_8_INDEX);
    ui->comboBox_stopbits->setCurrentIndex(CONFIG_STOPBIT_ONE_INDEX);
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
    connect(serial, &QSerialPort::bytesWritten, this, &MainWindow::onSerialBytesWritten);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
    serial->setReadBufferSize(MAX_DISPLAY_CHARACTERS);
    displayFlushTimer->setInterval(DISPLAY_FLUSH_INTERVAL_MS);
    displayFlushTimer->setSingleShot(true);
    connect(displayFlushTimer, &QTimer::timeout, this, &MainWindow::flushPendingDisplay);
    ui->textBrowser_rec->setLineWrapMode(QPlainTextEdit::NoWrap);
    RefreshTheUSBList();
    portWatchTimer->setInterval(1000);
    connect(portWatchTimer, &QTimer::timeout, this, &MainWindow::RefreshTheUSBList);
    portWatchTimer->start();
    on_checkBox_dispsend_clicked(false);
    on_checkBox_disptime_clicked(false);
}

MainWindow::~MainWindow()
{
    portWatchTimer->stop();
    displayFlushTimer->stop();
    on_pushButton_close_clicked();
    if (validator_combox_baudrate != nullptr) {
        delete validator_combox_baudrate;
        validator_combox_baudrate = nullptr;
        qDebug() << "delete validator before exit";
    }
    delete ui;
}

void MainWindow::serialRcvData()
{
    QByteArray recvArray = serial->readAll();
    if (recvArray.isEmpty()) {
        return;
    }

    recCount += static_cast<quint64>(recvArray.size());
    ui->labelRBytes->setText(QString::number(recCount));

    // Pause deliberately stops rendering, not draining the device buffer.
    // This keeps memory bounded and lets a resumed session receive new data
    // immediately instead of waiting for another readyRead notification.
    if (pauseComOutput) {
        return;
    }

    QString formattedStr = formatReceiveData(recvArray, recAsciiFormat);
    queueDisplayText(formattedStr);
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
    if (ui->comboBox_baudrate->currentIndex() == CONFIG_BAUDRATE_CUSTOM_INDEX) {
        bool valid = false;
        const qint32 baudRate = ui->comboBox_baudrate->currentText().toInt(&valid);
        if (!valid || baudRate <= 0) {
            QMessageBox::warning(this, tr("Invalid baud rate"), tr("Enter a custom baud rate between 1 and 9,999,999."));
            return;
        }
    }
    configureSerialPort();
    
    QString portInfo = ui->comboBox_serialPort->currentText();
    if (portInfo.isEmpty() || ui->comboBox_serialPort->currentIndex() == -1) {
        QMessageBox::warning(this, "Warning", 
            "Please click firstly the scan button to check your available devices.\n"
            "Then connect after selecting one device in the list.");
        return;
    }
    
    currentConnectCom = ui->comboBox_serialPort->currentData().toString();
    if (currentConnectCom.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Invalid port information!");
        return;
    }
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
    const auto ports = QSerialPortInfo::availablePorts();
    QStringList labels;
    bool openPortPresent = false;
    for (const QSerialPortInfo &info : ports) {
        labels.append(info.portName() + ",(" + info.description() + ")");
        if (serial->isOpen() && info.portName() == currentConnectCom)
            openPortPresent = true;
    }

    const bool openPortRemoved = serial->isOpen() && !openPortPresent;
    if (openPortRemoved) {
        const QString removedPort = currentConnectCom;
        on_pushButton_close_clicked();
        ui->statusBar->showMessage("Serial port " + removedPort + " was disconnected");
    }

    QStringList currentLabels;
    for (int i = 0; i < ui->comboBox_serialPort->count(); ++i)
        currentLabels.append(ui->comboBox_serialPort->itemText(i));
    if (currentLabels == labels)
        return;

    const QString selectedPortName = ui->comboBox_serialPort->currentData().toString();
    ui->comboBox_serialPort->blockSignals(true);
    ui->comboBox_serialPort->clear();
    for (int i = 0; i < labels.size(); ++i)
        ui->comboBox_serialPort->addItem(labels.at(i), ports.at(i).portName());
    int keep = -1;
    if (serial->isOpen()) {
        for (int i = 0; i < labels.size(); ++i) {
            if (ports.at(i).portName() == currentConnectCom) {
                keep = i;
                break;
            }
        }
    } else {
        keep = ui->comboBox_serialPort->findData(selectedPortName);
    }
    if (keep >= 0)
        ui->comboBox_serialPort->setCurrentIndex(keep);
    ui->comboBox_serialPort->blockSignals(false);
    ui->comboBox_serialPort->setToolTip(ui->comboBox_serialPort->currentText());

    if (openPortRemoved || serial->isOpen())
        return;
    if (labels.isEmpty())
        ui->statusBar->showMessage("No serial ports available", 2000);
    else
        ui->statusBar->showMessage(QString("Found %1 serial port(s)").arg(labels.size()), 2000);
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
    repeatSend = false;
    repeatSendTimer->stop();
}

void MainWindow::on_comboBox_baudrate_currentIndexChanged(int index)
{
    ui->comboBox_baudrate->setEditable(false);
    ui->comboBox_baudrate->setValidator(nullptr);
    delete validator_combox_baudrate;
    validator_combox_baudrate = nullptr;

    if (index == CONFIG_BAUDRATE_CUSTOM_INDEX) {
        ui->comboBox_baudrate->setEditable(true);
        ui->comboBox_baudrate->setCurrentText("");
        const QRegularExpression expression(QStringLiteral("^[1-9][0-9]{0,6}$"));
        validator_combox_baudrate = new QRegularExpressionValidator(expression, ui->comboBox_baudrate);
        ui->comboBox_baudrate->setValidator(validator_combox_baudrate);
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



void MainWindow::on_spinBox_repeat_valueChanged(int arg1)
{
    if (ui->checkBox_repeat->isChecked() && arg1 > 0) {
        repeatSendTimer->start(arg1);
        qDebug() << "Repeat interval changed:" << arg1 << "ms";
    }
}

void MainWindow::on_checkBox_repeat_clicked(bool checked)
{
    if (checked && !serial->isOpen()) {
        QMessageBox::warning(this, tr("Serial port closed"), tr("Open a serial port before enabling repeat send."));
        ui->checkBox_repeat->setChecked(false);
        repeatSend = false;
        return;
    }

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

void MainWindow::on_checkBox_wordwrap_clicked(bool checked)
{
    ui->textBrowser_rec->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
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

void MainWindow::on_actionAbout_MULTIBEANS_triggered()
{
    QMessageBox::information(this,
                             tr("About MULTIBEANS"),
                             tr("TinySerial is part of the MULTIBEANS project.\nhttps://github.com/carloscn/tinyserial"));
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
    bool valid = false;
    const qint32 baudRate = arg1.toInt(&valid);
    if (valid && baudRate > 0)
        serial->setBaudRate(baudRate);
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
    if (ui->comboBox_baudrate->currentIndex() == CONFIG_BAUDRATE_CUSTOM_INDEX) {
        bool valid = false;
        const qint32 baudRate = ui->comboBox_baudrate->currentText().toInt(&valid);
        if (valid && baudRate > 0)
            serial->setBaudRate(baudRate);
    } else {
        setBaudRate(ui->comboBox_baudrate->currentIndex());
    }
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
        str = SerialCodec::encodeHex(data);
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

QString MainWindow::formatSendData(const QByteArray &data, bool isAscii)
{
    QString str = isAscii ? QString::fromLatin1(data) : SerialCodec::encodeHex(data);

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
    } else {
        QString error;
        if (!SerialCodec::decodeHex(input, &dataToSend, &error)) {
            QMessageBox::warning(this, tr("Invalid HEX data"), error);
            return;
        }
    }

    if (serial->isOpen()) {
        qint64 bytesWritten = serial->write(dataToSend);
        if (bytesWritten == -1) {
            QMessageBox::warning(this, "Warning", "Failed to write data to serial port!");
            return;
        }

        if (showInDisplay) {
            queueDisplayText(formatSendData(dataToSend, sendAsciiFormat));
        }
    }
}

void MainWindow::queueDisplayText(const QString &text)
{
    if (text.isEmpty())
        return;

    pendingDisplay.append(text);
    if (pendingDisplay.size() > MAX_DISPLAY_CHARACTERS)
        pendingDisplay = pendingDisplay.right(MAX_DISPLAY_CHARACTERS);
    if (!displayFlushTimer->isActive())
        displayFlushTimer->start();
}

void MainWindow::flushPendingDisplay()
{
    if (pendingDisplay.isEmpty())
        return;

    const QString text = pendingDisplay;
    pendingDisplay.clear();
    appendDisplayText(text);
}

void MainWindow::appendDisplayText(const QString &text)
{
    QPlainTextEdit *display = ui->textBrowser_rec;
    QScrollBar *scrollbar = display->verticalScrollBar();
    const bool followTail = scrollbar->value() >= scrollbar->maximum();

    const QString retainedText = text.size() > MAX_DISPLAY_CHARACTERS ? text.right(MAX_DISPLAY_CHARACTERS) : text;
    const int overflow = display->document()->characterCount() + retainedText.size() - MAX_DISPLAY_CHARACTERS;
    if (overflow > 0) {
        QTextCursor cursor(display->document());
        cursor.setPosition(0);
        cursor.setPosition(overflow, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }

    display->moveCursor(QTextCursor::End);
    display->insertPlainText(retainedText);
    if (followTail)
        scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::onSerialBytesWritten(qint64 bytes)
{
    if (bytes <= 0)
        return;

    sendCount += static_cast<quint64>(bytes);
    ui->labelSBytes->setText(QString::number(sendCount));
}

void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || !serial->isOpen())
        return;

    const QString message = tr("Serial port error: %1").arg(serial->errorString());
    ui->statusBar->showMessage(message, 5000);
    qWarning() << message;
}
