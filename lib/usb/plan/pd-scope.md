# USB PD — Scope библиотеки (DRP, без VDM, до 60 Вт)

Документ фиксирует, какие PD-сообщения, PDO/RDO, таймеры и счётчики
библиотека реализует, а какие — нет. Источник — *USB Power Delivery
Specification Revision 3.2 Version 1.2, Mar 2026* (PDF в
[`lib/usb/docs/`](../docs/)). Целевая ревизия в эфире — **PD 3.0** (с
обратной совместимостью к PD 2.0); более новые PD 3.1/3.2 фичи (EPR,
AVS, новые messages) не реализуем.

Связанные документы:
- [`commands.md`](commands.md) — внешний контракт UCSI.
- [`architecture.md`](architecture.md) — слои библиотеки. PD policy engine — это L3 в той схеме.

Целевые ограничения (для понимания скоупа):

| Параметр                | Значение                                                                         |
|:------------------------|:---------------------------------------------------------------------------------|
| PD ревизия в эфире      | 3.0 (Specification Revision bits = 10b в Message Header)                         |
| Совместимость           | принимаем PD 2.0 (биты 01b) и понижаемся                                          |
| Power range             | SPR only (Standard Power Range, до 100 Вт)                                       |
| Целевой максимум        | ≤ 60 Вт (20 В × 3 А или эквивалент)                                              |
| Роль                    | DRP (Source ↔ Sink, без выделенной роли)                                         |
| Data role               | DFP / UFP с поддержкой DR_Swap                                                   |
| VCONN                   | не источаем (FUSB302 умеет, но у нас нет PD-cable-discovery → нет нужды)         |
| Cable discovery (SOP'/SOP'') | не делаем                                                                   |
| VDM (Vendor Defined)    | не реализуем                                                                     |
| Alt-modes               | не реализуем                                                                     |
| EPR / AVS               | не реализуем                                                                     |
| FRS (Fast Role Swap)    | не реализуем                                                                     |
| Security / FW Update    | не реализуем                                                                     |
| Battery messages        | не реализуем                                                                     |
| BIST                    | минимально (Carrier Mode 2 для compliance — опционально)                         |

---

## 1. Message Header

Все PD-пакеты начинаются с 16-битного Message Header (Table 6.2). Полный
layout одинаков для всех сообщений:

| Bit(s) | Поле                     | Применимо к | Описание                                                                                              |
|:------:|:-------------------------|:-----------:|:------------------------------------------------------------------------------------------------------|
| 15     | Extended                 | All         | 0 = Control/Data Message; 1 = Extended Message.                                                       |
| 14:12  | Number of Data Objects   | All         | Для Control = 0; для Data = N (1..7) объектов; для Extended см. §6.5.1.                              |
| 11:9   | MessageID                | All         | Rolling counter 0..7. Дубликаты ловятся приёмником через MessageIDCounter (§7.32).                    |
| 8      | Port Power Role          | SOP         | 0 = Sink, 1 = Source. Receiver **не верифицирует** (Shall Not lead to reset).                         |
| 8      | Cable Plug               | SOP'/SOP''  | 0 = Port (DFP/UFP), 1 = Cable Plug/VPD. (У нас SOP' не используется — игнор.)                         |
| 7:6    | Specification Revision   | All         | 00b = R1.0 (deprecated; интерпретируем как 2.0); 01b = 2.0; 10b = 3.x; 11b = Reserved.               |
| 5      | Port Data Role           | SOP         | 0 = UFP, 1 = DFP. Если приходит с тем же data role, что у нас → USB Type-C Error Recovery.            |
| 5      | Reserved                 | SOP'/SOP''  | Ignore.                                                                                               |
| 4:0    | Message Type             | All         | Тип сообщения. Контекст — см. §1.1/§1.2/§1.3 ниже.                                                    |

Если `Extended=1` → ещё 16 бит Extended Message Header (см. §1.3); если
`Extended=0` и Number of Data Objects > 0 → Data Message (§1.2); иначе
→ Control Message (§1.1).

---

## 1.1 Control Messages (Table 6.4)

Сообщения без payload — только Message Header. **`Number of Data Objects = 0`,
`Extended = 0`.**

