# UCSI 3.0 — Команды, поддерживаемые контроллером (PPM)

Документ описывает полный набор UCSI-команд, которые принимает PPM (Platform
Policy Manager). Это контракт между OPM (хост-драйвер ОС) и нашей
библиотекой/прошивкой. Источник — *USB Type-C Connector System Software
Interface (UCSI) Specification, Revision 3.0*, May 2023, раздел 6.5 (и
приложение A.1 для значений opcode).

> Цель: зафиксировать **скоуп**. Здесь перечислены все 32 команды; для каждой
> указано — обязательная она или опциональная, и почему её можно/нельзя
> исключить из первой реализации.

---

## 1. Структуры данных OPM ↔ PPM

PPM выставляет четыре регистра в общем адресном пространстве (см. §4 спеки):

| Offset | Mnemonic    | Направление    | Размер  | Назначение                              |
|-------:|:------------|:---------------|--------:|:----------------------------------------|
| 0      | VERSION     | PPM → OPM (RO) | 24 bit  | BCD-версия UCSI (0x0300 = 3.0)          |
| 3      | RESERVED    | —              | 8 bit   |                                         |
| 4      | CCI         | PPM → OPM (RO) | 32 bit  | Command Completion / Change Indication  |
| 8      | CONTROL     | OPM → PPM (RO) | 64 bit  | Тело команды (см. §2 каждой команды)    |
| 16     | MESSAGE IN  | PPM → OPM (RO) | 2040 bit (≤255 B) | Ответные данные команды        |
| 271    | RESERVED    | —              | 8 bit   |                                         |
| 272    | MESSAGE OUT | OPM → PPM (RO) | 2040 bit (≤255 B) | Входные данные команды         |
| 527    | RESERVED    | —              | 8 bit   |                                         |

Запись OPM-ом в `CONTROL[0..7]` (поле Command) запускает выполнение команды.
PPM по завершении выставляет `CCI.Command Completed Indicator = 1` и (если
поддерживается) поднимает аппаратный alert.

### 1.1 CCI — общая структура (одинакова для всех команд)

Полный layout — Table 4-3 спеки. Дальше в описаниях команд указаны только
*отличающиеся* поля (обычно — значение `Data Length`).

| Bit  | Field                            | Size | Описание                                                                                       |
|-----:|:---------------------------------|-----:|:-----------------------------------------------------------------------------------------------|
| 0    | End of Message Indicator         | 1    | 1 = последний/единственный чанк (только для chunked-команд: FW Update, Security Request).      |
| 1    | Connector Change Indicator       | 7    | Номер коннектора, на котором произошло асинхронное событие; 0 = нет события.                   |
| 8    | Data Length                      | 8    | Кол-во валидных байт в MESSAGE_IN. `0` если данных нет. ≤ `MAX_DATA_LENGTH` (0xFF).            |
| 16   | Vendor Defined Message Indicator | 1    | Готов нестандартный (vendor) ответ. Mutually exclusive с прочими Indicator-ами.                |
| 17   | Reserved                         | 6    | 0.                                                                                             |
| 23   | Security Request Indicator       | 1    | 1 = асинхронный Security Request от port partner.                                              |
| 24   | FW Update Request Indicator      | 1    | 1 = асинхронный FW Update Request от port partner.                                             |
| 25   | Not Supported Indicator          | 1    | 1 = команда не поддерживается (валидно при `Command Completed = 1`).                           |
| 26   | Cancel Completed Indicator       | 1    | 1 = команда CANCEL завершена.                                                                  |
| 27   | Reset Completed Indicator        | 1    | 1 = PPM_RESET завершён. Если 1, все остальные биты CCI = 0. PPM сбросит этот бит на след. cmd. |
| 28   | Busy Indicator                   | 1    | 1 = PPM занят, ответ позже. Если 1, все остальные биты CCI = 0.                                |
| 29   | Acknowledge Command Indicator    | 1    | 1 = подтверждение получения ACK_CC_CI.                                                         |
| 30   | Error Indicator                  | 1    | 1 = команда завершилась с ошибкой. Детали — через GET_ERROR_STATUS.                            |
| 31   | Command Completed Indicator      | 1    | 1 = команда (или отказ) обработана. OPM ждёт этот бит.                                         |

### 1.2 CONTROL — общая структура

Тело команды, 64 бит. Универсальная "шапка" (биты 0..15):

| Bit  | Field        | Size | Описание                                                                       |
|-----:|:-------------|-----:|:-------------------------------------------------------------------------------|
| 0    | Command      | 8    | Opcode команды (см. §1.4).                                                     |
| 8    | Data Length  | 8    | Длина данных в MESSAGE_OUT в байтах. Большинство команд — 0.                   |
| 16   | …            | 48   | Командно-специфичные поля.                                                     |

### 1.3 Параметры-константы (Appendix A.2)

| Константа                     | Min  | Max  | Единицы |
|:------------------------------|:----:|:----:|:--------|
| MAX_DATA_LENGTH               | —    | 0xFF | bytes   |
| MAX_NUM_ALT_MODE              | —    | 0x80 | —       |
| MIN_TIME_TO_RESPOND_WITH_BUSY | 0xBE | —    | ms      |
| GET_ERROR_STATUS_DATA_LENGTH  | —    | 0x10 | bytes   |
| LPM_BUSY_ATOMIC_TIME          | 0xF0 | —    | ms      |
| SENDER_RESPONSE_TIMEOUT       | —    | 0x3C | ms      |

### 1.4 Таблица opcode-ов команд (Appendix A.1)

| Opcode | Команда                    | §       | Скоуп v1 |
|:------:|:---------------------------|:--------|:--------:|
| 0x00   | RESERVED                   | —       | —        |
| 0x01   | PPM_RESET                  | 2.1     | ✅ MUST  |
| 0x02   | CANCEL                     | 2.2     | ✅ MUST  |
| 0x03   | CONNECTOR_RESET            | 2.3     | ✅ MUST  |
| 0x04   | ACK_CC_CI                  | 2.4     | ✅ MUST  |
| 0x05   | SET_NOTIFICATION_ENABLE    | 2.5     | ✅ MUST  |
| 0x06   | GET_CAPABILITY             | 2.6     | ✅ MUST  |
| 0x07   | GET_CONNECTOR_CAPABILITY   | 2.7     | ✅ MUST  |
| 0x08   | SET_CCOM                   | 2.8     | ⚠️ Opt¹  |
| 0x09   | SET_UOR                    | 2.9     | ✅ MUST  |
| 0x0A   | SET_PDM (obsolete)         | —       | ❌       |
| 0x0B   | SET_PDR                    | 2.10    | ✅ MUST  |
| 0x0C   | GET_ALTERNATE_MODES        | 2.11    | ⚠️ Opt²  |
| 0x0D   | GET_CAM_SUPPORTED          | 2.12    | ⚠️ Opt²  |
| 0x0E   | GET_CURRENT_CAM            | 2.13    | ⚠️ Opt²  |
| 0x0F   | SET_NEW_CAM                | 2.14    | ⚠️ Opt²  |
| 0x10   | GET_PDOS                   | 2.15    | ✅ MUST  |
| 0x11   | GET_CABLE_PROPERTY         | 2.16    | ✅ MUST³ |
| 0x12   | GET_CONNECTOR_STATUS       | 2.17    | ✅ MUST  |
| 0x13   | GET_ERROR_STATUS           | 2.18    | ✅ MUST  |
| 0x14   | SET_POWER_LEVEL            | 2.19    | ✅ MUST  |
| 0x15   | GET_PD_MESSAGE             | 2.20    | ⚠️ Opt⁴  |
| 0x16   | GET_ATTENTION_VDO          | 2.21    | ⚠️ Opt⁴  |
| 0x17   | RESERVED                   | —       | —        |
| 0x18   | GET_CAM_CS                 | 2.22    | ⚠️ Opt²  |
| 0x19   | LPM_FW_UPDATE_REQUEST      | 2.23    | ❌ Opt   |
| 0x1A   | SECURITY_REQUEST           | 2.24    | ❌ Opt   |
| 0x1B   | SET_RETIMER_MODE           | 2.25    | ❌ Opt   |
| 0x1C   | SET_SINK_PATH              | 2.26    | ✅ MUST  |
| 0x1D   | SET_PDOS                   | 2.28    | ✅ MUST  |
| 0x1E   | READ_POWER_LEVEL           | 2.32    | ✅ MUST  |
| 0x1F   | CHUNKING_SUPPORT           | 2.27    | ✅ MUST⁵ |
| 0x20   | VENDOR_DEFINED             | 2.29    | ❌ Opt   |
| 0x21   | SET_USB                    | 2.31    | ⚠️ Opt⁶  |
| 0x22   | GET_LPM_PPM_INFO           | 2.30    | ✅ MUST  |

Пометки скоупа:
- ✅ **MUST** — Normative для PPM по Table 6-87. Минимальный набор для соответствия UCSI 3.0.
- ⚠️ **Opt** — Optional/Conditional Normative; включается по фиче.
  - ¹ Опционален для PPM, но Conditional Normative для LPM, если коннектор DRP. Под нашу архитектуру (FUSB302 умеет DRP) — фактически обязателен.
  - ² Conditional Normative, если коннектор поддерживает Alternate Modes. Если первая версия библиотеки **не** заявляет AM-поддержку в `bmOptionalFeatures`, эти команды можно ответом Not Supported.
  - ³ Получение свойств кабеля; нужно если есть PD-eMarker и желание сообщать ОС о cable current capability. Минимум — отвечать «по тому, что есть» (без e-marker discovery).
  - ⁴ Только при наличии PD-партнёра и поддержки соответствующих фич.
  - ⁵ Обязательно если MESSAGE_IN/OUT нашего PPM меньше 2040 бит (Table 4-1). В RP2350-варианте, скорее всего, будем иметь полный размер — но безопаснее заявить поддержку и явный размер.
  - ⁶ Conditional Normative для PPM при поддержке USB3/USB4. Для FUSB302 (USB2 only по сигналам) формально неприменимо — но AM/USB-data role нужно корректно репортить.
