#include "uilanguage.h"

#include <QLatin1String>
#include <QHash>

namespace {
struct Translation {
    const char *key;
    const char *english;
    const char *chinese;
    const char *spanish;
};

const Translation translations[] = {
    {"connection", "Connection", "连接", "Conexión"},
    {"serial_port", "Serial port", "串口", "Puerto serie"},
    {"baud_rate", "Baud rate", "波特率", "Velocidad en baudios"},
    {"data_bits", "Data bits", "数据位", "Bits de datos"},
    {"stop_bits", "Stop bits", "停止位", "Bits de parada"},
    {"parity", "Parity", "校验位", "Paridad"},
    {"flow_control", "Flow control", "流控", "Control de flujo"},
    {"none", "None", "无", "Ninguno"},
    {"custom", "Custom", "自定义", "Personalizado"},
    {"even", "Even", "偶校验", "Par"},
    {"odd", "Odd", "奇校验", "Impar"},
    {"space", "Space", "空格", "Espacio"},
    {"mark", "Mark", "标记", "Marca"},
    {"hardware", "Hardware", "硬件", "Hardware"},
    {"software", "Software", "软件", "Software"},
    {"refresh_ports", "Refresh", "刷新", "Actualizar"},
    {"pause_output", "Pause", "暂停", "Pausar"},
    {"resume_output", "Resume", "继续", "Reanudar"},
    {"connect", "Connect", "连接", "Conectar"},
    {"disconnect", "Disconnect", "断开", "Desconectar"},
    {"receive", "Receive", "接收", "Recepción"},
    {"text_mode", "Text", "文本", "Texto"},
    {"hex_mode", "Hex", "十六进制", "Hex"},
    {"text_encoding", "Encoding", "编码", "Codificación"},
    {"escape_sequences", "Escape sequences", "转义序列", "Secuencias de escape"},
    {"append_line_ending", "Append line ending", "追加行结束符", "Añadir fin de línea"},
    {"line_ending_none", "None", "无", "Ninguno"},
    {"line_ending_lf", "LF (\\n)", "LF (\\n)", "LF (\\n)"},
    {"line_ending_crlf", "CRLF (\\r\\n)", "CRLF (\\r\\n)", "CRLF (\\r\\n)"},
    {"line_ending_cr", "CR (\\r)", "CR (\\r)", "CR (\\r)"},
    {"word_wrap", "Word wrap", "自动换行", "Ajuste de línea"},
    {"display_sent", "Show sent data", "显示发送数据", "Mostrar datos enviados"},
    {"display_time", "Show timestamp", "显示时间戳", "Mostrar marca de tiempo"},
    {"realtime_log", "Real-time logging", "实时记录日志", "Registro en tiempo real"},
    {"realtime_log_title", "Start real-time logging", "启动实时日志记录", "Iniciar registro en tiempo real"},
    {"start_logging", "Start logging", "开始记录", "Iniciar registro"},
    {"log_files", "Log files (*.log);;Text files (*.txt);;All files (*.*)", "日志文件 (*.log);;文本文件 (*.txt);;所有文件 (*.*)", "Archivos de registro (*.log);;Archivos de texto (*.txt);;Todos los archivos (*.*)"},
    {"realtime_log_started", "Real-time logging to:\n%1", "正在实时记录到：\n%1", "Registro en tiempo real en:\n%1"},
    {"realtime_log_stopped", "Real-time logging stopped.", "实时日志记录已停止。", "El registro en tiempo real se ha detenido."},
    {"realtime_log_failed", "Could not create the real-time log file.", "无法创建实时日志文件。", "No se pudo crear el archivo de registro en tiempo real."},
    {"realtime_log_write_failed", "Writing to the real-time log failed. Logging has stopped.", "实时日志写入失败，记录已停止。", "No se pudo escribir en el registro en tiempo real. El registro se ha detenido."},
    {"theme_system", "System", "跟随系统", "Sistema"},
    {"theme_light", "Light", "亮色", "Claro"},
    {"theme_dark", "Dark", "深色", "Oscuro"},
    {"transmit", "Transmit", "发送", "Transmisión"},
    {"repeat_send", "Repeat send", "循环发送", "Envío repetido"},
    {"terminal", "Terminal", "终端", "Terminal"},
    {"receive_placeholder", "Incoming serial data appears here", "接收到的串口数据将显示在这里", "Los datos recibidos aparecerán aquí"},
    {"send_placeholder", "Enter text or HEX data to send", "输入要发送的文本或 HEX 数据", "Introduzca texto o datos HEX para enviar"},
    {"send", "Send", "发送", "Enviar"},
    {"clear", "Clear", "清空", "Limpiar"},
    {"traffic", "Traffic", "流量统计", "Tráfico"},
    {"received", "Received", "已接收", "Recibido"},
    {"sent", "Sent", "已发送", "Enviado"},
    {"plotting", "Plotting", "绘图", "Gráficas"},
    {"plotting_future", "Plotting will be available in a future release.", "绘图功能将在后续版本中提供。", "Las gráficas estarán disponibles en una versión futura."},
    {"file", "File", "文件", "Archivo"},
    {"view", "View", "视图", "Vista"},
    {"about", "About", "关于", "Acerca de"},
    {"save_log", "Save log file", "保存日志", "Guardar registro"},
    {"open_log_folder", "Open log folder", "打开日志目录", "Abrir carpeta de registros"},
    {"open_log_folder_failed", "Could not open the log folder:\n%1", "无法打开日志目录：\n%1", "No se pudo abrir la carpeta de registros:\n%1"},
    {"terminal_cache_size", "Terminal cache size…", "终端缓存大小…", "Tamaño de caché del terminal…"},
    {"terminal_cache_size_label", "Keep terminal text (MiB):", "保留终端文本（MiB）：", "Conservar texto del terminal (MiB):"},
    {"terminal_font", "Terminal font…", "终端字体…", "Fuente del terminal…"},
    {"debug_mode", "Debug mode", "调试模式", "Modo de depuración"},
    {"full_terminal_mode", "Full terminal mode", "全终端模式", "Modo de terminal completo"},
    {"terminal_mode_tag", "Terminal", "终端", "Terminal"},
    {"receive_activity", "Receive activity", "接收活动", "Actividad de recepción"},
    {"send_activity", "Send activity", "发送活动", "Actividad de envío"},
    {"realtime_log_on", "Logging on", "正在记录", "Registro activo"},
    {"file_transfer", "File transfer", "文件传输", "Transferencia de archivos"},
    {"send_file", "Send file…", "发送文件…", "Enviar archivo…"},
    {"receive_file", "Receive file…", "接收文件…", "Recibir archivo…"},
    {"cancel_file_transfer", "Cancel file transfer", "取消文件传输", "Cancelar transferencia"},
    {"file_transfer_protocol", "Protocol:", "协议：", "Protocolo:"},
    {"xmodem_checksum", "XMODEM (checksum)", "XMODEM（校验和）", "XMODEM (suma de comprobación)"},
    {"xmodem_crc", "XMODEM-CRC", "XMODEM-CRC", "XMODEM-CRC"},
    {"xmodem_1k", "XMODEM-1K", "XMODEM-1K", "XMODEM-1K"},
    {"ymodem", "YMODEM", "YMODEM", "YMODEM"},
    {"file_transfer_connect_first", "Connect a serial port before starting a file transfer.", "请先连接串口再开始文件传输。", "Conecte un puerto serie antes de iniciar una transferencia."},
    {"file_transfer_busy", "A file transfer owns the serial port until it finishes or is cancelled.", "文件传输期间串口由传输任务独占。", "La transferencia de archivos controla el puerto serie hasta finalizar o cancelarse."},
    {"file_transfer_started", "File transfer started: %1", "文件传输已开始：%1", "Transferencia iniciada: %1"},
    {"file_transfer_progress", "Sending: %1 / %2 bytes", "正在发送：%1 / %2 字节", "Enviando: %1 / %2 bytes"},
    {"file_transfer_received", "Received: %1 bytes", "已接收：%1 字节", "Recibido: %1 bytes"},
    {"about_tinyserial", "About TinySerial", "关于 TinySerial", "Acerca de TinySerial"},
    {"about_multibeans", "About MULTIBEANS", "关于 MULTIBEANS", "Acerca de MULTIBEANS"},
    {"about_multibeans_body", "TinySerial is part of the MULTIBEANS project.\nhttps://github.com/carloscn/tinyserial", "TinySerial 是 MULTIBEANS 项目的一部分。\nhttps://github.com/carloscn/tinyserial", "TinySerial forma parte del proyecto MULTIBEANS.\nhttps://github.com/carloscn/tinyserial"},
    {"connected", "● Connected", "● 已连接", "● Conectado"},
    {"disconnected", "● Disconnected", "● 未连接", "● Desconectado"},
    {"warning", "Warning", "警告", "Advertencia"},
    {"information", "Information", "提示", "Información"},
    {"ok", "OK", "确定", "Aceptar"},
    {"close", "Close", "关闭", "Cerrar"},
    {"save", "Save", "保存", "Guardar"},
    {"cancel", "Cancel", "取消", "Cancelar"},
    {"look_in", "Look in", "查找范围", "Ubicación"},
    {"file_name", "File name", "文件名", "Nombre del archivo"},
    {"file_type", "File type", "文件类型", "Tipo de archivo"},
    {"invalid_baud_title", "Invalid baud rate", "波特率无效", "Velocidad no válida"},
    {"invalid_baud", "Enter a custom baud rate between 1 and 9,999,999.", "请输入 1 到 9,999,999 之间的自定义波特率。", "Introduzca una velocidad entre 1 y 9.999.999."},
    {"serial_closed", "Serial port closed", "串口未连接", "Puerto serie cerrado"},
    {"open_before_repeat", "Open a serial port before enabling repeat send.", "启用循环发送前请先连接串口。", "Abra un puerto serie antes de activar el envío repetido."},
    {"invalid_hex", "Invalid HEX data", "HEX 数据无效", "Datos HEX no válidos"},
    {"hex_empty", "HEX data is empty.", "HEX 数据不能为空。", "Los datos HEX están vacíos."},
    {"hex_incomplete", "HEX data must contain complete bytes (an even number of digits).", "HEX 数据必须是完整字节（十六进制数字数量应为偶数）。", "Los datos HEX deben contener bytes completos (un número par de dígitos)."},
    {"hex_characters", "HEX data may only contain digits A-F and whitespace.", "HEX 数据只能包含 0-9、A-F 和空白字符。", "Los datos HEX solo pueden contener dígitos 0-9, A-F y espacios."},
    {"empty_send", "Enter data before sending.", "请输入要发送的数据。", "Introduzca datos antes de enviar."},
    {"select_port", "Select an available serial port before connecting.", "请先刷新并选择可用串口。", "Seleccione un puerto serie disponible antes de conectar."},
    {"invalid_port", "The selected serial port is invalid.", "所选串口无效。", "El puerto serie seleccionado no es válido."},
    {"open_failed", "Could not open %1.\n\n%2", "无法打开 %1。\n\n%2", "No se pudo abrir %1.\n\n%2"},
    {"open_ok", "%1 is connected.", "%1 已连接。", "%1 está conectado."},
    {"no_ports", "No serial ports available", "未发现可用串口", "No hay puertos serie disponibles"},
    {"ports_found", "%1 serial port(s) found", "发现 %1 个串口", "Se encontraron %1 puerto(s) serie"},
    {"port_disconnected", "Serial port %1 was disconnected", "串口 %1 已断开", "El puerto serie %1 se desconectó"},
    {"no_connection", "No serial port connected", "未连接串口", "No hay ningún puerto serie conectado"},
    {"write_failed", "Failed to write data to the serial port.", "串口数据写入失败。", "No se pudieron escribir datos en el puerto serie."},
    {"text_encoding_failed", "The selected encoding cannot represent this text.", "所选编码无法表示这段文本。", "La codificación seleccionada no puede representar este texto."},
    {"invalid_escape", "The text contains an invalid escape sequence.", "文本包含无效的转义序列。", "El texto contiene una secuencia de escape no válida."},
    {"empty_log", "There is no terminal data to save.", "没有可保存的终端数据。", "No hay datos de terminal para guardar."},
    {"save_log_as", "Save log file", "保存日志文件", "Guardar registro"},
    {"text_files", "Text files (*.txt);;All files (*.*)", "文本文件 (*.txt);;所有文件 (*.*)", "Archivos de texto (*.txt);;Todos los archivos (*.*)"},
    {"save_failed", "Could not create the selected log file.", "无法创建所选日志文件。", "No se pudo crear el archivo de registro seleccionado."},
    {"saved_log", "Log saved to:\n%1", "日志已保存至：\n%1", "Registro guardado en:\n%1"},
    {"serial_error", "Serial port error: %1", "串口错误：%1", "Error del puerto serie: %1"},
    {"serial_device_not_found", "The serial device was not found.", "未找到串口设备。", "No se encontró el dispositivo serie."},
    {"serial_permission", "Permission to access the serial port was denied.", "没有访问串口的权限。", "Se denegó el acceso al puerto serie."},
    {"serial_open_error", "The serial port is already open or cannot be opened.", "串口已打开或无法打开。", "El puerto serie ya está abierto o no se puede abrir."},
    {"serial_not_open", "The serial port is not open.", "串口尚未打开。", "El puerto serie no está abierto."},
    {"serial_write_error", "An error occurred while writing to the serial port.", "写入串口时发生错误。", "Se produjo un error al escribir en el puerto serie."},
    {"serial_read_error", "An error occurred while reading from the serial port.", "读取串口时发生错误。", "Se produjo un error al leer del puerto serie."},
    {"serial_data_error", "The serial port received malformed data.", "串口接收到格式错误的数据。", "El puerto serie recibió datos con formato incorrecto."},
    {"serial_resource_error", "The serial device is unavailable or was disconnected.", "串口设备不可用或已断开。", "El dispositivo serie no está disponible o se desconectó."},
    {"serial_unsupported", "This serial operation is not supported.", "不支持此串口操作。", "Esta operación serie no es compatible."},
    {"serial_timeout", "The serial operation timed out.", "串口操作超时。", "La operación serie agotó el tiempo de espera."},
    {"serial_unknown", "An unknown serial port error occurred.", "发生未知串口错误。", "Se produjo un error desconocido del puerto serie."},
    {"about_subtitle", "Cross-platform serial terminal", "跨平台串口终端", "Terminal serie multiplataforma"},
    {"version_value", "Version %1", "版本 %1", "Versión %1"},
    {"about_warranty", "This program is provided as-is, without express or implied warranties.", "本程序按现状提供，不附带任何明示或暗示的担保。", "Este programa se proporciona tal cual, sin garantías expresas ni implícitas."},
    {"timeline", "Timeline", "时间线", "Cronología"},
    {"timeline_system", "System", "系统", "Sistema"},
    {"timeline_search", "Search content…", "搜索内容…", "Buscar contenido…"},
    {"timeline_autoscroll", "Auto-scroll", "自动滚动", "Desplazamiento automático"},
    {"timeline_open", "Open…", "打开…", "Abrir…"},
    {"timeline_export_csv", "Export CSV", "导出 CSV", "Exportar CSV"},
    {"timeline_export_jsonl", "Export JSONL", "导出 JSONL", "Exportar JSONL"},
    {"col_time", "Time", "时间", "Hora"},
    {"col_delta", "Since previous", "距上一条", "Desde anterior"},
    {"col_type", "Type", "类型", "Tipo"},
    {"col_source", "Source", "来源", "Origen"},
    {"col_bytes", "Bytes", "字节", "Bytes"},
    {"col_content", "Content", "内容", "Contenido"},
    {"source_device", "Device", "设备", "Dispositivo"},
    {"source_manual", "Manual", "手动", "Manual"},
    {"source_repeat", "Repeat", "循环", "Repetición"},
    {"timeline_summary", "%1 events · RX %2 · TX %3 · System %4", "共 %1 条 · RX %2 · TX %3 · 系统 %4", "%1 eventos · RX %2 · TX %3 · Sistema %4"},
    {"timeline_response", "Response: first RX after %1 ms, %2 RX events, %3 bytes", "响应：%1 ms 后首次收到，共 %2 条 RX，%3 字节", "Respuesta: primer RX tras %1 ms, %2 eventos RX, %3 bytes"},
    {"timeline_no_response", "No RX before the next TX", "下一次发送前没有收到数据", "Sin RX antes del siguiente TX"},
    {"timeline_empty", "The timeline is empty.", "时间线为空。", "La cronología está vacía."},
    {"timeline_export_title", "Export timeline", "导出时间线", "Exportar cronología"},
    {"timeline_open_title", "Open timeline", "打开时间线", "Abrir cronología"},
    {"csv_files", "CSV files (*.csv);;All files (*)", "CSV 文件 (*.csv);;所有文件 (*)", "Archivos CSV (*.csv);;Todos los archivos (*)"},
    {"jsonl_files", "JSON Lines files (*.jsonl);;All files (*)", "JSON Lines 文件 (*.jsonl);;所有文件 (*)", "Archivos JSON Lines (*.jsonl);;Todos los archivos (*)"},
    {"timeline_saved", "Timeline saved to %1", "时间线已保存到 %1", "Cronología guardada en %1"},
    {"timeline_open_failed", "Line %1 of the file is not a TinySerial timeline event.", "文件第 %1 行不是 TinySerial 时间线事件。", "La línea %1 del archivo no es un evento de cronología de TinySerial."},
    {"timeline_read_failed", "The file could not be read.", "无法读取该文件。", "No se pudo leer el archivo."},
    {"timeline_disconnect_first", "Disconnect the serial port before opening a saved timeline.", "打开已保存的时间线前，请先断开串口。", "Desconecte el puerto serie antes de abrir una cronología guardada."},
    {"timeline_replace", "Replace the current timeline with the file? Events that were not exported will be lost.", "用该文件替换当前时间线？未导出的事件将会丢失。", "¿Reemplazar la cronología actual por el archivo? Se perderán los eventos no exportados."},
    {"timeline_replace_button", "Replace", "替换", "Reemplazar"},
    {"timeline_loaded", "Loaded %1 events from %2", "已从 %2 载入 %1 条事件", "Se cargaron %1 eventos desde %2"},
    {"open", "Open", "打开", "Abrir"},
    {"ev_connected", "Connected to %1 (%2)", "已连接 %1（%2）", "Conectado a %1 (%2)"},
    {"ev_disconnected", "Disconnected from %1", "已断开 %1", "Desconectado de %1"},
    {"ev_paused", "Terminal display paused", "终端显示已暂停", "Visualización del terminal en pausa"},
    {"ev_resumed", "Terminal display resumed", "终端显示已恢复", "Visualización del terminal reanudada"}
};
}

