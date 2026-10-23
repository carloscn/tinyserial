// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2017-2025 Carlos Wei and MULTIBEANS ORG.

#include "appversion.h"
#include "mainwindow.h"
#include "serialcodec.h"
#include "ui_serialport.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSettings>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleHints>
#include <QStandardPaths>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QScrollArea>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QTableView>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Prefer a well-hinted monospace face; the system FixedFont mapping can be a
// thin or bitmap font on some Linux desktops and renders poorly.
QFont terminalMonospaceFont()
{
    const QStringList preferred = {
        QStringLiteral("JetBrains Mono"), QStringLiteral("Cascadia Mono"),
        QStringLiteral("Cascadia Code"), QStringLiteral("Consolas"),
        QStringLiteral("Menlo"), QStringLiteral("Noto Sans Mono"),
        QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Liberation Mono")
    };
    const QStringList available = QFontDatabase().families();
    for (const QString &family : preferred) {
        if (available.contains(family))
            return QFont(family);
    }
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

} // namespace

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SerialPort),
    aboutDialog(nullptr),
    repeatSendTimer(new QTimer(this)),
    portWatchTimer(new QTimer(this)),
    displayFlushTimer(new QTimer(this)),
    serial(new QSerialPort(this)),
    realtimeLogFile(new QFile(this)),
    connectionIndicator(new QLabel(this)),
    connectionTitle(new QLabel(this)),
    receiveTitle(new QLabel(this)),
    transmitTitle(new QLabel(this)),
    languageSelector(new QComboBox(this)),
    themeSelector(new QComboBox(this)),
    settingsGrid(nullptr),
    currentLanguage(UiLanguage::English),
    timelineModel(new TimelineModel(this)),
    timelineFilter(new TimelineFilterModel(this)),
    timelineIdleTimer(new QTimer(this)),
    timelineFlushTimer(new QTimer(this)),
    timelineTab(nullptr),
    timelineView(nullptr),
    timelineShowRx(nullptr),
    timelineShowTx(nullptr),
    timelineShowSystem(nullptr),
    timelineSearch(nullptr),
    timelineContentMode(nullptr),
    timelineAutoScroll(nullptr),
    timelineSummary(nullptr),
    timelineOpenButton(nullptr),
    timelineCsvButton(nullptr),
    timelineJsonButton(nullptr),
    timelineClearButton(nullptr),
    themeMode(UiThemeMode::System),
    darkMode(false)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("tinySerial v") + QLatin1String(APP_VERSION));
    setMinimumSize(1000, 680);
    resize(1240, 800);
    const QSettings settings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"));
    const QString savedTheme = settings.value(QStringLiteral("ui/themeMode"),
                                               QStringLiteral("system")).toString();
    if (savedTheme == QLatin1String("dark"))
        themeMode = UiThemeMode::Dark;
    else if (savedTheme == QLatin1String("light"))
        themeMode = UiThemeMode::Light;
    darkMode = themeMode == UiThemeMode::Dark
        || (themeMode == UiThemeMode::System && systemUsesDarkTheme());
    // Set the terminal font before the stylesheet polishes the text views,
    // otherwise their viewports keep the default font for the placeholder.
    QFont terminalFont = terminalMonospaceFont();
    terminalFont.setPointSizeF(10.5);
    ui->textBrowser_rec->setFont(terminalFont);
    ui->textEdit_send->setFont(terminalFont);
    timelineModel->setContentFont(terminalFont);
    initQssStyleSheet();
    configureLayout();
    configureLanguageSelector();
    qApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) { refreshThemeFromPreference(); });
#endif

    ui->statusBar->addPermanentWidget(connectionIndicator);

    // Send/Receive ASCII Format initialization
    sendAsciiFormat = true;
    recAsciiFormat = true;
    repeatSend = ui->checkBox_repeat->isChecked();
    pauseComOutput = false;
    recCount = 0;
    sendCount = 0;
    validator_combox_baudrate = nullptr;

    const QString defaultCode = QLocale::system().name();
    applyLanguage(uiLanguageFromCode(settings.value(QStringLiteral("ui/language"), defaultCode).toString()));

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

    connect(repeatSendTimer, &QTimer::timeout, this, &MainWindow::SoftAutoWriteUart);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::serialRcvData);
    connect(serial, &QSerialPort::bytesWritten, this, &MainWindow::onSerialBytesWritten);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
    serial->setReadBufferSize(MAX_DISPLAY_CHARACTERS);
    displayFlushTimer->setInterval(DISPLAY_FLUSH_INTERVAL_MS);
    displayFlushTimer->setSingleShot(true);
    connect(displayFlushTimer, &QTimer::timeout, this, &MainWindow::flushPendingDisplay);
    timelineIdleTimer->setInterval(TIMELINE_IDLE_GAP_MS);
    timelineIdleTimer->setSingleShot(true);
    connect(timelineIdleTimer, &QTimer::timeout, this, &MainWindow::flushTimelineLine);
    timelineFlushTimer->setInterval(TIMELINE_FLUSH_INTERVAL_MS);
    timelineFlushTimer->setSingleShot(true);
    connect(timelineFlushTimer, &QTimer::timeout, this, &MainWindow::flushTimelineEvents);
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
    qApp->removeEventFilter(this);
    stopRealtimeLog();
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

    recordTimelineEvents(timelineAssembler.append(recvArray, QDateTime::currentMSecsSinceEpoch()));
    if (timelineAssembler.hasPending())
        timelineIdleTimer->start();

    const QString formattedStr = formatReceiveData(recvArray, recAsciiFormat);
    appendRealtimeLog(formattedStr);

    // Pause deliberately stops rendering, not draining the device buffer or logging.
    // This keeps memory bounded and lets a resumed session receive new data
    // immediately instead of waiting for another readyRead notification.
    if (pauseComOutput) {
        return;
    }

    queueDisplayText(formattedStr);
}

void MainWindow::SoftAutoWriteUart()
{
    QString input = ui->textEdit_send->toPlainText();

    if (input.isEmpty()) {
        showWarning(text("warning"), text("empty_send"));
        ui->checkBox_repeat->setChecked(false);
        repeatSendTimer->stop();
        return;
    }

    sendData(input, TimelineSource::Repeat);
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
            showWarning(text("invalid_baud_title"), text("invalid_baud"));
            return;
        }
    }
    configureSerialPort();
    
    QString portInfo = ui->comboBox_serialPort->currentText();
    if (portInfo.isEmpty() || ui->comboBox_serialPort->currentIndex() == -1) {
        showWarning(text("warning"), text("select_port"));
        return;
    }
    
    currentConnectCom = ui->comboBox_serialPort->currentData().toString();
    if (currentConnectCom.isEmpty()) {
        showWarning(text("warning"), text("invalid_port"));
        return;
    }
    qDebug() << currentConnectCom;
    qDebug() << tr("SYSTEM: Serial port ") + portInfo + tr(" ,system is connecting with it.....");
    serial->setPortName(currentConnectCom);
    
    if (!serial->open(QIODevice::ReadWrite)) {
        const QString errorMsg = serialErrorText(serial->error());
        recordSystemEvent(text("open_failed").arg(portInfo, errorMsg));
        showWarning(text("warning"), text("open_failed").arg(portInfo, errorMsg));
        qDebug() << "SYSTEM: The serial port failed to open, error:" << serial->errorString();
        ui->comboBox_serialPort->setEnabled(true);
        ui->statusBar->showMessage(text("open_failed").arg(portInfo, errorMsg));
    } else {
        qDebug() << tr("SYSTEM: The system has been connected with ") + portInfo;
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_open->setEnabled(false);
        ui->comboBox_serialPort->setEnabled(false);
        ui->pushButton_scan->setEnabled(false);
        ui->pushButton_send->setEnabled(true);
        setConnectionState(true);
        recordSystemEvent(text("ev_connected").arg(currentConnectCom, connectionSummary()));
        showInformation(text("information"), text("open_ok").arg(portInfo));
        ui->statusBar->showMessage(text("open_ok").arg(portInfo));
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
        recordSystemEvent(text("port_disconnected").arg(removedPort));
        on_pushButton_close_clicked();
        ui->statusBar->showMessage(text("port_disconnected").arg(removedPort));
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
        ui->statusBar->showMessage(text("no_ports"), 2000);
    else
        ui->statusBar->showMessage(text("ports_found").arg(labels.size()), 2000);
}