- ❌ **Opt** — feature, который мы заведомо не делаем в v1 (FW update, Security, Retimers, Vendor commands). На команду отвечаем `Not Supported Indicator = 1`.

### 1.5 Applicability table (выдержка из Table 6-87)

`N` = Normative, `CN` = Conditional Normative, `NA` = Not Applicable,
`O` = Optional, `NS` = Not Supported.

| Команда                  | OPM | PPM | LPM   |
|:-------------------------|:---:|:---:|:-----:|
| PPM_RESET                | CN¹ | N   | NS²   |
| CANCEL                   | N   | N   | N     |
| CONNECTOR_RESET          | N   | NA  | N     |
| ACK_CC_CI                | N   | N   | CN³   |
| SET_NOTIFICATION_ENABLE  | O   | N   | N     |
| GET_CAPABILITY           | O   | N   | CN³   |
| GET_CONNECTOR_CAPABILITY | N   | NA  | N     |
| SET_CCOM                 | O   | O   | CN⁵   |
| SET_UOR                  | N   | NA  | N     |
| SET_PDR                  | N   | NA  | N     |
| GET_ALTERNATE_MODES      | O   | O   | CN⁶   |
| GET_CAM_SUPPORTED        | O   | O   | CN⁶   |
| GET_CURRENT_CAM          | O   | O   | CN⁶   |
| SET_NEW_CAM              | O   | NA  | CN⁶   |
| SET_USB                  | O   | CN⁴ | CN¹⁰  |
| GET_PDOS                 | N   | NA  | N     |
| GET_CABLE_PROPERTY       | O   | O   | N     |
| GET_CONNECTOR_STATUS     | N   | N   | N     |
| GET_ERROR_STATUS         | N   | N   | N     |
| SET_POWER_LEVEL          | N   | N   | N     |
| GET_PD_MESSAGE           | O   | NA  | CN⁹   |
| GET_ATTENTION_VDO        | O   | NA  | CN⁹   |
| GET_CAM_CS               | O   | NA  | CN⁶   |
| LPM_FW_UPDATE_REQUEST    | O   | NA  | O     |
| SECURITY_REQUEST         | O   | NA  | O     |
| SET_RETIMER_MODE         | O   | NA  | O     |
| SET_SINK_PATH            | N   | NA  | R     |
| CHUNKING_SUPPORT         | N   | NA  | CN⁸   |
| SET_PDOS                 | N   | NA  | N     |
| VENDOR_DEFINED           | O   | NA  | O     |
| GET_LPM_PPM_INFO         | N   | N   | N     |
| READ_POWER_LEVEL         | N   | NA  | N     |

Сноски: ① N/A для систем без PPM или с pass-through PPM. ② Если LPM
управляет двумя коннекторами и играет роль PPM — всё равно NS. ③
Применимо при pass-through PPM или конфиге OPM↔LPM. ⁴ Если OPM умеет SET_USB
и поддержаны USB3/USB4. ⁵ Только если коннектор DRP. ⁶ Только если коннектор
поддерживает alternate modes. ⁸ Если MESSAGE IN/OUT меньше Table 4-1. ⁹ Если
port partner PD-capable и команда поддержана. ¹⁰ Если OPM поддерживает.

> В нашей архитектуре «FUSB302 + MCU» PPM **и есть** наша библиотека; LPM —
> внутренний слой, общающийся с FUSB302. Внешний контракт — это столбец
> **PPM** (с натяжкой на LPM-обязанности, поскольку мы сами и есть LPM).

### 1.6 Optional Features bitmap (bmOptionalFeatures, GET_CAPABILITY)

24-битовый bitmap (Table 6-88). Декларирует, какие фичи PPM поддерживает.
OPM не должен пытаться включить нотификации/команды, которых нет.

| Bit | Фича                                |
|----:|:------------------------------------|
| 0   | SET_CCOM supported                  |
| 1   | SET_POWER_LEVEL supported* (всегда) |
| 2   | Alternate mode details supported    |
| 3   | Alternate mode override supported   |
| 4   | PDO details supported               |
| 5   | Cable details supported             |
| 6   | External supply notification supp.  |
| 7   | PD reset notification supported     |
| 8   | GET_PD_MESSAGE supported            |
| 9   | Get Attention VDO                   |
| 10  | FW Update Request                   |
| 11  | Negotiated Power Level Change       |
| 12  | Security Request                    |
| 13  | Set Re-timer Mode                   |
| 14  | Chunking Support                    |

`*` Bit 1 — для обратной совместимости всегда 1.

### 1.7 Соглашения по таблицам полей

В описаниях полей CONTROL/CCI/MESSAGE используется:
- **Offset** — смещение поля в битах от начала структуры.
- **Size** — размер поля в битах (для MESSAGE_OUT иногда в байтах — отмечено).
- Значения отдельных битов внутри поля указаны вложенными таблицами.

Везде, где сказано "Connector Number 7 bit", это номер коннектора от 1; `0`
обычно зарезервирован или означает "all/broadcast" (специфично каждой
команде — см. описание).

---

## 2. Команды

### 2.1 PPM_RESET (0x01) — MUST

Сбрасывает PPM. OPM шлёт PPM_RESET в любой момент; чтобы заодно сбросить
коннекторы, OPM должен предварительно выполнить CONNECTOR_RESET на каждом.

#### CONTROL

| Offset | Field         | Size | Описание                       |
|-------:|:--------------|-----:|:-------------------------------|
| 0      | Command       | 8    | = `0x01`                       |
| 8      | Data Length   | 8    | 0x00                           |
| 16     | Reserved      | 48   | 0                              |

#### CCI отличия

- `Reset Completed Indicator = 1` (остальные биты = 0).
- `Data Length = 0`.
- `Command Completed Indicator = 0`. PPM сам сбросит Reset Completed при
  получении следующей не-PPM_RESET команды.

OPM polls CCI, пока не увидит Reset Completed = 1.

---

### 2.2 CANCEL (0x02) — MUST

Отменяет предыдущую команду, которая ещё в Busy. OPM шлёт только если CCI
предыдущей команды имел `Busy Indicator = 1`. Если PPM уже завершил —
CANCEL дропается.

#### CONTROL

| Offset | Field       | Size | Описание |
|-------:|:------------|-----:|:---------|
| 0      | Command     | 8    | = `0x02` |
| 8      | Data Length | 8    | 0x00     |
| 16     | Reserved    | 48   | 0        |

#### CCI отличия

- `Cancel Completed Indicator = 1`, `Command Completed Indicator = 1`.
- `Data Length = 0`.

---

### 2.3 CONNECTOR_RESET (0x03) — MUST

Сбрасывает указанный коннектор. PPM шлёт Command Completion как только
*стартовал* reset, а Asynchronous notification — когда reset *завершён*.

Два режима:
1. **Hard Reset** — коннектор пройдёт disconnect-connect. Если подключён
   Type-C charger и нет другого источника питания (Dead Battery) — команда
   фейлится; причину OPM узнаёт через GET_ERROR_STATUS bit 5.
2. **Data Reset** — сброс USB-data + выход из всех alt-modes; VBUS
   сохраняется. Поведение по [USBPD] `Data_Reset`. Обязательна для USB4,
   опциональна иначе. Дедлайн: `tSenderResponse + tDataReset +
   LPM_BUSY_ATOMIC_TIME`.

#### CONTROL

| Offset | Field            | Size | Описание                                     |
|-------:|:-----------------|-----:|:---------------------------------------------|
| 0      | Command          | 8    | = `0x03`                                     |
| 8      | Data Length      | 8    | 0x00                                         |
| 16     | Connector Number | 7    | Номер коннектора (1..N).                     |
| 23     | Reset Type       | 1    | 0 = Hard Reset (default), 1 = Data Reset.    |
| 24     | Reserved         | 40   | 0                                            |

#### CCI отличия

- `Not Supported Indicator` может быть 1, если Reset Type = Data Reset, а
  порт не поддерживает USB4 / Data Reset.
- `Error Indicator = 1` при неуспехе.
- `Command Completed Indicator = 1`.

---

### 2.4 ACK_CC_CI (0x04) — MUST

OPM подтверждает PPM-у, что увидел Command Completion и/или Connector
Change Indication. Не нужен для Connector Change-ов, требующих чтения
MESSAGE_IN (например, FW Update Request от партнёра).

#### CONTROL

| Offset | Field                          | Size | Описание                                                  |
|-------:|:-------------------------------|-----:|:----------------------------------------------------------|
| 0      | Command                        | 8    | = `0x04`                                                  |
| 8      | Data Length                    | 8    | 0x00                                                      |
| 16     | Connector Change Acknowledge   | 1    | 1 = подтверждение connector change на коннекторе из CCI.  |
| 17     | Command Completed Acknowledge  | 1    | 1 = подтверждение завершения команды.                     |
| 18     | Reserved                       | 46   | 0                                                         |

#### CCI отличия

- `Acknowledge Command Indicator = 1`.
- `Data Length = 0`.

---

### 2.5 SET_NOTIFICATION_ENABLE (0x05) — MUST

Маска подписок: какие асинхронные нотификации PPM может слать OPM-у. OPM
может перенастраивать в любой момент. **Нельзя** включать нотификации,
которые PPM не объявил в GET_CAPABILITY. **Если включена хоть одна
нотификация — обязательно должен быть включён `Command Completed`.**

#### CONTROL

| Offset | Field               | Size | Описание                                                              |
|-------:|:--------------------|-----:|:----------------------------------------------------------------------|
| 0      | Command             | 8    | = `0x05`                                                              |
| 8      | Data Length         | 8    | 0x00                                                                  |
| 16     | Notification Enable | 17   | Битмаска нотификаций (см. ниже). 0 в бите = выключено, 1 = включено.  |
| 33     | Reserved            | 31   | 0                                                                     |

