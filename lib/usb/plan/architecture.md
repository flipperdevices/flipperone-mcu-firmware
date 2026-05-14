# Архитектура библиотеки UCSI-PPM поверх FUSB302

Документ описывает внутреннее устройство библиотеки, реализующей PPM
(UCSI 3.0) для одного коннектора, где CC-PHY — FUSB302. Уровень —
концептуальный: слои, их обязанности, границы, потоки данных. Без
сигнатур функций — они уйдут в `api.md` после согласования архитектуры.

Связанные документы:
- [`commands.md`](commands.md) — внешний контракт (набор UCSI-команд).

Допущения, на которых построен этот документ (из обсуждения):
- **Один коннектор** на инстанс библиотеки.
- **Транспорт абстрактен**: в библиотеку приходят «сырые байты» — записи и чтения по offset-у регистрового файла, без знания о I²C/HID/RPC.
- **v1 = обязательная часть** из §4 [`commands.md`](commands.md); остальное завершается `Not Supported`.
- **Готовность к alt-modes** — диспатчер таблично-управляемый, чтобы добавление AM-команд не требовало структурных правок.
- **Язык — C**.

---

## 1. Слои

```
┌──────────────────────────────────────────────────────────────────┐
│                              OPM                                 │
│           (Linux-драйвер, наш — за пределами библиотеки)         │
└────────────────────────────────────────┬─────────────────────────┘
                                         │  raw bytes по offset-у регистрового файла
                                         │  (read/write VERSION/CCI/CONTROL/MSG_IN/MSG_OUT)
                                         │  + alert наружу
─────────────────────────────────────────│─────────────────────────  библиотека ↓
                                         │
                  ┌──────────────────────▼──────────────────────┐
                  │              L1. Transport adapter          │
                  │   (тонкий мост между байтовым API и core;   │
                  │    парсит запись в CONTROL → событие cmd)   │
                  └──────────────────────┬──────────────────────┘
                                         │
                  ┌──────────────────────▼──────────────────────┐
                  │                 L2. PPM core                │
                  │  - регистровый файл (VERSION/CCI/CONTROL/   │
                  │    MSG_IN/MSG_OUT) — owner                  │
                  │  - таблица диспатча команд (opcode → hdlr)  │
                  │  - state-machine PPM (Idle/Busy/WaitAck/…)  │
                  │  - notification subscriptions (mask)        │
                  │  - очередь асинхронных событий              │
                  │  - таймауты, тики                           │
                  └──────────────────────┬──────────────────────┘
                                         │   внутренний API
                                         │   (типизированные команды/события)
                  ┌──────────────────────▼──────────────────────┐
                  │              L3. LPM (port logic)           │
                  │  - Type-C state machine                     │
                  │  - PRL (Protocol Layer)                     │
                  │  - PE (Policy Engine, source/sink/DRP)      │
                  │  - PDO репозиторий (current/advertised/max) │
                  │  - power readings (peak/avg/voltage)        │
                  │  - кэш partner/cable identity, alt-modes    │
                  │    (последнее — заглушка в v1)              │
                  └─────┬─────────────────┬─────────────────────┘
                        │                 │ PHY-уровень
                        │                 │ (CC detect, BMC TX/RX,
                        │                 │  регистры FUSB302)
                        │                 ▼
                        │   ┌─────────────────────────────────────┐
                        │   │       L4. FUSB302 driver            │
                        │   │  - I²C-доступ к регистрам FUSB302   │
                        │   │  - извлечение событий из INT/INTa/INTb │
                        │   │  - upload PD-сообщений в TX/FIFO    │
                        │   │  - download PD-сообщений из RX/FIFO │
                        │   └─────────────────────────────────────┘
                        │
                        │ direct measurement bypass
                        │ (PE→L4 для MDAC threshold checks
                        │  в PR_Swap / Hard Reset transitions)
                        ▼
              (см. примечание ниже про прямой PE→L4 access)
```