void MainWindow::on_pushButton_scan_clicked()
{
    RefreshTheUSBList();
}

void MainWindow::on_pushButton_close_clicked()
{
    const bool wasOpen = serial->isOpen();
    serial->close();
    if (wasOpen)
        recordSystemEvent(text("ev_disconnected").arg(currentConnectCom));
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_scan->setEnabled(true);
    ui->comboBox_serialPort->setEnabled(true);
    setConnectionState(false);
    ui->statusBar->showMessage(text("no_connection"));
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
            showWarning(text("warning"), text("empty_send"));
        }
        return;
    }
    
    sendData(input, TimelineSource::Manual);
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
        showWarning(text("serial_closed"), text("open_before_repeat"));
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

void MainWindow::configureLayout()
{
    ui->horizontalLayout_4->removeItem(ui->verticalLayout_6);
    ui->verticalLayout_6->setParent(nullptr);
    auto *sidebarContent = new QWidget(this);
    sidebarContent->setObjectName(QStringLiteral("sidebarContent"));
    sidebarContent->setLayout(ui->verticalLayout_6);
    sidebarContent->setMinimumWidth(344);
    sidebarContent->setMaximumWidth(344);

    auto *sidebarScroll = new QScrollArea(this);
    sidebarScroll->setObjectName(QStringLiteral("sidebarScroll"));
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarScroll->setWidget(sidebarContent);
    sidebarScroll->setMinimumWidth(344);
    sidebarScroll->setMaximumWidth(354);
    ui->horizontalLayout_4->insertWidget(0, sidebarScroll);

    ui->horizontalLayout_4->setStretch(0, 0);
    ui->horizontalLayout_4->setStretch(1, 1);
    ui->horizontalLayout_4->setContentsMargins(20, 14, 20, 12);
    ui->horizontalLayout_4->setSpacing(18);
    ui->verticalLayout_6->setSpacing(10);
    ui->verticalLayout_6->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout_6->setAlignment(Qt::AlignTop);

    const QList<QWidget *> panels = {ui->groupBox, ui->groupBox_2, ui->groupBox_3};
    for (QWidget *panel : panels) {
        panel->setMinimumWidth(344);
        panel->setMaximumWidth(344);
        panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    ui->groupBox->setTitle(QString());
    ui->groupBox_2->setTitle(QString());
    ui->groupBox_3->setTitle(QString());
    for (QLabel *title : {connectionTitle, receiveTitle, transmitTitle})
        title->setObjectName(QStringLiteral("sectionTitle"));

    ui->verticalLayout_5->setContentsMargins(18, 16, 18, 18);
    ui->verticalLayout_5->setSpacing(12);
    ui->verticalLayout_5->insertWidget(0, connectionTitle);
    ui->horizontalLayout_13->setContentsMargins(18, 16, 18, 18);
    ui->verticalLayout_2->setSpacing(8);
    ui->verticalLayout_2->insertWidget(0, receiveTitle);
    ui->horizontalLayout_12->setContentsMargins(18, 16, 18, 18);
    ui->verticalLayout_4->setSpacing(10);
    ui->verticalLayout_4->insertWidget(0, transmitTitle);

    const QList<QComboBox *> fields = {
        ui->comboBox_serialPort,
        ui->comboBox_baudrate,
        ui->comboBox_databits,
        ui->comboBox_stopbits,
        ui->comboBox_checkdigit,
        ui->comboBox_flowctrl
    };
    for (QComboBox *field : fields) {
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        field->setMinimumWidth(168);
        field->setMaximumWidth(QWIDGETSIZE_MAX);
    }

    const QList<QLabel *> fieldLabels = {
        ui->label, ui->label_2, ui->DataBits, ui->label_3, ui->label_4, ui->label_5
    };
    ui->verticalLayout_5->removeItem(ui->formLayout);
    delete ui->formLayout;
    ui->formLayout = nullptr;
    settingsGrid = new QGridLayout();
    settingsGrid->setContentsMargins(0, 0, 0, 0);
    settingsGrid->setHorizontalSpacing(18);
    settingsGrid->setVerticalSpacing(6);
    settingsGrid->setColumnStretch(0, 0);
    settingsGrid->setColumnStretch(1, 1);
    for (int row = 0; row < fields.size(); ++row) {
        fieldLabels.at(row)->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        settingsGrid->addWidget(fieldLabels.at(row), row, 0);
        settingsGrid->addWidget(fields.at(row), row, 1);
    }
    ui->verticalLayout_5->insertLayout(1, settingsGrid);
    ui->horizontalLayout_6->setSpacing(8);
    ui->horizontalLayout_7->setSpacing(8);
    ui->horizontalLayout_6->setContentsMargins(0, 2, 0, 0);
    ui->horizontalLayout_7->setContentsMargins(0, 0, 0, 0);
    ui->horizontalLayout->setSpacing(24);
    ui->horizontalLayout_3->setSpacing(24);
    ui->horizontalLayout_2->setSpacing(8);
    ui->verticalLayout_3->setContentsMargins(8, 8, 8, 8);
    ui->verticalLayout_3->setSpacing(10);
    ui->horizontalLayout_5->setSpacing(8);
    ui->horizontalLayout_8->setSpacing(9);
    ui->tabWidget->setDocumentMode(true);
    ui->textBrowser_rec->setReadOnly(true);
    ui->label_10->setAlignment(Qt::AlignCenter);

    ui->line_3->hide();
    ui->line_4->hide();
    ui->pushButton_send->setMinimumWidth(112);
    ui->pushButton_clear->setMinimumWidth(88);
    moveSendAreaBelowTabs();
    configureTimelineTab();
    updateLayoutMetrics();
}

void MainWindow::updateLayoutMetrics()
{
    const QFontMetrics metrics(font());
    const int textHeight = metrics.height();
    const int fieldHeight = qMax(28, textHeight + 8);
    const int buttonHeight = qMax(30, textHeight + 10);
    const int optionHeight = qMax(22, textHeight + 4);

    for (QComboBox *field : {ui->comboBox_serialPort, ui->comboBox_baudrate,
                              ui->comboBox_databits, ui->comboBox_stopbits,
                              ui->comboBox_checkdigit, ui->comboBox_flowctrl}) {
        field->setMinimumHeight(fieldHeight);
        field->setMaximumHeight(fieldHeight);
    }
    ui->spinBox_repeat->setMinimumHeight(fieldHeight);
    ui->spinBox_repeat->setMaximumHeight(fieldHeight);

    for (QWidget *field : {static_cast<QWidget *>(timelineSearch),
                           static_cast<QWidget *>(timelineContentMode)}) {
        field->setMinimumHeight(fieldHeight);
        field->setMaximumHeight(fieldHeight);
    }

    for (QPushButton *button : {ui->pushButton_scan, ui->pushButton_pause,
                                ui->pushButton_open, ui->pushButton_close,
                                ui->pushButton_send, ui->pushButton_clear,
                                timelineOpenButton, timelineCsvButton,
                                timelineJsonButton, timelineClearButton}) {
        button->setMinimumHeight(buttonHeight);
        button->setMaximumHeight(buttonHeight);
    }
    resizeTimelineColumns();

    const QList<QWidget *> options = {
        ui->radioButton_rec_ascii, ui->radioButton_rec_hex, ui->checkBox_wordwrap,
        ui->checkBox_dispsend, ui->checkBox_disptime, ui->checkBox_realtime_log,
        ui->radioButton_send_ascii,
        ui->radioButton_send_hex, ui->checkBox_repeat
    };
    for (QWidget *option : options) {
        option->setMinimumHeight(optionHeight);
    }

    const QList<QWidget *> panels = {ui->groupBox, ui->groupBox_2, ui->groupBox_3};
    const int connectionHeight = ui->verticalLayout_5->contentsMargins().top()
        + ui->verticalLayout_5->contentsMargins().bottom()
        + connectionTitle->sizeHint().height()
        + (3 * ui->verticalLayout_5->spacing())
        + (6 * fieldHeight) + (5 * settingsGrid->verticalSpacing())
        + (2 * buttonHeight) + 2;
    const int receiveHeight = ui->horizontalLayout_13->contentsMargins().top()
        + ui->horizontalLayout_13->contentsMargins().bottom()
        + receiveTitle->sizeHint().height()
        + (5 * ui->verticalLayout_2->spacing())
        + (5 * optionHeight) + 2;
    const int transmitHeight = ui->horizontalLayout_12->contentsMargins().top()
        + ui->horizontalLayout_12->contentsMargins().bottom()
        + transmitTitle->sizeHint().height()
        + (2 * ui->verticalLayout_4->spacing())
        + optionHeight + fieldHeight + 2;

    ui->groupBox->setFixedHeight(connectionHeight);
    ui->groupBox_2->setFixedHeight(receiveHeight);
    ui->groupBox_3->setFixedHeight(transmitHeight);
    if (QWidget *sidebarContent = ui->groupBox->parentWidget())
        sidebarContent->setMinimumHeight(connectionHeight + receiveHeight + transmitHeight
                                         + (2 * ui->verticalLayout_6->spacing()));
}

void MainWindow::configureLanguageSelector()
{
    auto *cornerWidget = new QWidget(ui->menuBar);
    cornerWidget->setObjectName(QStringLiteral("menuControls"));
    auto *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 12, 0);
    cornerLayout->setSpacing(8);

    themeSelector->setObjectName(QStringLiteral("themeSelector"));
    themeSelector->setAccessibleName(QStringLiteral("Theme"));
    themeSelector->addItem(QString(), QStringLiteral("system"));
    themeSelector->addItem(QString(), QStringLiteral("light"));
    themeSelector->addItem(QString(), QStringLiteral("dark"));
    themeSelector->setMinimumWidth(96);
    themeSelector->setMaximumWidth(116);
    switch (themeMode) {
    case UiThemeMode::System: themeSelector->setCurrentIndex(0); break;
    case UiThemeMode::Light: themeSelector->setCurrentIndex(1); break;
    case UiThemeMode::Dark: themeSelector->setCurrentIndex(2); break;
    }
    cornerLayout->addWidget(themeSelector, 0, Qt::AlignVCenter);

    languageSelector->setObjectName(QStringLiteral("languageSelector"));
    languageSelector->setAccessibleName(QStringLiteral("Language"));
    languageSelector->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageSelector->addItem(QString::fromUtf8("中文"), QStringLiteral("zh_CN"));
    languageSelector->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    languageSelector->setMinimumWidth(108);
    languageSelector->setMaximumWidth(124);
    cornerLayout->addWidget(languageSelector, 0, Qt::AlignVCenter);
    ui->menuBar->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    connect(themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        const QString mode = themeSelector->itemData(index).toString();
        if (mode == QLatin1String("dark"))
            themeMode = UiThemeMode::Dark;
        else if (mode == QLatin1String("light"))
            themeMode = UiThemeMode::Light;
        else
            themeMode = UiThemeMode::System;
        refreshThemeFromPreference();
        QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
            .setValue(QStringLiteral("ui/themeMode"), mode);
    });

    connect(languageSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0)
            return;
        const UiLanguage language = uiLanguageFromCode(languageSelector->itemData(index).toString());
        applyLanguage(language);
        QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
            .setValue(QStringLiteral("ui/language"), uiLanguageCode(language));
    });
}

