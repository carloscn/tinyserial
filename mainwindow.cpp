// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2017-2025 Carlos Wei and MULTIBEANS ORG.

#include "appversion.h"
#include "mainwindow.h"
#include "serialcodec.h"
#include "serialdisplayformatter.h"
#include "serialportselection.h"
#include "terminalhighlighter.h"
#include "textcodec.h"
#include "xmodemtransfer.h"
#include "ui_serialport.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QFontDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
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
#include <QResizeEvent>
#include <QScrollBar>
#include <QScrollArea>
#include <QScreen>
#include <QSerialPortInfo>
#include <utility>
#include <QSignalBlocker>
#include <QTableView>
#include <QTabBar>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
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
    receiveBatchTimer(new QTimer(this)),
    serial(new QSerialPort(this)),
    realtimeLogFile(new QFile(this)),
    connectionIndicator(new QLabel(this)),
    rxActivityIndicator(new QLabel(this)),
    txActivityIndicator(new QLabel(this)),
    rxActivityTimer(new QTimer(this)),
    txActivityTimer(new QTimer(this)),
    connectionTitle(new QLabel(this)),
    receiveTitle(new QLabel(this)),
    transmitTitle(new QLabel(this)),
    languageSelector(new QComboBox(this)),
    themeSelector(new QComboBox(this)),
    settingsGrid(nullptr),
    sidebarScroll(nullptr),
    sidebarContent(nullptr),
    sendAreaWidget(nullptr),
    fullTerminalBar(nullptr),
    fullTerminalButton(new QPushButton(this)),
    fullTerminalPort(nullptr),
    fullTerminalBaud(nullptr),
    fullTerminalDataBits(nullptr),
    fullTerminalStopBits(nullptr),
    fullTerminalParity(nullptr),
    fullTerminalFlowControl(nullptr),
    fullTerminalConnect(nullptr),
    fullTerminalLog(nullptr),
    fullTerminalOpenLog(nullptr),
    currentLanguage(UiLanguage::English),
    validator_combox_baudrate(nullptr),
    displayCharacterLimit(DEFAULT_DISPLAY_CHARACTERS),
    pendingDisplay(MAX_PENDING_DISPLAY_CHARACTERS),
    fileTransfer(new XmodemTransfer(this)),
    terminalHighlighter(nullptr),
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
    showSentData(false),
    isShowTime(false),
    recAsciiFormat(true),
    sendAsciiFormat(true),
    repeatSend(false),
    pauseComOutput(false),
    themeMode(UiThemeMode::System),
    darkMode(false),
    layoutUpdatePending(false),
    fullTerminalMode(false),
    recCount(0),
    sendCount(0)
{
    ui->setupUi(this);
    configureTextFormatControls();
    setWindowTitle(QStringLiteral("tinySerial v") + QLatin1String(APP_VERSION));
    setMinimumSize(860, 600);
    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QSize available = primaryScreen ? primaryScreen->availableGeometry().size() : QSize(1240, 800);
    resize(qMin(1240, qMax(860, available.width() - 80)),
           qMin(920, qMax(600, available.height() - 24)));
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
    const QVariant savedTerminalFont = settings.value(QStringLiteral("terminal/font"));
    if (savedTerminalFont.canConvert<QFont>())
        terminalFont = qvariant_cast<QFont>(savedTerminalFont);
    const int cacheMiB = qBound(MIN_DISPLAY_CACHE_MIB,
                                settings.value(QStringLiteral("terminal/cacheMiB"),
                                               MIN_DISPLAY_CACHE_MIB).toInt(),
                                MAX_DISPLAY_CACHE_MIB);
    displayCharacterLimit = cacheMiB * 1024 * 1024;
    applyTerminalFont(terminalFont);
    terminalHighlighter = new TerminalHighlighter(ui->textBrowser_rec->document());
    initQssStyleSheet();
    configureLayout();
    configureLanguageSelector();
    qApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) { refreshThemeFromPreference(); });
#endif

    rxActivityIndicator->setObjectName(QStringLiteral("rxActivityIndicator"));
    txActivityIndicator->setObjectName(QStringLiteral("txActivityIndicator"));
    rxActivityIndicator->setText(QStringLiteral("● RX"));
    txActivityIndicator->setText(QStringLiteral("● TX"));
    rxActivityIndicator->setProperty("active", false);
    txActivityIndicator->setProperty("active", false);
    for (const auto &[indicator, timer] : { std::pair<QLabel *, QTimer *>(rxActivityIndicator, rxActivityTimer),
                                            std::pair<QLabel *, QTimer *>(txActivityIndicator, txActivityTimer) }) {
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [indicator] {
            indicator->setProperty("active", false);
            indicator->style()->unpolish(indicator);
            indicator->style()->polish(indicator);
        });
        ui->statusBar->addPermanentWidget(indicator);
    }
    ui->statusBar->addPermanentWidget(connectionIndicator);

    // Send/Receive ASCII Format initialization
    repeatSend = ui->checkBox_repeat->isChecked();

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
    connect(fileTransfer, &XmodemTransfer::bytesReady, this, [this](const QByteArray &bytes) {
        if (!serial->isOpen())
            return;
        pendingWrite.append(bytes);
        writePendingData();
    });
    connect(fileTransfer, &XmodemTransfer::progress, this, [this](qint64 completed, qint64 total) {
        ui->statusBar->showMessage(total > 0 ? text("file_transfer_progress").arg(completed).arg(total)
                                             : text("file_transfer_received").arg(completed));
    });
    connect(fileTransfer, &XmodemTransfer::finished, this, [this](bool success, const QString &message) {
        setFileTransferUi(false);
        recordSystemEvent(message);
        ui->statusBar->showMessage(message, 5000);
        if (!success)
            showWarning(text("file_transfer"), message);
    });
    serial->setReadBufferSize(DEFAULT_DISPLAY_CHARACTERS);
    displayFlushTimer->setInterval(DISPLAY_FLUSH_INTERVAL_MS);
    displayFlushTimer->setSingleShot(true);
    connect(displayFlushTimer, &QTimer::timeout, this, &MainWindow::flushPendingDisplay);
    receiveBatchTimer->setInterval(DISPLAY_FLUSH_INTERVAL_MS);
    receiveBatchTimer->setSingleShot(true);
    connect(receiveBatchTimer, &QTimer::timeout, this, &MainWindow::flushPendingReceive);
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
    setFileTransferUi(false);
    syncFullTerminalControls();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    stopRealtimeLog();
    portWatchTimer->stop();
    displayFlushTimer->stop();
    receiveBatchTimer->stop();
    flushPendingReceive();
    on_pushButton_close_clicked();
    if (validator_combox_baudrate != nullptr) {
        delete validator_combox_baudrate;
        validator_combox_baudrate = nullptr;
    }
    delete ui;
}