Внешние зависимости (callbacks, инжектируемые caller-ом библиотеки):
- **Time provider** — возвращает монотонное время в миллисекундах.
- **Alert sink** — функция, которую библиотека дёргает, когда OPM-у есть что прочитать (изменение в CCI).
- **I²C HAL** — read/write для FUSB302.
- **GPIO HAL** — линия INT от FUSB302, плюс VBUS source enable/disable, опциональный discharge.
- **Power supply controller** — `power_supply_set(voltage_mv, current_ma)` для установки PD-target напряжения как Source; обратный callback `power_supply_ready` когда PSU settled. Если в системе только vSafe5V Fixed без поддержки переключения уровня — `power_supply_set` для других voltage возвращает error, и Source advertise только 5V PDO.
- **Power source availability flag** — `has_alternative_power_source()` (bool). Сообщает PE, есть ли альтернативный источник питания (battery/USB charger на другом порте). Используется для policy-decision: можно ли делать Hard Reset как Sink или дропать VBUS как Source без риска вырубиться.

Библиотека сама **никогда** ничего не вызывает по своей воле — всё работает
по событию «caller дёрнул нас» (запись в регистр, FUSB302-IRQ, тик
таймера). Это даёт чистую переносимость между голым железом, FreeRTOS,
Zephyr и Linux user-space.

> **Прямой доступ PE→L4 для measurement**. Большинство вызовов L3→L4 идут через
> Type-C SM или PRL. Но для voltage measurement через MDAC (например, vSafe0V/vSafe5V
> detection в PR_Swap / Hard Reset transitions) PE обращается к L4 **напрямую**.
> Это нарушение «строгой» иерархии слоёв, но конкретная необходимость:
> Type-C SM не имеет логики PR_Swap, а PE нужен точный мониторинг в этот момент.

---

## 2. L1. Transport adapter

**Цель**: превратить байтовый интерфейс OPM↔PPM в типизированные события
для L2.

**Что делает**:
- Хранит ничего своего; вся память — в L2 (регистровый файл).
- На `register_write(offset, len, buf)`:
  - копирует байты в нужное место регистрового файла L2;
  - если запись попала в `CONTROL` (offset 8..15) **и включает байт-0 (поле Command)** — поднимает событие «новая команда» в L2.
- На `register_read(offset, len, buf)`: копирует байты из регистрового файла.
- Никакой бизнес-логики; никакого знания о командах.

**Почему отдельный слой**: чтобы L2 не знал ничего про «писали 8 байт по одному / писали целиком», «выровнено / нет», «частичное чтение MSG_IN». Эта политика — в L1.

**Granularity записи**. UCSI-spec говорит, что запись `CONTROL` инициирует
команду. На практике OPM может писать байты не атомарно. Договорённость L1:
команда считается отправленной, когда **записан байт с offset = 8** (поле
Command) с ненулевым значением. Всё, что записано выше до этого момента,
рассматривается как часть текущей команды.

---

## 3. L2. PPM core

Сердце библиотеки. Владеет регистровым файлом, исполняет команды, общается
с L3.

### 3.1 Регистровый файл

Структура совпадает с Table 4-1 UCSI 3.0. Адреса фиксированы:

| Offset | Размер | Имя         | Кто пишет                       | Кто читает |
|-------:|:-------|:------------|:--------------------------------|:-----------|
| 0      | 3 B    | VERSION     | L2 (один раз на старте)         | OPM        |
| 3      | 1 B    | RESERVED    | —                               | —          |
| 4      | 4 B    | CCI         | L2                              | OPM        |
| 8      | 8 B    | CONTROL     | OPM (через L1)                  | L2         |
| 16     | 255 B  | MESSAGE_IN  | L2 (в обработчике команды)      | OPM        |
| 271    | 1 B    | RESERVED    | —                               | —          |
| 272    | 255 B  | MESSAGE_OUT | OPM (через L1)                  | L2         |
| 527    | 1 B    | RESERVED    | —                               | —          |

Lifecycle регистрового файла:

1. **Power-On / `ucsi_init()`**: L2 заполняет `VERSION = 0x0300` (UCSI 3.0); `CCI = 0`, `MESSAGE_IN` обнулён, `MESSAGE_OUT` обнулён. Состояние PPM: `Idle`.
2. **Operating**: OPM пишет `CONTROL` → L2 диспатчит → L2 заполняет `CCI`/`MESSAGE_IN` → выставляет alert → OPM читает.
3. **PPM_RESET**: L2 сбрасывает себя и L3 (включая LPM-state и FUSB302 reset). `VERSION` остаётся, `MESSAGE_IN/OUT` обнуляются, `CCI` ставится в финальное состояние PPM_RESET-команды (`Reset Completed = 1`).

### 3.2 Таблица диспатча команд

В L2 живёт массив из 35 элементов (по числу opcode-ов 0x00..0x22),
индексируемый opcode-ом:

