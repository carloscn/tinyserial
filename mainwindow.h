// SPDX-License-Identifier: MPL-2.0
// Qt Cross-platform Project.                                             GitHub
/****************************************************************************/
/*                                                                          */
/*  @file       : mainwindow.h                                              */
/*  @License    : MPL-2.0                                                  */
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
#include "global.h"
#include <QValidator>
#include "aboutdialog.h"
#include "displaybuffer.h"
#include "modemlines.h"
#include "timeline.h"
#include "uilanguage.h"
#include "xmodemtransfer.h"

namespace Ui {
class SerialPort;
}

class QLabel;
class TerminalHighlighter;
class QComboBox;
class QFile;
class QGridLayout;
class QSpinBox;
class QEvent;
class QResizeEvent;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTableView;
class QWidget;

enum class UiThemeMode {
    System,
    Light,
    Dark
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

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

    void on_actionAbout_MULTIBEANS_triggered();

    void SoftAutoWriteUart( void );

    void on_checkBox_dispsend_clicked(bool checked);

    void on_checkBox_disptime_clicked(bool checked);

    void on_actionSave_Log_File_triggered();
    void on_actionOpen_Log_Folder_triggered();
    void on_actionTerminal_Cache_Size_triggered();
    void on_actionTerminal_Font_triggered();
    void on_actionSend_File_triggered();
    void on_actionReceive_File_triggered();
    void on_actionCancel_File_Transfer_triggered();
    void on_actionFull_Terminal_Mode_triggered();
    void on_actionDebug_Mode_triggered();

    void on_comboBox_serialPort_currentIndexChanged(int index);

    void on_comboBox_baudrate_editTextChanged(const QString &arg1);

    void on_checkBox_wordwrap_clicked(bool checked);

    void on_checkBox_realtime_log_clicked(bool checked);

    void flushPendingDisplay();
    void flushPendingReceive();
    void onSerialBytesWritten(qint64 bytes);
    void onSerialError(QSerialPort::SerialPortError error);
    void flushTimelineLine();
    void pollModemLines();
    void startResetPulse();
    void finishResetPulse();
    void flushTimelineEvents();
    void exportTimelineCsv();
    void exportTimelineJsonLines();
    void openTimelineFile();
    void clearTimeline();
    void updateTimelineSelection();

private:
    static constexpr int DEFAULT_REPEAT_INTERVAL_MS = 1000;
    static constexpr int DISPLAY_FLUSH_INTERVAL_MS = 33;
    static constexpr int DEFAULT_DISPLAY_CHARACTERS = 1024 * 1024;
    static constexpr int MIN_DISPLAY_CACHE_MIB = 1;
    static constexpr int MAX_DISPLAY_CACHE_MIB = 64;
    static constexpr int MAX_PENDING_DISPLAY_CHARACTERS = 256 * 1024;
    static constexpr int MAX_PENDING_WRITE_BYTES = 4 * 1024 * 1024;
    // Received bytes with no newline become one event after this much silence.
    static constexpr int TIMELINE_IDLE_GAP_MS = 20;
    static constexpr int TIMELINE_FLUSH_INTERVAL_MS = 100;
    // QSerialPort has no change notification for the control lines.
    static constexpr int MODEM_POLL_INTERVAL_MS = 100;
    static constexpr int DEFAULT_RESET_PULSE_MS = 100;
    Ui::SerialPort *ui;
    AboutDialog *aboutDialog;
    QTimer *repeatSendTimer;
    QTimer *portWatchTimer;
    QTimer *displayFlushTimer;
    QTimer *receiveBatchTimer;
    QSerialPort *serial;
    QFile *realtimeLogFile;
    QLabel *connectionIndicator;
    QLabel *rxActivityIndicator;
    QLabel *txActivityIndicator;
    QTimer *rxActivityTimer;
    QTimer *txActivityTimer;
    QLabel *connectionTitle;
    QLabel *receiveTitle;
    QLabel *transmitTitle;
    QComboBox *languageSelector;
    QComboBox *themeSelector;
    QGridLayout *settingsGrid;
    QScrollArea *sidebarScroll;
    QWidget *sidebarContent;
    QWidget *sendAreaWidget;
    QWidget *fullTerminalBar;
    QPushButton *fullTerminalButton;
    QComboBox *fullTerminalPort;
    QComboBox *fullTerminalBaud;
    QComboBox *fullTerminalDataBits;
    QComboBox *fullTerminalStopBits;
    QComboBox *fullTerminalParity;
    QComboBox *fullTerminalFlowControl;
    QPushButton *fullTerminalConnect;
    QPushButton *fullTerminalLog;
    QPushButton *fullTerminalOpenLog;
    UiLanguage currentLanguage;
    QString currentConnectCom;
    QValidator *validator_combox_baudrate;
    int displayCharacterLimit;
    DisplayBuffer pendingDisplay;
    XmodemTransfer *fileTransfer;
    TerminalHighlighter *terminalHighlighter;
    QByteArray pendingReceive;
    QByteArray pendingWrite;

