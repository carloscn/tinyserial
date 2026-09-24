// Qt Cross-platform Project.                                             GitHub
/****************************************************************************/
/*                                                                          */
/*  @file       : main.cpp                                                  */
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
/*  | 2025.12.11 |   optimize| Carlos Wei (M) | Code refactoring & fix.  |  */
/*  ---------------------------------------------------------------------   */
/*  Email: carlos.wei.hk@gmail.com                              MULTIBEANS.*/
/****************************************************************************/
#include "mainwindow.h"
#include "appversion.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TinySerial"));
    app.setApplicationVersion(QLatin1String(APP_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/img/desktop.ico")));
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    app.setDesktopFileName(QStringLiteral("tinyserial"));
#endif

    MainWindow window;
    window.show();

    return app.exec();
}