```
   opcode → { applicability, handler_fn_ref, response_data_length }
```

- `applicability` — флаг: «N (поддерживаем)» / «NS (отвечаем Not Supported)» / «UNKNOWN (Unrecognized)».
- `handler_fn_ref` — указатель на обработчик в L2 (часть обработчиков делегирует в L3).
- `response_data_length` — статическое поле для команд с фиксированной длиной ответа (для GET_CAPABILITY=16, GET_CONNECTOR_CAPABILITY=4 и т.п.). Для команд с переменной длиной — `0xFF` (заполняется в runtime).

**Расширение под alt-modes** = поменять `applicability` пяти строк
(GET_ALTERNATE_MODES, GET_CAM_SUPPORTED, GET_CURRENT_CAM, SET_NEW_CAM,
GET_CAM_CS) с `NS` на `N` и реализовать handler-ы. Никакая другая часть
архитектуры не трогается.

### 3.3 Машина состояний PPM

```
                  ┌──────────────────────────────────────┐
                  │                                      │
                  │              ┌──────────┐            │
   PPM_RESET ───►│              │   Idle   │◄───────────┤
                  │              └─────┬────┘            │
                  │                    │                 │
                  │     OPM writes CONTROL.Command       │
                  │                    │                 │
                  │                    ▼                 │
                  │           ┌────────────────┐         │
                  │           │  Processing    │         │
                  │           └────────┬───────┘         │
                  │                    │                 │
                  │      ┌─────────────┼─────────┐       │
                  │      │             │         │       │
                  │   fast path    long path   error     │
                  │      │             │         │       │
                  │      ▼             ▼         ▼       │
                  │  Set CCI       ┌─────────┐  Set CCI  │
                  │  (Cmd          │  Busy   │  (Error)  │
                  │   Completed)   └────┬────┘           │
                  │      │              │                │
                  │      │      ...completion in L3…     │
                  │      │              │                │
                  │      │              ▼                │
                  │      │         Set CCI               │
                  │      │       (Cmd Completed)         │
                  │      │              │                │
                  │      ▼              ▼                │
                  │  ┌────────────────────────┐          │
                  │  │      WaitForAck        │          │
                  │  └────────────┬───────────┘          │
                  │               │                      │
                  │  OPM sends ACK_CC_CI                  │
                  │               │                      │
                  └───────────────┴──────────────────────┘
```

Состояния:

- **Idle** — `CCI.Command Completed = 0`. Можно принимать команду.
- **Processing** — короткое транзитное состояние внутри одной «итерации» библиотеки: handler выполняется (синхронно для большинства команд).
- **Busy** — handler решил, что обработка займёт > `MIN_TIME_TO_RESPOND_WITH_BUSY` (190 мс). CCI: `Busy = 1`, остальные биты 0. L2 опрашивает L3 на каждом тике.
- **WaitForAck** — `CCI.Command Completed = 1`, alert поднят. L2 ждёт `ACK_CC_CI` от OPM, прежде чем вернуться в Idle. Без ACK новые команды (кроме `PPM_RESET`/`CANCEL`/`ACK_CC_CI`) принимаются, но это плохая практика OPM-а.

Особые пути:
- **PPM_RESET** валиден всегда: из любого состояния → Idle (через transient «reset in progress», где CCI выставляется один раз с `Reset Completed=1`).
- **CANCEL** валиден только из Busy: переводит обратно в Idle через CCI с `Cancel Completed=1`.

### 3.4 Команды без ответа в MESSAGE_IN

Большинство SET_-команд просто завершаются с `Command Completed=1, Data
Length=0`. GET_-команды и `GET_ERROR_STATUS` пишут в MESSAGE_IN и
выставляют `Data Length`.

### 3.5 Notification subscriptions

Маска нотификаций (17 бит, см. §2.5 в commands.md) живёт в L2 как
отдельное поле; меняется только обработчиком `SET_NOTIFICATION_ENABLE`.
Используется в очереди событий (§4) для фильтрации.

---

## 4. Асинхронные события и Connector Change Indicator

UCSI имеет ровно один канал асинхронных событий — `CCI.Connector Change
Indicator` (7-бит, номер коннектора, на котором что-то произошло) плюс
сопутствующие индикаторы (`Security Request`, `FW Update Request`).
Детали изменения OPM получает через `GET_CONNECTOR_STATUS` (битмап
Status Change).

### 4.1 Внутренняя очередь событий

