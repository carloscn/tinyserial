// Qt Cross-platform Project.                                             GitHub
/****************************************************************************/
/*                                                                          */
/*  @file       : global.h                                                  */
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
/*  | 2017.09.17 |   create  | Carlos Wei (M) | create the document.     |  */
/*  | 2025.12.11 |   optimize| Carlos Wei (M) | Code refactoring & fix.  |  */
/*  ---------------------------------------------------------------------   */
/*  Email: carlos.wei.hk@gmail.com                              MULTIBEANS.*/
/****************************************************************************/

#ifndef GLOBAL_H
#define GLOBAL_H

// Baud rate configuration indices
#define CONFIG_BAUDRATE_1200_INDEX      0
#define CONFIG_BAUDRATE_2400_INDEX      1
#define CONFIG_BAUDRATE_4800_INDEX      2
#define CONFIG_BAUDRATE_9600_INDEX      3
#define CONFIG_BAUDRATE_19200_INDEX     4
#define CONFIG_BAUDRATE_38400_INDEX     5
#define CONFIG_BAUDRATE_57600_INDEX     6
#define CONFIG_BAUDRATE_115200_INDEX    7
#define CONFIG_BAUDRATE_CUSTOM_INDEX    8

// Stop bits configuration indices
#define CONFIG_STOPBIT_ONE_INDEX        0
#define CONFIG_STOPBIT_ONEANDHALF_INDEX 1
#define CONFIG_STOPBIT_TWO_INDEX        2

// Parity configuration indices
#define CONFIG_PARITY_NONE_INDEX        0
#define CONFIG_PARITY_EVEN_INDEX        1
#define CONFIG_PARITY_ODD_INDEX         2
#define CONFIG_PARITY_SPACE_INDEX       3
#define CONFIG_PARITY_MARK_INDEX        4

// Flow control configuration indices
#define CONFIG_FLOWCTRL_NONE_INDEX      0
#define CONFIG_FLOWCTRL_HARD_INDEX      1
#define CONFIG_FLOWCTRL_SOFT_INDEX      2

// Data bits configuration indices
#define CONFIG_DATABITS_5_INDEX         0
#define CONFIG_DATABITS_6_INDEX         1
#define CONFIG_DATABITS_7_INDEX         2
#define CONFIG_DATABITS_8_INDEX         3

#endif // GLOBAL_H
