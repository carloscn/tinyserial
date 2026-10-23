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
#include "timeline.h"
#include "uilanguage.h"

namespace Ui {
class SerialPort;
}

class QLabel;
class QComboBox;
class QFile;
class QGridLayout;
class QEvent;
class QCheckBox;
class QLineEdit;
class QPushButton;
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

    void on_comboBox_serialPort_currentIndexChanged(int index);

    void on_comboBox_baudrate_editTextChanged(const QString &arg1);

    void on_checkBox_wordwrap_clicked(bool checked);

    void on_checkBox_realtime_log_clicked(bool checked);

    void flushPendingDisplay();
    void onSerialBytesWritten(qint64 bytes);
    void onSerialError(QSerialPort::SerialPortError error);
    void flushTimelineLine();
    void flushTimelineEvents();
    void exportTimelineCsv();
    void exportTimelineJsonLines();
    void openTimelineFile();
    void clearTimeline();
    void updateTimelineSelection();

private:
    static constexpr int DEFAULT_REPEAT_INTERVAL_MS = 1000;
    static constexpr int DISPLAY_FLUSH_INTERVAL_MS = 33;
    static constexpr int MAX_DISPLAY_CHARACTERS = 1024 * 1024;
    // Received bytes with no newline become one event after this much silence.
    static constexpr int TIMELINE_IDLE_GAP_MS = 20;
    static constexpr int TIMELINE_FLUSH_INTERVAL_MS = 100;
    Ui::SerialPort *ui;
    AboutDialog *aboutDialog;
    QTimer *repeatSendTimer;
    QTimer *portWatchTimer;
    QTimer *displayFlushTimer;
    QSerialPort *serial;
    QFile *realtimeLogFile;
    QLabel *connectionIndicator;
    QLabel *connectionTitle;
    QLabel *receiveTitle;
    QLabel *transmitTitle;
    QComboBox *languageSelector;
    QComboBox *themeSelector;
    QGridLayout *settingsGrid;
    UiLanguage currentLanguage;
    QString currentConnectCom;
    QValidator *validator_combox_baudrate;
    QString pendingDisplay;

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

    bool isShowSend;
    bool isShowTime;
    bool recAsciiFormat;
    bool sendAsciiFormat;
    bool repeatSend;
    bool pauseComOutput;
    UiThemeMode themeMode;
    bool darkMode;

    void RefreshTheUSBList();

    // Helper functions for code reuse
    void configureSerialPort();
    void initQssStyleSheet();
    void configureLayout();
    void updateLayoutMetrics();
    void configureLanguageSelector();
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
    void setConnectionState(bool connected);
    void setBaudRate(int index);
    void setStopBits(int index);
    void setParity(int index);
    void setDataBits(int index);
    void setFlowControl(int index);
    QString formatReceiveData(const QByteArray &data, bool isAscii);
    QString formatSendData(const QByteArray &data, bool isAscii);
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

    quint64 recCount;
    quint64 sendCount;
};

#endif // SERIALPORT_H