L3 (LPM) генерирует события (подключение партнёра, PD-контракт, error,
alt-mode entered). Эти события:
1. Транслируются в **bitmap изменений** (Table 6-44 — Connector Status Change), который накапливается в LPM state до следующего `GET_CONNECTOR_STATUS`.
2. Добавляются в очередь L2 на доставку OPM-у.

**Механизм publication L3→L2**: общий `FuriMessageQueue<LpmEvent>`, который
L2 опрашивает на каждом tick (см. §5). LpmEvent — sum type:
`{attached, detached, partner_changed, power_op_mode_changed, power_direction_changed,
negotiated_power_level_changed, sink_path_status_changed, pd_reset_complete,
battery_charging_status_changed, supported_provider_caps_changed, error(info)}`.
L2 транслирует каждое событие в соответствующий бит Connector Status Change
bitmap-а (см. Table 6-44 в [`commands.md`](commands.md) §2.17).

### 4.1.1 Broadcast events внутри L3

Некоторые события L4 интересны **нескольким** компонентам L3:

- **`BcLvlChanged`** — слушают:
  - **PRL** для SinkTx collision avoidance (см. [`prl-sm.md`](prl-sm.md) §7).
  - **Type-C SM** для определения partner Rp current → `Power Operation Mode` в `GET_CONNECTOR_STATUS`.
- **`VbusChanged`** — слушают:
  - **Type-C SM** для attach/detach detection.
  - **PE** во время Hard Reset / PR_Swap для определения vSafe0V / vSafe5V переходов.
- **`CompChanged`** — слушают:
  - **Type-C SM** для detach detection в Attached.SRC.
  - **PE** для voltage threshold crossings (PR_Swap vSafe0V detect).

Реализация: один `FuriPubSub` или несколько subscribers на общем event
queue. Конкретная форма — деталь имплементации.

L2 каждый тик берёт из очереди следующее событие; если у нас сейчас
**Idle** или **WaitForAck**:
- выставляет `CCI.Connector Change Indicator = <port>` (т.е. 1, у нас один порт);
- поднимает alert наружу;
- считает событие доставленным после `ACK_CC_CI` с `Connector Change Acknowledge=1`.

### 4.2 Коллизия «событие во время команды»

Если событие пришло пока мы в **Busy** или в середине обработки
команды — спека разрешает выставить `CCI.Connector Change Indicator` в
финальном CCI команды одновременно с `Command Completed=1`. То есть один
CCI несёт и завершение команды, и информацию об асинхронном событии.
ACK_CC_CI имеет отдельные биты для подтверждения каждого из двух — OPM
может подтвердить только команду, только событие, или оба сразу.

### 4.3 Фильтрация по маске

События, отключённые в `SET_NOTIFICATION_ENABLE` (биты маски =0), L2
**не** показывает в CCI и не добавляет в очередь. LPM-bitmap-change
накапливается **всегда** (он часть состояния порта), но «всплытие» к OPM
гасится маской.

---

## 5. Время и тики

Библиотека не владеет таймером. Caller обязан:
- предоставить функцию «текущее время в мс»;
- регулярно дёргать `ucsi_tick()` (хотя бы раз в 10–50 мс), при поднятии FUSB302-IRQ, и при записи в CONTROL.

Внутри тика L2 делает:
1. Прокачивает FUSB302 (читает IRQ-флаги через L4, если был сигнал).
2. Прокачивает L3 (PD state machine продвигается, если есть события).
3. Если в **Busy** — проверяет, не завершилась ли отложенная команда.
4. Проверяет таймауты:
   - `MIN_TIME_TO_RESPOND_WITH_BUSY` (190 мс): команда висит дольше — выставляем Busy.
   - `SENDER_RESPONSE_TIMEOUT` (60 мс): между чанками SET_PDOS — не релевантно в v1 (broadcast/aggregation мы не делаем).
   - таймауты PD (tSenderResponse, tDataReset и т.п.) — внутри L3.
5. Достаёт следующее событие из очереди (§4.1).

«Тик» — единственная точка cooperative-многозадачности. Это держит
архитектуру single-threaded и предсказуемой.

---

## 6. Внутренний API L2 ↔ L3

L3 (LPM) — это **типизированный сервис**, который L2 использует для
исполнения команд и от которого получает события.

### 6.1 Команды L2 → L3 (примеры, не сигнатуры)

Каждый из этих вызовов — синхронный или возвращающий «in progress»:

- *get connector status* → структура (Status Change bitmap, Power Operation Mode, Connect Status, Power Direction, Partner Flags/Type, RDO, Battery Charging Status, Provider Cap Limited Reason, bcdPDVersion, Orientation, Sink Path Status, RCP Status, power readings).
- *get connector capability* → структура (Operation Mode bitmap, Provider/Consumer/Swap flags, Extended Operation Mode, Misc, RCP support, Partner PD Revision).
- *get pdos* → массив PDOs с параметрами (которые из source-cap-типов, range SPR/EPR, offset, count, partner или own).
- *get cable property* → структура (speed, current, VBUS-in-cable, type, plug end, mode-support, PD revision, latency).
- *get error status* → битмап ошибок + vendor-defined хвост.
- *set cc operation mode* → результат (ok / not supported / error code).
- *set usb operation role* → результат с возможным «нужно сделать DR-swap, partner отказал».
- *set power direction role* → аналогично.
- *set power level* → ok/error.
- *set sink path* → ok/error.
- *set pdos* → ok/error (с учётом валидации).
- *connector reset (hard/data)* → стартует процесс, далее «in progress», после завершения — событие.
- *read power level* → стартует измерение; результат — через очередь событий (Power Reading Ready) и следующий get-connector-status.

### 6.2 События L3 → L2

LPM публикует события, L2 их превращает в Connector Status Change bits и
CCI-нотификации:

- *connect change* — устройство подключилось/отключилось.
- *partner change* — изменился тип партнёра.
- *power op mode change* — теперь BC/PD/Type-C-3A/etc.
- *power direction change* — PR-swap.
- *negotiated power level change* — новый RDO.
- *supported provider capabilities change* — пересмотрели свои PDOs.
- *battery charging status change*.
- *PD reset complete* — партнёр сделал Hard Reset.
- *attention received* — VDM Attention (под alt-modes; в v1 копится, не доставляется).
- *error* — ошибка на коннекторе (детализируется в Error Information).
- *sink path status change*.
- *power reading ready* — измерения готовы.

### 6.3 Владение MESSAGE_IN / MESSAGE_OUT

В v1 модель простая:
- L3 возвращает в L2 типизированные структуры.
- L2 сам марашалит их в байты MESSAGE_IN по разметке UCSI (§2.* в commands.md).
- Для MESSAGE_OUT обратно: L2 парсит байты в типизированную структуру и передаёт в L3.

Когда дойдём до chunked-команд (FW Update / Security / SET_PDOS чанками), модель усложнится: L3 сможет писать прямо в окно MESSAGE_IN через предоставленный L2 указатель. Это — задел, не v1.

---

## 7. Внутренний API L3 ↔ L4

L4 (FUSB302 driver) — это **PHY-уровень**. Никакой PD-логики там нет;
только знание регистровой карты микросхемы и интерпретация её событий.

### 7.1 Вызовы L3 → L4

- *init* — Power-On reset, базовая конфигурация (mask, switches, measure, control).
- *set cc role* — выставить Rp/Rd/DRP (источник, потребитель, dual-role) с нужными Rp-current (default/1.5/3A).
- *set vconn* — включить/выключить VCONN на нужном CC-pin.
- *set switches* — выбор полярности (какой CC активен), пропуск BMC через нужный CC.
- *transmit pd message* — записать в TX-FIFO заголовок+payload, инициировать передачу.
- *flush tx*, *flush rx*.
- *enable / mask interrupts* — какие события поднимают INT.
- *read measure* — измерение Rp/Rd/VBUS (для CC detection и power readings).
- *hard reset* — отправить Hard_Reset.
- *bist* — для compliance/тестов (опционально).

### 7.2 События L4 → L3

Поднимаются по INT и читаются L3 на тике:

- *CC change* — CC1/CC2 detect-уровень изменился (включая «cable plug в, плюс полярность»).
- *VBUS event* — VBUS detected/lost/crossed-threshold.
- *PD message received* — байты доступны в RX-FIFO.
- *PD transmit success / fail* — GoodCRC получен или нет, или Hard_Reset/Cable_Reset.
- *crc error*, *retry exceeded*.
- *over-current / over-voltage* (если поддерживается).

### 7.3 Что L4 **не** знает

L4 не знает про PD-стейт-машину, про SOP'/SOP'', про PDO/RDO, про
alt-modes, про UCSI-команды. Это всё в L3.

---