Поле `Notification Enable` (биты внутри):

| Bit | Нотификация                                  | Тип |
|----:|:---------------------------------------------|:---:|
| 0   | Command Completed                            | N   |
| 1   | External Supply Change                       | O   |
| 2   | Power Operation Mode Change                  | N   |
| 3   | Attention                                    | O   |
| 4   | LPM FW Update Request from Port Partner      | O   |
| 5   | Supported Provider Capabilities Change       | O   |
| 6   | Negotiated Power Level Change                | O   |
| 7   | PD Reset Complete                            | O   |
| 8   | Supported CAM Change                         | O   |
| 9   | Battery Charging Status Change               | N   |
| 10  | Security Request from Port Partner           | O   |
| 11  | Connector Partner Change                     | N   |
| 12  | Power Direction Change                       | N   |
| 13  | Set Re-timer Mode                            | O   |
| 14  | Connect Change                               | N   |
| 15  | Error                                        | N   |
| 16  | Sink Path Status Change                      | N   |

#### CCI отличия

Стандартное завершение: `Command Completed = 1`, `Error` при неуспехе.

---

### 2.6 GET_CAPABILITY (0x06) — MUST

Возвращает capability-структуру PPM-а.

#### CONTROL

| Offset | Field       | Size | Описание |
|-------:|:------------|-----:|:---------|
| 0      | Command     | 8    | = `0x06` |
| 8      | Data Length | 8    | 0x00     |
| 16     | Reserved    | 48   | 0        |

#### CCI отличия

- `Data Length = 0x10` (16 байт) при успехе.

#### MESSAGE_IN — GET_CAPABILITY Data (16 байт)

| Offset | Field               | Size | Описание                                                              |
|-------:|:--------------------|-----:|:----------------------------------------------------------------------|
| 0      | bmAttributes        | 32   | Битмап общих фич — см. ниже.                                          |
| 32     | bNumConnectors      | 7    | Кол-во коннекторов. 0 — нелегально.                                   |
| 39     | Reserved            | 1    | 0                                                                     |
| 40     | bmOptionalFeatures  | 24   | Битмап опциональных фич (см. §1.6).                                   |
| 64     | bNumAltModes        | 8    | Кол-во alt modes (≤ `MAX_NUM_ALT_MODE`). 0 = AM не поддерживаются.    |
| 72     | Reserved            | 8    | 0                                                                     |
| 80     | bcdBCVersion        | 16   | BCD версия Battery Charging spec (если bit Battery Charging в bmAttr).|
| 96     | bcdPDVersion        | 16   | BCD версия USB PD (если bit USB PD в bmAttr).                         |
| 112    | bcdUSBTypeCVersion  | 16   | BCD версия Type-C (если bit USB Type-C Current в bmAttr).             |

`bmAttributes` (Table 6-14):

| Bit   | Описание                                                            |
|:------|:--------------------------------------------------------------------|
| 0     | Disabled State Support ([USBTYPEC] §4.5.2.2.1)                      |
| 1     | Battery Charging (см. bcdBCVersion)                                 |
| 2     | USB Power Delivery (см. bcdPDVersion)                               |
| 5:3   | Reserved (0)                                                        |
| 6     | USB Type-C Current (см. bcdUSBTypeCVersion)                         |
| 7     | Reserved (0)                                                        |
| 8     | AC Supply (один из bmPowerSource)                                   |
| 9     | Reserved (0)                                                        |
| 10    | Other (один из bmPowerSource)                                       |
| 13:11 | Reserved (0)                                                        |
| 14    | Uses VBUS (один из bmPowerSource)                                   |
| 15    | Reserved (0)                                                        |
| 31:16 | Reserved (0)                                                        |

Минимум один из бит 8/10/14 должен быть установлен.

---

### 2.7 GET_CONNECTOR_CAPABILITY (0x07) — MUST

Возвращает capability-структуру конкретного коннектора.

#### CONTROL

| Offset | Field            | Size | Описание                       |
|-------:|:-----------------|-----:|:-------------------------------|
| 0      | Command          | 8    | = `0x07`                       |
| 8      | Data Length      | 8    | 0x00                           |
| 16     | Connector Number | 7    | Номер коннектора. 0 нелегален. |
| 23     | Reserved         | 41   | 0                              |

#### CCI отличия

- `Data Length = 0x04` (4 байта) при успехе.

#### MESSAGE_IN — GET_CONNECTOR_CAPABILITY Data (4 байта = 32 бит)

| Offset | Field                          | Size | Описание                                                                                       |
|-------:|:-------------------------------|-----:|:-----------------------------------------------------------------------------------------------|
| 0      | Operation Mode                 | 8    | Режимы, поддерживаемые коннектором (битмап, см. ниже).                                         |
| 8      | Provider                       | 1    | Может ли коннектор давать питание (валидно при Rp/DRP).                                        |
| 9      | Consumer                       | 1    | Может ли принимать (Rd/DRP).                                                                   |
| 10     | Swap to DFP                    | 1    | Принимает swap to DFP.                                                                         |
| 11     | Swap to UFP                    | 1    | Принимает swap to UFP.                                                                         |
| 12     | Swap to SRC                    | 1    | Принимает swap to SRC (DRP only).                                                              |
| 13     | Swap to SNK                    | 1    | Принимает swap to SNK (DRP only).                                                              |
| 14     | Extended Operation Mode        | 8    | Дополнительные режимы (USB4 Gen2/3/4, EPR Src/Sink). См. ниже.                                 |
| 22     | Miscellaneous Capabilities     | 4    | bit0 FW Update, bit1 Security, bit2/3 Reserved.                                                |
| 26     | Reverse Current Protection Support | 1 | Debug-level: умеет ли LPM защищать от обратного тока.                                          |
| 27     | Partner PD Revision            | 2    | major PD-revision партнёра (из PD-header).                                                     |
| 29     | Reserved                       | 3    | 0                                                                                              |

`Operation Mode` биты:

| Bit | Значение                              |
|----:|:--------------------------------------|
| 0   | Rp only                               |
| 1   | Rd only                               |
| 2   | DRP (Rp/Rd)                           |
| 3   | Analog Audio Accessory Mode (Ra/Ra)   |
| 4   | Debug Accessory Mode (Rd/Rd)          |
| 5   | USB2                                  |
| 6   | USB3                                  |
| 7   | Alternate Mode                        |

`Extended Operation Mode` биты:

| Bit | Значение     |
|----:|:-------------|
| 0   | USB4 Gen 2   |
| 1   | EPR Source   |
| 2   | EPR Sink     |
| 3   | USB4 Gen 3   |
| 4   | USB4 Gen 4   |
| 5–7 | Reserved     |

---

### 2.8 SET_CCOM (0x08) — Optional (для PPM); de-facto MUST для нашего LPM

Устанавливает CC operation mode коннектора. Подмножество поддерживаемых
режимов. Сбрасывается при PPM_RESET; по умолчанию — DRP (если коннектор это
умеет). Если LPM не умеет запрошенную роль — отвечает Not Supported (кроме
Disable; если Disable не поддерживается — переводит порт в Unattached.SNK
или Unattached.SRC).

#### CONTROL

| Offset | Field             | Size | Описание                                                                        |
|-------:|:------------------|-----:|:------------------------------------------------------------------------------ -|
| 0      | Command           | 8    | = `0x08`                                                                        |
| 8      | Data Length       | 8    | 0x00                                                                            |
| 16     | Connector Number  | 7    | Номер коннектора. 0 нелегален.                                                  |
| 23     | CC Operation Mode | 4    | Битмап (см. ниже). Установка всех в 0 — нелегально.                             |
| 27     | Reserved          | 37   | 0                                                                               |

`CC Operation Mode` биты:

| Bit | Значение                                                                                                      |
|----:|:--------------------------------------------------------------------------------------------------------------|
| 0   | Rp Only                                                                                                       |
| 1   | Rd Only                                                                                                       |
| 2   | DRP                                                                                                           |
| 3   | Disabled (терминаторы убраны; для перехода из Attached.SNK — минимум tErrorRecovery без терминаторов на CC).  |

#### CCI отличия

Стандартное завершение; `Error Indicator = 1` при неуспехе.

---

### 2.9 SET_UOR (0x09) — MUST

Устанавливает USB Data Role коннектора для текущей сессии. Если коннектор
не подключён или не поддерживает запрошенную роль — возврат с ошибкой.
Сбрасывается при PPM_RESET, power cycle или disconnect-е партнёра. Может
вернуть ошибку при: partner reject swap, hard reset во время выполнения,
PPM Policy Conflict.

#### CONTROL

| Offset | Field             | Size | Описание                                                                                                      |
|-------:|:------------------|-----:|:--------------------------------------------------------------------------------------------------------------|
| 0      | Command           | 8    | = `0x09`                                                                                                      |
| 8      | Data Length       | 8    | 0                                                                                                             |
| 16     | Connector Number  | 7    | Номер коннектора. 0 нелегален.                                                                                |
| 23     | USB Operation Role| 3    | Битмап (см. ниже). Установка bit0+bit1 одновременно — нелегально. Команда валидна только если коннектор PD.   |
| 26     | Reserved          | 38   | 0                                                                                                             |

`USB Operation Role`:

| Bit | Значение                                                                                            |
|----:|:----------------------------------------------------------------------------------------------------|
| 0   | Инициировать swap to DFP (если ещё не DFP).                                                         |
| 1   | Инициировать swap to UFP (если ещё не UFP).                                                         |
| 2   | 1 = принимать swap-запросы от партнёра; 0 = отвергать.                                              |

#### CCI отличия

Стандартное; успешный data role swap **не** триггерит connector status change notification.

---

### 2.10 SET_PDR (0x0B) — MUST