    TimelineModel *timelineModel;
    TimelineFilterModel *timelineFilter;
    TimelineLineAssembler timelineAssembler;
    QVector<TimelineEvent> pendingTimelineEvents;
    QTimer *timelineIdleTimer;
    QTimer *timelineFlushTimer;
    QWidget *timelineTab;
    QTableView *timelineView;
    QCheckBox *timelineShowRx;
    QCheckBox *timelineShowTx;
    QCheckBox *timelineShowSystem;
    QLineEdit *timelineSearch;
    QComboBox *timelineContentMode;
    QCheckBox *timelineAutoScroll;
    QLabel *timelineSummary;
    QPushButton *timelineOpenButton;
    QPushButton *timelineCsvButton;
    QPushButton *timelineJsonButton;
    QPushButton *timelineClearButton;

    bool showSentData;
    bool isShowTime;
    bool recAsciiFormat;
    bool sendAsciiFormat;
    bool repeatSend;
    bool pauseComOutput;
    UiThemeMode themeMode;
    bool darkMode;
    bool layoutUpdatePending;

    QWidget *modemControls = nullptr;
    QPushButton *modemDtrButton = nullptr;
    QPushButton *modemRtsButton = nullptr;
    QVector<QLabel *> modemInputIndicators;
    QComboBox *modemPulseLine = nullptr;
    QSpinBox *modemPulseDuration = nullptr;
    QPushButton *modemPulseButton = nullptr;
    QTimer *modemPollTimer = nullptr;
    QTimer *modemPulseTimer = nullptr;
    QSerialPort::PinoutSignals modemSignals = QSerialPort::NoSignal;
    bool modemSignalsKnown = false;
    bool modemSignalsSupported = true;
    // Set while TinySerial itself queries or drives a control line, so
    // onSerialError leaves the reporting of that failure to the caller.
    bool modemCallInProgress = false;
    ModemLine pulseLine = ModemLine::Dtr;
    bool pulseRestoreActive = false;
    bool fullTerminalMode;

    void RefreshTheUSBList();

    // Helper functions for code reuse
    bool configureSerialPort();
    void initQssStyleSheet();
    void configureLayout();
    void updateLayoutMetrics();
    void scheduleLayoutMetricsUpdate();
    void configureLanguageSelector();
    void configureTextFormatControls();
    void syncTextFormatControls();
    void refreshThemeFromPreference();
    bool systemUsesDarkTheme() const;
    void applyLanguage(UiLanguage language);
    QString text(const char *key) const;
    QString serialErrorText(QSerialPort::SerialPortError error) const;
    void showWarning(const QString &title, const QString &message);
    void showInformation(const QString &title, const QString &message);
    void appendRealtimeLog(const QString &data);
    void stopRealtimeLog();
    QString realtimeLogDirectory() const;
    QString configuredLogDirectory() const;
    void setConnectionState(bool connected);
    void setBaudRate(int index);
    void setStopBits(int index);
    void setParity(int index);
    void setDataBits(int index);
    void setFlowControl(int index);
    bool selectedBaudRate(qint32 *baudRate) const;
    bool writePendingData();
    QString formatReceiveData(const QByteArray &data, bool isAscii);
    QString formatSendData(const QByteArray &data, bool isAscii);
    QString receiveTextEncoding() const;
    QString sendTextEncoding() const;
    QByteArray selectedLineEnding() const;
    void sendData(const QString &input, TimelineSource source, bool showInDisplay = true);
    void queueDisplayText(const QString &text);
    void appendDisplayText(const QString &text);
    void configureTimelineTab();
    void moveSendAreaBelowTabs();
    void recordTimelineEvent(const TimelineEvent &event);
    void recordTimelineEvents(const QVector<TimelineEvent> &events);
    void recordSystemEvent(const QString &message);
    QString connectionSummary() const;
    QString askTimelineFileName(const char *titleKey, const char *filterKey, const QString &suffix, bool save);
    void updateTimelineSummary();
    int selectedTimelineRow() const;
    void resizeTimelineColumns();
    void applyTerminalFont(const QFont &font);
    void setFileTransferUi(bool active);
    XmodemTransfer::Mode chooseXmodemMode(bool *accepted) const;
    void setTerminalMode(bool fullScreen);
    bool sendTerminalKey(int key, const QString &text);
    void syncFullTerminalControls();
    void configureModemControls();
    void updateModemControls();
    void startModemMonitoring();
    void stopModemMonitoring();
    void setModemIndicator(QWidget *indicator, const char *state);
    bool setModemOutput(ModemLine line, bool active, bool record);
    void flashActivity(QLabel *indicator, QTimer *timer);

    quint64 recCount;
    quint64 sendCount;
};

#endif // SERIALPORT_H