QString MainWindow::text(const char *key) const
{
    return uiText(currentLanguage, key);
}

bool MainWindow::systemUsesDarkTheme() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = qApp->styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        return true;
    if (scheme == Qt::ColorScheme::Light)
        return false;
#endif
    return qApp->palette().color(QPalette::Window).lightness() < 128;
}

void MainWindow::refreshThemeFromPreference()
{
    const bool useDarkTheme = themeMode == UiThemeMode::Dark
        || (themeMode == UiThemeMode::System && systemUsesDarkTheme());
    if (useDarkTheme == darkMode)
        return;
    darkMode = useDarkTheme;
    initQssStyleSheet();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && themeMode == UiThemeMode::System
        && event->type() == QEvent::ApplicationPaletteChange) {
        refreshThemeFromPreference();
    }
    return QMainWindow::eventFilter(watched, event);
}

QString MainWindow::serialErrorText(QSerialPort::SerialPortError error) const
{
    switch (error) {
    case QSerialPort::DeviceNotFoundError: return text("serial_device_not_found");
    case QSerialPort::PermissionError: return text("serial_permission");
    case QSerialPort::OpenError: return text("serial_open_error");
    case QSerialPort::NotOpenError: return text("serial_not_open");
    case QSerialPort::WriteError: return text("serial_write_error");
    case QSerialPort::ReadError: return text("serial_read_error");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    case QSerialPort::ParityError:
    case QSerialPort::FramingError:
    case QSerialPort::BreakConditionError:
        return text("serial_data_error");
#endif
    case QSerialPort::ResourceError: return text("serial_resource_error");
    case QSerialPort::UnsupportedOperationError: return text("serial_unsupported");
    case QSerialPort::TimeoutError: return text("serial_timeout");
    case QSerialPort::UnknownError:
    case QSerialPort::NoError:
        return text("serial_unknown");
    }
    return text("serial_unknown");
}

void MainWindow::showWarning(const QString &title, const QString &message)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(message);
    QPushButton *okButton = box.addButton(text("ok"), QMessageBox::AcceptRole);
    box.setDefaultButton(okButton);
    box.exec();
}

void MainWindow::showInformation(const QString &title, const QString &message)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(message);
    QPushButton *okButton = box.addButton(text("ok"), QMessageBox::AcceptRole);
    box.setDefaultButton(okButton);
    box.exec();
}