Устанавливает Power Direction (source/sink). Если порт не подключён или
партнёр не PD-capable — fail. Сбрасывается при PPM_RESET / power cycle /
disconnect. Успешный power role swap не триггерит connector status change
notification. По умолчанию — принимать power swap-ы.

#### CONTROL

| Offset | Field                | Size | Описание                                                                                |
|-------:|:---------------------|-----:|:----------------------------------------------------------------------------------------|
| 0      | Command              | 8    | = `0x0B`                                                                                |
| 8      | Data Length          | 8    | 0                                                                                       |
| 16     | Connector Number     | 7    | Номер коннектора. 0 нелегален.                                                          |
| 23     | Power Direction Role | 3    | Битмап (см. ниже). Все 0 — нелегально.                                                  |
| 26     | Reserved             | 38   | 0                                                                                       |

`Power Direction Role`:

| Bit | Значение                                                                                |
|----:|:----------------------------------------------------------------------------------------|
| 0   | Инициировать swap to Source (если ещё не Source).                                       |
| 1   | Инициировать swap to Sink (если ещё не Sink).                                           |
| 2   | 1 = принимать power swap от партнёра; 0 = отвергать.                                    |

---

### 2.11 GET_ALTERNATE_MODES (0x0C) — Optional

Возвращает alt-modes (SVID/MID), которые умеет Connector / Cable /
Attached Device. Если запрошенное количество > поддерживаемого — вернётся
меньше байт.

#### CONTROL

| Offset | Field                    | Size | Описание                                                                            |
|-------:|:-------------------------|-----:|:------------------------------------------------------------------------------------|
| 0      | Command                  | 8    | = `0x0C`                                                                            |
| 8      | Data Length              | 8    | 0                                                                                   |
| 16     | Recipient                | 3    | 0=Connector, 1=SOP, 2=SOP', 3=SOP'', 4–7=Reserved.                                  |
| 19     | Reserved                 | 5    | 0                                                                                   |
| 24     | Connector Number         | 7    | Номер коннектора.                                                                   |
| 31     | Reserved                 | 1    | 0                                                                                   |
| 32     | Alternate Mode Offset    | 8    | Стартовый offset в списке alt-modes.                                                |
| 40     | Number of Alternate Modes| 2    | Кол-во alt-modes = (значение + 1). Максимум поля = 1, т.е. до 2 alt-mode-ов за раз. |
| 42     | Reserved                 | 22   | 0                                                                                   |

#### CCI отличия

- `Data Length` = число байт в MESSAGE_IN (≤ MAX_DATA_LENGTH). 12 байт на каждый
  alt mode (SVID 16-бит + MID 32-бит = 6 байт, ×2 = 12).

#### MESSAGE_IN — GET_ALTERNATE_MODES Data

| Offset | Field   | Size | Описание                          |
|-------:|:--------|-----:|:----------------------------------|
| 0      | SVID[0] | 16   | Standard / Vendor ID.             |
| 16     | MID[0]  | 32   | Mode ID для SVID[0].              |
| 48     | SVID[1] | 16   | SVID[1] (если запрошен).          |
| 64     | MID[1]  | 32   | MID[1] (если запрошен).           |

---

### 2.12 GET_CAM_SUPPORTED (0x0D) — Optional

Возвращает alt-modes, которые **сейчас** доступны на коннекторе. Это
подмножество всех возможных (некоторые могут быть заняты другим
коннектором). Возвращается как битмап: `floor((N+7)/8)` байт, по биту на
alt-mode в том же порядке, как их вернул GET_ALTERNATE_MODES.

#### CONTROL

| Offset | Field            | Size | Описание         |
|-------:|:-----------------|-----:|:-----------------|
| 0      | Command          | 8    | = `0x0D`         |
| 8      | Data Length      | 8    | 0                |
| 16     | Connector Number | 7    | Номер коннектора.|
| 23     | Reserved         | 41   | 0                |

#### CCI отличия

- `Data Length = floor((N+7)/8)`, где N — общее число alt-modes.

#### MESSAGE_IN — GET_CAM_SUPPORTED Data

| Offset | Field                   | Size | Описание                                              |
|-------:|:------------------------|-----:|:------------------------------------------------------|
| 0      | bmAlternateModeSupported| N    | Битмап, бит = 1 если соответствующий alt-mode доступен|
| N      | ZeroBits                | M    | Паддинг до байта; M = 0 если N кратно 8, иначе 8−(N%8)|

---

### 2.13 GET_CURRENT_CAM (0x0E) — Optional

Возвращает массив индексов alt-mode-ов, в которых коннектор **сейчас**
работает.

#### CONTROL

| Offset | Field            | Size | Описание         |
|-------:|:-----------------|-----:|:-----------------|
| 0      | Command          | 8    | = `0x0E`         |
| 8      | Data Length      | 8    | 0                |
| 16     | Connector Number | 7    | Номер коннектора.|
| 23     | Reserved         | 41   | 0                |

#### CCI отличия

- `Data Length` = число активных alt-modes.

#### MESSAGE_IN — GET_CURRENT_CAM Data

Массив байт по числу активных alt-mode-ов:

| Offset | Field                  | Size | Описание                                                                |
|-------:|:-----------------------|-----:|:------------------------------------------------------------------------|
| 0      | Current Alternate Mode[0] | 8 | Offset в список alt-modes PPM. `0xFF` = коннектор не в alt-mode.        |
| 8      | Current Alternate Mode[1] | 8 | …если работает в нескольких alt-modes.                                  |
| …      | …                      | …    | …                                                                       |
| N·8    | Current Alternate Mode[N] | 8 | Финальный.                                                              |

---

### 2.14 SET_NEW_CAM (0x0F) — Optional

Устанавливает новый alt-mode для текущей и будущих сессий. `New CAM =
0xFF` → выйти из всех AM и не входить в новые. Не входит/выходит дважды —
не ошибка. Сбрасывается при reset / power cycle.

#### CONTROL

| Offset | Field            | Size | Описание                                                                  |
|-------:|:-----------------|-----:|:--------------------------------------------------------------------------|
| 0      | Command          | 8    | = `0x0F`                                                                  |
| 8      | Data Length      | 8    | 0                                                                         |
| 16     | Connector Number | 7    | Номер коннектора.                                                         |
| 23     | EnterOrExit      | 1    | 1 = enter, 0 = exit.                                                      |
| 24     | New CAM          | 8    | Offset в список alt-modes PPM. `0xFF` = не анонсировать ничего.           |
| 32     | AMSpecific       | 32   | AM-specific конфигурация (например, для DP — конфигурация DP внутри AM).  |

---

### 2.15 GET_PDOS (0x10) — MUST

Возвращает Source/Sink PDOs коннектора **или** партнёра. Для нашего
коннектора-source возможны три варианта:
- Maximum Supported Source Capabilities — статичные max-capabilities source-а;
- Current Supported Source Capabilities — текущие (могут быть снижены из-за power budget);
- Advertised Source Capabilities — те, что объявлены партнёру при контракт-негошен (могут быть ниже из-за кабеля).

#### CONTROL

| Offset | Field                      | Size | Описание                                                                                            |
|-------:|:---------------------------|-----:|:----------------------------------------------------------------------------------------------------|
| 0      | Command                    | 8    | = `0x10`                                                                                            |
| 8      | Data Length                | 8    | 0                                                                                                   |
| 16     | Connector Number           | 7    | Номер коннектора.                                                                                   |
| 23     | Partner PDO                | 1    | 1 = вернуть PDOs партнёра, 0 = свои (PPM).                                                          |
| 24     | PDO Offset                 | 8    | Стартовый offset. SPR: 0..7; EPR: 0..4; SPR+EPR: 0..11.                                             |
| 32     | Number of PDOs             | 2    | Кол-во PDOs = поле + 1 (1..4).                                                                      |
| 34     | Source or Sink PDOs        | 1    | 1 = Source, 0 = Sink.                                                                               |
| 35     | Source Capabilities Type   | 2    | Валидно при Partner PDO=0 и Source=1. 0=Current Supp., 1=Advertised, 2=Maximum Supp., 3=Not Used.   |
| 37     | Range                      | 2    | 0=SPR, 1=EPR, 2=SPR+EPR, 3=Not Used.                                                                |
| 39     | Reserved                   | 25   | 0                                                                                                   |

#### CCI отличия

- `Data Length = 4 × (число PDOs)`.

#### MESSAGE_IN — GET_PDOS Data (до 16 байт)

| Offset | Field   | Size | Описание                  |
|-------:|:--------|-----:|:--------------------------|
| 0      | PDO[0]  | 32   | Первый PDO.               |
| 32     | PDO[1]  | 32   | …                         |
| 64     | PDO[2]  | 32   | …                         |
| 96     | PDO[3]  | 32   | …                         |

Ошибки:
- Сумма `PDO Offset + Number of PDOs` > 7 (SPR) / 4 (EPR) / 11 (SPR+EPR) → Error + `Invalid Command Specific Parameters` в GET_ERROR_STATUS.
- Partner PDO=1, нет PD-устройства → Error + `Incompatible Connector Partner`.
- Partner PDO=1, нет партнёра вообще → Error + `CC Communication Error`.
- Запрос source-PDO у sink-only target-а → Error + `Invalid Command Specific Parameters` (для PPM) / `Incompatible Connector Partner` (для партнёра).

---

### 2.16 GET_CABLE_PROPERTY (0x11) — MUST (для нас)

Возвращает свойства кабеля на коннекторе.

#### CONTROL

| Offset | Field            | Size | Описание |
|-------:|:-----------------|-----:|:---------|
| 0      | Command          | 8    | = `0x11` |
| 8      | Data Length      | 8    | 0        |
| 16     | Connector Number | 7    | …        |
| 23     | Reserved         | 41   | 0        |

#### CCI отличия

- `Data Length = 0x05` при успехе.

#### MESSAGE_IN — GET_CABLE_PROPERTY Data (5 байт)