void MainWindow::serialRcvData()
{
    QByteArray recvArray = serial->readAll();
    if (recvArray.isEmpty()) {
        return;
    }
    flashActivity(rxActivityIndicator, rxActivityTimer);

    if (fileTransfer->isActive()) {
        fileTransfer->consume(recvArray);
        return;
    }
    recCount += static_cast<quint64>(recvArray.size());

    recordTimelineEvents(timelineAssembler.append(recvArray, QDateTime::currentMSecsSinceEpoch()));
    if (timelineAssembler.hasPending())
        timelineIdleTimer->start();

    pendingReceive.append(recvArray);
    if (!receiveBatchTimer->isActive())
        receiveBatchTimer->start();
}

void MainWindow::flushPendingReceive()
{
    if (pendingReceive.isEmpty())
        return;

    QByteArray batch;
    batch.swap(pendingReceive);
    ui->labelRBytes->setText(QString::number(recCount));
    const QString formatted = formatReceiveData(batch, recAsciiFormat);
    appendRealtimeLog(formatted);

    // Pausing only suppresses terminal rendering.  The device is still
    // drained, counted, logged, and recorded in the timeline.
    if (!pauseComOutput)
        queueDisplayText(formatted);
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

    if (!configureSerialPort()) {
        showWarning(text("invalid_baud_title"), text("invalid_baud"));
        return;
    }
    serial->setPortName(currentConnectCom);
    
    if (!serial->open(QIODevice::ReadWrite)) {
        const QString errorMsg = serialErrorText(serial->error());
        recordSystemEvent(text("open_failed").arg(portInfo, errorMsg));
        showWarning(text("warning"), text("open_failed").arg(portInfo, errorMsg));
        ui->comboBox_serialPort->setEnabled(true);
        ui->statusBar->showMessage(text("open_failed").arg(portInfo, errorMsg));
    } else {
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_open->setEnabled(false);
        ui->comboBox_serialPort->setEnabled(false);
        ui->pushButton_scan->setEnabled(false);
        ui->pushButton_send->setEnabled(true);
        setFileTransferUi(false);
        setConnectionState(true);
        recordSystemEvent(text("ev_connected").arg(currentConnectCom, connectionSummary()));
        showInformation(text("information"), text("open_ok").arg(portInfo));
        ui->statusBar->showMessage(text("open_ok").arg(portInfo));
    }
}