Колонка «Скоуп» — наша политика обработки:
- **✅ MUST** — реализуем полностью (отправляем и/или принимаем).
- **⚠️ Опц.** — реализуем по мере необходимости.
- **❌ NS** — на приём отвечаем `Not_Supported`; никогда не отправляем.
- **🚫 DEP** — Deprecated в PD 3.x; не отправляем; на приём — Not_Supported (или Ignore).

| Type | Имя                          | Скоуп   | Зачем / почему                                                                                    |
|:----:|:-----------------------------|:--------|:--------------------------------------------------------------------------------------------------|
| 0x00 | Reserved                     | ❌ NS   | Если придёт — Not_Supported (мандатно по спеке).                                                   |
| 0x01 | **GoodCRC**                  | ✅ MUST | Базовая ACK PRL. FUSB302 умеет авто-GoodCRC; **программно отправлять не нужно**.                  |
| 0x02 | GotoMin                      | 🚫 DEP  | Spec говорит: Not_Supported. На передачу — никогда.                                                |
| 0x03 | **Accept**                   | ✅ MUST | Положительный ACK на Request / Soft_Reset / PR_Swap / DR_Swap / VCONN_Swap. Отправляем и принимаем. |
| 0x04 | **Reject**                   | ✅ MUST | Отрицательный ACK на Request / role swap. Отправляем и принимаем.                                  |
| 0x05 | Ping                         | 🚫 DEP  | Spec говорит: May respond Not_Supported, May Ignore. Будем игнорировать на приём.                  |
| 0x06 | **PS_RDY**                   | ✅ MUST | Power Supply Ready: после успешной транзиции мощности. И source-side, и sink-side.                 |
| 0x07 | **Get_Source_Cap**           | ✅ MUST | Sink → Source: "пришли свой Source_Capabilities". Sink-side: отправляем. Source-side: получаем и шлём caps. |
| 0x08 | **Get_Sink_Cap**             | ✅ MUST | Source → Sink: запрос Sink_Capabilities. Source-side: можем отправить. Sink-side: отвечаем Sink_Capabilities. |
| 0x09 | **DR_Swap**                  | ✅ MUST | Data Role Swap request. DRP-port — must support приём; отправка — опционально по policy.          |
| 0x0A | **PR_Swap**                  | ✅ MUST | Power Role Swap request. DRP-port — must support и приём, и отправку.                              |
| 0x0B | VCONN_Swap                   | ❌ NS   | Только нужен для cable-discovery (SOP'). У нас VCONN не источаем → Not_Supported на приём.        |
| 0x0C | **Wait**                     | ✅ MUST | Negative ACK с просьбой подождать. Принимаем; иногда отправляем (если Policy не готово).           |
| 0x0D | **Soft_Reset**               | ✅ MUST | Reset PRL counters (без Hard Reset). Принимаем и инициируем при protocol error.                    |
| 0x0E | Data_Reset                   | ❌ NS   | Только для USB4-capable портов. Мы не USB4 → Not_Supported.                                       |
| 0x0F | Data_Reset_Complete          | ❌ NS   | Часть Data_Reset flow. Not_Supported.                                                              |
| 0x10 | **Not_Supported**            | ✅ MUST | Отправляем в ответ на все NS-сообщения; принимаем — отмечаем "partner не умеет это".              |
| 0x11 | Get_Source_Cap_Extended      | ❌ NS   | PD 3.0+ extended caps. Излишне для скоупа.                                                         |
| 0x12 | Get_Status                   | ❌ NS   | PD 3.0+. Status message — temp/power flags. В v1 не публикуем.                                    |
| 0x13 | FR_Swap                      | ❌ NS   | Fast Role Swap. Не делаем.                                                                         |
| 0x14 | Get_PPS_Status               | ❌ NS   | PPS не делаем.                                                                                     |
| 0x15 | Get_Country_Codes            | ❌ NS   | Country messages не делаем.                                                                        |
| 0x16 | Get_Sink_Cap_Extended        | ❌ NS   | Extended caps не делаем.                                                                           |
| 0x17 | Get_Source_Info              | ❌ NS   | PD 3.2 only. Не наша ревизия.                                                                      |
| 0x18 | Get_Revision                 | ⚠️ Опц. | PD 3.0+ (есть в R3.0 ECN). На приём можем ответить Revision (Data Message 0x0C); на отправку — для определения partner-PD-rev. Можно отложить. |
| 0x19..0x1F | Reserved              | ❌ NS   | Not_Supported.                                                                                     |

**Итого Control:** 11 must-have + 2 опциональных. Остальные — Not_Supported.

---

## 1.2 Data Messages (Table 6.5)

Сообщения с N Data Objects (1..7). **`Number of Data Objects > 0`,
`Extended = 0`.**

| Type | Имя                    | Скоуп   | Объекты | Описание / скоуп                                                                                   |
|:----:|:-----------------------|:--------|:-------:|:---------------------------------------------------------------------------------------------------|
| 0x00 | Reserved               | ❌ NS   | —       | Not_Supported.                                                                                     |
| 0x01 | **Source_Capabilities**| ✅ MUST | 1..7 PDO| Source отправляет каждые tTypeCSendSourceCap пока нет explicit contract. Sink получает.            |
| 0x02 | **Request**            | ✅ MUST | 1 RDO   | Sink отправляет в ответ на Source_Capabilities. Source принимает и отвечает Accept/Reject/Wait.    |
| 0x03 | **BIST**               | ⚠️ Опц. | 1 BDO   | Compliance testing. Минимум: BIST Carrier Mode 2 (CRTM) при необходимости certify. Иначе NS.       |
| 0x04 | **Sink_Capabilities**  | ✅ MUST | 1..7 PDO| Отправляем в ответ на Get_Sink_Cap. Принимаем чтобы знать что partner может потреблять.            |
| 0x05 | Battery_Status         | ❌ NS   | 1 BSDO  | Battery messages не делаем.                                                                        |
| 0x06 | Alert                  | ❌ NS   | 1 ADO   | Alert messages не делаем (Status / OCP / OVP / OTP — не публикуем).                                |
| 0x07 | Get_Country_Info       | ❌ NS   | —       | Country не делаем.                                                                                 |
| 0x08 | Enter_USB              | ❌ NS   | 1 EUDO  | USB4 entry. Мы не USB4.                                                                            |
| 0x09 | EPR_Request            | ❌ NS   | 2 PDO+RDO | EPR не делаем.                                                                                  |
| 0x0A | EPR_Mode               | ❌ NS   | 1 EPRMDO| EPR не делаем.                                                                                     |
| 0x0B | Source_Info            | ❌ NS   | 2 SIDO  | PD 3.2 only.                                                                                       |
| 0x0C | Revision               | ⚠️ Опц. | 1 RMDO  | Ответ на Get_Revision. Парный с Control 0x18.                                                      |
| 0x0D..0x0E | Reserved         | ❌ NS   | —       | Not_Supported.                                                                                     |
| 0x0F | Vendor_Defined         | ❌ NS   | 1..7 VDO| **Не реализуем VDM** — отвечаем Not_Supported. Включает Discover Identity / SVIDs / Mode / Attention. |
| 0x10..0x1F | Reserved         | ❌ NS   | —       | Not_Supported.                                                                                     |

---

## 1.3 Extended Messages (Table 6.47)

Сообщения с extended header после Message Header. **`Extended = 1`.**
Могут быть длинными (chunked). В скоупе DRP-no-VDM **не реализуем ни
одного** — на любой extended приходит ответ `Not_Supported` (Control
message). Перечисление — для документации.

| Type | Имя                          | Скоуп  |
|:----:|:-----------------------------|:------:|
| 0x00 | Reserved                     | ❌ NS  |
| 0x01 | Source_Capabilities_Extended | ❌ NS  |
| 0x02 | Status                       | ❌ NS  |
| 0x03 | Get_Battery_Cap              | ❌ NS  |
| 0x04 | Get_Battery_Status           | ❌ NS  |
| 0x05 | Battery_Capabilities         | ❌ NS  |
| 0x06 | Get_Manufacturer_Info        | ❌ NS  |
| 0x07 | Manufacturer_Info            | ❌ NS  |
| 0x08 | Security_Request             | ❌ NS  |
| 0x09 | Security_Response            | ❌ NS  |
| 0x0A | Firmware_Update_Request      | ❌ NS  |
| 0x0B | Firmware_Update_Response     | ❌ NS  |
| 0x0C | PPS_Status                   | ❌ NS  |
| 0x0D | Country_Info                 | ❌ NS  |
| 0x0E | Country_Codes                | ❌ NS  |
| 0x0F | Sink_Capabilities_Extended   | ❌ NS  |
| 0x10 | Extended_Control             | ❌ NS  |
| 0x11 | EPR_Source_Capabilities      | ❌ NS  |
| 0x12 | EPR_Sink_Capabilities        | ❌ NS  |
| 0x13..0x1D | Reserved               | ❌ NS  |
| 0x1E | Vendor_Defined_Extended      | ❌ NS  |
| 0x1F | Reserved                     | ❌ NS  |

> **Поэтому на приём** в v1 нам **не нужен полноценный chunking-receiver**:
> любой extended → emit Not_Supported. Если в будущем подключим Status
> или extended caps — придётся реализовать chunking-machinery (Table 6.49
> Chunked flag, ChunkSenderRequest/Response timers и т.п.).

---

## 2. Power Data Objects (PDO)

PDO — 32-битная структура, описывающая одну "линию питания" Source-а или
"требование" Sink-а. До 7 PDO-ов в `Source_Capabilities` / `Sink_Capabilities`.

Старшие 2 бита определяют тип (Table 6.6):

| Bits 31:30 | Тип             | Скоуп v1     |
|:----------:|:----------------|:-------------|
| `00`       | Fixed Voltage   | ✅ MUST      |
| `01`       | Battery         | ⚠️ принимаем, не отправляем |
| `10`       | Variable        | ⚠️ принимаем, не отправляем |
| `11`       | Augmented (APDO)| ⚠️ принимаем (PPS); APDO subtype = SPR-AVS не делаем |

### 2.1 Fixed Supply PDO (Table 6.8 для 5V Source, Table 6.10 для других)

**Это основной PDO, который мы отправляем как Source.** PDO #1
обязательно — vSafe5V Fixed.

PDO #1 (Fixed 5V Source) специальный: содержит decl-флаги:

| Bit | Поле                              | Что мы выставляем (Source)                                                            |
|:---:|:----------------------------------|:--------------------------------------------------------------------------------------|
| 29  | Dual-Role Power                   | **1** (DRP — умеем PR_Swap)                                                           |
| 28  | USB Suspend Supported             | 0 (партнёр может не applied USB-suspend; мы потребитель не управляем им)              |
| 27  | Unconstrained Power               | 0 или 1 в зависимости от источника (внешнее питание есть/нет)                         |
| 26  | USB Communications Capable        | 1 если есть USB-data; иначе 0 (зависит от аппаратной части устройства)                |
| 25  | Dual-Role Data                    | 1 (DR_Swap support)                                                                   |
| 24  | Unchunked Extended Messages       | 0 (мы chunked-only)                                                                   |
| 23  | EPR Capable                       | **0** (мы SPR-only)                                                                   |
| 22  | Reserved                          | 0                                                                                     |
| 21:20 | Peak Current                    | 00 (Iop = ioc, без overcurrent allowance)                                             |
| 19:10 | Voltage                         | `0x064` (= 100 в 50mV units → 5000 mV)                                                |
| 9:0   | Maximum Current                 | в 10 мА (e.g. 300 = 3000 мА = 3А)                                                     |

PDO #N (Fixed >5V): тот же layout, но биты 29:22 — Reserved (PD 3.0
allows их использование только в PDO #1).

**Какие PDO-ы мы отправляем как Source (по умолчанию для ≤ 60Вт):**

| PDO # | Voltage | Max Current | Power | Заметка                                |
|:-----:|:-------:|:-----------:|:-----:|:---------------------------------------|
| 1     | 5 V     | 3 A         | 15 W  | Mandatory (vSafe5V Fixed)              |
| 2     | 9 V     | 3 A         | 27 W  | Опционально                            |
| 3     | 15 V    | 3 A         | 45 W  | Опционально                            |
| 4     | 20 V    | 3 A         | 60 W  | Опционально (наш max)                  |

Точный список — конфигурируется через PPM-API (`SET_PDOS`, см.
[`commands.md`](commands.md) §2.28).

### 2.2 Fixed Supply PDO (Sink — Table 6.9)

Sink-вариант мы отправляем в ответ на `Get_Sink_Cap`. Layout отличается:

| Bit | Поле                              | Что мы выставляем (Sink)                              |
|:---:|:----------------------------------|:------------------------------------------------------|
| 29  | Dual-Role Power                   | **1**                                                  |
| 28  | Higher Capability                 | 1 если для full functionality нужно >5V               |
| 27  | Unconstrained Power               | 0 / 1 в зависимости от устройства                     |
| 26  | USB Communications Capable        | 0 / 1                                                 |
| 25  | Dual-Role Data                    | **1**                                                  |
| 22:20 | Reserved                        | 0                                                     |
| 19:10 | Voltage                         | в 50mV                                                |
| 9:0   | Maximum Current                 | в 10 mA                                               |

### 2.3 Variable / Battery / PPS / AVS PDO

- **Variable PDO** (Table 6.12) — *принимаем* от partner-а, *не отправляем*.
- **Battery PDO** (Table 6.11) — *принимаем*, *не отправляем*.
- **SPR PPS Source APDO** (Table 6.13) — *не отправляем*; на приём от partner-а — не запрашиваем (PPS request не делаем).
- **SPR PPS Sink APDO** (Table 6.14) — *не отправляем*.
- **SPR AVS APDO** (Table 6.15) — не отправляем, на приём — игнор.
- **EPR Source APDO** (Table 6.16) — *не отправляем*.
- **EPR Sink APDO** (Table 6.17) — *не отправляем*.

---

## 3. Request Data Object (RDO)

RDO — 32-битный объект, отправляемый Sink-ом в `Request` message в ответ
на `Source_Capabilities`. Один RDO — выбор одного PDO из source-списка с
указанием желаемого тока.

### 3.1 Fixed / Variable RDO (Table 6.19)

| Bit | Поле                                              | Описание                                                                       |
|:---:|:--------------------------------------------------|:-------------------------------------------------------------------------------|
| 31  | Reserved                                          | 0                                                                              |
| 30:28 | Object Position                                 | Номер выбранного PDO в Source_Capabilities (1..7). 0 = Reserved.               |
| 27  | GiveBack Flag                                     | 0 (мы не делаем GiveBack — это для конкуренции за power budget)                |
| 26  | Capability Mismatch                               | 1 если sink требует больше, чем source может; партнёр запомнит, можно потом renegotiate |
| 25  | USB Communications Capable                        | флаг данных                                                                    |
| 24  | No USB Suspend                                    | 1 = не позволять USB suspend (мы — обычно 1)                                   |
| 23  | Unchunked Extended Messages Supported             | 0 (только chunked)                                                             |
| 22  | EPR Mode Capable                                  | 0 (нет EPR)                                                                    |
| 21:20 | Reserved                                        | 0                                                                              |
| 19:10 | Operating Current                               | Запрашиваемый ток в 10 мА                                                      |
| 9:0   | Maximum Operating Current                       | Максимальный ток в 10 мА                                                       |

### 3.2 Battery RDO (Table 6.20)

Принимаем только если partner предложил Battery PDO. В v1 — отвечаем
Accept на наш стандартный 5V request, к Battery-PDO не апеллируем как
Sink.

### 3.3 PPS RDO (Table 6.21) / AVS RDO (Table 6.22)

Не отправляем (мы не PPS-sink / AVS-sink).

---

## 4. Hard Reset / Soft Reset / Cable Reset

### 4.1 Hard Reset

**Сигнализация** — не PD-сообщение, а специфичный BMC-паттерн. Отсылается
напрямую BMC PHY (FUSB302 умеет — регистр `Control3.SEND_HARD_RESET`,
поднимается флаг по факту).

Triggers (когда отсылаем):
- Источнику: SinkWaitCapTimer expired (Sink) → требуем reset от Source-а.
- Источнику: explicit ошибка в protocol (хотя обычно сначала Soft_Reset, потом Hard).
- HardResetCounter < nHardResetCount.

Receive: FUSB302 поднимает INT (`Interrupta.I_HARDRESET`). Действие — полный сброс PRL/PE, возврат в Type-C unattached цикл (для source — `Hard Reset complete → vSafe0V → vSafe5V → Source_Capabilities`).

**Скоуп: ✅ MUST.**

### 4.2 Soft_Reset (Control message 0x0D)

Не сигнализация, а control-message. Очищает MessageIDCounter в обе
стороны и возвращает PE в `Ready`-подобное состояние, не сбрасывая
power-contract. Используется для recovery от protocol errors (например,
получили message с unexpected MessageID).

**Скоуп: ✅ MUST.**

### 4.3 Cable Reset

Не делаем — это инструмент для сброса SOP'/SOP'' (cable). Мы не общаемся
с cable plug.

---

## 5. BIST (Built-In Self-Test)

BDO (Table 6.23) с 1 объектом. Mode (биты 31:28):

| Mode | Имя                       | Описание                                                          | Скоуп v1 |
|:----:|:--------------------------|:------------------------------------------------------------------|:---------|
| 0x0  | Reserved                  | —                                                                 | ❌       |
| 0x1  | BIST Receiver Mode (DEP)  | Deprecated                                                        | ❌       |
| 0x2  | BIST Transmit Mode (DEP)  | Deprecated                                                        | ❌       |
| 0x3  | Returned BIST Counters    | —                                                                 | ❌       |
| 0x4  | BIST Carrier Mode         | Передача BMC carrier для PHY-теста. **Может потребоваться compliance.** | ⚠️ Опц. |
| 0x5  | BIST Test Data            | Тестовый поток                                                    | ⚠️ Опц.  |
| 0x6  | BIST Shared Test Mode Entry | —                                                               | ❌       |
| 0x7  | BIST Shared Test Mode Exit | —                                                                | ❌       |
| 0x8..0xF | Reserved              | —                                                                 | ❌       |

В v1: на приём отвечаем `Not_Supported`. Если позже понадобится
сертификация — реализуем BIST Carrier Mode 2 (FUSB302 поддерживает в
hardware: регистр `Control1.BIST_MODE2`).

---

## 6. Timers

Полный список из Table 7.9 — отфильтрован по нашему скоупу. Колонка
«Реализуем» — нужно ли создать `FuriEventLoopTimer` для нас.

### 6.1 Protocol Layer Timers

| Timer                         | Параметр              | Min   | Nom   | Max   | Реализуем | Назначение                                                                                  |
|:------------------------------|:----------------------|:------|:------|:------|:----------|:--------------------------------------------------------------------------------------------|
| CRCReceiveTimer               | tReceive              | 900   | 1000  | 1100  μs| ⚠️ (HW)   | TX→ожидание GoodCRC. **Делает FUSB302 в железе** (auto-retry).                              |
| SinkTxTimer                   | tSinkTx               | 16    | 18    | 20  ms| ✅        | Sink ждёт перед инициированием AMS, чтобы Source мог.                                       |
| HardResetCompleteTimer        | tHardResetComplete    | 4000  | 4500  | 5000 μs| ✅       | Sender Hard Reset ждёт PHY-BMC завершения.                                                  |
| ChunkSenderRequestTimer       | tChunkSenderRequest   | 24    | 27    | 30  ms| ❌        | Chunked extended messages — мы не делаем.                                                   |
| ChunkSenderResponseTimer      | tChunkSenderResponse  | 24    | 27    | 30  ms| ❌        | Chunked extended messages — мы не делаем.                                                   |

### 6.2 Policy Engine Timers

| Timer                         | Параметр              | Min   | Nom   | Max   | Реализуем | Назначение                                                                                  |
|:------------------------------|:----------------------|:------|:------|:------|:----------|:--------------------------------------------------------------------------------------------|
| **SenderResponseTimer**       | tSenderResponse       | 271   |       | 501 ms| ✅        | Базовый timeout ожидания ответа на отправленное request-message (Source_Caps→Request, и т.д.) |
| **SinkWaitCapTimer**          | tTypeCSinkWaitCap     | 310   | 465   | 620 ms| ✅        | Sink: ждать Source_Capabilities после attach. Expire → Hard Reset.                          |
| **SourceCapabilityTimer**     | tTypeCSendSourceCap   | 100   | 150   | 200 ms| ✅        | Source: интервал между отправками Source_Capabilities до первого Request.                   |
| **NoResponseTimer**           | tNoResponse           | 4.5   | 5.0   | 5.5  s| ✅        | Если Hard Reset не помог nHardResetCount раз → partner unresponsive, выйти из PD.           |
| **PSTransitionTimer**         | tPSTransition (SPR)   | 450   | 500   | 550 ms| ✅        | Source/Sink ждёт PS_RDY после Accept на Request. Expire → Hard Reset.                       |
| **PSHardResetTimer**          | tPSHardReset          | 25    | 30    | 35  ms| ✅        | Source ждёт после Hard Reset перед поднятием VBUS.                                          |
| **PSSourceOffTimer**          | tPSSourceOff (SPR)    | 750   | 835   | 920 ms| ✅        | PR_Swap: old-source ждёт пока new-source поднимет VBUS.                                     |
| **PSSourceOnTimer**           | tPSSourceOn (SPR)     | 390   | 435   | 480 ms| ✅        | PR_Swap: new-source ждёт пока old-source опустит VBUS.                                      |
| **SwapSourceStartTimer**      | tSwapSourceStart      | 20    |       |       ms| ✅       | PR_Swap: после PS_RDY → пауза → отправка Source_Capabilities.                               |
| SinkRequestTimer              | tSinkRequest          | 100   |       |       ms| ⚠️       | Min delay между Sink-initiated Request-ами. Реализуем как guard.                            |
| BISTContModeTimer             | tBISTContMode         | 30    | 45    | 60  ms| ❌        | BIST не делаем.                                                                              |
| ChunkingNotSupportedTimer     | tChunkingNotSupported | 40    | 45    | 50  ms| ❌        | Если получили unchunked extended и мы chunked-only — таймер для ответа Not_Supported.       |
| DataResetFailTimer            | tDataResetFail        | 300   |       | 400 ms| ❌        | Data_Reset не делаем.                                                                        |
| DataResetFailUFPTimer         | tDataResetFailUFP     | 450   |       | 550 ms| ❌        | Data_Reset не делаем.                                                                        |
| DiscoverIdentityTimer         | tDiscoverIdentity     | 40    |       | 50  ms| ❌        | Cable discovery — не делаем.                                                                |
| SinkPPSPeriodicTimer          | tPPSRequest           | —     |       |       s| ❌        | PPS не делаем.                                                                              |
| SinkEPRKeepAliveTimer         | tSinkEPRKeepAlive     | 250   | 375   | 500 ms| ❌        | EPR не делаем.                                                                              |
| SourceEPRKeepAliveTimer       | tSourceEPRKeepAlive   | 750   | 875   | 1000 ms| ❌       | EPR не делаем.                                                                              |
| SinkEPREnterTimer             | tEnterEPR             | 450   | 500   | 550 ms| ❌        | EPR не делаем.                                                                              |
| VconnDischargeTimer           | tVconnSourceDischarge | 160   | 200   | 240 ms| ❌        | VCONN не источаем.                                                                          |
| VconnOnTimer                  | tVconnSourceTimeout   | 100   | 150   | 200 ms| ❌        | VCONN не источаем.                                                                          |
| VDMResponseTimer              | tVDMSenderResponse    | 24    |       | 501 ms| ❌        | VDM не делаем.                                                                              |
| VDMModeEntryTimer             | tVDMWaitModeEntry     | 40    | 45    | 50  ms| ❌        | VDM не делаем.                                                                              |
| VDMModeExitTimer              | tVDMWaitModeExit      | 40    | 45    | 50  ms| ❌        | VDM не делаем.                                                                              |

### 6.3 Type-C Layer timers

Эти таймеры относятся к Type-C state machine (CC detection / attach
debounce), а не к PD. Перечислены здесь для полноты — реальные значения
из *USB Type-C Cable and Connector Specification*. Будут детализированы в
[`type-c-sm.md`](type-c-sm.md):

- tCCDebounce, tPDDebounce, tDRPTry, tDRPTryWait, tErrorRecovery, tTryCCDebounce — все ✅ MUST.

### 6.4 Реализация в FURI

Каждый «✅» таймер выше = один `FuriEventLoopTimer` типа
`FuriEventLoopTimerTypeOnce`. Запуск:
`furi_event_loop_timer_start(timer, pdMS_TO_TICKS(nominal_value))`.
Истечение → handler публикует timer-event в очередь LPM, дальше PE/PRL
обрабатывает по своему текущему состоянию.

Точные значения берём из колонки **Nom**, если она заполнена; иначе
**Max** (для дедлайнов) или **Min** (для guard-задержек). Где Min/Max —
не пытаемся оптимизировать в пределах окна; берём середину или max
безопасное.

---

## 7. Counters (Table 7.11 / 7.12)

| Counter                  | Max (`n*Count`)              | Реализуем | Назначение                                                                            |
|:-------------------------|:-----------------------------|:----------|:--------------------------------------------------------------------------------------|
| **MessageIDCounter**     | nMessageIDCount = 7          | ✅        | Rolling 0..7 counter в Message Header. Per-port + per-SOP*.                          |
| **RetryCounter**         | nRetryCount = 2              | ⚠️ (HW)   | TX retries. **FUSB302 делает в железе** (регистр `Control3.N_RETRIES`).               |
| **HardResetCounter**     | nHardResetCount = 2          | ✅        | Сколько раз Hard Reset до признания partner unresponsive.                             |
| CapsCounter              | nCapsCount = 50              | ⚠️        | Опциональный счётчик Source_Capabilities. Можно реализовать чтобы не спамить.         |
| DiscoverIdentityCounter  | nDiscoverIdentityCount = 20  | ❌        | Cable discovery — не делаем.                                                          |
| VDMBusyCounter           | nBusyCount = 5               | ❌        | VDM — не делаем.                                                                       |

---

## 8. Что мы **не делаем** (явный exclusion-список)

Чтобы зафиксировать: вот функциональность, которая существует в PD 3.0+
spec, но **в v1 нашей библиотеки её нет**. На любые соответствующие
сообщения отвечаем `Not_Supported`. Не нужно её мониторить, не нужно
учитывать её состояние, не нужно держать для неё таймеры.

1. **Vendor Defined Messages** (VDM) — Discover Identity, Discover SVIDs, Discover Mode, Enter Mode, Exit Mode, Attention, и любые unstructured VDM.
2. **Alternate Modes** — DisplayPort, Thunderbolt, USB4, и прочие.
3. **Cable discovery** — SOP'/SOP''. Не общаемся с cable plug.
4. **Active cables** — не отличаем от passive; не используем e-marker info.
5. **VCONN sourcing / swap** — не источаем VCONN, не делаем VCONN_Swap.
6. **EPR** (Extended Power Range, >100 Вт) — никакие EPR-messages, никакой EPR Mode.
7. **AVS** (Adjustable Voltage Supply, SPR-AVS) — не отправляем как Source, не запрашиваем как Sink.
8. **PPS** (Programmable Power Supply) — не отправляем PPS APDO как Source, не запрашиваем как Sink.
9. **Battery messages** — Get_Battery_Cap, Battery_Capabilities, Battery_Status, Battery PDO как Source.
10. **Status / Alert messages** — мы не публикуем OCP/OVP/OTP/Status proactively. Если получаем — Not_Supported.
11. **Fast Role Swap (FRS)** — не реализуем.
12. **Data_Reset** — Not_Supported (это USB4-only фича).
13. **Security messages** — Not_Supported.
14. **Firmware Update over PD** — Not_Supported.
15. **Country / Country_Info** — Not_Supported.
16. **Extended messages в принципе** — на любое `Extended=1` сообщение отвечаем Not_Supported. Это упрощает PRL: chunking-receiver-логика не нужна.
17. **BIST** — кроме опциональной поддержки Carrier Mode 2 для compliance, не реализуем.

Если что-то из этого списка станет нужно — это отдельный feature-cycle с
изменением scope в PPM-capability-полях (`bmAttributes`,
`bmOptionalFeatures`) и расширением соответствующих state-machine.

---

## 9. Связанные документы и порядок имплементации

Следующие документы:

- [`fusb302.md`](fusb302.md) — что FUSB302 умеет в железе (auto-GoodCRC, auto-retry, BMC encoding, BIST Mode 2). Это **режет скоуп PRL** ещё сильнее, чем сама PD-спека.
- [`type-c-sm.md`](type-c-sm.md) — Type-C state machine. Дисциплина CC detection / attach / detach / polarity / DRP cycling. PD «включается» когда Type-C в `Attached.SRC` / `Attached.SNK`.
- [`prl-sm.md`](prl-sm.md) — Protocol Layer state machine. Тонкий слой над FUSB302 (большую часть PRL делает hardware).
- [`pe-sm.md`](pe-sm.md) — Policy Engine state machine для source + sink + role swaps.