void MainWindow::applyLanguage(UiLanguage language)
{
    currentLanguage = language;
    {
        const QSignalBlocker blocker(languageSelector);
        const int index = languageSelector->findData(uiLanguageCode(language));
        if (index >= 0)
            languageSelector->setCurrentIndex(index);
    }

    connectionTitle->setText(text("connection"));
    ui->label->setText(text("serial_port"));
    ui->label_2->setText(text("baud_rate"));
    ui->DataBits->setText(text("data_bits"));
    ui->label_3->setText(text("stop_bits"));
    ui->label_4->setText(text("parity"));
    ui->label_5->setText(text("flow_control"));

    ui->comboBox_baudrate->setItemText(CONFIG_BAUDRATE_CUSTOM_INDEX, text("custom"));
    for (int index = 0; index < ui->comboBox_databits->count(); ++index) {
        const QString bits = QString::number(index + 5);
        if (language == UiLanguage::Chinese)
            ui->comboBox_databits->setItemText(index, bits + QString::fromUtf8(" 位"));
        else if (language == UiLanguage::Spanish)
            ui->comboBox_databits->setItemText(index, bits + QStringLiteral(" bits"));
        else
            ui->comboBox_databits->setItemText(index, bits + QStringLiteral(" bits"));
    }
    ui->comboBox_checkdigit->setItemText(0, text("none"));
    ui->comboBox_checkdigit->setItemText(1, text("even"));
    ui->comboBox_checkdigit->setItemText(2, text("odd"));
    ui->comboBox_checkdigit->setItemText(3, text("space"));
    ui->comboBox_checkdigit->setItemText(4, text("mark"));
    ui->comboBox_flowctrl->setItemText(0, text("none"));
    ui->comboBox_flowctrl->setItemText(1, text("hardware"));
    ui->comboBox_flowctrl->setItemText(2, text("software"));

    ui->pushButton_scan->setText(text("refresh_ports"));
    ui->pushButton_pause->setText(text(pauseComOutput ? "resume_output" : "pause_output"));
    ui->pushButton_open->setText(text("connect"));
    ui->pushButton_close->setText(text("disconnect"));
    receiveTitle->setText(text("receive"));
    ui->checkBox_wordwrap->setText(text("word_wrap"));
    ui->checkBox_dispsend->setText(text("display_sent"));
    ui->checkBox_disptime->setText(text("display_time"));
    ui->checkBox_realtime_log->setText(text("realtime_log"));
    themeSelector->setItemText(0, text("theme_system"));
    themeSelector->setItemText(1, text("theme_light"));
    themeSelector->setItemText(2, text("theme_dark"));
    transmitTitle->setText(text("transmit"));
    ui->checkBox_repeat->setText(text("repeat_send"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->tab), text("terminal"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(ui->draw), text("plotting"));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(timelineTab), text("timeline"));
    timelineShowSystem->setText(text("timeline_system"));
    timelineSearch->setPlaceholderText(text("timeline_search"));
    timelineAutoScroll->setText(text("timeline_autoscroll"));
    timelineOpenButton->setText(text("timeline_open"));
    timelineCsvButton->setText(text("timeline_export_csv"));
    timelineJsonButton->setText(text("timeline_export_jsonl"));
    timelineClearButton->setText(text("clear"));
    timelineModel->setTextProvider([this](const char *key) { return text(key); });
    updateTimelineSummary();
    ui->pushButton_send->setText(text("send"));
    ui->pushButton_clear->setText(text("clear"));
    ui->textBrowser_rec->setPlaceholderText(text("receive_placeholder"));
    ui->textEdit_send->setPlaceholderText(text("send_placeholder"));
    ui->label_7->setText(text("traffic"));
    ui->label_8->setText(text("received"));
    ui->label_9->setText(text("sent"));
    ui->label_10->setText(text("plotting_future"));

    ui->menuFIle->setTitle(text("file"));
    ui->menuAbout->setTitle(text("about"));
    ui->actionSave_Log_File->setText(text("save_log"));
    ui->actionAbout_TinySerialPort->setText(text("about_tinyserial"));
    ui->actionAbout_MULTIBEANS->setText(text("about_multibeans"));
    ui->statusBar->clearMessage();
    updateLayoutMetrics();
    setConnectionState(serial->isOpen());
}