| Offset | Field              | Size | Описание                                                                                                   |
|-------:|:-------------------|-----:|:-----------------------------------------------------------------------------------------------------------|
| 0      | bmSpeedSupported   | 16   | bits 1:0 = Speed Exponent (0=b/s, 1=Kb/s, 2=Mb/s, 3=Gb/s); bits 15:2 = Speed Mantissa.                     |
| 16     | bCurrentCapability | 8    | Допустимый ток кабеля в единицах 50 мА.                                                                    |
| 24     | VBUSInCable        | 1    | 1 = VBUS-проводник идёт сквозь кабель.                                                                     |
| 25     | CableType          | 1    | 1 = Active, 0 = Passive.                                                                                   |
| 26     | Directionality     | 1    | 1 = направление линий конфигурируемое, 0 = фиксированное.                                                  |
| 27     | Plug End Type      | 2    | 0=Type-A, 1=Type-B, 2=Type-C, 3=Other.                                                                     |
| 29     | Mode Support       | 1    | Валидно только если Active. 1 = кабель поддерживает alt-modes (см. GET_ALTERNATE_MODES с Recipient=SOP').  |
| 30     | Cable PD Revision  | 2    | major PD-revision кабеля.                                                                                  |
| 32     | Latency            | 4    | Cable latency (см. Table 6-41 в [USBPD]).                                                                  |
| 36     | Reserved           | 28   | 0                                                                                                          |

---

### 2.17 GET_CONNECTOR_STATUS (0x12) — MUST

Главная команда для опроса состояния коннектора. После подключения
устройства `Power Reading Ready` сначала = 0; станет 1 либо после
истечения default Time To Read (без alarm-а), либо после
READ_POWER_LEVEL. На последующем GET_CONNECTOR_STATUS без
READ_POWER_LEVEL — сбрасывается в 0. LPM непрерывно обновляет
voltage/current и отдаёт последние замеры. При power state change /
reset / disconnect — average обнуляется.

#### CONTROL

| Offset | Field            | Size | Описание |
|-------:|:-----------------|-----:|:---------|
| 0      | Command          | 8    | = `0x12` |
| 8      | Data Length      | 8    | 0        |
| 16     | Connector Number | 7    | …        |
| 23     | Reserved         | 41   | 0        |

#### CCI отличия

- `Data Length = 0x13` (19 байт) при успехе.

#### MESSAGE_IN — GET_CONNECTOR_STATUS Data (19 байт = 152 бит)

| Offset | Field                                 | Size | Описание                                                                                                                                   |
|-------:|:--------------------------------------|-----:|:-------------------------------------------------------------------------------------------------------------------------------------------|
| 0      | Connector Status Change               | 16   | Битмап произошедших изменений (см. ниже). Сбрасывается при чтении GET_CONNECTOR_STATUS.                                                    |
| 16     | Power Operation Mode                  | 3    | 0=Reserved, 1=USB Default, 2=BC, 3=PD, 4=Type-C 1.5A, 5=Type-C 3A, 6=Type-C 5A, 7=Reserved. Валидно при Connect Status=1.                  |
| 19     | Connect Status                        | 1    | 1 = подключено что-то.                                                                                                                     |
| 20     | Power Direction                       | 1    | 0=consumer, 1=provider. Валидно при Connect Status=1.                                                                                      |
| 21     | Connector Partner Flags               | 8    | bit0 USB(2/3), bit1 Alt Mode, bit2 USB4 Gen3, bit3 USB4 Gen4, остальное reserved.                                                          |
| 29     | Connector Partner Type                | 3    | 0=Reserved, 1=DFP attached, 2=UFP attached, 3=Powered cable/no UFP, 4=Powered cable/UFP, 5=Debug accessory, 6=Audio adapter, 7=Reserved.   |
| 32     | Request Data Object (O)               | 32   | Текущий PD-RDO; валидно при Connect Status=1, Power Op Mode=PD, и если PPM объявил фичу.                                                   |
| 64     | Battery Charging Capability Status    | 2    | 0=Not charging, 1=Nominal, 2=Slow, 3=Very slow. Валидно если коннектор sink.                                                               |
| 66     | Provider Capabilities Limited Reason  | 4    | Битмап (см. ниже). Валидно если provider.                                                                                                  |
| 70     | bcdPDVersion Operation Mode           | 16   | BCD-версия PD текущего Explicit Contract. Валидно если PD и фича объявлена.                                                                |
| 86     | Orientation                           | 1    | 0=direct, 1=flipped.                                                                                                                       |
| 87     | Sink Path Status                      | 1    | 1=sink path enabled. PPM может менять без OPM — тогда выставится `Sink Path Status Change`.                                                |
| 88     | Reverse Current Protection Status     | 1    | Валидно если поддерживается. 1 = RCP сработала.                                                                                            |
| 89     | Power Reading Ready                   | 1    | 1 = ниже валидные замеры (см. семантику в описании выше).                                                                                  |
| 90     | Scale (current)                       | 3    | Разрешение тока в шагах 5 мА (1b=5mA, 101b=25mA).                                                                                          |
| 93     | Peak Current                          | 16   | Пиковое значение тока (MSB-пад 0 если ADC <16-бит).                                                                                        |
| 109    | Average Current                       | 16   | Скользящее среднее за интервал (default 100 мс с шагом 5 мс или из READ_POWER_LEVEL).                                                      |
| 125    | Scale (voltage)                       | 4    | Разрешение напряжения в шагах 5 мВ (010b=10mV, 0101b=25mV, 1010b=50mV).                                                                    |
| 129    | Voltage Reading                       | 16   | Последний замер VBUS (MSB-пад 0).                                                                                                          |
| 145    | Reserved                              | 7    | 0                                                                                                                                          |

`Connector Status Change` биты (Table 6-44):

| Bit | Описание                                                                                                                  |
|----:|:--------------------------------------------------------------------------------------------------------------------------|
| 0   | Reserved (0)                                                                                                              |
| 1   | External Supply Change — OPM использует GET_PDO для актуальных данных.                                                    |
| 2   | Power Operation Mode Change — поле Power Operation Mode валидно.                                                          |
| 3   | Attention — LPM получил Attention от партнёра.                                                                            |
| 4   | Reserved (0)                                                                                                              |
| 5   | Supported Provider Capabilities Change — обновить через GET_PDOS.                                                         |
| 6   | Negotiated Power Level Change — RDO в STATUS отражает новый уровень.                                                      |
| 7   | PD Reset Complete — PPM завершил PD Hard Reset, инициированный партнёром.                                                 |
| 8   | Supported CAM Change — обновить GET_CAM_SUPPORTED.                                                                        |
| 9   | Battery Charging Status Change.                                                                                           |
| 10  | Reserved (0)                                                                                                              |
| 11  | Connector Partner Changed — изменился тип/флаги партнёра.                                                                 |
| 12  | Power Direction Changed — PPM/партнёр сделали PR swap или это побочный эффект DR swap.                                    |
| 13  | Sink Path Status Change.                                                                                                  |
| 14  | Connect Change — устройство подключено/отключено. Сбрасывается при чтении GET_CONNECTOR_STATUS.                           |
| 15  | Error.                                                                                                                    |

`Provider Capabilities Limited Reason` биты (Table 6-45):

| Bit | Описание                                                                          |
|----:|:----------------------------------------------------------------------------------|
| 0   | Power Budget Lowered — отключение от внешнего питания.                            |
| 1   | Reaching Power Budget Limit — слишком много подключённых sink-ов.                 |
| 2–3 | Reserved (0).                                                                     |

---

### 2.18 GET_ERROR_STATUS (0x13) — MUST

Возвращает детали последней ошибки. PPM сохраняет Error Status до тех пор,
пока OPM не подтвердит обработку (через ACK_CC_CI) или до PPM_RESET.

#### CONTROL

| Offset | Field            | Size | Описание |
|-------:|:-----------------|-----:|:---------|
| 0      | Command          | 8    | = `0x13` |
| 8      | Data Length      | 8    | 0        |
| 16     | Connector Number | 7    | …        |
| 23     | Reserved         | 41   | 0        |

#### CCI отличия

- `Data Length = GET_ERROR_STATUS_DATA_LENGTH` (≤ 16 байт) при успехе.

#### MESSAGE_IN — GET_ERROR_STATUS Data (16 байт)

| Offset | Field             | Size | Описание                                                |
|-------:|:------------------|-----:|:--------------------------------------------------------|
| 0      | Error Information | 16   | Битмап (см. ниже).                                      |
| 16     | Vendor Defined    | N    | Vendor-specific.                                        |
| 16+N   | Reserved          | 112−N| 0                                                       |

`Error Information` биты:

| Bit | Описание                                              |
|----:|:------------------------------------------------------|
| 0   | Unrecognized command                                  |
| 1   | Non-existent connector number                         |
| 2   | Invalid command specific parameters                   |
| 3   | Incompatible connector partner                        |
| 4   | CC communication error                                |
| 5   | Command unsuccessful due to dead battery condition    |
| 6   | Contract negotiation failure                          |
| 7   | Overcurrent                                           |
| 8   | Undefined                                             |
| 9   | Port partner rejected swap                            |
| 10  | Hard Reset                                            |
| 11  | PPM Policy Conflict                                   |
| 12  | Swap Rejected                                         |
| 13  | Reverse Current Protection                            |
| 14  | Set Sink Path Rejected                                |
| 15  | Reserved (0)                                          |

---

### 2.19 SET_POWER_LEVEL (0x14) — MUST

Устанавливает максимальный negotiable power level (sink или source) для
текущей сессии. Если нет активного соединения / power direction не
совпадает — ошибка `Invalid Command Specific Parameters`. По умолчанию PPM
сам решает максимум. После команды PPM сразу подтверждает завершение и
*потом* renegotiates контракт (если нужно). Если новый контракт отличается
от старого по power level — будет Negotiated Power Level Change notification
(если включена).

Сбрасывается при: PPM reset, power cycle, connector reset, detach.

Policy conflict (PPM не может поддержать запрошенный уровень) → ошибка
`PPM Policy Conflict`.

#### CONTROL

| Offset | Field             | Size | Описание                                                                                                                                                             |
|-------:|:------------------|-----:|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0      | Command           | 8    | = `0x14`                                                                                                                                                             |
| 8      | Data Length       | 8    | 0                                                                                                                                                                    |
| 16     | Connector Number  | 7    | 0 = USB PD Max Power применяется ко всему пулу портов суммарно.                                                                                                      |
| 23     | Source or Sink    | 1    | 1 = source, 0 = sink.                                                                                                                                                |
| 24     | USB PD Max Power  | 8    | Max negotiable level в 0.5W (LsbControl=0) или 1.0W (LsbControl=1). 0 = PPM сам определяет по voltage/current. При implicit/не-PD соединении LPM может игнорировать. |
| 32     | USB Type-C Current| 3    | Max current (только для Connector ≠ 0): 0=PPM default, 1=3A, 2=1.5A, 3=Type-C Default.                                                                               |
| 35     | LsbControl        | 1    | 0 = 0.5W/20mV, 1 = 1.0W/25mV (для следующих полей).                                                                                                                  |
| 36     | Operating Current | 7    | Шаги по 50 мА. ≤ 5 А; иначе Error.                                                                                                                                   |
| 43     | Reserved          | 2    | 0                                                                                                                                                                    |
| 45     | Output Voltage    | 12   | LsbControl=0: 20 мВ steps, ≤ 21V. LsbControl=1: 25 мВ steps, eff. LSB 100 мВ, ≤ 48V. Только для AVS/PPS Request Message; 0 если AVS/PPS не поддержаны.               |
| 57     | Reserved          | 7    | 0                                                                                                                                                                    |

---

### 2.20 GET_PD_MESSAGE (0x15) — Optional

Получить PD-сообщение (response) от коннектора, port partner-а или cable
plug-а. Два режима:
- Recipient = 0 (Connector): LPM возвращает то, что *сам бы отправил* в ответ на соответствующий request от партнёра.
- Recipient = 1/2/3 (SOP/SOP'/SOP''): LPM физически шлёт request партнёру/кабелю и возвращает ответ. При Message Offset=0 — отправка новой; при Offset≠0 — отдаёт кэш. Если кэша нет — Error + `CC Communication Error`.

#### CONTROL

| Offset | Field                 | Size | Описание                                                                                                                               |
|-------:|:----------------------|-----:|:---------------------------------------------------------------------------------------------------------------------------------------|
| 0      | Command               | 8    | = `0x15`                                                                                                                               |
| 8      | Data Length           | 8    | 0                                                                                                                                      |
| 16     | Connector Number      | 7    | …                                                                                                                                      |
| 23     | Recipient             | 3    | 0=Connector, 1=SOP, 2=SOP', 3=SOP'', 4–7=Reserved.                                                                                     |
| 26     | Message Offset        | 8    | Стартовый offset в байтах. Для Extended Message: < Data Size в Ext header. Для Data Message/Structured VDM: multiple-of-4 < 4·Nbjects. |
| 34     | Number of Bytes       | 8    | Сколько байт вернуть. ≤ MAX_DATA_LENGTH; для Data Message — ненулевое кратное 4.                                                       |
| 42     | Response Message Type | 6    | Какой PD-response (см. ниже).                                                                                                          |
| 48     | Reserved              | 16   | 0                                                                                                                                      |

`Response Message Type`:

| Value | PD Response Message            | Соответствующий request    |
|------:|:-------------------------------|:---------------------------|
| 0     | Sink_Capabilities_Extended     | Get_Sink_Cap_Extended      |
| 1     | Source_Capabilities_Extended   | Get_Source_Cap_Extended    |
| 2     | Battery_Capabilities           | Get_Battery_Cap            |
| 3     | Battery_Status                 | Get_Battery_Status         |
| 4     | Discover Identity Response (ACK/NAK/BUSY, structured VDM) | Discover Identity Request |
| 5     | Revision (data message)        | Get_Revision               |
| 6–63  | Reserved                       | —                          |

#### CCI отличия

- `Data Length` = реально возвращённых байт; может быть < запрошенного.

#### MESSAGE_IN

PPM возвращает **только** Data Block extended-message-а / Data Object-ы /
VDM Header+Objects. PD Message Header / Extended Header **не**
включаются. Chunked Extended-сообщения — merged до возврата.

---

### 2.21 GET_ATTENTION_VDO (0x16) — Optional

Получить VDO от партнёра, который прислал ATTENTION.

#### CONTROL

| Offset | Field            | Size | Описание |
|-------:|:-----------------|-----:|:---------|
| 0      | Command          | 8    | = `0x16` |
| 8      | Data Length      | 8    | 0        |
| 16     | Connector Number | 7    | …        |
| 23     | Reserved         | 41   | 0        |

#### CCI отличия

- `Data Length` = число байт в MESSAGE_IN (≤ 33).
- `Not Supported Indicator = 1` если коннектор команду не поддерживает.

#### MESSAGE_IN — GET_ATTENTION_VDO Data

| Offset | Field             | Size | Описание                                                                                                          |
|-------:|:------------------|-----:|:------------------------------------------------------------------------------------------------------------------|
| 0      | Alt Mode Index    | 16   | Индекс alt-mode-а, в котором сейчас коннектор. 0xFF = не в alt-mode.                                              |
| 16     | Number of VDOs    | 3    | 1 если VDO есть, 0 если нет.                                                                                      |
| 19     | Reserved          | 2    | 0                                                                                                                 |
| 21     | Sequence Number   | 3    | Инкрементируется на каждом возврате; роллится через 0.                                                            |
| 24     | VDM Header        | 32   | VDM Header.                                                                                                       |
| 56     | VDO               | 32   | Сам VDO.                                                                                                          |

---

### 2.22 GET_CAM_CS (0x18) — Optional

Получить configuration & status конкретного active alt-mode (например, DP
status). Используется в паре с GET_CURRENT_CAM.

#### CONTROL

| Offset | Field            | Size | Описание                                                  |
|-------:|:-----------------|-----:|:----------------------------------------------------------|
| 0      | Command          | 8    | = `0x18`                                                  |
| 8      | Data Length      | 8    | 0                                                         |
| 16     | Connector Number | 7    | …                                                         |
| 23     | Reserved         | 1    | 0                                                         |
| 24     | Current Alt Mode | 8    | Индекс i из массива GET_CURRENT_CAM.                      |

#### CCI отличия

- `Data Length` = байт в MESSAGE_IN, иначе 0.
- `Not Supported Indicator = 1` при отсутствии поддержки.

#### MESSAGE_IN — GET_CAM_CS Data (переменная длина)

| Offset      | Field                 | Size | Описание                                                                                                |
|------------:|:----------------------|-----:|:--------------------------------------------------------------------------------------------------------|
| 0           | Current Alternate Mode| 8    | Индекс активного alt-mode.                                                                              |
| 8           | Status                | 32   | Status alt-mode-а; формат специфичен AM (для DP — VESA DisplayPort Alt Mode §5-3/5-4). 0 если не задано.|
| 40          | Number of VDOs        | 8    | N — кол-во возвращаемых VDO.                                                                            |
| 48 + N·32   | VDO[N]                | 32   | N штук VDO.                                                                                             |

---

### 2.23 LPM_FW_UPDATE_REQUEST (0x19) — Optional

FW-update через механизм [PDFU]. Connector Number = 0x7F → broadcast на
все LPM (только для фабрики). Команда **chunked**: длинное сообщение
разбивается на чанки по 255 байт; синхронизация через Data Index.

#### CONTROL

| Offset | Field             | Size | Описание                                                                                                   |
|-------:|:------------------|-----:|:-----------------------------------------------------------------------------------------------------------|
| 0      | Command           | 8    | = `0x19`                                                                                                   |
| 8      | Data Length       | 8    | OPM→LPM/PortPartner/CablePlug: длина FW-чанка в MESSAGE_OUT. From-PortPartner: максимум для MESSAGE_IN.    |
| 16     | Connector Number  | 7    | Номер коннектора; `0x7F` = broadcast.                                                                      |
| 23     | Direction         | 2    | 0=OPM→LPM, 1=OPM→Port Partner, 2=OPM→Cable Plug, 3=Update from Port Partner.                               | 
| 25     | FW Update Request | 8    | [PDFU] request types. Валидно при Direction = 1/2.                                                         |
| 33     | Data Index        | 7    | Индекс чанка (0..0x7F с роллом).                                                                           |
| 40     | End of Message    | 1    | 1 = последний/единственный чанк.                                                                           |
| 41     | Reserved          | 23   | 0                                                                                                          |

#### CCI отличия

- `End of Message Indicator` (бит 0) переиспользован для multi-chunk-а.
- `Data Index` (биты 16..22) — индекс чанка ответа.
- `FW Update Request Indicator = 1` если это асинхронный запрос от партнёра.
- `Not Supported Indicator = 1` если партнёр/LPM не поддерживает.

#### MESSAGE_OUT (когда OPM — инициатор)

| Offset | Field        | Size       | Описание                                          |
|-------:|:-------------|-----------:|:--------------------------------------------------|
| 0      | Data Payload | 0..N bytes | Полезные данные. N ≤ 255, длина = Data Length.    |

#### MESSAGE_IN (когда запрос от партнёра, Direction=3)

| Offset | Field             | Size      | Описание                                  |
|-------:|:------------------|----------:|:------------------------------------------|
| 0      | FW Update Request | 1 byte    | [PDFU] request type.                      |
| 1      | Data Payload      | 1..254 B  | Полезные данные.                          |

Семантика Data Index — синхронизация: если Data Index в CCI ≠ Data Index в
команде, OPM/PPM считает команду failed (можно retry или reset).

---

### 2.24 SECURITY_REQUEST (0x1A) — Optional

Authentication по [USBAUTH]. Тоже **chunked**, по той же схеме.

#### CONTROL

| Offset | Field                  | Size | Описание                                                                                                            |
|-------:|:-----------------------|-----:|:--------------------------------------------------------------------------------------------------------------------|
| 0      | Command                | 8    | = `0x1A`                                                                                                            |
| 8      | Data Length            | 8    | OPM→LPM/PortPartner/CablePlug: длина чанка MESSAGE_OUT. From-PortPartner: максимум для MESSAGE_IN.                  |
| 16     | Connector Number       | 7    | …                                                                                                                   |
| 23     | Direction              | 2    | 0=OPM→LPM, 1=OPM→Port Partner, 2=OPM→Cable Plug, 3=Request from Port Partner.                                       |
| 25     | Security Request       | 1    | 0 = аутентификация LPM-а; 1 = запрос идёт к Port Partner.                                                           |
| 26     | Auth Protocol Revision | 8    | [USBAUTH] версия. Валидно при OPM-инициированном запросе.                                                           |
| 34     | Authentication Message | 8    | [USBAUTH] request type. Валидно при OPM-инициированном запросе.                                                     |
| 42     | Data Index             | 7    | Индекс чанка (0..0x7F).                                                                                             |
| 49     | End of Message         | 1    | 1 = последний чанк.                                                                                                 |
| 50     | Reserved               | 14   | 0                                                                                                                   |

#### CCI отличия

- `End of Message Indicator` — multi-chunk.
- `Data Index` — индекс чанка ответа.
- `Security Request Indicator = 1` если запрос от партнёра.
- `Not Supported Indicator = 1` если партнёр не поддерживает.

#### MESSAGE_OUT (OPM-инициатор)

| Offset | Field        | Size       | Описание                       |
|-------:|:-------------|-----------:|:-------------------------------|
| 0      | Data Payload | 0..N bytes | Полезные данные (≤ 255).       |

#### MESSAGE_IN (запрос от партнёра)

| Offset | Field           | Size       | Описание                              |
|-------:|:----------------|-----------:|:--------------------------------------|
| 0      | Security Header | 4 bytes    | См. [USBAUTH].                        |
| 4      | Data Payload    | 4..251 B   | Полезные данные = Data Length.        |

Security Request и FW Update Request Indicator **не** могут быть = 1 одновременно.

---

### 2.25 SET_RETIMER_MODE (0x1B) — Optional

Установить functional mode re-timer-а (для FW update / EV-DV).

#### CONTROL

| Offset | Field            | Size | Описание                                                                                                                                                                                                  |
|-------:|:-----------------|-----:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0      | Command          | 8    | = `0x1B`                                                                                                                                                                                                  |
| 8      | Data Length      | 8    | 0 если State≠Flashing-w-Payload; иначе длина FW-чанка.                                                                                                                                                    |
| 16     | Connector Number | 7    | …                                                                                                                                                                                                         |
| 23     | Re-timer Number  | 2    | 0=Reserved, 1=facing connector (default), 2=facing SoC, 3=оба.                                                                                                                                            |
| 25     | State            | 3    | 0=Off, 1=On/Force Power, 2=Low Power, 3=Compliance, 4=Flashing, 5=Flashing-w-Payload, 6–7=Reserved.                                                                                                       |
| 28     | Functional Mode  | 4    | Валидно при State=1. 0=USB3.2 Gen1, 1=USB3.2 Gen2, 2=USB3.2 2x2, 3=USB4 Gen2, 4=USB4 Gen3, 5=USB4 Gen4, 6=TBT3, 6=TBT4, 7=DP1.4, 8=DP2.0, 9=MFD USB3.2+DP, 10=Debug accessory, 11–15=Reserved.            |
| 32     | DP Source-Sink   | 1    | 0 = DP source, 1 = DP sink.                                                                                                                                                                               |
| 33     | Gain             | 8    | Amplification gain (валидно при State=1).                                                                                                                                                                 |
| 41     | Orientation      | 1    | 0=direct, 1=flipped.                                                                                                                                                                                      |
| 42     | Reserved         | 4    | 0                                                                                                                                                                                                         |
| 46     | Data Index       | 7    | Только при State=Flashing-w-Payload. Иначе 0.                                                                                                                                                             |
| 53     | End of Message   | 1    | Только при State=Flashing-w-Payload.                                                                                                                                                                      |
| 54     | Reserved         | 8    | 0                                                                                                                                                                                                         |

(В спецификации Functional Mode имеется дубль значения 6 для TBT3/TBT4 — это особенность исходной таблицы; при имплементации сверяться с актуальной редакцией.)

#### CCI отличия

- `Data Index` биты 16..22 — индекс обработанного чанка (только при Flashing-w-Payload).
- `Not Supported Indicator = 1` если LPM не поддерживает.

---

### 2.26 SET_SINK_PATH (0x1C) — MUST

Включить/выключить sink path (приём питания от партнёра). Включение
sink path при провайдер-моде или без партнёра → Error.

#### CONTROL

| Offset | Field            | Size | Описание                                          |
|-------:|:-----------------|-----:|:--------------------------------------------------|
| 0      | Command          | 8    | = `0x1C`                                          |
| 8      | Data Length      | 8    | 0                                                 |
| 16     | Connector Number | 7    | …; 0 нелегален.                                   |
| 23     | Sink Path        | 1    | 1 = enable, 0 = disable.                          |
| 24     | Reserved         | 40   | 0                                                 |

#### CCI отличия

- `Error Indicator = 1` если LPM в source mode (детали — через GET_ERROR_STATUS, error `Set Sink Path Rejected`).

---

### 2.27 CHUNKING_SUPPORT (0x1F) — MUST

Опрос максимального chunk size для MESSAGE_IN/OUT. Если не
поддерживается — `Not Supported Indicator = 1`, и тогда OPM/LPM обязаны
поддерживать чанкование вплоть до полного размера MESSAGE_IN/OUT текущей
UCSI-spec (Table 4-1, т.е. 255 байт). Рекомендуется размер чанка = размеру
MESSAGE_IN/OUT текущей версии или не менее 16 байт (как в UCSI 1.2).

#### CONTROL

| Offset | Field            | Size | Описание                                                                                       |
|-------:|:-----------------|-----:|:-----------------------------------------------------------------------------------------------|
| 0      | Command          | 8    | = `0x1F`                                                                                       |
| 8      | Data Length      | 8    | 0                                                                                              |
| 16     | Connector Number | 7    | 0 = broadcast на все LPM-ы; ответ — минимальный chunk size.                                    |
| 23     | Reserved         | 41   | 0                                                                                              |

#### CCI отличия

- `Data Length = 0x01` при успехе.
- `Not Supported Indicator = 1` если chunking не поддерживается.

#### MESSAGE_IN — CHUNKING_SUPPORT Data

| Offset | Field         | Size | Описание                                  |
|-------:|:--------------|-----:|:------------------------------------------|
| 0      | Chunking Size | 8    | Максимальный chunk size в байтах.         |

---

### 2.28 SET_PDOS (0x1D) — MUST

Полная перезапись PDOs (source или sink, SPR или EPR) на коннекторе.
Поддерживает atomic-обновление через `End of Message`: пока EoM=0, LPM
**не** заключает Explicit Contract; контракт устанавливается при получении
команды с EoM=1. Если OPM прислал EoM=0 и не прислал следующую команду в
`SENDER_RESPONSE_TIMEOUT` — LPM может продолжить с накопленными PDO.

`Connector Number = 0` → broadcast (PPM сам аггрегирует статусы).

Команда может быть chunked: `Data Index` синхронизирует чанки; при ошибке
OPM может retry с тем же Data Index или сбросить серию с Data Index=0.
Рекомендуется укладывать все PDO в один чанк.

`SET_PDOS` **отменяет** эффект `SET_POWER_LEVEL` — последний работает
лишь в рамках доступных PDO-уровней.

#### CONTROL

| Offset | Field                          | Size | Описание                                                                                                                       |
|-------:|:-------------------------------|-----:|:-------------------------------------------------------------------------------------------------------------------------------|
| 0      | Command                        | 8    | = `0x1D`                                                                                                                       |
| 8      | Data Length                    | 8    | (число PDO в текущем чанке) × 4.                                                                                               |
| 16     | Connector Number               | 7    | …; 0 = broadcast.                                                                                                              |
| 23     | Reserved                       | 3    | 0                                                                                                                              |
| 26     | Source or Sink Capabilities PDO| 1    | 1 = source, 0 = sink.                                                                                                          |
| 27     | Number of PDOs                 | 4    | Общее число PDO в серии (не больше, чем допускает PD-spec для диапазона; SPR=7). Валидно в первом чанке; в последующих optional.|
| 31     | Data Index                     | 7    | Индекс чанка (роллится через 0).                                                                                               |
| 38     | End of Message                 | 1    | 1 = конец серии (старт переговоров); 0 = ждём ещё чанки.                                                                       |
| 39     | Reserved                       | 25   | 0                                                                                                                              |

#### MESSAGE_OUT — SET_PDOS Data

| Offset | Field   | Size | Описание           |
|-------:|:--------|-----:|:-------------------|
| 0      | PDO[0]  | 32   | Первый PDO в чанке.|
| 32     | PDO[1]  | 32   | …                  |
| …      | …       | …    | …                  |
| N·32   | PDO[N]  | 32   | …                  |

#### CCI отличия

- `Data Length = 0`.
- `Data Index` — биты 16..22.
- `Not Supported Indicator = 1` если LPM не поддерживает.
- `Error Indicator = 1` если число PDO превышает разрешённое спецификацией.

---

### 2.29 VENDOR_DEFINED (0x20) — Optional

Vendor-specific обмен между OPM, PPM и LPM. Каждый VDC обязан содержать
VendorID для уникальности.

#### CONTROL

| Offset | Field                  | Size | Описание                                                                       |
|-------:|:-----------------------|-----:|:-------------------------------------------------------------------------------|
| 0      | Command                | 8    | = `0x20`                                                                       |
| 8      | Data Length            | 8    | Длина MESSAGE_OUT (если есть).                                                 |
| 16     | Connector Number       | 7    | 0 = команда для PPM; иначе — для конкретного коннектора.                       |
| 23     | Vendor Defined Command | 5    | Вендорский код команды.                                                        |
| 28     | VDC Structure Version  | 4    | На текущей ревизии = 1.                                                        |
| 32     | Vendor ID              | 16   | VID requester-а.                                                               |
| 48     | Product ID             | 16   | PID requester-а.                                                               |

#### MESSAGE_IN — VDM Data Structure (если есть данные)

| Offset | Field        | Size | Описание           |
|-------:|:-------------|-----:|:-------------------|
| 0      | Vendor ID    | 16   | VID.               |
| 16     | Product ID   | 16   | PID.               |
| 32     | Message Body | …    | Vendor-specific.   |

MESSAGE_OUT — vendor-specific.

---

### 2.30 GET_LPM_PPM_INFO (0x22) — MUST

HW/FW info PPM-а или LPM-а.

#### CONTROL

| Offset | Field            | Size | Описание                                                              |
|-------:|:-----------------|-----:|:----------------------------------------------------------------------|
| 0      | Command          | 8    | = `0x22`                                                              |
| 8      | Data Length      | 8    | 0                                                                     |
| 16     | Connector Number | 7    | 0 = команда адресована PPM-у; иначе — LPM-у конкретного коннектора.   |
| 23     | Reserved         | 41   | 0                                                                     |

#### CCI отличия

- `Data Length = 0x10` (16 байт) при успехе.

#### MESSAGE_IN — GET_LPM_PPM_INFO Data (16 байт)

| Offset | Field            | Size | Описание                                       |
|-------:|:-----------------|-----:|:-----------------------------------------------|
| 0      | VID              | 16   | Vendor ID.                                     |
| 16     | PID              | 16   | Product ID.                                    |
| 32     | XID              | 32   | USB-IF assigned XID продукта.                  |
| 64     | FW Version Upper | 32   | FW version.                                    |
| 96     | FW Version Lower | 32   | Sub FW version.                                |
| 32¹    | HW Version       | 32   | HW version. (¹ В исходной Table 6-82 указано смещение 32 — это, скорее всего, опечатка спеки; ожидаемое значение — 128.) |

> **Замечание.** В оригинальной таблице 6-82 спецификации поле HW Version
> повторно указано на offset 32, что противоречит ширине предыдущих полей
> (totally 128 бит до HW Version). При реализации сверять с актуальной
> errata UCSI.

---

### 2.31 SET_USB (0x21) — Conditional Normative

Включает/выключает USB3 и USB4 modes для текущей и будущих сессий. Если
выключаем USB.X-режим, а текущее соединение в нём — LPM делает Data Reset.
Если включаем, а текущее соединение не в нём — тоже Data Reset.
Сбрасывается в default при LPM reset.

#### CONTROL

| Offset | Field            | Size | Описание                                          |
|-------:|:-----------------|-----:|:--------------------------------------------------|
| 0      | Command          | 8    | = `0x21`                                          |
| 8      | Data Length      | 8    | 0                                                 |
| 16     | Connector Number | 7    | …; 0 нелегален.                                   |
| 23     | USB3 Enable      | 1    | 0=disable, 1=enable USB3.                         |
| 24     | USB4 Enable      | 1    | 0=disable, 1=enable USB4.                         |
| 25     | Reserved         | 4    | 0                                                 |
| 29     | EUDO             | 32   | Enter USB Data Object.                            |
| 61     | Reserved         | 3    | 0                                                 |

---

### 2.32 READ_POWER_LEVEL (0x1E) — MUST

OPM запрашивает peak и average power. Только для source-режима с активным
соединением; иначе Error `Invalid Command Specific Parameters`. После
команды PPM сразу подтверждает завершение, измерения готовятся
асинхронно. Когда LPM готов отдать данные — выставит Connector Change
Indicator (соответствующий бит status change), а сами данные — в
MESSAGE_IN следующего GET_CONNECTOR_STATUS.

#### CONTROL

| Offset | Field                       | Size | Описание                                                                          |
|-------:|:----------------------------|-----:|:----------------------------------------------------------------------------------|
| 0      | Command                     | 8    | = `0x1E`                                                                          |
| 8      | Data Length                 | 8    | 0                                                                                 |
| 16     | Connector Number            | 7    | …                                                                                 |
| 23     | Time to Read Power          | 5    | Окно измерения. 1 LSB = 100 мс (0 → 100 мс, 1 → 200 мс, …).                       |
| 28     | Reserved                    | 3    | 0                                                                                 |
| 31     | Time Interval between reads | 2    | Шаг внутри окна. 1 LSB = 5 мс.                                                    |
| 33     | Reserved                    | 31   | 0                                                                                 |

#### CCI отличия

- Стандартное завершение (Command Completed=1, без MESSAGE_IN).
- Когда измерения готовы — приходит Connector Change Indicator; данные читаются через GET_CONNECTOR_STATUS (`Power Reading Ready=1`).

---

## 3. Сноски по реализации

- **Команды, на которые мы обязаны отвечать `Not Supported`** (а не молчать или возвращать произвольное): согласно §5.2/§6.6 UCSI, при получении неизвестной/неподдержанной команды PPM завершает её с `Command Completed=1, Not Supported Indicator=1`. Это — основной механизм безопасного "выключения" опциональных фич.
- **Busy / тайминги**: если PPM не успевает ответить за `MIN_TIME_TO_RESPOND_WITH_BUSY` (190 мс), он обязан выставить `Busy Indicator=1`. Внутри busy — никакие другие биты CCI не валидны. По окончанию работы — обычный Command Completed.
- **Connector Number = 0**: значение `0` имеет разную семантику у разных команд: где-то нелегально (SET_CCOM, SET_UOR, SET_PDR, SET_USB, SET_SINK_PATH), где-то означает broadcast/PPM (CHUNKING_SUPPORT, SET_PDOS, GET_LPM_PPM_INFO, VENDOR_DEFINED, SET_POWER_LEVEL — там 0 значит "пул"). Перед маршрутизацией внутри библиотеки эту семантику нужно учитывать.
- **`0x7F` Connector Number** — broadcast для FW_UPDATE_REQUEST (factory-only).
- **Chunked-команды** (FW Update, Security Request, SET_PDOS, опционально SET_RETIMER_MODE с Flashing-w-Payload): синхронизация через `Data Index`; mismatch → команда failed на стороне OPM/PPM.
- **`SENDER_RESPONSE_TIMEOUT` = 60 мс** для inter-chunk gap в SET_PDOS.

---

## 4. Предлагаемый скоуп v1 библиотеки (для обсуждения)

Минимально достаточный для Linux/Windows-драйвера UCSI 3.0 поверх одного
коннектора FUSB302:

**Обязательно (✅)** — 18 команд:

`PPM_RESET`, `CANCEL`, `CONNECTOR_RESET`, `ACK_CC_CI`,
`SET_NOTIFICATION_ENABLE`, `GET_CAPABILITY`, `GET_CONNECTOR_CAPABILITY`,
`SET_CCOM` (де-факто для DRP), `SET_UOR`, `SET_PDR`, `GET_PDOS`,
`GET_CABLE_PROPERTY`, `GET_CONNECTOR_STATUS`, `GET_ERROR_STATUS`,
`SET_POWER_LEVEL`, `SET_SINK_PATH`, `CHUNKING_SUPPORT`, `SET_PDOS`,
`GET_LPM_PPM_INFO`, `READ_POWER_LEVEL`.

(Это и есть все Normative-команды Table 6-87 для PPM, плюс SET_CCOM как
обязательный для DRP-LPM, плюс ACK_CC_CI как часть OPM-LPM-конфигурации.)

**Под фичу (⚠️)** — добавляем когда нужно alt-modes / PD-сообщения:

`GET_ALTERNATE_MODES`, `GET_CAM_SUPPORTED`, `GET_CURRENT_CAM`,
`SET_NEW_CAM`, `GET_CAM_CS` — пакет «Alternate Modes»; включается, если
объявляем `bNumAltModes>0`.

`GET_PD_MESSAGE`, `GET_ATTENTION_VDO` — пакет «extended PD интроспекция»;
включается, если выставляем соответствующие биты `bmOptionalFeatures`.

`SET_USB` — если хотим управлять USB3/USB4 режимом (для нашего FUSB302
пока не релевантно — он не PHY USB3/USB4).

**Не делаем в v1 (❌)** — на запрос отвечаем `Not Supported`:

`LPM_FW_UPDATE_REQUEST`, `SECURITY_REQUEST`, `SET_RETIMER_MODE`,
`VENDOR_DEFINED`.

`SET_PDM (0x0A)` — obsolete с UCSI 2.x, не реализовываем.

---

## 5. Связанные документы

- [`USB Type-C Connector System Software Interface UCSI Revision_3.0  CLEAN.pdf`](../docs/USB%20Type-C%20Connector%20System%20Software%20Interface%20UCSI%20Revision_3.0%20%20CLEAN.pdf) — первоисточник.
- [`USB_PD_R3.2_V1.2_2026_03_17.pdf`](../docs/USB_PD_R3.2_V1.2_2026_03_17.pdf) — USB PD, на которую UCSI ссылается ([USBPD]).
- `[USBTYPEC]` — USB Type-C Cable and Connector Specification (не в репозитории).
- `[USBAUTH]` — USB Authentication (не в репозитории, нужно если делаем Security).
- `[PDFU]` — USB PD Firmware Update (не в репозитории, нужно если делаем FW Update).