UiLanguage uiLanguageFromCode(const QString &code)
{
    if (code.startsWith(QLatin1String("zh"), Qt::CaseInsensitive))
        return UiLanguage::Chinese;
    if (code.startsWith(QLatin1String("es"), Qt::CaseInsensitive))
        return UiLanguage::Spanish;
    return UiLanguage::English;
}

QString uiLanguageCode(UiLanguage language)
{
    switch (language) {
    case UiLanguage::Chinese: return QStringLiteral("zh_CN");
    case UiLanguage::Spanish: return QStringLiteral("es");
    case UiLanguage::English: return QStringLiteral("en");
    }
    return QStringLiteral("en");
}

QString uiText(UiLanguage language, const char *key)
{
    static const QHash<QString, const Translation *> index = [] {
        QHash<QString, const Translation *> result;
        result.reserve(static_cast<int>(sizeof(translations) / sizeof(translations[0])));
        for (const Translation &entry : translations)
            result.insert(QString::fromLatin1(entry.key), &entry);
        return result;
    }();

    const auto found = index.constFind(QString::fromLatin1(key));
    if (found == index.cend())
        return QString::fromLatin1(key);

    const Translation &entry = **found;
    switch (language) {
    case UiLanguage::Chinese: return QString::fromUtf8(entry.chinese);
    case UiLanguage::Spanish: return QString::fromUtf8(entry.spanish);
    case UiLanguage::English: return QString::fromUtf8(entry.english);
    }
    return QString::fromLatin1(key);
}

QStringList uiTranslationKeys()
{
    QStringList keys;
    keys.reserve(static_cast<int>(sizeof(translations) / sizeof(translations[0])));
    for (const Translation &entry : translations)
        keys.append(QString::fromLatin1(entry.key));
    return keys;
}