void MainWindow::initQssStyleSheet()
{
    // The default placeholder color is a very faint grey; make it legible.
    // Set here so it follows theme changes, not just the startup theme.
    QPalette placeholderPalette = ui->textBrowser_rec->palette();
    placeholderPalette.setColor(QPalette::PlaceholderText,
                                darkMode ? QColor(0x8f, 0xa1, 0xaa) : QColor(0x61, 0x75, 0x7f));
    ui->textBrowser_rec->setPalette(placeholderPalette);
    ui->textEdit_send->setPalette(placeholderPalette);
    timelineModel->setDarkMode(darkMode);

    setStyleSheet(QStringLiteral(R"QSS(
        QMainWindow, QDialog {
            background: #f6f8fa;
            color: #172b35;
        }
        QWidget {
            color: #172b35;
        }
        QMenuBar {
            background: #ffffff;
            color: #172b35;
            border-bottom: 1px solid #dbe4e8;
            padding: 3px 12px;
        }
        QMenuBar::item { padding: 5px 10px; border-radius: 4px; }
        QMenuBar::item:selected, QMenu::item:selected { background: #fff1eb; color: #dc430b; }
        QMenu { background: #ffffff; border: 1px solid #dbe4e8; padding: 5px; }
        QMenu::item { padding: 7px 22px; border-radius: 4px; }
        QStatusBar {
            background: #ffffff;
            color: #425660;
            border-top: 1px solid #dbe4e8;
            padding: 5px 12px;
        }
        QStatusBar::item { border: none; }
        QScrollArea#sidebarScroll, QWidget#sidebarContent {
            background: transparent;
            border: none;
        }
        QGroupBox {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 10px;
            margin: 0;
            padding: 0;
        }
        /* QLabel inherits QFrame, so the QFrame rule must come first or it wins. */
        QFrame { color: #dbe4e8; }
        QLabel { color: #425660; }
        QLabel#label, QLabel#label_2, QLabel#DataBits, QLabel#label_3, QLabel#label_4, QLabel#label_5 { color: #000000; }
        QMessageBox QLabel { color: #000000; }
        QLabel#sectionTitle {
            color: #172b35;
            font-size: 14px;
            font-weight: 600;
            padding: 0;
            margin: 0 0 2px 0;
        }
        QComboBox, QSpinBox {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 6px;
            padding: 0 10px;
            color: #172b35;
        }
        QComboBox:hover, QSpinBox:hover { background: #f6f8fa; border-color: #dc430b; }
        QComboBox:focus, QSpinBox:focus { border: 1px solid #dc430b; }
        QComboBox::drop-down { border: 0; width: 24px; }
        QComboBox QAbstractItemView {
            background: #ffffff;
            color: #172b35;
            border: 1px solid #dbe4e8;
            selection-background-color: #fff1eb;
            selection-color: #172b35;
            outline: 0;
        }
        QComboBox#languageSelector {
            background: #f6f8fa;
            border: 1px solid #dbe4e8;
            border-radius: 15px;
            min-height: 28px;
            padding: 0 24px 0 12px;
            margin: 2px 0;
        }
        QComboBox#languageSelector:hover, QComboBox#languageSelector:focus {
            background: #ffffff;
            border-color: #dc430b;
        }
        QWidget#menuControls { background: transparent; }
        QComboBox#themeSelector {
            background: #f6f8fa;
            border: 1px solid #dbe4e8;
            border-radius: 15px;
            min-height: 28px;
            padding: 0 24px 0 12px;
            margin: 2px 0;
            color: #425660;
        }
        QComboBox#themeSelector:hover, QComboBox#themeSelector:focus {
            background: #ffffff;
            border-color: #dc430b;
        }
        QPushButton {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 7px;
            padding: 0 14px;
            color: #172b35;
            font-weight: 500;
        }
        QPushButton:hover { background: #eef3f5; border-color: #dc430b; }
        QPushButton:pressed { background: #e5ebee; }
        QPushButton:disabled { background: #f6f8fa; color: #9aadb5; border-color: #dbe4e8; }
        QPushButton#pushButton_open:disabled, QPushButton#pushButton_send:disabled {
            background: #f6f8fa; color: #9aadb5; border-color: #dbe4e8;
        }
        QPushButton#pushButton_close:disabled { background: transparent; color: #c3cfd4; border-color: #e6ecef; }
        QPushButton#pushButton_open:disabled:hover, QPushButton#pushButton_send:disabled:hover {
            background: #f6f8fa; border-color: #dbe4e8;
        }
        QComboBox:disabled, QSpinBox:disabled { background: #f6f8fa; color: #9aadb5; border-color: #dbe4e8; }
        QPushButton#pushButton_open, QPushButton#pushButton_send {
            background: #dc430b;
            border-color: #dc430b;
            color: white;
            font-weight: 600;
        }
        QPushButton#pushButton_open:hover, QPushButton#pushButton_send:hover { background: #bd3607; border-color: #bd3607; }
        QPushButton#pushButton_close { background: transparent; border-color: #e6b7bd; color: #bf3348; }
        QPushButton#pushButton_close:hover { background: #fff0f1; border-color: #bf3348; }
        QPushButton#pushButton_clear { background: transparent; color: #425660; }
        QCheckBox, QRadioButton { spacing: 7px; color: #425660; padding: 3px 0; }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 15px; height: 15px;
            border: 1px solid #b8c7cd;
            background: #ffffff;
        }
        QCheckBox::indicator { border-radius: 4px; }
        QRadioButton::indicator { border-radius: 8px; }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background: #dc430b;
            border-color: #dc430b;
        }
        QPlainTextEdit#textBrowser_rec, QTextEdit#textEdit_send {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 10px;
            padding: 10px;
            color: #172b35;
            selection-background-color: #fff1eb;
        }
        QPlainTextEdit#textBrowser_rec:focus, QTextEdit#textEdit_send:focus { border-color: #dc430b; }
        QFileDialog QLineEdit, QFileDialog QAbstractItemView {
            background: #ffffff;
            color: #172b35;
            border: 1px solid #dbe4e8;
            selection-background-color: #fff1eb;
            selection-color: #172b35;
        }
        QFileDialog QToolButton { background: transparent; border-radius: 5px; padding: 4px; }
        QFileDialog QToolButton:hover { background: #eef3f5; }
        QToolTip { background: #172b35; color: #ffffff; border: 1px solid #172b35; padding: 4px 7px; }
        QTabWidget::pane { border: 0; background: transparent; }
        QTabBar { qproperty-drawBase: 0; }
        QTabBar::tab {
            background: transparent;
            color: #425660;
            border: 0;
            border-bottom: 2px solid transparent;
            padding: 12px 18px;
            margin-right: 8px;
            font-weight: 600;
            min-width: 86px;
        }
        QTabBar::tab:selected { color: #172b35; border-bottom: 3px solid #dc430b; }
        QTabBar::tab:hover:!selected { color: #dc430b; }
        QLabel#labelRBytes { color: #d93900; font-weight: 700; }
        QLabel#labelSBytes { color: #147d5b; font-weight: 700; }
        QLabel#label_8, QLabel#label_9 { color: #425660; }
        QLabel#connectionIndicator {
            background: transparent;
            border: 0;
            border-radius: 6px;
            color: #425660;
            font-weight: 600;
            padding: 3px 4px;
        }
        QLabel#connectionIndicator[connected="true"] {
            color: #147d5b;
        }
        QFrame#line { border: 0; background: #dbe4e8; min-height: 1px; max-height: 1px; }
        QFrame#line_2 { border: 0; background: #dbe4e8; min-width: 1px; max-width: 1px; }
        QLineEdit#timelineSearch {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 6px;
            padding: 0 10px;
            color: #172b35;
            selection-background-color: #fff1eb;
            selection-color: #172b35;
        }
        QLineEdit#timelineSearch:focus { border-color: #dc430b; }
        QTableView#timelineView {
            background: #ffffff;
            border: 1px solid #dbe4e8;
            border-radius: 10px;
            color: #172b35;
            gridline-color: transparent;
            selection-background-color: #ffd9c9;
            selection-color: #172b35;
        }
        QTableView#timelineView QHeaderView::section {
            background: #f6f8fa;
            color: #425660;
            border: 0;
            border-bottom: 1px solid #dbe4e8;
            padding: 6px 8px;
            font-weight: 600;
        }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 3px 1px; }
        QScrollBar::handle:vertical { background: #dbe4e8; min-height: 30px; border-radius: 4px; }
        QScrollBar::handle:vertical:hover { background: #9aadb5; }
        QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: transparent; border: 0; }
    )QSS"));

    if (!darkMode)
        return;

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
        QMainWindow, QDialog { background: #11181d; color: #e7edf0; }
        QWidget { color: #e7edf0; }
        QMenuBar {
            background: #182127;
            color: #e7edf0;
            border-bottom-color: #30404a;
        }
        QMenuBar::item:selected, QMenu::item:selected { background: #3a241c; color: #ff713d; }
        QMenu { background: #1b252b; color: #e7edf0; border-color: #34444e; }
        QMenu::item:disabled { color: #6f808a; }
        QStatusBar { background: #182127; color: #c9d5db; border-top-color: #30404a; }
        QScrollArea#sidebarScroll, QWidget#sidebarContent, QWidget#menuControls { background: transparent; }
        QGroupBox { background: #192228; border-color: #30404a; }
        QFrame { color: #30404a; }
        QLabel { color: #c9d5db; }
        QLabel#label, QLabel#label_2, QLabel#DataBits, QLabel#label_3, QLabel#label_4, QLabel#label_5 { color: #e7edf0; }
        QMessageBox QLabel { color: #e7edf0; }
        QLabel#sectionTitle { color: #eef3f5; }
        QComboBox, QSpinBox, QFileDialog QLineEdit {
            background: #202b32;
            color: #e7edf0;
            border-color: #3a4b55;
        }
        QComboBox:hover, QSpinBox:hover, QFileDialog QLineEdit:hover {
            background: #253139;
            border-color: #f05a24;
        }
        QComboBox:focus, QSpinBox:focus, QFileDialog QLineEdit:focus { border-color: #f05a24; }
        QComboBox QAbstractItemView, QFileDialog QAbstractItemView {
            background: #202b32;
            color: #e7edf0;
            border-color: #3a4b55;
            selection-background-color: #3a241c;
            selection-color: #ffffff;
        }
        QComboBox#languageSelector, QComboBox#themeSelector {
            background: #202b32;
            color: #dce5e9;
            border-color: #3a4b55;
        }
        QComboBox#languageSelector:hover, QComboBox#languageSelector:focus,
        QComboBox#themeSelector:hover, QComboBox#themeSelector:focus {
            background: #27343c;
            border-color: #f05a24;
        }
        QPushButton {
            background: #202b32;
            color: #e7edf0;
            border-color: #3a4b55;
        }
        QPushButton:hover { background: #29363e; border-color: #f05a24; }
        QPushButton:pressed { background: #303e47; }
        QPushButton:disabled { background: #172027; color: #657680; border-color: #293841; }
        QPushButton#pushButton_open:disabled, QPushButton#pushButton_send:disabled {
            background: #172027; color: #657680; border-color: #293841;
        }
        QPushButton#pushButton_close:disabled { background: transparent; color: #4a5c66; border-color: #33424c; }
        QPushButton#pushButton_open:disabled:hover, QPushButton#pushButton_send:disabled:hover {
            background: #172027; border-color: #293841;
        }
        QComboBox:disabled, QSpinBox:disabled { background: #1b252b; color: #5d6e78; border-color: #2b3840; }
        QPushButton#pushButton_open, QPushButton#pushButton_send {
            background: #e9521d;
            border-color: #e9521d;
            color: #ffffff;
        }
        QPushButton#pushButton_open:hover, QPushButton#pushButton_send:hover {
            background: #ff6730;
            border-color: #ff6730;
        }
        QPushButton#pushButton_close { color: #ff8290; border-color: #75434b; }
        QPushButton#pushButton_close:hover { background: #392126; border-color: #ff8290; }
        QPushButton#pushButton_clear { color: #c9d5db; }
        QCheckBox, QRadioButton { color: #c9d5db; }
        QCheckBox::indicator, QRadioButton::indicator {
            background: #202b32;
            border-color: #60727c;
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background: #f05a24;
            border-color: #f05a24;
        }
        QPlainTextEdit#textBrowser_rec, QTextEdit#textEdit_send {
            background: #141c21;
            color: #e7edf0;
            border-color: #34444e;
            selection-background-color: #704028;
        }
        QPlainTextEdit#textBrowser_rec:focus, QTextEdit#textEdit_send:focus { border-color: #f05a24; }
        QTabBar::tab { color: #b8c6cd; }
        QTabBar::tab:selected { color: #eef3f5; border-bottom-color: #f05a24; }
        QTabBar::tab:hover:!selected { color: #ff713d; }
        QLabel#labelRBytes { color: #ff713d; }
        QLabel#labelSBytes, QLabel#connectionIndicator[connected="true"] { color: #4fc49a; }
        QLabel#label_8, QLabel#label_9, QLabel#connectionIndicator { color: #c9d5db; }
        QFrame#line { background: #30404a; }
        QFrame#line_2 { background: #30404a; }
        QFileDialog QToolButton { color: #dce5e9; background: transparent; }
        QFileDialog QToolButton:hover { background: #29363e; }
        QToolTip { background: #27343c; color: #ffffff; border-color: #4a5c66; }
        QLineEdit#timelineSearch {
            background: #202b32;
            color: #e7edf0;
            border-color: #3a4b55;
            selection-background-color: #704028;
            selection-color: #ffffff;
        }
        QLineEdit#timelineSearch:focus { border-color: #f05a24; }
        QTableView#timelineView {
            background: #141c21;
            color: #e7edf0;
            border-color: #34444e;
            selection-background-color: #704028;
            selection-color: #ffffff;
        }
        QTableView#timelineView QHeaderView::section {
            background: #192228;
            color: #c9d5db;
            border-bottom-color: #30404a;
        }
        QScrollBar::handle:vertical { background: #3a4b55; }
        QScrollBar::handle:vertical:hover { background: #60727c; }
    )QSS"));
}

void MainWindow::setConnectionState(bool connected)
{
    connectionIndicator->setProperty("connected", connected);
    connectionIndicator->setText(text(connected ? "connected" : "disconnected"));
    connectionIndicator->style()->unpolish(connectionIndicator);
    connectionIndicator->style()->polish(connectionIndicator);
}

void MainWindow::on_pushButton_pause_clicked()
{
    if (pauseComOutput == false) {
        pauseComOutput = true;
        ui->pushButton_pause->setText(text("resume_output"));
        recordSystemEvent(text("ev_paused"));
    } else {
        pauseComOutput = false;
        ui->pushButton_pause->setText(text("pause_output"));
        recordSystemEvent(text("ev_resumed"));
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
    }
    aboutDialog->setLanguage(currentLanguage);
    aboutDialog->setModal(true);
    aboutDialog->show();
    aboutDialog->raise();
    aboutDialog->activateWindow();
}

void MainWindow::on_actionAbout_MULTIBEANS_triggered()
{
    showInformation(text("about_multibeans"), text("about_multibeans_body"));
}

void MainWindow::on_checkBox_dispsend_clicked(bool checked)
{
    isShowSend = checked;
}

void MainWindow::on_checkBox_disptime_clicked(bool checked)
{
    isShowTime = checked;
}

void MainWindow::on_checkBox_realtime_log_clicked(bool checked)
{
    if (!checked) {
        stopRealtimeLog();
        ui->statusBar->showMessage(text("realtime_log_stopped"), 3000);
        return;
    }

    QFileDialog dialog(this, text("realtime_log_title"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(text("log_files").split(QStringLiteral(";;")));
    dialog.setDefaultSuffix(QStringLiteral("log"));
    dialog.setDirectory(realtimeLogDirectory());
    dialog.selectFile(QStringLiteral("tinyserial-%1.log")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setLabelText(QFileDialog::LookIn, text("look_in"));
    dialog.setLabelText(QFileDialog::FileName, text("file_name"));
    dialog.setLabelText(QFileDialog::FileType, text("file_type"));
    dialog.setLabelText(QFileDialog::Accept, text("start_logging"));
    dialog.setLabelText(QFileDialog::Reject, text("cancel"));

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        stopRealtimeLog();
        return;
    }

    realtimeLogFile->setFileName(dialog.selectedFiles().constFirst());
    if (!realtimeLogFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "Could not create real-time log:" << realtimeLogFile->errorString();
        stopRealtimeLog();
        showWarning(text("warning"), text("realtime_log_failed"));
        return;
    }

    ui->checkBox_realtime_log->setToolTip(realtimeLogFile->fileName());
    ui->statusBar->showMessage(text("realtime_log_started").arg(realtimeLogFile->fileName()), 5000);
}

void MainWindow::on_actionSave_Log_File_triggered()
{
    if (ui->textBrowser_rec->toPlainText().isEmpty()) {
        showWarning(text("warning"), text("empty_log"));
        return;
    }
    QFileDialog dialog(this, text("save_log_as"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(text("text_files").split(QStringLiteral(";;")));
    dialog.setDefaultSuffix(QStringLiteral("txt"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setLabelText(QFileDialog::LookIn, text("look_in"));
    dialog.setLabelText(QFileDialog::FileName, text("file_name"));
    dialog.setLabelText(QFileDialog::FileType, text("file_type"));
    dialog.setLabelText(QFileDialog::Accept, text("save"));
    dialog.setLabelText(QFileDialog::Reject, text("cancel"));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    const QString fileName = dialog.selectedFiles().constFirst();
    if (fileName.isEmpty()) {
        return;  // User cancelled
    }
    
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        qWarning() << "Could not create log file:" << file.errorString();
        showWarning(text("warning"), text("save_failed"));
        return;
    }
    
    QTextStream stream(&file);
    stream << ui->textBrowser_rec->toPlainText();
    file.close();
    showInformation(text("information"), text("saved_log").arg(fileName));
}

void MainWindow::on_comboBox_serialPort_currentIndexChanged(int index)
{
    ui->comboBox_serialPort->setToolTip(index >= 0
                                       ? ui->comboBox_serialPort->itemText(index)
                                       : QString());
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

void MainWindow::sendData(const QString &input, TimelineSource source, bool showInDisplay)
{
    QByteArray dataToSend;

    if (sendAsciiFormat) {
        dataToSend = input.toLatin1();
    } else {
        SerialCodec::HexError hexError = SerialCodec::HexError::None;
        if (!SerialCodec::decodeHex(input, &dataToSend, &hexError)) {
            const char *errorKey = hexError == SerialCodec::HexError::Empty ? "hex_empty"
                : hexError == SerialCodec::HexError::IncompleteByte ? "hex_incomplete"
                : "hex_characters";
            showWarning(text("invalid_hex"), text(errorKey));
            return;
        }
    }

    if (serial->isOpen()) {
        qint64 bytesWritten = serial->write(dataToSend);
        if (bytesWritten == -1) {
            recordSystemEvent(text("write_failed"));
            showWarning(text("warning"), text("write_failed"));
            return;
        }

        TimelineEvent sent;
        sent.timestampMs = QDateTime::currentMSecsSinceEpoch();
        sent.kind = TimelineKind::Tx;
        sent.source = source;
        sent.data = dataToSend;
        recordTimelineEvent(sent);

        // Sent data is echoed to the terminal and log only when "Show sent data" is on.
        if (!isShowSend)
            return;

        const QString formattedData = formatSendData(dataToSend, sendAsciiFormat);
        appendRealtimeLog(formattedData);
        if (showInDisplay) {
            queueDisplayText(formattedData);
        }
    }
}

void MainWindow::appendRealtimeLog(const QString &data)
{
    if (!realtimeLogFile->isOpen() || data.isEmpty())
        return;

    const QByteArray encoded = data.toUtf8();
    if (realtimeLogFile->write(encoded) == encoded.size())
        return;

    qWarning() << "Could not write real-time log:" << realtimeLogFile->errorString();
    stopRealtimeLog();
    showWarning(text("warning"), text("realtime_log_write_failed"));
}

void MainWindow::stopRealtimeLog()
{
    if (realtimeLogFile->isOpen()) {
        realtimeLogFile->flush();
        realtimeLogFile->close();
    }
    realtimeLogFile->setFileName(QString());
    if (ui) {
        const QSignalBlocker blocker(ui->checkBox_realtime_log);
        ui->checkBox_realtime_log->setChecked(false);
        ui->checkBox_realtime_log->setToolTip(QString());
    }
}

QString MainWindow::realtimeLogDirectory() const
{
#if defined(Q_OS_LINUX)
    const QString systemLogDirectory = QStringLiteral("/var/log/tinyserial");
    const QDir systemDirectory(systemLogDirectory);
    if ((systemDirectory.exists() || QDir().mkpath(systemLogDirectory))
        && QFileInfo(systemLogDirectory).isWritable()) {
        return systemLogDirectory;
    }
#endif

    const QString userLogDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(userLogDirectory);
    return userLogDirectory;
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

    const QString message = text("serial_error").arg(serialErrorText(error));
    recordSystemEvent(message);
    ui->statusBar->showMessage(message, 5000);
    qWarning() << message;
}

void MainWindow::moveSendAreaBelowTabs()
{
    // The send box, its buttons and the traffic counters sit below the tabs so
    // they stay usable while the timeline tab is open.
    auto *workArea = new QWidget(this);
    auto *workLayout = new QVBoxLayout(workArea);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->setSpacing(0);
    ui->horizontalLayout_4->replaceWidget(ui->tabWidget, workArea);

    ui->verticalLayout_8->removeWidget(ui->textEdit_send);
    ui->verticalLayout_8->removeItem(ui->horizontalLayout_5);
    ui->horizontalLayout_5->setParent(nullptr);
    ui->verticalLayout_3->removeWidget(ui->line);
    ui->verticalLayout_3->removeItem(ui->horizontalLayout_8);
    ui->horizontalLayout_8->setParent(nullptr);

    auto *sendArea = new QVBoxLayout();
    sendArea->setContentsMargins(8, 0, 8, 8);
    sendArea->setSpacing(10);
    sendArea->addWidget(ui->textEdit_send);
    sendArea->addLayout(ui->horizontalLayout_5);
    sendArea->addWidget(ui->line);
    sendArea->addLayout(ui->horizontalLayout_8);

    workLayout->addWidget(ui->tabWidget, 1);
    workLayout->addLayout(sendArea);
}

void MainWindow::configureTimelineTab()
{
    timelineTab = new QWidget(this);
    auto *layout = new QVBoxLayout(timelineTab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    timelineShowRx = new QCheckBox(QStringLiteral("RX"), timelineTab);
    timelineShowTx = new QCheckBox(QStringLiteral("TX"), timelineTab);
    timelineShowSystem = new QCheckBox(timelineTab);
    timelineSearch = new QLineEdit(timelineTab);
    timelineSearch->setObjectName(QStringLiteral("timelineSearch"));
    timelineSearch->setClearButtonEnabled(true);
    timelineContentMode = new QComboBox(timelineTab);
    timelineContentMode->addItem(QStringLiteral("ASCII"));
    timelineContentMode->addItem(QStringLiteral("HEX"));
    timelineAutoScroll = new QCheckBox(timelineTab);
    for (QCheckBox *box : {timelineShowRx, timelineShowTx, timelineShowSystem, timelineAutoScroll})
        box->setChecked(true);

    auto *filters = new QHBoxLayout();
    filters->setSpacing(14);
    filters->addWidget(timelineShowRx);
    filters->addWidget(timelineShowTx);
    filters->addWidget(timelineShowSystem);
    filters->addWidget(timelineSearch, 1);
    filters->addWidget(timelineContentMode);
    filters->addWidget(timelineAutoScroll);
    layout->addLayout(filters);

    timelineFilter->setSourceModel(timelineModel);
    timelineView = new QTableView(timelineTab);
    timelineView->setObjectName(QStringLiteral("timelineView"));
    timelineView->setModel(timelineFilter);
    timelineView->setSelectionBehavior(QAbstractItemView::SelectRows);
    timelineView->setSelectionMode(QAbstractItemView::SingleSelection);
    timelineView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    timelineView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    timelineView->setShowGrid(false);
    timelineView->setWordWrap(false);
    timelineView->verticalHeader()->hide();
    timelineView->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 8);
    QHeaderView *header = timelineView->horizontalHeader();
    header->setHighlightSections(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setStretchLastSection(true);
    layout->addWidget(timelineView, 1);

    timelineSummary = new QLabel(timelineTab);
    timelineSummary->setWordWrap(true);
    layout->addWidget(timelineSummary);
    timelineOpenButton = new QPushButton(timelineTab);
    timelineCsvButton = new QPushButton(timelineTab);
    timelineJsonButton = new QPushButton(timelineTab);
    timelineClearButton = new QPushButton(timelineTab);
    auto *actions = new QHBoxLayout();
    actions->setSpacing(8);
    actions->addStretch(1);
    for (QPushButton *button : {timelineOpenButton, timelineCsvButton, timelineJsonButton, timelineClearButton}) {
        button->setMinimumWidth(88);
        actions->addWidget(button);
    }
    layout->addLayout(actions);

    ui->tabWidget->insertTab(ui->tabWidget->indexOf(ui->tab) + 1, timelineTab, QString());

    connect(timelineShowRx, &QCheckBox::toggled, this,
            [this](bool visible) { timelineFilter->setKindVisible(TimelineKind::Rx, visible); });
    connect(timelineShowTx, &QCheckBox::toggled, this,
            [this](bool visible) { timelineFilter->setKindVisible(TimelineKind::Tx, visible); });
    connect(timelineShowSystem, &QCheckBox::toggled, this,
            [this](bool visible) { timelineFilter->setKindVisible(TimelineKind::System, visible); });
    connect(timelineSearch, &QLineEdit::textChanged, timelineFilter, &TimelineFilterModel::setSearchText);
    connect(timelineContentMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { timelineModel->setHexContent(index == 1); });
    connect(timelineAutoScroll, &QCheckBox::toggled, this, [this](bool follow) {
        if (follow)
            timelineView->scrollToBottom();
    });
    connect(timelineView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTimelineSelection);
    connect(timelineOpenButton, &QPushButton::clicked, this, &MainWindow::openTimelineFile);
    connect(timelineCsvButton, &QPushButton::clicked, this, &MainWindow::exportTimelineCsv);
    connect(timelineJsonButton, &QPushButton::clicked, this, &MainWindow::exportTimelineJsonLines);
    connect(timelineClearButton, &QPushButton::clicked, this, &MainWindow::clearTimeline);
}

void MainWindow::recordTimelineEvent(const TimelineEvent &event)
{
    // A partially received line started earlier than this event, so it goes first.
    if (event.kind != TimelineKind::Rx && timelineAssembler.hasPending()) {
        timelineIdleTimer->stop();
        pendingTimelineEvents += timelineAssembler.flush();
    }
    pendingTimelineEvents.append(event);
    if (!timelineFlushTimer->isActive())
        timelineFlushTimer->start();
}

void MainWindow::recordTimelineEvents(const QVector<TimelineEvent> &events)
{
    for (const TimelineEvent &event : events)
        recordTimelineEvent(event);
}

void MainWindow::recordSystemEvent(const QString &message)
{
    TimelineEvent event;
    event.timestampMs = QDateTime::currentMSecsSinceEpoch();
    event.kind = TimelineKind::System;
    event.source = TimelineSource::App;
    event.message = message;
    recordTimelineEvent(event);
}

void MainWindow::flushTimelineLine()
{
    recordTimelineEvents(timelineAssembler.flush());
}

void MainWindow::flushTimelineEvents()
{
    if (pendingTimelineEvents.isEmpty())
        return;
    timelineModel->append(pendingTimelineEvents);
    pendingTimelineEvents.clear();
    updateTimelineSummary();
    if (timelineAutoScroll->isChecked())
        timelineView->scrollToBottom();
}

QString MainWindow::connectionSummary() const
{
    QString parity = QStringLiteral("N");
    switch (serial->parity()) {
    case QSerialPort::EvenParity: parity = QStringLiteral("E"); break;
    case QSerialPort::OddParity: parity = QStringLiteral("O"); break;
    case QSerialPort::SpaceParity: parity = QStringLiteral("S"); break;
    case QSerialPort::MarkParity: parity = QStringLiteral("M"); break;
    default: break;
    }
    QString stopBits = QStringLiteral("1");
    if (serial->stopBits() == QSerialPort::OneAndHalfStop)
        stopBits = QStringLiteral("1.5");
    else if (serial->stopBits() == QSerialPort::TwoStop)
        stopBits = QStringLiteral("2");

    QString summary = QStringLiteral("%1 %2%3%4").arg(serial->baudRate())
        .arg(static_cast<int>(serial->dataBits())).arg(parity, stopBits);
    if (serial->flowControl() == QSerialPort::HardwareControl)
        summary += QStringLiteral(", RTS/CTS");
    else if (serial->flowControl() == QSerialPort::SoftwareControl)
        summary += QStringLiteral(", XON/XOFF");
    return summary;
}

int MainWindow::selectedTimelineRow() const
{
    const QModelIndexList rows = timelineView->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return -1;
    return timelineFilter->mapToSource(rows.constFirst()).row();
}

void MainWindow::resizeTimelineColumns()
{
    // Size each fixed column to its title or widest typical value, whichever is wider.
    QHeaderView *header = timelineView->horizontalHeader();
    QFont titleFont = header->font();
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics cellMetrics(timelineView->font());
    const QFontMetrics timeMetrics(ui->textEdit_send->font());
    const int padding = 28;
    auto fit = [&](int column, int valueWidth) {
        const QString title = timelineModel->headerData(column, Qt::Horizontal).toString();
        header->resizeSection(column, qMax(titleMetrics.horizontalAdvance(title), valueWidth) + padding);
    };

    int sourceWidth = 0;
    for (const char *key : {"source_device", "source_manual", "source_repeat"})
        sourceWidth = qMax(sourceWidth, cellMetrics.horizontalAdvance(text(key)));

    fit(TimelineModel::TimeColumn, timeMetrics.horizontalAdvance(QStringLiteral("00:00:00.000")));
    fit(TimelineModel::DeltaColumn, cellMetrics.horizontalAdvance(QStringLiteral("+00.000 s")));
    fit(TimelineModel::TypeColumn, cellMetrics.horizontalAdvance(QStringLiteral("SYS")));
    fit(TimelineModel::SourceColumn, sourceWidth);
    fit(TimelineModel::BytesColumn, cellMetrics.horizontalAdvance(QStringLiteral("00000")));
}

void MainWindow::updateTimelineSelection()
{
    timelineModel->setResponseAnchor(selectedTimelineRow());
    updateTimelineSummary();
}

void MainWindow::updateTimelineSummary()
{
    QString summary = text("timeline_summary")
        .arg(timelineModel->rowCount())
        .arg(timelineModel->count(TimelineKind::Rx))
        .arg(timelineModel->count(TimelineKind::Tx))
        .arg(timelineModel->count(TimelineKind::System));

    const int row = selectedTimelineRow();
    if (row >= 0 && timelineModel->eventAt(row).kind == TimelineKind::Tx) {
        const TimelineModel::Response response = timelineModel->responseTo(row);
        summary += QString::fromUtf8("   ·   ");
        if (response.rxEvents == 0)
            summary += text("timeline_no_response");
        else
            summary += text("timeline_response").arg(response.firstLatencyMs)
                .arg(response.rxEvents).arg(response.rxBytes);
    }
    timelineSummary->setText(summary);
}

QString MainWindow::askTimelineFileName(const char *titleKey, const char *filterKey,
                                        const QString &suffix, bool save)
{
    QFileDialog dialog(this, text(titleKey));
    dialog.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    dialog.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    dialog.setNameFilters(text(filterKey).split(QStringLiteral(";;")));
    dialog.setDefaultSuffix(suffix);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setLabelText(QFileDialog::LookIn, text("look_in"));
    dialog.setLabelText(QFileDialog::FileName, text("file_name"));
    dialog.setLabelText(QFileDialog::FileType, text("file_type"));
    dialog.setLabelText(QFileDialog::Accept, text(save ? "save" : "open"));
    dialog.setLabelText(QFileDialog::Reject, text("cancel"));
    if (save) {
        dialog.selectFile(QStringLiteral("tinyserial-timeline-%1.%2")
                              .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")), suffix));
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return QString();
    return dialog.selectedFiles().constFirst();
}

void MainWindow::exportTimelineCsv()
{
    flushTimelineEvents();
    if (timelineModel->rowCount() == 0) {
        showWarning(text("warning"), text("timeline_empty"));
        return;
    }
    const QString fileName = askTimelineFileName("timeline_export_title", "csv_files", QStringLiteral("csv"), true);
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(TimelineFile::toCsv(timelineModel->allEvents())) < 0) {
        qWarning() << "Could not write timeline:" << file.errorString();
        showWarning(text("warning"), text("save_failed"));
        return;
    }
    showInformation(text("information"), text("timeline_saved").arg(fileName));
}

void MainWindow::exportTimelineJsonLines()
{
    flushTimelineEvents();
    if (timelineModel->rowCount() == 0) {
        showWarning(text("warning"), text("timeline_empty"));
        return;
    }
    const QString fileName = askTimelineFileName("timeline_export_title", "jsonl_files", QStringLiteral("jsonl"), true);
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(TimelineFile::toJsonLines(timelineModel->allEvents())) < 0) {
        qWarning() << "Could not write timeline:" << file.errorString();
        showWarning(text("warning"), text("save_failed"));
        return;
    }
    showInformation(text("information"), text("timeline_saved").arg(fileName));
}

void MainWindow::openTimelineFile()
{
    if (serial->isOpen()) {
        showWarning(text("warning"), text("timeline_disconnect_first"));
        return;
    }
    flushTimelineEvents();
    if (timelineModel->rowCount() > 0) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(text("timeline_open_title"));
        box.setText(text("timeline_replace"));
        QPushButton *replaceButton = box.addButton(text("timeline_replace_button"), QMessageBox::AcceptRole);
        QPushButton *cancelButton = box.addButton(text("cancel"), QMessageBox::RejectRole);
        box.setDefaultButton(cancelButton);
        box.exec();
        if (box.clickedButton() != replaceButton)
            return;
    }

    const QString fileName = askTimelineFileName("timeline_open_title", "jsonl_files", QStringLiteral("jsonl"), false);
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        showWarning(text("warning"), text("timeline_read_failed"));
        return;
    }
    QVector<TimelineEvent> events;
    int badLine = 0;
    if (!TimelineFile::fromJsonLines(file.readAll(), &events, &badLine)) {
        showWarning(text("warning"), text("timeline_open_failed").arg(badLine));
        return;
    }

    timelineAssembler.clear();
    timelineModel->setEvents(events);
    updateTimelineSummary();
    timelineView->scrollToTop();
    ui->statusBar->showMessage(text("timeline_loaded").arg(events.size()).arg(fileName), 5000);
}

void MainWindow::clearTimeline()
{
    pendingTimelineEvents.clear();
    timelineAssembler.clear();
    timelineIdleTimer->stop();
    timelineModel->clear();
    updateTimelineSummary();
}