void MainWindow::RefreshTheUSBList()
{
    const auto ports = QSerialPortInfo::availablePorts();
    QStringList labels;
    QStringList portNames;
    bool openPortPresent = false;
    for (const QSerialPortInfo &info : ports) {
        labels.append(info.portName() + ",(" + info.description() + ")");
        portNames.append(info.portName());
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
    const int keep = serialPortIndexToKeep(portNames, selectedPortName,
                                            currentConnectCom, serial->isOpen());
    if (keep >= 0)
        ui->comboBox_serialPort->setCurrentIndex(keep);
    ui->comboBox_serialPort->blockSignals(false);
    ui->comboBox_serialPort->setToolTip(ui->comboBox_serialPort->currentText());
    syncFullTerminalControls();

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
    if (fileTransfer->isActive())
        fileTransfer->cancel();
    flushPendingReceive();
    const bool wasOpen = serial->isOpen();
    serial->close();
    pendingWrite.clear();
    if (wasOpen)
        recordSystemEvent(text("ev_disconnected").arg(currentConnectCom));
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_scan->setEnabled(true);
    ui->comboBox_serialPort->setEnabled(true);
    setFileTransferUi(false);
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
    syncTextFormatControls();
}

void MainWindow::on_radioButton_send_hex_clicked()
{
    sendAsciiFormat = false;
    syncTextFormatControls();
}

void MainWindow::on_radioButton_rec_ascii_clicked()
{
    flushPendingReceive();
    recAsciiFormat = true;
    syncTextFormatControls();
}

void MainWindow::on_radioButton_rec_hex_clicked()
{
    flushPendingReceive();
    recAsciiFormat = false;
    syncTextFormatControls();
}

void MainWindow::on_pushButton_clear_clicked()
{
    receiveBatchTimer->stop();
    pendingReceive.clear();
    displayFlushTimer->stop();
    pendingDisplay.clear();
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
    sidebarContent = new QWidget(this);
    sidebarContent->setObjectName(QStringLiteral("sidebarContent"));
    sidebarContent->setLayout(ui->verticalLayout_6);

    sidebarScroll = new QScrollArea(this);
    sidebarScroll->setObjectName(QStringLiteral("sidebarScroll"));
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebarScroll->setWidget(sidebarContent);
    ui->horizontalLayout_4->insertWidget(0, sidebarScroll);

    ui->horizontalLayout_4->setStretch(0, 0);
    ui->horizontalLayout_4->setStretch(1, 1);
    ui->horizontalLayout_4->setContentsMargins(20, 14, 20, 12);
    ui->horizontalLayout_4->setSpacing(18);
    ui->verticalLayout_6->setSpacing(10);
    ui->verticalLayout_6->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout_6->setAlignment(Qt::AlignTop);
    ui->verticalLayout_6->setSizeConstraint(QLayout::SetMinAndMaxSize);

    const QList<QWidget *> panels = {ui->groupBox, ui->groupBox_2, ui->groupBox_3};
    for (QWidget *panel : panels) {
        panel->setMinimumWidth(0);
        panel->setMaximumWidth(QWIDGETSIZE_MAX);
        panel->setMinimumHeight(0);
        panel->setMaximumHeight(QWIDGETSIZE_MAX);
        panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    // Let each widget's polished font/style determine its height.  Using the
    // main-window font and locking hand-calculated heights clips CJK text and
    // indicators when the desktop scale or accessibility font size changes.
    const int fieldHeight = qMax(30, ui->comboBox_baudrate->sizeHint().height());
    const int buttonHeight = qMax(32, ui->pushButton_open->sizeHint().height());

    // On scaled or short displays, show all three sidebar cards at startup
    // instead of requiring the user to discover the Transmit card by scrolling.
    // Widget heights still come from Qt's style/font metrics, so this remains
    // safe for CJK and accessibility fonts.
    const bool compactSidebar = height() < 960;
    const int topMargin = compactSidebar ? 10 : 16;
    const int bottomMargin = compactSidebar ? 12 : 18;
    const int cardSpacing = compactSidebar ? 8 : 10;
    ui->verticalLayout_6->setSpacing(cardSpacing);
    ui->verticalLayout_5->setContentsMargins(18, topMargin, 18, bottomMargin);
    ui->horizontalLayout_13->setContentsMargins(18, topMargin, 18, bottomMargin);
    ui->horizontalLayout_12->setContentsMargins(18, topMargin, 18, bottomMargin);
    ui->verticalLayout_5->setSpacing(compactSidebar ? 8 : 12);
    ui->verticalLayout_2->setSpacing(compactSidebar ? 6 : 8);
    ui->verticalLayout_4->setSpacing(compactSidebar ? 7 : 10);
    settingsGrid->setVerticalSpacing(compactSidebar ? 4 : 6);

    // The card layouts cache their minimum size.  Invalidate and activate
    // them before measuring: otherwise a previous language (for example
    // Spanish) can leave its wider cached minimum behind after switching to
    // Chinese, or vice versa.
    for (QWidget *panel : {ui->groupBox, ui->groupBox_2, ui->groupBox_3}) {
        if (panel->layout()) {
            panel->layout()->invalidate();
            panel->layout()->activate();
        }
        panel->updateGeometry();
    }
    settingsGrid->invalidate();
    settingsGrid->activate();

    for (QComboBox *field : {ui->comboBox_serialPort, ui->comboBox_baudrate,
                              ui->comboBox_databits, ui->comboBox_stopbits,
                              ui->comboBox_checkdigit, ui->comboBox_flowctrl,
                              ui->comboBox_receiveEncoding, ui->comboBox_sendEncoding,
                              ui->comboBox_lineEnding}) {
        field->setMinimumHeight(fieldHeight);
        field->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    ui->spinBox_repeat->setMinimumHeight(fieldHeight);
    ui->spinBox_repeat->setMaximumHeight(QWIDGETSIZE_MAX);

    for (QWidget *field : {static_cast<QWidget *>(timelineSearch),
                           static_cast<QWidget *>(timelineContentMode)}) {
        field->setMinimumHeight(fieldHeight);
        field->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    for (QPushButton *button : {ui->pushButton_scan, ui->pushButton_pause,
                                ui->pushButton_open, ui->pushButton_close,
                                ui->pushButton_send, ui->pushButton_clear,
                                timelineOpenButton, timelineCsvButton,
                                timelineJsonButton, timelineClearButton}) {
        button->setMinimumHeight(buttonHeight);
        button->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    resizeTimelineColumns();

    const QList<QWidget *> options = {
        ui->radioButton_rec_ascii, ui->radioButton_rec_hex, ui->checkBox_wordwrap,
        ui->checkBox_dispsend, ui->checkBox_disptime, ui->checkBox_realtime_log,
        ui->radioButton_send_ascii,
        ui->radioButton_send_hex, ui->checkBox_escapeSequences, ui->checkBox_repeat
    };
    for (QWidget *option : options)
        option->setMinimumHeight(option->sizeHint().height());

    int labelWidth = 0;
    for (QLabel *label : {ui->label, ui->label_2, ui->DataBits,
                          ui->label_3, ui->label_4, ui->label_5})
        labelWidth = qMax(labelWidth, label->sizeHint().width());
    settingsGrid->setColumnMinimumWidth(0, labelWidth);

    int fieldWidth = 180;
    for (QComboBox *field : {ui->comboBox_serialPort, ui->comboBox_baudrate,
                             ui->comboBox_databits, ui->comboBox_stopbits,
                             ui->comboBox_checkdigit, ui->comboBox_flowctrl})
        fieldWidth = qMax(fieldWidth, field->sizeHint().width());

    const QMargins margins = ui->verticalLayout_5->contentsMargins();
    int desiredWidth = margins.left() + labelWidth
        + settingsGrid->horizontalSpacing() + fieldWidth + margins.right() + 2;
    // Connection labels are not necessarily the widest content: translated
    // Receive/Transmit controls (notably Spanish) can require more room.
    // Size the sidebar from every card's real minimum, so the scroll view
    // never clips its right half under the terminal area.
    for (QWidget *panel : {ui->groupBox, ui->groupBox_2, ui->groupBox_3}) {
        if (panel->layout())
            desiredWidth = qMax(desiredWidth, panel->layout()->minimumSize().width());
    }
    const int availableSidebarWidth = qMax(320, width() * 45 / 100);
    // The cards' real minimum wins over the 45% cap: below it the layout
    // forces the content wider than the scroll view and clips the cards.
    const int contentMinimum = ui->verticalLayout_6->minimumSize().width();
    const int sidebarWidth = qMax(qBound(320, desiredWidth, availableSidebarWidth), contentMinimum);
    const int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    sidebarContent->setMinimumWidth(sidebarWidth);
    sidebarContent->setMaximumWidth(sidebarWidth);
    sidebarScroll->setMinimumWidth(sidebarWidth + scrollBarExtent);
    sidebarScroll->setMaximumWidth(sidebarWidth + scrollBarExtent);

    // Recompute natural card heights after language/font changes.  The scroll
    // area's content then grows vertically instead of overlapping.
    ui->verticalLayout_6->invalidate();
    sidebarContent->updateGeometry();
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

    fullTerminalButton->setObjectName(QStringLiteral("fullTerminalButton"));
    fullTerminalButton->setCheckable(true);
    fullTerminalButton->setMinimumHeight(30);
    connect(fullTerminalButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked != fullTerminalMode)
            setTerminalMode(checked);
    });
    cornerLayout->addWidget(fullTerminalButton, 0, Qt::AlignVCenter);

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

void MainWindow::configureTextFormatControls()
{
    const QList<QPair<QString, QString>> encodings = {
        {QStringLiteral("UTF-8"), QStringLiteral("UTF-8")},
        {QStringLiteral("GB18030"), QStringLiteral("GB18030")},
        {QStringLiteral("GBK"), QStringLiteral("GBK")},
        {QStringLiteral("GB2312"), QStringLiteral("GB2312")},
        {QStringLiteral("Big5"), QStringLiteral("Big5")},
        {QStringLiteral("Shift-JIS"), QStringLiteral("Shift-JIS")},
        {QStringLiteral("EUC-KR"), QStringLiteral("EUC-KR")},
        {QStringLiteral("ISO-8859-1"), QStringLiteral("ISO-8859-1")},
        {QStringLiteral("Windows-1252"), QStringLiteral("Windows-1252")},
        {QStringLiteral("System"), QStringLiteral("System")}
    };
    for (QComboBox *box : {ui->comboBox_receiveEncoding, ui->comboBox_sendEncoding}) {
        for (const auto &encoding : encodings)
            box->addItem(encoding.first, encoding.second);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        box->setMinimumWidth(168);
    }

    ui->comboBox_lineEnding->addItem(QString(), QByteArray());
    ui->comboBox_lineEnding->addItem(QString(), QByteArrayLiteral("\n"));
    ui->comboBox_lineEnding->addItem(QString(), QByteArrayLiteral("\r\n"));
    ui->comboBox_lineEnding->addItem(QString(), QByteArrayLiteral("\r"));
    ui->comboBox_lineEnding->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->comboBox_lineEnding->setMinimumWidth(168);

    const QSettings settings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"));
    auto restore = [&settings](QComboBox *box, const QString &key, const QVariant &fallback) {
        const int index = box->findData(settings.value(key, fallback));
        box->setCurrentIndex(index >= 0 ? index : 0);
    };
    restore(ui->comboBox_receiveEncoding, QStringLiteral("text/receiveEncoding"), QStringLiteral("UTF-8"));
    restore(ui->comboBox_sendEncoding, QStringLiteral("text/sendEncoding"), QStringLiteral("UTF-8"));
    restore(ui->comboBox_lineEnding, QStringLiteral("text/lineEnding"), QByteArray());
    ui->checkBox_escapeSequences->setChecked(settings.value(QStringLiteral("text/escapeSequences"), false).toBool());

    auto persistCombo = [](QComboBox *box, const QString &key) {
        QObject::connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), box,
                         [box, key](int) {
            QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
                .setValue(key, box->currentData());
        });
    };
    persistCombo(ui->comboBox_receiveEncoding, QStringLiteral("text/receiveEncoding"));
    persistCombo(ui->comboBox_sendEncoding, QStringLiteral("text/sendEncoding"));
    persistCombo(ui->comboBox_lineEnding, QStringLiteral("text/lineEnding"));
    connect(ui->checkBox_escapeSequences, &QCheckBox::toggled, this, [](bool checked) {
        QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
            .setValue(QStringLiteral("text/escapeSequences"), checked);
    });
    syncTextFormatControls();
}

void MainWindow::syncTextFormatControls()
{
    ui->comboBox_receiveEncoding->setEnabled(recAsciiFormat);
    ui->comboBox_sendEncoding->setEnabled(sendAsciiFormat);
    ui->checkBox_escapeSequences->setEnabled(sendAsciiFormat);
    ui->comboBox_lineEnding->setEnabled(sendAsciiFormat);
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
    if (fullTerminalMode && watched == ui->textBrowser_rec && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        sendTerminalKey(keyEvent->key(), keyEvent->text());
        // Keep the terminal document as output-only.  It remains editable only
        // so Qt renders the familiar blinking terminal caret.
        return true;
    }
    if (watched == qApp && themeMode == UiThemeMode::System
        && event->type() == QEvent::ApplicationPaletteChange) {
        refreshThemeFromPreference();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::ScreenChangeInternal) {
        scheduleLayoutMetricsUpdate();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    scheduleLayoutMetricsUpdate();
}

void MainWindow::scheduleLayoutMetricsUpdate()
{
    if (!settingsGrid || layoutUpdatePending)
        return;
    layoutUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        layoutUpdatePending = false;
        updateLayoutMetrics();
    });
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
    ui->radioButton_rec_ascii->setText(text("text_mode"));
    ui->radioButton_rec_hex->setText(text("hex_mode"));
    ui->label_receiveEncoding->setText(text("text_encoding"));
    ui->checkBox_wordwrap->setText(text("word_wrap"));
    ui->checkBox_dispsend->setText(text("display_sent"));
    ui->checkBox_disptime->setText(text("display_time"));
    ui->checkBox_realtime_log->setText(text("realtime_log"));
    themeSelector->setItemText(0, text("theme_system"));
    themeSelector->setItemText(1, text("theme_light"));
    themeSelector->setItemText(2, text("theme_dark"));
    transmitTitle->setText(text("transmit"));
    ui->radioButton_send_ascii->setText(text("text_mode"));
    ui->radioButton_send_hex->setText(text("hex_mode"));
    ui->label_sendEncoding->setText(text("text_encoding"));
    ui->checkBox_escapeSequences->setText(text("escape_sequences"));
    ui->label_lineEnding->setText(text("append_line_ending"));
    ui->comboBox_lineEnding->setItemText(0, text("line_ending_none"));
    ui->comboBox_lineEnding->setItemText(1, text("line_ending_lf"));
    ui->comboBox_lineEnding->setItemText(2, text("line_ending_crlf"));
    ui->comboBox_lineEnding->setItemText(3, text("line_ending_cr"));
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
    ui->menuView->setTitle(text("view"));
    ui->menuAbout->setTitle(text("about"));
    ui->actionSave_Log_File->setText(text("save_log"));
    ui->actionOpen_Log_Folder->setText(text("open_log_folder"));
    ui->actionSend_File->setText(text("send_file"));
    ui->actionReceive_File->setText(text("receive_file"));
    ui->actionCancel_File_Transfer->setText(text("cancel_file_transfer"));
    ui->actionDebug_Mode->setText(text("debug_mode"));
    ui->actionFull_Terminal_Mode->setText(text("full_terminal_mode"));
    fullTerminalButton->setText(text("terminal_mode_tag"));
    rxActivityIndicator->setToolTip(text("receive_activity"));
    txActivityIndicator->setToolTip(text("send_activity"));
    ui->actionTerminal_Cache_Size->setText(text("terminal_cache_size"));
    ui->actionTerminal_Font->setText(text("terminal_font"));
    ui->actionAbout_TinySerialPort->setText(text("about_tinyserial"));
    ui->actionAbout_MULTIBEANS->setText(text("about_multibeans"));
    ui->statusBar->clearMessage();
    updateLayoutMetrics();
    // The widget hierarchy receives its final translated size hints on the
    // next event-loop turn.  A second, deferred pass keeps the sidebar width
    // responsive when changing language without resizing the window.
    scheduleLayoutMetricsUpdate();
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
    terminalHighlighter->setDarkSurface(darkMode || fullTerminalMode);
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
        QPushButton#fullTerminalButton {
            background: #dc430b; color: #ffffff; border: 1px solid #dc430b;
            border-radius: 15px; padding: 0 12px; font-weight: 600;
        }
        QPushButton#fullTerminalButton:hover { background: #bd3607; }
        QPushButton#fullTerminalButton:checked { background: #172b35; border-color: #172b35; }
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
        QLabel#rxActivityIndicator, QLabel#txActivityIndicator {
            color: #9aadb5;
            font-weight: 600;
            padding: 3px 4px;
        }
        QLabel#rxActivityIndicator[active="true"], QLabel#txActivityIndicator[active="true"] {
            color: #dc430b;
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

    // Full terminal mode deliberately has its own visual identity.  It stays
    // dark even when the application uses a light theme, just like a physical
    // terminal emulator, while the application menu and status bar still
    // honour the selected application theme.
    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
        QTabWidget#tabWidget[fullTerminal="true"]::pane,
        QWidget#tab[fullTerminal="true"] {
            background: #0d1117;
            border: 0;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] {
            background: #111820;
            border: 0;
            border-bottom: 1px solid #26333d;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QComboBox {
            min-height: 30px;
            background: #18232c;
            color: #dce7de;
            border: 1px solid #354650;
            border-radius: 5px;
            padding: 0 24px 0 9px;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QComboBox:hover,
        QWidget#fullTerminalBar[fullTerminal="true"] QComboBox:focus {
            background: #1f2d36;
            border-color: #ef5b25;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QComboBox:disabled {
            background: #121b22;
            color: #687980;
            border-color: #283740;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QPushButton {
            min-height: 30px;
            background: #e9521d;
            color: #ffffff;
            border: 1px solid #e9521d;
            border-radius: 5px;
            padding: 0 10px;
            font-weight: 600;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QPushButton:hover {
            background: #ff6730;
            border-color: #ff6730;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QPushButton:pressed {
            background: #bd3607;
            border-color: #bd3607;
        }
        QWidget#fullTerminalBar[fullTerminal="true"] QPushButton:disabled {
            background: #4a2b20;
            color: #bca49a;
            border-color: #4a2b20;
        }
        QPushButton#fullTerminalConnect,
        QPushButton#fullTerminalLog,
        QPushButton#fullTerminalOpenLog {
            background: #e9521d;
            color: #ffffff;
            border: 1px solid #e9521d;
            font-weight: 600;
        }
        QPushButton#fullTerminalConnect:hover,
        QPushButton#fullTerminalLog:hover,
        QPushButton#fullTerminalOpenLog:hover {
            background: #ff6730;
            border-color: #ff6730;
        }
        QPlainTextEdit#textBrowser_rec[terminalMode="true"] {
            background: #0d1117;
            color: #dce7de;
            border: 0;
            border-radius: 0;
            padding: 14px;
            selection-background-color: #3a5a4a;
            selection-color: #ffffff;
        }
        QPlainTextEdit#textBrowser_rec[terminalMode="true"]:focus {
            border: 0;
        }
        QPlainTextEdit#textBrowser_rec[terminalMode="true"] QScrollBar:vertical {
            background: #0d1117;
            width: 11px;
            margin: 4px 2px;
        }
        QPlainTextEdit#textBrowser_rec[terminalMode="true"] QScrollBar::handle:vertical {
            background: #33454f;
            min-height: 32px;
            border-radius: 4px;
        }
        QPlainTextEdit#textBrowser_rec[terminalMode="true"] QScrollBar::handle:vertical:hover {
            background: #526b77;
        }
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
        QPushButton#fullTerminalButton {
            background: #f05a24; color: #ffffff; border: 1px solid #f05a24;
            border-radius: 15px; padding: 0 12px; font-weight: 600;
        }
        QPushButton#fullTerminalButton:hover { background: #ff6730; }
        QPushButton#fullTerminalButton:checked { background: #60727c; border-color: #60727c; }
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
        QLabel#rxActivityIndicator, QLabel#txActivityIndicator { color: #667983; font-weight: 600; padding: 3px 4px; }
        QLabel#rxActivityIndicator[active="true"], QLabel#txActivityIndicator[active="true"] { color: #ff713d; }
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
        QPushButton#fullTerminalConnect,
        QPushButton#fullTerminalLog,
        QPushButton#fullTerminalOpenLog {
            background: #e9521d;
            color: #ffffff;
            border: 1px solid #e9521d;
            font-weight: 600;
        }
        QPushButton#fullTerminalConnect:hover,
        QPushButton#fullTerminalLog:hover,
        QPushButton#fullTerminalOpenLog:hover {
            background: #ff6730;
            border-color: #ff6730;
        }
        QPushButton#fullTerminalConnect:pressed,
        QPushButton#fullTerminalLog:pressed,
        QPushButton#fullTerminalOpenLog:pressed {
            background: #bd3607;
            border-color: #bd3607;
        }
    )QSS"));
}

