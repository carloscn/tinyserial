// Qt for Linux   Project.                                             GitLab
/****************************************************************************/
/*                                                                          */
/*  @file       : main.cpp                        	                        */
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
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SerialPort w;
    w.show();

    return a.exec();
}