## 8. Concurrency и контексты

Библиотека спроектирована как **single-context cooperative** state-machine.

**Контракты**:
- Все публичные API библиотеки (включая регистровые read/write от L1 и tick) **не реентрабельны**. Caller обеспечивает сериализацию.
- Внутри одного вызова библиотека не «спит» и не блокируется в ожидании I/O. Если нужна задержка — она запоминается как deadline и проверяется на следующем тике.
- I²C-операции к FUSB302 в v1 — **блокирующие, но короткие** (миллисекунды). Это упрощает L4 в разы; цена — пока идёт I²C, мы держим caller-а. На RP2350 это приемлемо.

**Контексты, из которых caller может дёрнуть API**:
- *Thread / main loop* — основной случай. tick + транспорт.
- *ISR* — FUSB302-IRQ. Caller может либо обработать в bottom-half (дешевле всего), либо дёрнуть `ucsi_tick()` прямо из ISR. Главное — не пересечься с уже идущим вызовом из другого контекста.

**Что не делаем в v1**: межпроцессорные мьютексы, lock-free очереди,
async I²C. Если позже понадобится non-blocking I²C (например, переход на
DMA) — это правка только L4 и интерфейса L3→L4.

---

## 9. Обработка ошибок

UCSI-ошибки имеют два уровня:
1. **CCI.Error Indicator** — двоичный «команда не выполнилась».
2. **GET_ERROR_STATUS** — 16-битный битмап причины + vendor-defined хвост.

Внутри библиотеки:
- L3 при возврате результата команды возвращает либо «ok», либо «error со списком бит».
- L2 хранит **последний error bitmap** в своей памяти. Этот bitmap сбрасывается:
  - после `ACK_CC_CI`, подтвердившего событие/команду с ошибкой;
  - после `PPM_RESET`;
  - **не сбрасывается** простым следующим успешным GET-запросом — это важно для механики «OPM может прочитать details позже».
- Команды с собственной ошибочной семантикой (например, `SET_SINK_PATH` в source-mode → `Set Sink Path Rejected`) попадают сюда же.

Любая ошибка от L4 (I²C failure) → транслируется в L3 как «CC communication error» → в L2 как Error Indicator + соответствующий бит в Error Information.

---

## 10. Точки расширения

1. **Alt-modes** (пять команд) — добавляем строки в таблицу диспатча; в L3 — модуль «alt-mode manager» (хранит SVID/MID кэш партнёра, текущий AM, конфигурация DP/TBT). Биты в `bmAttributes` (Alternate Mode) и `bmOptionalFeatures` (Alt-mode details/override) выставляем.
2. **PD-introspection** (`GET_PD_MESSAGE`, `GET_ATTENTION_VDO`) — добавляем кэш последних PD-response-ов в L3.
3. **Vendor Defined Command** — отдельный регистр обработчиков с диспатчем по VID:Vendor-cmd; вытаскивается через linker section или регистрацию из caller-а.
4. **Multi-port** — выносим LPM в массив, диспатчер по `Connector Number` в L2. Это **большая** правка: меняется владение регистровым файлом MESSAGE_IN/OUT (один или per-port), очередь событий (per-port или общая), `bNumConnectors > 1`. В v1 явно зафиксировали — один порт.
5. **Транспорт UCSI-over-I²C / HID / другое** — L1 заменяется/дополняется; L2..L4 не трогаются.
6. **Re-timer / Security / FW Update** — каждое — отдельный модуль в L3 и снятие `NS` с соответствующих строк диспатчера. Эти команды chunked → требуют доработки модели владения MESSAGE_IN/OUT (см. §6.3).

---

## 11. Что осталось открытым

Дальше нужно решить (отдельно для каждого):

- **L4 driver**: на чём он работает (CMSIS-HAL / Pico SDK / абстрактный HAL). Это влияет на размер задела «переноса между платформами».
- **PD policy engine в L3**: писать с нуля или использовать существующий (TI tcpci, libtypec, FUSB302-Linux-driver port). Если с нуля — это самый трудоёмкий компонент.
- **Тестирование**: где граница «unit-тестов без железа». Естественные точки: L1/L2 — полностью unit-тестируемы (regfile + state-machine, без FUSB302). L3 — частично (mock L4). L4 — только integration.
- **Логирование**: формат, уровни, transport. Скорее всего — callback наружу, как и alert.

Эти пункты — материал для следующих документов в `lib/usb/plan/`.