void MainWindow::setConnectionState(bool connected)
{
    connectionIndicator->setProperty("connected", connected);
    connectionIndicator->setText(text(connected ? "connected" : "disconnected"));
    connectionIndicator->style()->unpolish(connectionIndicator);
    connectionIndicator->style()->polish(connectionIndicator);
    syncFullTerminalControls();
}

void MainWindow::on_pushButton_pause_clicked()
{
    flushPendingReceive();
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
    flushPendingReceive();
    showSentData = checked;
}

void MainWindow::on_checkBox_disptime_clicked(bool checked)
{
    flushPendingReceive();
    isShowTime = checked;
}

void MainWindow::on_checkBox_realtime_log_clicked(bool checked)
{
    if (!checked) {
        flushPendingReceive();
        stopRealtimeLog();
        ui->statusBar->showMessage(text("realtime_log_stopped"), 3000);
        return;
    }

    QFileDialog dialog(this, text("realtime_log_title"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(text("log_files").split(QStringLiteral(";;")));
    dialog.setDefaultSuffix(QStringLiteral("log"));
    dialog.setDirectory(configuredLogDirectory());
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

    QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
        .setValue(QStringLiteral("logging/directory"), QFileInfo(realtimeLogFile->fileName()).absolutePath());
    ui->checkBox_realtime_log->setToolTip(realtimeLogFile->fileName());
    ui->statusBar->showMessage(text("realtime_log_started").arg(realtimeLogFile->fileName()), 5000);
}

void MainWindow::on_actionSave_Log_File_triggered()
{
    flushPendingReceive();
    flushPendingDisplay();
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

void MainWindow::on_actionOpen_Log_Folder_triggered()
{
    const QString directory = configuredLogDirectory();
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
        return;
    showWarning(text("warning"), text("open_log_folder_failed").arg(directory));
}

void MainWindow::on_actionTerminal_Font_triggered()
{
    QFontDialog dialog(ui->textBrowser_rec->font(), this);
    dialog.setWindowTitle(text("terminal_font"));
    dialog.setOption(QFontDialog::MonospacedFonts, true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QFont selected = dialog.selectedFont();
    applyTerminalFont(selected);
    QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
        .setValue(QStringLiteral("terminal/font"), selected);
}

void MainWindow::on_actionTerminal_Cache_Size_triggered()
{
    bool accepted = false;
    const int cacheMiB = QInputDialog::getInt(this, text("terminal_cache_size"),
                                              text("terminal_cache_size_label"),
                                              displayCharacterLimit / (1024 * 1024),
                                              MIN_DISPLAY_CACHE_MIB, MAX_DISPLAY_CACHE_MIB,
                                              1, &accepted);
    if (!accepted)
        return;

    displayCharacterLimit = cacheMiB * 1024 * 1024;
    QSettings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"))
        .setValue(QStringLiteral("terminal/cacheMiB"), cacheMiB);
    appendDisplayText(QString());
}

XmodemTransfer::Mode MainWindow::chooseXmodemMode(bool *accepted) const
{
    const QStringList modes = {text("xmodem_checksum"), text("xmodem_crc"), text("xmodem_1k"), text("ymodem")};
    bool ok = false;
    const QString selected = QInputDialog::getItem(const_cast<MainWindow *>(this), text("file_transfer"),
                                                   text("file_transfer_protocol"), modes, 1, false, &ok);
    if (accepted)
        *accepted = ok;
    return selected == modes.at(0) ? XmodemTransfer::Mode::Checksum
        : selected == modes.at(2) ? XmodemTransfer::Mode::OneK
        : selected == modes.at(3) ? XmodemTransfer::Mode::Ymodem : XmodemTransfer::Mode::Crc;
}

void MainWindow::setFileTransferUi(bool active)
{
    ui->pushButton_send->setEnabled(serial->isOpen() && !active);
    ui->checkBox_repeat->setEnabled(!active);
    ui->pushButton_pause->setEnabled(!active);
    ui->actionSend_File->setEnabled(serial->isOpen() && !active);
    ui->actionReceive_File->setEnabled(serial->isOpen() && !active);
    ui->actionCancel_File_Transfer->setEnabled(active);
}

void MainWindow::on_actionSend_File_triggered()
{
    if (!serial->isOpen()) {
        showWarning(text("file_transfer"), text("file_transfer_connect_first"));
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(this, text("send_file"));
    if (fileName.isEmpty())
        return;
    bool accepted = false;
    const XmodemTransfer::Mode mode = chooseXmodemMode(&accepted);
    if (!accepted)
        return;
    if (fileTransfer->startSending(fileName, mode)) {
        setFileTransferUi(true);
        recordSystemEvent(text("file_transfer_started").arg(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::on_actionReceive_File_triggered()
{
    if (!serial->isOpen()) {
        showWarning(text("file_transfer"), text("file_transfer_connect_first"));
        return;
    }
    const QString fileName = QFileDialog::getSaveFileName(this, text("receive_file"));
    if (fileName.isEmpty())
        return;
    bool accepted = false;
    const XmodemTransfer::Mode mode = chooseXmodemMode(&accepted);
    if (!accepted)
        return;
    QString error;
    if (!fileTransfer->startReceiving(fileName, mode, &error)) {
        showWarning(text("file_transfer"), error);
        return;
    }
    setFileTransferUi(true);
    recordSystemEvent(text("file_transfer_started").arg(QFileInfo(fileName).fileName()));
}

void MainWindow::on_actionCancel_File_Transfer_triggered()
{
    fileTransfer->cancel();
}

void MainWindow::on_actionFull_Terminal_Mode_triggered()
{
    setTerminalMode(true);
}

void MainWindow::on_actionDebug_Mode_triggered()
{
    setTerminalMode(false);
}

void MainWindow::applyTerminalFont(const QFont &font)
{
    ui->textBrowser_rec->setFont(font);
    ui->textEdit_send->setFont(font);
    timelineModel->setContentFont(font);
    if (timelineView)
        resizeTimelineColumns();
}

void MainWindow::on_comboBox_serialPort_currentIndexChanged(int index)
{
    ui->comboBox_serialPort->setToolTip(index >= 0
                                       ? ui->comboBox_serialPort->itemText(index)
                                       : QString());
}


void MainWindow::on_comboBox_baudrate_editTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1)
    qint32 baudRate = 0;
    if (serial->isOpen() && selectedBaudRate(&baudRate))
        serial->setBaudRate(baudRate);
}

// Helper function implementations
void MainWindow::setBaudRate(int index)
{
    static constexpr qint32 rates[] = {
        QSerialPort::Baud1200, QSerialPort::Baud2400,
        QSerialPort::Baud4800, QSerialPort::Baud9600,
        QSerialPort::Baud19200, QSerialPort::Baud38400,
        QSerialPort::Baud57600, QSerialPort::Baud115200
    };
    if (index >= 0 && index < static_cast<int>(sizeof(rates) / sizeof(rates[0])))
        serial->setBaudRate(rates[index]);
}

void MainWindow::setStopBits(int index)
{
    switch (index) {
    case CONFIG_STOPBIT_ONE_INDEX:
        serial->setStopBits(QSerialPort::OneStop);
        break;
    case CONFIG_STOPBIT_ONEANDHALF_INDEX:
        serial->setStopBits(QSerialPort::OneAndHalfStop);
        break;
    case CONFIG_STOPBIT_TWO_INDEX:
        serial->setStopBits(QSerialPort::TwoStop);
        break;
    }
}

void MainWindow::setParity(int index)
{
    switch (index) {
    case CONFIG_PARITY_NONE_INDEX:
        serial->setParity(QSerialPort::NoParity);
        break;
    case CONFIG_PARITY_EVEN_INDEX:
        serial->setParity(QSerialPort::EvenParity);
        break;
    case CONFIG_PARITY_ODD_INDEX:
        serial->setParity(QSerialPort::OddParity);
        break;
    case CONFIG_PARITY_SPACE_INDEX:
        serial->setParity(QSerialPort::SpaceParity);
        break;
    case CONFIG_PARITY_MARK_INDEX:
        serial->setParity(QSerialPort::MarkParity);
        break;
    }
}

void MainWindow::setDataBits(int index)
{
    switch (index) {
    case CONFIG_DATABITS_5_INDEX:
        serial->setDataBits(QSerialPort::Data5);
        break;
    case CONFIG_DATABITS_6_INDEX:
        serial->setDataBits(QSerialPort::Data6);
        break;
    case CONFIG_DATABITS_7_INDEX:
        serial->setDataBits(QSerialPort::Data7);
        break;
    case CONFIG_DATABITS_8_INDEX:
        serial->setDataBits(QSerialPort::Data8);
        break;
    }
}

void MainWindow::setFlowControl(int index)
{
    switch (index) {
    case CONFIG_FLOWCTRL_NONE_INDEX:
        serial->setFlowControl(QSerialPort::NoFlowControl);
        break;
    case CONFIG_FLOWCTRL_HARD_INDEX:
        serial->setFlowControl(QSerialPort::HardwareControl);
        break;
    case CONFIG_FLOWCTRL_SOFT_INDEX:
        serial->setFlowControl(QSerialPort::SoftwareControl);
        break;
    }
}

bool MainWindow::selectedBaudRate(qint32 *baudRate) const
{
    if (!baudRate)
        return false;

    if (ui->comboBox_baudrate->currentIndex() == CONFIG_BAUDRATE_CUSTOM_INDEX) {
        bool valid = false;
        const qint32 customRate = ui->comboBox_baudrate->currentText().toInt(&valid);
        if (!valid || customRate <= 0)
            return false;
        *baudRate = customRate;
        return true;
    }

    static constexpr qint32 standardRates[] = {
        1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
    };
    const int index = ui->comboBox_baudrate->currentIndex();
    if (index < 0 || index >= static_cast<int>(sizeof(standardRates) / sizeof(standardRates[0])))
        return false;
    *baudRate = standardRates[index];
    return true;
}

bool MainWindow::configureSerialPort()
{
    qint32 baudRate = 0;
    if (!selectedBaudRate(&baudRate))
        return false;
    serial->setBaudRate(baudRate);
    setStopBits(ui->comboBox_stopbits->currentIndex());
    setParity(ui->comboBox_checkdigit->currentIndex());
    setDataBits(ui->comboBox_databits->currentIndex());
    setFlowControl(ui->comboBox_flowctrl->currentIndex());
    return true;
}

QString MainWindow::formatReceiveData(const QByteArray &data, bool isAscii)
{
    if (!isAscii)
        return formatSerialDisplayData(data, false, SerialDirection::Receive,
                                       showSentData, isShowTime, QTime::currentTime());
    return formatSerialDisplayText(decodeTextBytes(data, receiveTextEncoding()),
                                   SerialDirection::Receive, showSentData, isShowTime,
                                   QTime::currentTime());
}

QString MainWindow::formatSendData(const QByteArray &data, bool isAscii)
{
    if (!isAscii)
        return formatSerialDisplayData(data, false, SerialDirection::Transmit,
                                       showSentData, isShowTime, QTime::currentTime());
    return formatSerialDisplayText(decodeTextBytes(data, sendTextEncoding()),
                                   SerialDirection::Transmit, showSentData, isShowTime,
                                   QTime::currentTime());
}

QString MainWindow::receiveTextEncoding() const
{
    return ui->comboBox_receiveEncoding->currentData().toString();
}

QString MainWindow::sendTextEncoding() const
{
    return ui->comboBox_sendEncoding->currentData().toString();
}

QByteArray MainWindow::selectedLineEnding() const
{
    return ui->comboBox_lineEnding->currentData().toByteArray();
}

void MainWindow::sendData(const QString &input, TimelineSource source, bool showInDisplay)
{
    if (fileTransfer->isActive()) {
        showWarning(text("file_transfer"), text("file_transfer_busy"));
        return;
    }
    // Preserve RX/TX order when a receive batch and a user send occur within
    // the same 33 ms display window.
    flushPendingReceive();
    QByteArray dataToSend;

    if (sendAsciiFormat) {
        TextEncodeError textError = TextEncodeError::None;
        const bool encoded = ui->checkBox_escapeSequences->isChecked()
            ? encodeTextWithEscapes(input, sendTextEncoding(), &dataToSend, &textError)
            : encodeTextBytes(input, sendTextEncoding(), &dataToSend, &textError);
        if (!encoded) {
            const char *errorKey = textError == TextEncodeError::InvalidEscape
                ? "invalid_escape" : "text_encoding_failed";
            showWarning(text("warning"), text(errorKey));
            return;
        }
        dataToSend += selectedLineEnding();
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
        if (pendingWrite.size() + dataToSend.size() > MAX_PENDING_WRITE_BYTES) {
            recordSystemEvent(text("write_failed"));
            showWarning(text("warning"), text("write_failed"));
            return;
        }
        pendingWrite.append(dataToSend);
        if (!writePendingData())
            return;

        TimelineEvent sent;
        sent.timestampMs = QDateTime::currentMSecsSinceEpoch();
        sent.kind = TimelineKind::Tx;
        sent.source = source;
        sent.data = dataToSend;
        recordTimelineEvent(sent);

        // Sent data is echoed to the terminal and log only when "Show sent data" is on.
        if (!showSentData)
            return;

        const QString formattedData = formatSendData(dataToSend, sendAsciiFormat);
        appendRealtimeLog(formattedData);
        if (showInDisplay) {
            queueDisplayText(formattedData);
        }
    }
}

bool MainWindow::writePendingData()
{
    while (!pendingWrite.isEmpty() && serial->isOpen()) {
        const qint64 accepted = serial->write(pendingWrite);
        if (accepted < 0) {
            recordSystemEvent(text("write_failed"));
            showWarning(text("warning"), text("write_failed"));
            pendingWrite.clear();
            return false;
        }
        if (accepted == 0)
            break;
        pendingWrite.remove(0, static_cast<int>(accepted));
    }
    return true;
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

QString MainWindow::configuredLogDirectory() const
{
    const QSettings settings(QStringLiteral("MULTIBEANS"), QStringLiteral("TinySerial"));
    const QString configured = settings.value(QStringLiteral("logging/directory")).toString();
    return configured.isEmpty() ? realtimeLogDirectory() : configured;
}

void MainWindow::queueDisplayText(const QString &text)
{
    if (text.isEmpty())
        return;

    pendingDisplay.append(text);
    if (!displayFlushTimer->isActive())
        displayFlushTimer->start();
}

void MainWindow::flushPendingDisplay()
{
    if (pendingDisplay.isEmpty())
        return;

    const QString text = pendingDisplay.take();
    appendDisplayText(text);
}

void MainWindow::appendDisplayText(const QString &text)
{
    QPlainTextEdit *display = ui->textBrowser_rec;
    QScrollBar *scrollbar = display->verticalScrollBar();
    const bool followTail = scrollbar->value() >= scrollbar->maximum();

    const QString retainedText = text.size() > displayCharacterLimit
        ? text.right(displayCharacterLimit) : text;
    const int overflow = display->document()->characterCount() + retainedText.size()
        - displayCharacterLimit;
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

    flashActivity(txActivityIndicator, txActivityTimer);
    sendCount += static_cast<quint64>(bytes);
    ui->labelSBytes->setText(QString::number(sendCount));
    writePendingData();
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

    sendAreaWidget = new QWidget(workArea);
    auto *sendArea = new QVBoxLayout(sendAreaWidget);
    sendArea->setContentsMargins(8, 0, 8, 8);
    sendArea->setSpacing(10);
    sendArea->addWidget(ui->textEdit_send);
    sendArea->addLayout(ui->horizontalLayout_5);
    sendArea->addWidget(ui->line);
    sendArea->addLayout(ui->horizontalLayout_8);

    workLayout->addWidget(ui->tabWidget, 1);
    workLayout->addWidget(sendAreaWidget);

    fullTerminalBar = new QWidget(workArea);
    fullTerminalBar->setObjectName(QStringLiteral("fullTerminalBar"));
    auto *terminalLayout = new QHBoxLayout(fullTerminalBar);
    terminalLayout->setContentsMargins(8, 6, 8, 6);
    terminalLayout->setSpacing(8);
    fullTerminalPort = new QComboBox(fullTerminalBar);
    fullTerminalBaud = new QComboBox(fullTerminalBar);
    fullTerminalDataBits = new QComboBox(fullTerminalBar);
    fullTerminalStopBits = new QComboBox(fullTerminalBar);
    fullTerminalParity = new QComboBox(fullTerminalBar);
    fullTerminalFlowControl = new QComboBox(fullTerminalBar);
    fullTerminalConnect = new QPushButton(fullTerminalBar);
    fullTerminalLog = new QPushButton(fullTerminalBar);
    fullTerminalOpenLog = new QPushButton(fullTerminalBar);
    fullTerminalConnect->setObjectName(QStringLiteral("fullTerminalConnect"));
    fullTerminalLog->setObjectName(QStringLiteral("fullTerminalLog"));
    fullTerminalOpenLog->setObjectName(QStringLiteral("fullTerminalOpenLog"));
    fullTerminalPort->setMinimumWidth(140);
    fullTerminalBaud->setMinimumWidth(90);
    fullTerminalDataBits->setMinimumWidth(54);
    fullTerminalStopBits->setMinimumWidth(50);
    fullTerminalParity->setMinimumWidth(76);
    fullTerminalFlowControl->setMinimumWidth(84);
    fullTerminalDataBits->setToolTip(text("data_bits"));
    fullTerminalStopBits->setToolTip(text("stop_bits"));
    fullTerminalParity->setToolTip(text("parity"));
    fullTerminalFlowControl->setToolTip(text("flow_control"));
    terminalLayout->addWidget(fullTerminalPort, 1);
    terminalLayout->addWidget(fullTerminalBaud);
    terminalLayout->addWidget(fullTerminalDataBits);
    terminalLayout->addWidget(fullTerminalStopBits);
    terminalLayout->addWidget(fullTerminalParity);
    terminalLayout->addWidget(fullTerminalFlowControl);
    terminalLayout->addWidget(fullTerminalConnect);
    terminalLayout->addWidget(fullTerminalLog);
    terminalLayout->addWidget(fullTerminalOpenLog);
    workLayout->insertWidget(0, fullTerminalBar);
    fullTerminalBar->hide();

    connect(fullTerminalPort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const int index = ui->comboBox_serialPort->findData(fullTerminalPort->currentData());
        if (index >= 0)
            ui->comboBox_serialPort->setCurrentIndex(index);
    });
    connect(fullTerminalBaud, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && index < ui->comboBox_baudrate->count())
            ui->comboBox_baudrate->setCurrentIndex(index);
    });
    connect(fullTerminalDataBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { ui->comboBox_databits->setCurrentIndex(index); });
    connect(fullTerminalStopBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { ui->comboBox_stopbits->setCurrentIndex(index); });
    connect(fullTerminalParity, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { ui->comboBox_checkdigit->setCurrentIndex(index); });
    connect(fullTerminalFlowControl, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { ui->comboBox_flowctrl->setCurrentIndex(index); });
    connect(fullTerminalConnect, &QPushButton::clicked, this, [this] {
        serial->isOpen() ? on_pushButton_close_clicked() : on_pushButton_open_clicked();
    });
    connect(fullTerminalLog, &QPushButton::clicked, this, [this] {
        ui->checkBox_realtime_log->setChecked(!ui->checkBox_realtime_log->isChecked());
        on_checkBox_realtime_log_clicked(ui->checkBox_realtime_log->isChecked());
        syncFullTerminalControls();
    });
    connect(fullTerminalOpenLog, &QPushButton::clicked, this, &MainWindow::on_actionOpen_Log_Folder_triggered);
}

void MainWindow::setTerminalMode(bool fullScreen)
{
    fullTerminalMode = fullScreen;
    terminalHighlighter->setDarkSurface(darkMode || fullScreen);
    for (QWidget *widget : {static_cast<QWidget *>(ui->tabWidget), ui->tab,
                            static_cast<QWidget *>(ui->textBrowser_rec), fullTerminalBar}) {
        widget->setProperty(widget == ui->textBrowser_rec ? "terminalMode" : "fullTerminal", fullScreen);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    sidebarScroll->setVisible(!fullScreen);
    sendAreaWidget->setVisible(!fullScreen);
    fullTerminalBar->setVisible(fullScreen);
    ui->tabWidget->tabBar()->setVisible(!fullScreen);
    ui->textBrowser_rec->setReadOnly(!fullScreen);
    if (fullScreen) {
        ui->tabWidget->setCurrentWidget(ui->tab);
        ui->textBrowser_rec->moveCursor(QTextCursor::End);
        ui->textBrowser_rec->setFocus(Qt::OtherFocusReason);
    }
    ui->actionFull_Terminal_Mode->setChecked(fullScreen);
    ui->actionDebug_Mode->setChecked(!fullScreen);
    {
        const QSignalBlocker blocker(fullTerminalButton);
        fullTerminalButton->setChecked(fullScreen);
    }
    syncFullTerminalControls();
    scheduleLayoutMetricsUpdate();
}

void MainWindow::flashActivity(QLabel *indicator, QTimer *timer)
{
    indicator->setProperty("active", true);
    indicator->style()->unpolish(indicator);
    indicator->style()->polish(indicator);
    timer->start(140);
}

void MainWindow::syncFullTerminalControls()
{
    if (!fullTerminalBar)
        return;
    const QSignalBlocker portBlocker(fullTerminalPort);
    const QSignalBlocker baudBlocker(fullTerminalBaud);
    const QSignalBlocker dataBitsBlocker(fullTerminalDataBits);
    const QSignalBlocker stopBitsBlocker(fullTerminalStopBits);
    const QSignalBlocker parityBlocker(fullTerminalParity);
    const QSignalBlocker flowControlBlocker(fullTerminalFlowControl);
    fullTerminalPort->clear();
    for (int index = 0; index < ui->comboBox_serialPort->count(); ++index)
        fullTerminalPort->addItem(ui->comboBox_serialPort->itemText(index),
                                  ui->comboBox_serialPort->itemData(index));
    const int selectedPort = fullTerminalPort->findData(ui->comboBox_serialPort->currentData());
    fullTerminalPort->setCurrentIndex(selectedPort);
    fullTerminalBaud->clear();
    for (int index = 0; index < ui->comboBox_baudrate->count(); ++index)
        fullTerminalBaud->addItem(ui->comboBox_baudrate->itemText(index));
    fullTerminalBaud->setCurrentIndex(ui->comboBox_baudrate->currentIndex());
    const auto copyOptions = [](QComboBox *target, QComboBox *source) {
        target->clear();
        for (int index = 0; index < source->count(); ++index)
            target->addItem(source->itemText(index), source->itemData(index));
        target->setCurrentIndex(source->currentIndex());
    };
    copyOptions(fullTerminalDataBits, ui->comboBox_databits);
    copyOptions(fullTerminalStopBits, ui->comboBox_stopbits);
    copyOptions(fullTerminalParity, ui->comboBox_checkdigit);
    copyOptions(fullTerminalFlowControl, ui->comboBox_flowctrl);
    fullTerminalPort->setEnabled(!serial->isOpen());
    fullTerminalBaud->setEnabled(!serial->isOpen());
    fullTerminalDataBits->setEnabled(!serial->isOpen());
    fullTerminalStopBits->setEnabled(!serial->isOpen());
    fullTerminalParity->setEnabled(!serial->isOpen());
    fullTerminalFlowControl->setEnabled(!serial->isOpen());
    fullTerminalConnect->setText(text(serial->isOpen() ? "disconnect" : "connect"));
    fullTerminalLog->setText(text(ui->checkBox_realtime_log->isChecked() ? "realtime_log_on" : "realtime_log"));
    fullTerminalOpenLog->setText(text("open_log_folder"));
}

bool MainWindow::sendTerminalKey(int key, const QString &input)
{
    if (!serial->isOpen() || fileTransfer->isActive())
        return false;
    QByteArray bytes;
    if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)
        && key >= Qt::Key_A && key <= Qt::Key_Z) {
        bytes = QByteArray(1, static_cast<char>(key - Qt::Key_A + 1));
    } else {
        switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter: bytes = QByteArrayLiteral("\r"); break;
        case Qt::Key_Backspace: bytes = QByteArrayLiteral("\b"); break;
        case Qt::Key_Tab: bytes = QByteArrayLiteral("\t"); break;
        case Qt::Key_Escape: bytes = QByteArray(1, char(0x1b)); break;
        default: bytes = input.toUtf8(); break;
        }
    }
    if (bytes.isEmpty())
        return false;
    pendingWrite.append(bytes);
    writePendingData();
    return true;
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
