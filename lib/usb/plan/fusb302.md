# FUSB302 — Использование (L4: PHY-драйвер)

Документ описывает, что FUSB302 делает в железе автоматически (и тем
самым **режет наш программный скоуп**), что остаётся за софтом, и каков
будет внутренний API между L3 (LPM, Type-C/PRL/PE state machines) и L4
(FUSB302-драйвер).

Источник: [FUSB302B Datasheet (Onsemi)](https://www.onsemi.com/download/data-sheet/pdf/fusb302b-d.pdf).
Существующий код: [`lib/drivers/fusb302/`](../../drivers/fusb302/) —
~1200 строк (driver + register bitfields + header).

Связанные:
- [`pd-scope.md`](pd-scope.md) — что мы делаем на уровне PD-сообщений и таймеров. **Часть из этого FUSB302 делает в железе.**
- [`architecture.md`](architecture.md) — L4 в общей схеме.
- [`type-c-sm.md`](type-c-sm.md) — что L4 даёт Type-C SM и что ей делать самой.
- [`prl-sm.md`](prl-sm.md) — какая часть PRL фактически реализуется в железе FUSB302.

---

## 1. Что FUSB302 делает в железе (KEY)

Это самый важный раздел. Каждый пункт здесь — кусок работы, которую
**нам не надо писать в софте**.

### 1.1 Авто-обнаружение партнёра — `TOGGLE` режим

Регистры: `CONTROL2.TOGGLE`, `CONTROL2.MODE`, `CONTROL2.TOG_RD_ONLY`,
`CONTROL2.TOG_SAVE_PWR`.

FUSB302 умеет автономно циклически:
- выставлять Rp на CC1, потом на CC2 (если MODE=DRP или SRC),
- выставлять Rd на CC1, потом на CC2 (если MODE=DRP или SNK),
- мерить CC-напряжения,
- определять, что прицепилось (host/device/audio-accessory),
- и **остановиться** в финальной позиции, отправив `I_TOGDONE`.

Результат — в `STATUS1A.TOGSS`:

| TOGSS | Состояние                                                                    |
|:------|:-----------------------------------------------------------------------------|
| 000   | Toggle logic running (ещё не settled)                                        |
| 001   | Settled SRC on CC1                                                           |
| 010   | Settled SRC on CC2                                                           |
| 101   | Settled SNK on CC1                                                           |
| 110   | Settled SNK on CC2                                                           |
| 111   | Audio Accessory (vRa on both CC1/CC2; settles to STOP_SRC1)                  |

**Что это убирает из Type-C state machine:**

- `Unattached.SNK` cycling — не нужно.
- `Unattached.SRC` cycling — не нужно.
- `AttachWait.*` debounce первого уровня — частично железом.
- Audio Accessory detection — железом.

**Что остаётся в софте после I_TOGDONE:**

- `tCCDebounce` / `tPDDebounce` финальная валидация (можно положиться на железо, но spec требует софтового debounce ~100 мс перед переходом в `Attached.*`).
- Перевод TOGGLE=0 (это важно — иначе HW продолжит toggling).
- Установка `SWITCHES0.MEAS_CC1` или `MEAS_CC2` (lock в правильной полярности).
- Установка `SWITCHES1.TX_CC1/TX_CC2` под BMC TX в правильную полярность.
- Включение VBUS (если SRC) — через **внешний** GPIO/power switch.
- Установка PD-related регистров (auto-CRC, n_retries и т.п.).

**Конфигурация для DRP** (в существующем `fusb302_start_drp_logic()`):
```
CONTROL2.TOGGLE = 1
CONTROL2.MODE   = 01b   (DRP)
CONTROL2.TOG_SAVE_PWR = 00b или 01b (40 мс между циклами для энергосбережения)
CONTROL2.TOG_RD_ONLY  = 0 (вернуть на Audio Accessory если оба CC = Ra)
```

Маска: открыть `MASKA.M_TOGDONE=0`, остальное в MASKA — masked.

### 1.2 Авто-GoodCRC TX/RX

Регистр: `SWITCHES1.AUTO_CRC`.

С `AUTO_CRC=1` FUSB302:
- На приёме корректного PD-сообщения автоматически отправляет `GoodCRC` (заголовок собирается из `SWITCHES1.DATA_ROLE`, `SWITCHES1.POWER_ROLE`, `SWITCHES1.SPEC_REV`).
- Заполняет MessageID в GoodCRC из MessageID входящего сообщения.
- Перед отправкой ждёт ~170 мкс если `CONTROL0.AUTO_PRE=1` (даёт софту окно если бы захотели вмешаться).

Событие `I_GCRCSENT` (в `MASKB.M_GCRCSENT=0`) сигналит, что
GoodCRC отправлен — это наш способ узнать "сообщение действительно
получено и подтверждено в обе стороны".

**Что это убирает:** GoodCRC handling в PRL целиком в направлении
приёма. Не нужно ни заголовок собирать, ни TX-FIFO нагружать в ответ на
RX.

### 1.3 Авто-retry на TX

Регистры: `CONTROL3.AUTO_RETRY`, `CONTROL3.N_RETRIES`.

С `AUTO_RETRY=1` и `N_RETRIES=10b` (=2 retries, итого 3 попытки) FUSB302:
- После каждой TX ждёт GoodCRC от партнёра.
- Если не получил — retransmit с тем же MessageID.
- После `N_RETRIES` неудач — выставляет `I_RETRYFAIL` и `STATUS0A.RETRY_FAIL`.
- Если получил GoodCRC — `I_TXSENT`.

**Что это убирает:**
- `RetryCounter` — целиком в железе.
- `CRCReceiveTimer` (tReceive ~1 мс) — в железе.
- "Send + wait + retransmit" логика в PRL — нет.

PRL получает только бинарный результат: `I_TXSENT` (всё ок) или `I_RETRYFAIL`
(transmit failure → PRL должен инициировать Soft_Reset).

### 1.4 Авто-Soft-Reset и авто-Hard-Reset (мы их **отключаем**)

Регистры: `CONTROL3.AUTO_SOFTRESET`, `CONTROL3.AUTO_HARDRESET`.

FUSB302 умеет автоматически: после `RETRYFAIL` отправить Soft_Reset, а
после **его** retry-fail — Hard_Reset. **Не используем**, потому что:

- PE должна решать **когда** делать Soft_Reset (это часть состояния PE).
- Авто-Hard-Reset обходит логику HardResetCounter и tBESTtSafe-таймеров (мы PD compliant).

Конфигурация: `AUTO_SOFTRESET=0, AUTO_HARDRESET=0`.

### 1.5 Hard Reset signalling

Регистр: `CONTROL3.SEND_HARD_RESET`.

Отправка Hard Reset BMC-паттерна делается **одной записью бита** — FUSB302
сам генерирует Reset-1 + Reset-2 + EOP-like последовательность. По
завершению поднимается `I_HARDSENT`.

На приёме Hard Reset (от партнёра) — `I_HARDRST` в InterruptA.

**Что это убирает:** ручное формирование Hard Reset BMC-pattern в TX FIFO
не нужно. Также `HardResetCompleteTimer` (tHardResetComplete ~4.5 мс) —
делается в железе.

### 1.6 BMC кодирование/декодирование

Полностью железом. Софт работает только с **байтовыми токенами** через
FIFO (адрес `0x43`):

**RX FIFO tokens** (старшие 3 бита определяют SOP-тип):

| Token              | Hex (bits 7:5) | Значение                       |
|:-------------------|:--------------:|:-------------------------------|
| `RX_TOKEN_SOP`     | `111<<5`       | SOP                            |
| `RX_TOKEN_SOP1`    | `110<<5`       | SOP' (Cable plug)              |
| `RX_TOKEN_SOP2`    | `101<<5`       | SOP'' (Cable plug)             |
| `RX_TOKEN_SOP1DB`  | `100<<5`       | SOP'_DEBUG                     |
| `RX_TOKEN_SOP2DB`  | `011<<5`       | SOP''_DEBUG                    |

После SOP-токена идут 2 байта Message Header (little-endian) + N×4 байт
data objects, где N — поле "Number of Data Objects" из Header.

**TX FIFO tokens** (отправляются как байты в FIFO):

| Token                | Hex   | Назначение                                                                   |
|:---------------------|:-----:|:-----------------------------------------------------------------------------|
| `SYNC1`              | 0x12  | Sync-1                                                                       |
| `SYNC2`              | 0x13  | Sync-2 (для SOP)                                                             |
| `SYNC3`              | 0x1B  | Sync-3 (для SOP'/SOP'')                                                      |
| `RST1`               | 0x15  | Reset-1 (для Hard Reset через FIFO, но мы используем CONTROL3.SEND_HARD_RESET) |
| `RST2`               | 0x16  | Reset-2                                                                      |
| `PACKSYM` `\| (5-bit len)`| 0x80 + N| Объявить N байт данных дальше                                              |
| `JAMCRC`             | 0xFF  | Сюда FUSB302 вставит CRC32                                                   |
| `EOP`                | 0x14  | End-of-packet                                                                |
| `TXOFF`              | 0xFE  | Выключить транзитер                                                          |
| `TXON`               | 0xA1  | Включить транзитер (фактически запускает отправку — TX_START register тоже работает) |

Sequence для отправки SOP-сообщения:
```
SYNC1 SYNC1 SYNC1 SYNC2  PACKSYM|(2+4N)  hdr_lo hdr_hi  obj0_lo .. obj0_hi3  ...  JAMCRC EOP TXOFF TXON
```
(уже реализовано в `fusb302_pd_message_send()`)

### 1.7 BIST Mode 2

Регистр: `CONTROL1.BIST_MODE2 = 1` → FUSB302 шлёт 010101...-pattern для
PHY-теста. По таймеру софта (tBISTContMode = 30-60 мс) → выключаем.

Делать его — опционально по [`pd-scope.md`](pd-scope.md) §5. Реализация
тривиальна.

### 1.8 VBUS detect

Регистр: `STATUS0.VBUSOK` + interrupt `I_VBUSOK`. FUSB302 имеет
компаратор vVBUSthr (~4.5 В). Поднимает interrupt когда VBUS пересекает
порог.

**Это не значит, что FUSB302 управляет VBUS** — управление source VBUS
у нас через **внешний** power switch (GPIO). FUSB302 только наблюдает.

### 1.9 Toggle settle с Save Power

`CONTROL2.TOG_SAVE_PWR` — после полного цикла toggle (CC1→CC2→CC1→CC2 без
обнаружения партнёра) FUSB302 уходит в DISABLE на tDIS (0/40/80/160 мс).
Это нативно убавляет ток в idle. Используем 40 мс (значение `01b`) —
баланс между responsiveness и потреблением.

---

## 2. Что мы **сами** делаем в софте

### 2.1 Часть Type-C state machine после I_TOGDONE

- Финальный debounce `tCCDebounce` (100-200 мс) — spec требует.
- Lock полярности (`SWITCHES0.MEAS_CC1` или `MEAS_CC2`).
- Lock BMC TX direction (`SWITCHES1.TX_CC1` или `TX_CC2`).
- Если SRC: включить **внешний** VBUS power switch, ждать vSafe5V.
- Если SNK: ждать VBUS от партнёра (через I_VBUSOK).
- Перейти в `Attached.SRC` или `Attached.SNK`.
- Запустить PE.

### 2.2 PRL — то, что осталось

После того как FUSB302 берёт на себя GoodCRC + retry + CRC:

- **MessageID counter** — отслеживание per-SOP* (per-direction). Хотя FUSB302 сам выставляет MessageID в auto-GoodCRC из header входящего, для **наших исходящих** MessageID мы ставим в header сами и инкрементируем после `I_TXSENT`.
- **Duplicate detection на приёме** — если прилетел тот же MessageID, что и в предыдущем сообщении, FUSB302 всё равно вернёт GoodCRC (auto), но нам надо понять "это retry партнёра, не обрабатывать дважды".
- **Soft_Reset MessageID semantics** — Soft_Reset всегда имеет MessageID=0 и обнуляет counter в обе стороны.
- **Discard на коллизии** — если получили message пока пытаемся отправить (`I_COLLISION`), TX отменяется в железе, но PRL должен повторить попытку.
- **SinkTxTimer (tSinkTx, ~18 мс)** — sink ждёт перед инициированием AMS, чтобы дать source шанс. PRL должен ждать этот таймер.

### 2.3 PE state machine — целиком в софте

PE остаётся как есть из [`pd-scope.md`](pd-scope.md) §6.2. FUSB302 не
понимает понятий PDO/RDO/Accept/Reject — это всё PE.

### 2.4 VBUS control (source side)

Только GPIO к внешнему power switch. FUSB302 не управляет питанием.

### 2.5 Discharge VBUS

Тоже внешний (резистор/switch). FUSB302 не имеет встроенного discharge.

### 2.6 Voltage measurement через MDAC

Для проверки vSafe0V / vSafe5V / vBus5V и т.п. — `MEASURE.MEAS_VBUS=1` +
`MEASURE.MDAC` (6-битный DAC, LSB=42 мВ → диапазон 0..2.6 В,
*scaled* — VBUS делится внутри ÷10, т.е. эффективный диапазон 0..26 В).
Сравнение через `STATUS0.COMP` — 1 если CC/VBUS > MDAC threshold.

**Event-driven threshold crossings**: вместо polling-а `STATUS0.COMP`, мы
используем `I_COMP_CHNG` interrupt. Сценарий:
1. PE / Type-C SM хочет дождаться, что VBUS перешёл через porog (например, vSafe5V = 4.5 В).
2. Устанавливаем `MEASURE.MEAS_VBUS=1`, `MEASURE.MDAC = (4500 мВ / 10 / 42 мВ) ≈ 0x0B`.
3. Открываем маску `I_COMP_CHNG`.
4. При пересечении порога — interrupt → событие `Fusb302EventCompChanged`.
5. Software проверяет `STATUS0.COMP` (1 = выше порога, 0 = ниже).
6. Если нужен следующий порог (например, vSafe0V после vSafe5V dropped) — переустанавливаем MDAC и ждём следующий event.

Это **существенно лучше** чем polling: ноль I²C-трафика в idle, чёткая
семантика «событие случилось», простая интеграция с FuriEventLoop.

**API L4 для этого**: `set_compare_threshold_mv(threshold_mv, direction)` — устанавливает
MDAC + переоткрывает I_COMP_CHNG; direction может фильтровать (above-only / below-only / both).

Для CC voltage detection используется `MEASURE.MDAC` + `SWITCHES0.MEAS_CC*` +
`STATUS0.BC_LVL` (2-битный quantized уровень). BC_LVL:
- `00`: < 200 мВ
- `01`: 200-660 мВ — Rd detected, USB default current
- `10`: 660-1230 мВ — Rd detected, 1.5 A Type-C current
- `11`: > 1230 мВ — Rd detected, 3 A Type-C current

Используется как для source-side detection (sink subscribed для current
level) так и для sink-side detection (определение какой Rp source даёт).

### 2.7 Rp/Rd switching и host current

`SWITCHES0.PU_EN1/PU_EN2` (Rp pull-up) + `SWITCHES0.PDWN1/PDWN2` (Rd
pull-down) + `CONTROL0.HOST_CUR` для уровня Rp:
- `00`: No current
- `01`: 80 μA — Default USB power
- `10`: 180 μA — 1.5 A
- `11`: 330 μA — 3 A

При TOGGLE=1 FUSB302 сам управляет PDWN/PU_EN, но HOST_CUR ставим мы (он
определяет, что мы advertised как source).

---

## 3. Карта регистров (reference)

Полный bitfield-layout — в [`fusb302_reg.h`](../../drivers/fusb302/fusb302_reg.h). Краткая сводка:

| Addr | Регистр       | Тип    | Назначение                                                           |
|:----:|:--------------|:-------|:---------------------------------------------------------------------|
| 0x01 | DEVICE_ID     | RO     | Version/Product/Revision ID                                          |
| 0x02 | SWITCHES0     | RW     | Pull-downs, measure-select, VCONN-enable, pull-ups (CC1/CC2 каждый)  |
| 0x03 | SWITCHES1     | RW     | TX direction, AUTO_CRC, DataRole, SpecRev, PowerRole (для auto-GoodCRC) |
| 0x04 | MEASURE       | RW     | MDAC value (6-bit), MEAS_VBUS bit                                    |
| 0x05 | SLICE         | RW     | BMC slicer DAC + hysteresis                                          |
| 0x06 | CONTROL0      | RW/C   | TX_START, AUTO_PRE, HOST_CUR, INT_MASK, TX_FLUSH                     |
| 0x07 | CONTROL1      | RW/C   | ENSOP1/2, RX_FLUSH, BIST_MODE2, ENSOP1DB/2DB                         |
| 0x08 | CONTROL2      | RW     | TOGGLE, MODE, WAKE_EN, TOG_RD_ONLY, TOG_SAVE_PWR                     |
| 0x09 | CONTROL3      | RW     | AUTO_RETRY, N_RETRIES, AUTO_SOFTRESET, AUTO_HARDRESET, BIST_TMODE, SEND_HARD_RESET |
| 0x0A | MASK          | RW     | Маски для interrupt-bits в register `INTERRUPT` (8 шт)               |
| 0x0B | POWER         | RW     | PWR[3:0] — power domains (bandgap, receiver, measure, oscillator)    |
| 0x0C | RESET         | W/C    | SW_RESET, PD_RESET                                                   |
| 0x0D | OCP           | RW     | OCP_CUR, OCP_RANGE — для VCONN OCP (мы VCONN не источаем)            |
| 0x0E | MASKA         | RW     | Маски для interrupt-bits в register `INTERRUPTA` (8 шт)              |
| 0x0F | MASKB         | RW     | Маска для `I_GCRCSENT`                                               |
| 0x10 | CONTROL4      | RW     | TOG_EXIT_AUD — выход из toggle при Audio Accessory                   |
| 0x3C | STATUS0A      | RO     | HARDRST, SOFTRST, POWER_STATE, RETRY_FAIL, SOFT_FAIL                 |
| 0x3D | STATUS1A      | RO     | RX_SOP1DB/2DB, RX_SOP, TOGSS (toggle state)                          |
| 0x3E | INTERRUPTA    | RO/C   | I_HARDRST, I_SOFTRST, I_TXSENT, I_HARDSENT, I_RETRYFAIL, I_SOFTFAIL, I_TOGDONE, I_OCP_TEMP |
| 0x3F | INTERRUPTB    | RO/C   | I_GCRCSENT                                                            |
| 0x40 | STATUS0       | RO     | BC_LVL, WAKE, ALERT, CRC_CHK, COMP, ACTIVITY, VBUSOK                 |
| 0x41 | STATUS1       | RO     | OCP, OVERTEMP, TX_FULL/EMPTY, RX_FULL/EMPTY, RX_SOP1/2               |
| 0x42 | INTERRUPT     | RO/C   | I_BC_LVL, I_COLLISION, I_WAKE, I_ALERT, I_CRC_CHK, I_COMP_CHNG, I_ACTIVITY, I_VBUSOK |
| 0x43 | FIFOS         | RW     | TX/RX FIFO (одни и те же 256-байтные буферы PD-сообщений)            |

I²C-адрес: `0x22` (`FUSB302_ADDRESS` в [`fusb302.h`](../../drivers/fusb302/fusb302.h)).

---

## 4. Interrupt-модель

Три регистра interrupt-флагов, каждый — `Read/Clear` (чтение очищает).
Соответствующие маски (`MASK`, `MASKA`, `MASKB`) определяют, какие из
флагов поднимают линию INT (active-low) наружу.

**INT pin** маскируется на самом устройстве через `CONTROL0.INT_MASK`
(глобальная маска).

### 4.1 Маска по умолчанию для DRP-no-VDM

Используем как стартовую точку:

| Bit             | Регистр    | Маска | Используем? | Назначение в нашей SM                                                |
|:----------------|:-----------|:-----:|:-----------:|:---------------------------------------------------------------------|
| I_BC_LVL        | INTERRUPT  | 0     | ✅          | Source-side: партнёр сменил current request. Sink-side: Rp изменилcя. |
| I_COLLISION     | INTERRUPT  | 0     | ✅          | TX attempt пока партнёр шлёт нам → PRL retry.                        |
| I_WAKE          | INTERRUPT  | 1     | ❌          | Wake-detect — не используем (всегда in active mode).                  |
| I_ALERT         | INTERRUPT  | 1     | ❌          | TX_FULL/RX_FULL — не должно быть в нормальной работе, мы flushим.    |
| I_CRC_CHK       | INTERRUPT  | 1     | ❌          | CRC valid — auto-GoodCRC сам реагирует; нам важно I_GCRCSENT.        |
| I_COMP_CHNG     | INTERRUPT  | 0     | ✅          | CC voltage threshold пересечён — detach detection.                   |
| I_ACTIVITY      | INTERRUPT  | 1     | ❌          | Транзишены на CC — слишком noisy.                                    |
| I_VBUSOK        | INTERRUPT  | 0     | ✅          | VBUS прошёл vVBUSthr — sink wake, source-detach.                     |
| I_HARDRST       | INTERRUPTA | 0     | ✅          | Получили Hard Reset от партнёра.                                     |
| I_SOFTRST       | INTERRUPTA | 1     | ❌ (см.ниже) | Получили Soft_Reset. Можно detect-ить по RX-FIFO, маскируем.         |
| I_TXSENT        | INTERRUPTA | 0     | ✅          | Сообщение успешно ушло (GoodCRC получен).                            |
| I_HARDSENT      | INTERRUPTA | 0     | ✅          | Hard Reset мы отправили — таймер на следующий шаг.                   |
| I_RETRYFAIL     | INTERRUPTA | 0     | ✅          | TX retry exhausted → PE → Soft_Reset (или Hard, в зависимости).      |
| I_SOFTFAIL      | INTERRUPTA | 1     | ❌          | AUTO_SOFTRESET=0, не используется.                                   |
| I_TOGDONE       | INTERRUPTA | 0     | ✅          | TOGGLE settled — приступаем к attach detection.                      |
| I_OCP_TEMP      | INTERRUPTA | 1     | ❌          | VCONN OCP — мы VCONN не источаем.                                    |
| I_GCRCSENT      | INTERRUPTB | 0     | ✅          | Auto-GoodCRC отправлен в ответ на RX — обработать RX-FIFO.           |

> **Соглашение spec.**: маска=0 значит "не masked, interrupt прорывается на
> INT pin"; маска=1 значит "masked". Это противоположно интуиции — внимание.

### 4.2 Обработка IRQ

В существующем коде: ISR на падающем фронте `INT` зовёт user-callback.
Что нужно делать в callback (в LPM-thread, не в ISR):

1. Read `INTERRUPT`, `INTERRUPTA`, `INTERRUPTB` (это очищает их в железе).
2. Для каждого выставленного флага — публикуем типизированное event в очередь LPM.
3. Если `I_TOGDONE` — дополнительно прочитать `STATUS1A.TOGSS` для роли/полярности.
4. Если `I_GCRCSENT` — прочитать `STATUS1.RX_EMPTY`; пока не empty — читать сообщения из FIFO.
5. Если `I_VBUSOK` — прочитать `STATUS0.VBUSOK` для текущего состояния.
6. Если `I_BC_LVL` или `I_COMP_CHNG` — прочитать `STATUS0.BC_LVL` / `STATUS0.COMP`.

Существующая реализация (`fusb302_read_role()`) делает часть этой работы,
но в ней зашита Type-C state-логика (определение SRC/SNK CC1/CC2) — это
надо вынести в L3. L4 должен только публиковать "сырое" событие "TOGGLE
settled, TOGSS=X", а интерпретация — в L3.

### 4.3 ISR ↔ LPM-event-loop переход

В FURI ISR подписана через `furi_hal_gpio_add_int_callback()`. Внутри
callback'а — **не выполнять I²C** (он блокирующий, нельзя в ISR). Только
послать сигнал в LPM-thread через `FuriMessageQueue` или `FuriEventFlag`,
а I²C-чтение делать на event-loop-стороне.

Это значит, что **существующая** реализация (`fusb302_interrupt_handler`
просто зовёт user-callback) предполагает, что user-callback — это и есть
"послать сигнал в очередь". Это работает, если caller так и делает.

---

## 5. CC detection / Type-C измерения вне TOGGLE

Когда TOGGLE settled, мы перешли в `Attached.*`. Если хотим продолжать
мониторить CC (например, для detect-ить detach как source) — TOGGLE
выключен, нужно мерить вручную.

### 5.1 Source-side: измеряем Rd на активном CC

```
SWITCHES0.PDWN1 = 0, PDWN2 = 0
SWITCHES0.PU_EN1 = 1 (если CC1 активен) или PU_EN2 = 1
SWITCHES0.MEAS_CC1 = 1 (если CC1 активен) или MEAS_CC2 = 1
CONTROL0.HOST_CUR = 11b (3 A) — мы advertised 3A
```

Затем читаем `STATUS0.BC_LVL`:
- `00` (< 200 мВ) — Rd не виден → партнёр detached → перейти в `Unattached.SRC`.
- `01`/`10`/`11` — Rd виден.

Для detect-ить detach мы можем выставить `I_COMP_CHNG` interrupt со
`MEASURE.MDAC` ниже Rd-threshold — он сработает когда CC уйдёт ниже.

### 5.2 Sink-side: определяем Rp source-а

```
SWITCHES0.PDWN1 = 1, PDWN2 = 1
SWITCHES0.PU_EN* = 0
SWITCHES0.MEAS_CC1 = 1 или MEAS_CC2 = 1
```

Затем `STATUS0.BC_LVL` декодирует уровень:
- `01` — Default USB current (Rp = 80 μA, 500 мА)
- `10` — 1.5 A capability
- `11` — 3 A capability

Это попадает в `Power Operation Mode` поля `GET_CONNECTOR_STATUS` (см.
[`commands.md`](commands.md) §2.17).

---

## 6. Power management

Регистр `POWER` — PWR[3:0]:

- `PWR[0]` — Bandgap + wake-detect circuit.
- `PWR[1]` — Receiver и current references для Measure block.
- `PWR[2]` — Measure block (MDAC, comparator).
- `PWR[3]` — Internal oscillator.

В существующем коде стартует `PWR = 0111b` (всё кроме PWR[3]=oscillator).
Это неправильно — для PD-связи нам нужен oscillator. Делаем `PWR = 1111b`
после init.

Для idle (TOGGLE крутится, нет партнёра) — можно опустить PWR[3] и PWR[1]
для энергосбережения, но проще оставить full power — экономия маленькая,
а сложность выше.

---

## 7. Reset operations

Регистр `RESET`:

- `RESET.SW_RESET = 1` — полный сброс. **Включая I²C-регистры в дефолт.** Используем при init и при PPM_RESET.
- `RESET.PD_RESET = 1` — сброс только PD-логики (FIFO, MessageID-counters в железе, retry counter). Используем после Hard Reset / Soft Reset на стороне PE.

Bit самосбрасывается.

---

## 8. Существующий драйвер: что есть и что менять

### 8.1 Что хорошо

- Полная карта регистров с typed bitfields в [`fusb302_reg.h`](../../drivers/fusb302/fusb302_reg.h) — самая трудоёмкая часть уже сделана.
- I²C-обёртка над `furi_hal_i2c_*` с обработкой ошибок (`Fusb302Status`).
- Auto-GoodCRC / auto-retry setters работают.
- TX/RX FIFO logic — формирование SYNC sequence, PACKSYM, JAMCRC, EOP — реализованы.
- DRP toggle init — правильно конфигурирует TOGGLE/MODE/маски.
- CC orientation setter работает (lock в MEAS/TX полярности).

### 8.2 Что переделать для production

1. **Blocking `furi_delay_ms(10)` в `fusb302_pd_tx_flush()`** — это бесит event-loop. Заменить на: записать TX_FLUSH=1, не ждать (поле self-clearing); вернуть управление; если очень надо проверить, делать через `FuriEventLoopTimer`.
2. **Read race в `fusb302_pd_tx_flush()`** — читает `Fusb302RegControl0` для проверки `tx_flush==0`, но регистр `Control0` имеет тип `RW/C` (некоторые биты self-clearing) — чтение даёт текущее состояние, не «factual ack». Лучше проверять через `STATUS1.TX_EMPTY`.
3. **Read из CONTROL0 вместо STATUS1 в `fusb302_pd_message_receive`** — баг: `fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&status1_bits)`. Должно быть `Fusb302RegStatus1`. Исправить.
4. **Type-C state-логика в `fusb302_read_role()`** — она в L3, не в L4. Драйвер должен возвращать "сырой" TOGSS, а интерпретацию делает type-c-sm.
5. **`malloc` в `fusb302_init`** — это нормально для одиночного экземпляра, но если делать heap-allocation — желательно `furi_alloc()` или явный аллокатор.
6. **ISR callback без context-кадров для типа события** — `Fusb302Callback` принимает только `context`, без описания "что случилось". Хорошее упрощение: ISR просто говорит "что-то произошло, проснись"; event-loop сам читает регистры и разбирается. Уже сейчас так и есть — лучше не менять.
7. **VBUS auto-control в `fusb302_read_role`** — там TODO "turn on VBUS through external GPIO" / "wait for VBUS from partner". Это **policy** L3, не L4. Удалить из драйвера.

### 8.3 Что добавить

1. **Auto-CRC enable в init**. Сейчас `fusb302_pd_autogoodcrc_set` есть, но не дёргается в `fusb302_start_drp_logic`. Должно быть включено по умолчанию после settle TOGGLE.
2. **Auto-Retry enable в init**. Аналогично — нужно вызывать `fusb302_pd_autoretry_set(2)`.
3. **Configurable Rp current** — для source-mode правильный `HOST_CUR` зависит от того, что мы advertised. По умолчанию 80 μA (USB default); для DRP — нужно 3 A когда становимся source.
4. **Event-typed callback alternative** — опционально: высокоуровневый callback типа `Fusb302Event { TOGGLE_DONE, MESSAGE_RECEIVED, MESSAGE_SENT, RETRY_FAIL, HARD_RESET_RX, HARD_RESET_SENT, COMP_CHANGED, VBUS_CHANGED, BC_LVL_CHANGED }` который драйвер сам формирует после чтения регистров. Это упростит L3 — сейчас в L3 пришлось бы читать те же три INTERRUPT-регистра.

### 8.4 Existing API surface

```
fusb302_init / fusb302_deinit                       ─ lifecycle
fusb302_sw_reset                                    ─ full reset
fusb302_set_input_callback                          ─ register IRQ handler
fusb302_start_drp_logic                             ─ init DRP mode
fusb302_read_role                                   ─ TOGGLE result reading (refactor: вынести SM в L3)
fusb302_cc_orientation_set                          ─ lock polarity
fusb302_pd_reset_logic                              ─ PD-only reset
fusb302_pd_reset_hard                               ─ send Hard Reset
fusb302_pd_autogoodcrc_set                          ─ AUTO_CRC bit
fusb302_pd_autoretry_set                            ─ AUTO_RETRY + N_RETRIES
fusb302_pd_rx_flush / fusb302_pd_tx_flush           ─ FIFO clear (refactor: убрать blocking)
fusb302_pd_message_receive                          ─ RX next PD message
fusb302_pd_message_send                             ─ TX PD message
```

Это уже близко к нужному. Дальше — расширить до L4 API из §9.

---

## 9. Концептуальный L3 ↔ L4 API

### 9.1 L3 → L4 (команды)

| Команда                       | Регистры / действие                                                              |
|:------------------------------|:---------------------------------------------------------------------------------|
| init                          | SW_RESET → POWER=1111 → MASKs → CONTROL0.INT_MASK=0                              |
| start_toggle(mode)            | CONTROL2 = {TOGGLE=1, MODE=mode (DRP/SRC/SNK), TOG_SAVE_PWR}; разрешить I_TOGDONE |
| stop_toggle                   | CONTROL2.TOGGLE = 0                                                              |
| read_toggle_result            | STATUS1A.TOGSS → возврат типизированной структуры                                |
| lock_polarity(cc)             | SWITCHES0.MEAS_CC{1\|2}; SWITCHES1.TX_CC{1\|2}                                  |
| set_role(power, data, rev)    | SWITCHES1.{POWER_ROLE,DATA_ROLE,SPEC_REV} — для auto-GoodCRC заголовка           |
| set_rp_current(ua)            | CONTROL0.HOST_CUR = 80/180/330                                                   |
| enable_pd(auto_crc, n_retry)  | SWITCHES1.AUTO_CRC=1; CONTROL3.{AUTO_RETRY=1, N_RETRIES=n}                       |
| pd_reset                      | RESET.PD_RESET = 1                                                               |
| send_message(msg)             | FIFO write (SYNC + PACKSYM + header + objects + JAMCRC + EOP + TXOFF + TXON)     |
| receive_message → msg         | FIFO read (SOP-token + header + objects)                                         |
| send_hard_reset               | CONTROL3.SEND_HARD_RESET = 1                                                     |
| set_bist_mode2(enable)        | CONTROL1.BIST_MODE2                                                              |
| measure_cc_level → BC_LVL     | STATUS0.BC_LVL                                                                   |
| measure_vbus_threshold(mv) → above? | MEASURE.MEAS_VBUS=1; MEASURE.MDAC=value; STATUS0.COMP (одноразовое чтение)   |
| set_compare_threshold_mv(mv)  | MEASURE.MEAS_VBUS=1; MEASURE.MDAC; разоткрывает I_COMP_CHNG → событие при пересечении |

### 9.2 L4 → L3 (события)

Все события идут через `FuriMessageQueue<Fusb302Event>`. Тип события:

```
Fusb302EventToggleDone     { TOGSS }
Fusb302EventMessageRx      { sop_type, header, objects, count }
Fusb302EventMessageTxOk    ─ (I_TXSENT)
Fusb302EventMessageTxFail  ─ (I_RETRYFAIL)
Fusb302EventHardResetRx    ─ (I_HARDRST)
Fusb302EventHardResetSent  ─ (I_HARDSENT)
Fusb302EventVbusChanged    { vbus_ok }
Fusb302EventBcLvlChanged   { bc_lvl }
Fusb302EventCompChanged    { comp }
Fusb302EventCollision      ─ (I_COLLISION) — PRL должен retry
Fusb302EventGoodCrcSent    ─ (I_GCRCSENT) — обычно идёт с MessageRx, можно дедуплицировать
```

### 9.3 Связь с FURI

- L4 владеет одной `FuriEventLoop` или живёт на L3-event-loop (предпочтительнее — на L3, чтобы все события L3-thread).
- ISR от GPIO пушит signal в `FuriEventFlag` или `FuriMessageQueue`.
- В L3-event-loop подписка читает регистры через L4 API и публикует `Fusb302Event`-ы в L3-внутреннюю очередь.
- Никаких **прямых** I²C-вызовов из ISR.

---

## 10. Подводные камни

- **TOGGLE и AUTO_CRC одновременно**: пока TOGGLE=1, нет смысла в AUTO_CRC (нет partner-а). Включаем AUTO_CRC **после** TOGGLE settled и lock полярности.
- **PD_RESET сбрасывает MessageID counter** в железе FUSB302 — это значит после PD_RESET партнёр-MessageID counter в железе FUSB302 = 0, но **наш** программный counter надо тоже обнулить.
- **RX FIFO может переполниться**, если мы не вычитываем после I_GCRCSENT. FUSB302 dropит дальнейшие сообщения и поднимает I_ALERT + STATUS1.RX_FULL. PRL обязан читать RX в течение нескольких миллисекунд от I_GCRCSENT.
- **TX FIFO race в `pd_message_send`**: текущий код делает TX_FLUSH **перед** записью данных. Это правильно, но flush может race с активным retry — лучше дождаться `I_TXSENT` или `I_RETRYFAIL` предыдущей отправки.
- **`STATUS1A.TOGSS = 111` Audio Accessory** — нам в DRP-no-VDM режиме не интересно. Можно либо `CONTROL4.TOG_EXIT_AUD = 0` (стандартно — Audio Accessory считается settled), либо игнорировать в L3.
- **Re-initialize after PPM_RESET**: SW_RESET сбрасывает регистры в дефолт. Маски надо переустанавливать. Сейчас в `fusb302_init` это делается через `fusb302_start_drp_logic`, но `start_drp_logic` сам вызывается separately — двойная init не должна ломать.
- **VBUS measurement scale**: при `MEAS_VBUS=1` входное VBUS делится на 10 внутри. Т.е. MDAC LSB 42 мВ × 10 = 420 мВ "scaled". Для проверки 5В надо MDAC ≈ 0x0C (~5040 мВ effective).
- **CONTROL2.WAKE_EN**: для production wake-from-detach в low-power режиме — пока не используем. В default 0.

---

## 11. Что дальше

Когда [`type-c-sm.md`](type-c-sm.md) будет написана, она будет
ссылаться на этот документ в местах:
- "после I_TOGDONE начинаем debounce" — §1.1 + §2.1
- "lock полярности и переходим в Attached.*" — §1.1 + §2.1
- "VBUS detect через I_VBUSOK" — §1.8 + §2.4

[`prl-sm.md`](prl-sm.md) будет в основном про то, чего FUSB302 не делает
из §2.2.

[`pe-sm.md`](pe-sm.md) — про PD-сообщения, FUSB302 не появится почти
вообще, только через L3/L4 API.
