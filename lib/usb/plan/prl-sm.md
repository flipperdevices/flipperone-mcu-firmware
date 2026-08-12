# Protocol Layer (PRL) State Machine

Документ описывает Protocol Layer — тонкий слой между Policy Engine
(PE) и FUSB302. С учётом того, что FUSB302 делает в железе
auto-GoodCRC, auto-retry, BMC encoding и CRC32 (см.
[`fusb302.md`](fusb302.md) §1), PRL получается **очень тонким**:
основная работа — MessageID counter, duplicate detection, SinkTx
collision avoidance и Soft_Reset semantics.

Связанные:
- [`pd-scope.md`](pd-scope.md) — что мы реализуем, что NS. **Extended Messages — все NS**, поэтому chunking-receiver-логика не нужна.
- [`fusb302.md`](fusb302.md) — что hardware делает за нас. PRL очень рассчитывает на §1.2 (AUTO_CRC) и §1.3 (AUTO_RETRY).
- [`pe-sm.md`](pe-sm.md) — PRL обслуживает PE.

---

## 1. Что PRL делает / не делает

### 1.1 Что **не делает** PRL (всё в железе FUSB302)

| Функция                                        | FUSB302 регистр / событие                         |
|:-----------------------------------------------|:--------------------------------------------------|
| BMC encoding / decoding                        | PHY                                               |
| CRC32 compute / check                          | PHY                                               |
| Auto-GoodCRC на приём правильного сообщения    | `SWITCHES1.AUTO_CRC=1`; event `I_GCRCSENT`        |
| Auto-retry с CRCReceiveTimer (tReceive)        | `CONTROL3.{AUTO_RETRY, N_RETRIES}`; event `I_TXSENT` / `I_RETRYFAIL` |
| Hard Reset BMC signaling                       | `CONTROL3.SEND_HARD_RESET`; event `I_HARDSENT`    |
| Hard Reset reception                           | event `I_HARDRST`                                 |
| HardResetCompleteTimer (tHardResetComplete)    | PHY                                               |
| Message framing (SOP/SYNC/PACKSYM/JAMCRC/EOP)  | TX FIFO tokens                                    |
| Auto Soft_Reset / Hard_Reset на retry-fail     | **отключаем** (`AUTO_SOFTRESET=0, AUTO_HARDRESET=0`) — это policy-decision PE |

### 1.2 Что **делает** PRL (мы пишем)

| Функция                                                 | Где                                             |
|:--------------------------------------------------------|:------------------------------------------------|
| MessageIDCounter (наш TX counter)                       | software, 3-bit, инкремент после `I_TXSENT`     |
| Stored MessageID (последний полученный)                 | software, для duplicate detection               |
| Duplicate detection on RX                               | сравнение `header.MessageID` со stored          |
| Soft_Reset semantics (MessageID=0 в обе стороны)        | software                                        |
| Collision handling (`I_COLLISION`)                      | software, retry TX после resolved collision     |
| SinkTx (tSinkTx) collision avoidance                    | software timer + BC_LVL мониторинг              |
| Reporting TX outcome to PE                              | events `MessageSent / MessageFailed / Discarded`|
| Reporting RX message to PE                              | event `MessageReceived`                         |
| Reset of all PRL state on Soft_Reset / Hard Reset / Disconnect | software                                |
| PD-revision tracking (что у партнёра, что мы шлём)      | software, см. §8                                |
| Forwarding raw RX FIFO bytes в типизированный struct    | software (`fusb302_pd_message_receive` уже есть)|

### 1.3 Чего **нет** в PRL v1

- **Chunking receiver / sender** — мы все Extended Messages отвечаем NS, см. [`pd-scope.md`](pd-scope.md) §1.3. Нет ChunkSenderRequest/ResponseTimer.
- **SOP'/SOP'' handling** — cable discovery не делаем. `CONTROL1.ENSOP1=0, ENSOP2=0` (по дефолту). Если каким-то образом прилетит SOP'-message — игнор.
- **BIST receive mode** — `CONTROL3.BIST_TMODE=0` (по дефолту).
- **Discard logic для AMS** — TCPMv2 имеет сложную логику Message Discarding (Table 7.3 в [USBPD]); у нас простая: collision → retry, всё.

---

## 2. PRL TX state machine

Канонический PRL_Tx из спеки имеет ~10 состояний; с FUSB302 нам нужно
**3 состояния**:

```
                  ┌──────────────┐
                  │     IDLE     │◄────────────┐
                  └──────┬───────┘              │
        PE: send(msg)    │                      │
                         ▼                      │
                  ┌──────────────┐              │
                  │ CONSTRUCTING │              │ I_TXSENT (success)
                  │  (build hdr, │              │ → notify PE: ok
                  │   write FIFO,│              │ → inc MessageIDCounter
                  │   TXON)      │              │
                  └──────┬───────┘              │
                         │                      │
                         ▼                      │
                  ┌──────────────┐              │
                  │  WAITING_FOR │──────────────┤
                  │   TX_RESULT  │              │
                  └──────┬───────┘              │
                         │ I_RETRYFAIL          │
                         │ → notify PE: failed  │
                         │ → (PE решает: Soft   │
                         │    Reset или Hard)   │
                         │                      │
                         │ I_COLLISION          │
                         │ → retry TX (back to  │
                         │   CONSTRUCTING)      │
                         │                      │
                         │ I_HARDRST received   │
                         │ → notify PE          │
                         │ → reset PRL state    │
                         └──────────────────────┘
```

### 2.1 IDLE

**Когда**: ничего не отправляем; ожидаем команды от PE.

**Принимаемые события**:
- `PE: tx_request { sop_type, header, objects[N] }` → если sink-role и tSinkTx ещё не истёк → отложить (см. §7); иначе → **CONSTRUCTING**.
- `RX message arrived` (через PRL_Rx) — отдельный pipe, не блокирует TX.
- `PE: send_hard_reset` → особый путь, см. §5.

### 2.2 CONSTRUCTING

**Действия** (всё внутри одной итерации event-loop, без await):
1. Заполнить header.MessageID = текущее значение `tx_message_id_counter`.
2. Заполнить header: PowerRole / DataRole / SpecRev — из Type-C SM.
3. Сформировать TX-buffer: `SYNC1 SYNC1 SYNC1 SYNC2`(SOP) или варианты для SOP'/SOP''; `PACKSYM | (5-bit len)`; header bytes; N×4 object bytes; `JAMCRC EOP TXOFF TXON`.
4. `fusb302_pd_tx_flush()` (без блокирующего ожидания, см. [`fusb302.md`](fusb302.md) §8.2 — надо переделать).
5. Запись buffer-а в `FIFOS` (адрес 0x43) одним I²C burst-write.
6. Запомнить: «pending tx = { sop_type, header, objects }».
7. → **WAITING_FOR_TX_RESULT**.

### 2.3 WAITING_FOR_TX_RESULT

**Принимаемые события**:
- `I_TXSENT` → успех. Notify PE: `MessageSent { sop_type, header }`. Inc `tx_message_id_counter` (modulo 8). → **IDLE**.
- `I_RETRYFAIL` → retry exhausted. Notify PE: `MessageFailed { sop_type, header, reason=retry_fail }`. Inc `tx_message_id_counter` (per spec — даже на failure). → **IDLE**. PE дальше решает Soft_Reset / Hard_Reset.
- `I_COLLISION` (партнёр начал TX пока мы пытались) → FUSB302 abort-ит наш TX, прерывает retry counter. Действие: подождать пока `I_GCRCSENT` подтвердит прием от партнёра (т.е. наша коллизия → их TX в нашу сторону прошло), потом retry → **CONSTRUCTING** (с тем же MessageID, потому что ничего не было отправлено).
- `I_HARDRST` received → отменить pending TX. Notify PE: `MessageFailed { reason=hard_reset_received }`. → **IDLE** (PE дальше сбросит state).
- PE: `cancel_tx` → отменить (TX_FLUSH в железе). Inc `tx_message_id_counter` (spec — abort инкрементирует). → **IDLE**.

---

## 3. PRL RX state machine

RX почти не имеет состояний — он реактивный, всю работу делает FUSB302.

```
        ┌─────────────────┐
        │  RX_PASSIVE     │
        └────────┬────────┘
       I_GCRCSENT│ (FUSB302 ответил GoodCRC партнёру)
                 ▼
        ┌─────────────────┐
        │ READ_FIFO       │
        │ - read SOP token│
        │ - read header   │
        │ - read N obj    │
        └────────┬────────┘
                 │
                 ▼
        ┌─────────────────┐
        │ CHECK_MSGID     │
        │ - сравнить с    │
        │   stored        │
        └────────┬────────┘
                 │
        ┌────────┴────────────┐
        │                     │
   duplicate              new message
        │                     │
        ▼                     ▼
   DROP                  STORE_MSGID
   (already               + FORWARD_TO_PE
    handled,
    GoodCRC                  │
    автоматически             ▼
    послан HW)            RX_PASSIVE
```

### 3.1 Логика обработки RX

При `I_GCRCSENT`:

1. Прочитать `STATUS1.RX_EMPTY` — если empty, baseline: ничего делать (но обычно есть message).
2. Прочитать SOP token из FIFO. Декодировать в `sop_type`.
3. Прочитать 2 байта header (little-endian).
4. Извлечь `NumberOfDataObjects = (header >> 12) & 0x7`.
5. Прочитать `4 × N` байт data objects.
6. Если SOP-type ≠ SOP (т.е. SOP'/SOP''/SOP'_DEBUG/SOP''_DEBUG):
   - Cable discovery мы не делаем → drop с логированием.
7. Извлечь `MessageID = (header >> 9) & 0x7`.
8. Извлечь `SpecRev = (header >> 6) & 0x3` — обновить tracking партнёра (см. §8).
9. Если `header & SoftResetMessageTypeMatch` (Control type 0x0D и NDO=0):
   - **Special case**: Soft_Reset всегда обрабатывается, MessageID игнорируется. Сбросить counters в нашу сторону (`tx_message_id_counter=0`, очистить stored RX MessageID). Forward в PE.
10. Иначе: сравнить `MessageID == stored_rx_message_id`?
   - Если **да** (и это не первый принятый message after reset) → **duplicate**, drop. FUSB302 уже отправил GoodCRC.
   - Если **нет** → сохранить `stored_rx_message_id = MessageID`, forward в PE.

### 3.2 Forward в PE

Опубликовать в очередь PE типизированное событие:
```
RxMessageEvent {
    sop_type    : Sop,
    header      : MessageHeader (parsed: msg_type, extended, ndo, msg_id, power_role, data_role, spec_rev),
    objects     : [u32; N],
}
```

PE сама различит Control / Data / Extended и обработает соответственно.

### 3.3 Extended Messages на приёме

Extended bit (header bit 15) поднят → PE отвечает Not_Supported
(см. [`pd-scope.md`](pd-scope.md) §1.3). PRL **сам** в Not_Supported не
вмешивается — это PE-обязанность.

---

## 4. MessageID counter mechanics

### 4.1 Состояние

```
tx_message_id_counter        : u3    // наш счётчик для исходящих
stored_rx_message_id         : u3 | None  // последний принятый MID; None после reset
```

### 4.2 Правила (из [USBPD] §7.32)

**TX**:
- Перед отправкой: `header.MessageID = tx_message_id_counter`.
- После `I_TXSENT` (GoodCRC получен): `tx_message_id_counter = (tx_message_id_counter + 1) & 7`.
- После `I_RETRYFAIL`: то же — `tx_message_id_counter += 1`. (Spec: aborted message инкрементирует.)
- После `PE: cancel_tx`: то же.
- После **получения** Hard Reset: `tx_message_id_counter = 0`.
- После **отправки** Hard Reset: `tx_message_id_counter = 0`.
- После **получения** Soft_Reset: `tx_message_id_counter = 0`.
- После **отправки** Soft_Reset: `tx_message_id_counter = 0` (Soft_Reset сам имеет MessageID=0).

**RX**:
- При первом принятом сообщении после reset: `stored_rx_message_id = header.MessageID`. Forward в PE.
- При следующем: если `header.MessageID == stored_rx_message_id` → duplicate, drop. Если другой → `stored_rx_message_id = new`, forward в PE.
- Hard Reset received → `stored_rx_message_id = None`.
- Hard Reset sent → `stored_rx_message_id = None`.
- Soft_Reset received: специальная команда (всегда MessageID=0), forward в PE без duplicate-check; затем `stored_rx_message_id = 0` (или None и следующий message пересохранит).

### 4.3 Why "duplicate detection"?

Сценарий: партнёр отправил M, FUSB302 принял, выслал GoodCRC, но GoodCRC
до партнёра не дошёл (потеря пакета). Партнёр retry-ит M с тем же
MessageID. FUSB302 снова отправит GoodCRC, и мы получим тот же message
дважды. Не должны обработать его повторно — отсюда сравнение MessageID.

---

## 5. Hard Reset и PRL

### 5.1 Hard Reset received (`I_HARDRST`)

1. Отменить pending TX (если есть).
2. Сбросить counters: `tx_message_id_counter = 0`, `stored_rx_message_id = None`.
3. `fusb302_pd_reset_logic()` — `RESET.PD_RESET=1`, flush FIFO в железе.
4. PRL state → IDLE (TX) и RX_PASSIVE (RX).
5. Notify PE: `HardResetReceived`. PE дальше решает что делать (для Sink — ждать VBUS bounce; для Source — сделать VBUS bounce и заново послать Source_Caps).

### 5.2 Hard Reset sent (`I_HARDSENT`)

1. Сбросить counters: те же, что выше.
2. `fusb302_pd_reset_logic()`.
3. PRL state → IDLE / RX_PASSIVE.
4. Notify PE: `HardResetCompleted`. PE дальше делает VBUS-orchestration.

### 5.3 PE: send_hard_reset (special path)

Не идёт через CONSTRUCTING/WAITING_FOR_TX_RESULT — Hard Reset
**не** PD-сообщение, а BMC-сигнал. PRL должен:

1. Отменить любой pending TX (TX_FLUSH).
2. Записать `CONTROL3.SEND_HARD_RESET=1`.
3. Дождаться `I_HARDSENT` → см. §5.2.

---

## 6. Soft_Reset и PRL

### 6.1 Soft_Reset received (control message type 0x0D, NDO=0)

Распознаётся в PRL_Rx (см. §3.1 step 9).

1. `tx_message_id_counter = 0`.
2. `stored_rx_message_id = 0` (или None — следующее сообщение поставит).
3. **Не делаем** PD_RESET в железе (не нужно — это software counters).
4. Forward в PE как обычное сообщение (PE отправит Accept в ответ).

### 6.2 Soft_Reset sent

PE говорит: «отправь Soft_Reset».

1. Перед отправкой: установить `tx_message_id_counter = 0`.
2. Header.MessageID = 0 (всегда).
3. Отправить как обычный Control message через PRL_Tx.
4. После `I_TXSENT`: `tx_message_id_counter = 1` (стандартный инкремент).
5. Также: `stored_rx_message_id = None` (ждём свежего message от партнёра).

---

## 7. SinkTx — collision avoidance (PD 3.0+)

PD 3.0 ввёл механизм, чтобы Sink не пытался начать AMS пока Source может
быть в середине отправки Source_Capabilities. Реализуется через
**advertised Rp current** от Source:

| Rp current advertised  | Значение                | BC_LVL у Sink |
|:-----------------------|:------------------------|:-------------:|
| 3.0A (330 μA pull-up)  | "SinkTxOk" — sink может| `11`          |
| 1.5A (180 μA pull-up)  | "SinkTxNG" — sink ждёт | `10`          |
| Default (80 μA)        | (legacy USB)            | `01`          |

### 7.1 Поведение Sink (нас, когда мы Sink)

- При `Fusb302EventBcLvlChanged`:
  - Если BC_LVL = 11 → `sink_tx_ok = true`. Если был timer, отменить.
  - Если BC_LVL = 10 → `sink_tx_ok = false`. Запустить SinkTxTimer (если не запущен), tSinkTx ≈ 18 мс. Когда timer expires, можно начать TX даже если BC_LVL ещё 10 (это safety net).
- При попытке TX:
  - Если `sink_tx_ok || SinkTxTimer expired` → можно TX.
  - Иначе → отложить TX (PRL остаётся в IDLE с pending request).

### 7.2 Поведение Source (нас, когда мы Source)

- Когда мы хотим, чтобы sink не инициировал AMS (например, мы готовимся отправить Source_Capabilities) → выставить `CONTROL0.HOST_CUR = 10b` (1.5A — SinkTxNG).
- Когда мы готовы дать sink-у право инициировать AMS → `HOST_CUR = 11b` (3.0A — SinkTxOk).

В обычной работе: Source держит `HOST_CUR=11b` (SinkTxOk). PE опускает в
`10b` только когда сам собирается отправить (например, перед Get_Sink_Cap),
и поднимает обратно после.

### 7.3 Default (legacy USB, BC_LVL=01)

В PD 2.0 SinkTx mechanism нет. BC_LVL=01 означает legacy default — sink
может TX в любое время. У нас, как Source, никогда не advertised
80 μA (мы всегда 3.0A или 1.5A); но если попали в Sink-роль на legacy
charger, BC_LVL может быть 01 — тогда `sink_tx_ok = true` всегда.

---

## 8. PD revision tracking

В Message Header биты 7:6 — `SpecRev`:
- `00b` — PD 1.0 (deprecated; интерпретируем как 2.0)
- `01b` — PD 2.0
- `10b` — PD 3.x
- `11b` — Reserved

### 8.1 Что мы advertised в наших исходящих

Стартовое значение `our_spec_rev = 10b` (PD 3.0) — как требует §8.2 ниже.
Храповик работает **только вниз**: стартовать с 01b и повышаться, как
предполагала более ранняя редакция этого раздела, нельзя — партнёр,
объявивший 3.0 один раз в Source_Capabilities, больше её не повторит, а мы
навсегда остались бы на 2.0.

При первом RX-сообщении от партнёра с `SpecRev = 01b`:
- Партнёр заявляет PD 2.0.
- Обновляем `our_spec_rev = 01b` — наши исходящие будут с PD 2.0 header.
- Это применяется через `SWITCHES1.SPEC_REV = 10b → 01b` (для auto-GoodCRC) и в нашем header construction.
- Обратно вверх в пределах одного соединения не поднимаемся: уже отправленные
  сообщения нас зафиксировали. Сброс к 10b — только на detach (не на
  Soft_Reset и не на Hard Reset: партнёр от них не меняется).

**Кто записывает `SWITCHES1.SPEC_REV`**: при изменении `our_spec_rev`
обновление в FUSB302 делает **PRL** (через L4 API `set_role(power, data,
rev)` или dedicated `set_spec_rev`). Это критично для auto-GoodCRC —
если мы получаем PD 3.0 message, а auto-GoodCRC отвечает с SpecRev=01b
в header, партнёр может это интерпретировать как несоответствие.

При получении SpecRev=01b — остаёмся на PD 2.0.

### 8.2 SOP Specification Revision Detection Process

Из [USBPD] §6.1.3.1, PD 3.x порт **должен**:
1. Стартовать с advertised SpecRev = 3.0.
2. Если партнёр в первом сообщении (Source_Capabilities или Sink_Capabilities) шлёт rev 2.0 — даунгрейдиться до 2.0 для дальнейших сообщений.

Альтернатива (более безопасная и проще в реализации): стартуем с 2.0,
апгрейдимся если партнёр 3.0. Так делает большинство open-source стэков.
**Используем эту альтернативу.**

### 8.3 Где revision хранится

```
peer_spec_rev   : SpecRev   // что у партнёра по header
our_spec_rev    : SpecRev   // что мы шлём
```

Также передаётся в PPM для `GET_CONNECTOR_CAPABILITY.PartnerPDRevision`
(см. [`commands.md`](commands.md) §2.7) и `GET_CONNECTOR_STATUS.bcdPDVersion`
(см. [`commands.md`](commands.md) §2.17).

---

## 9. Message construction (TX detail)

PE передаёт в PRL:

```
TxRequest {
    sop_type      : Sop,                  // обычно SOP
    msg_type      : u5,                   // control type или data type
    extended      : bool,                 // 0 в v1 (мы Extended не шлём)
    ndo           : u3,                   // number of data objects (0..7)
    power_role    : PowerRole,            // current
    data_role    : DataRole,             // current
    objects       : [u32; ndo]
}
```

PRL строит header:

```
header = (extended      << 15) |
         (ndo            << 12) |
         (msg_id         <<  9) |
         (power_role     <<  8) |     // SOP only; для SOP'/SOP'' = Cable Plug bit
         (spec_rev       <<  6) |
         (data_role      <<  5) |     // SOP only
         (msg_type       <<  0)
```

И отправляет через FUSB302.

---

## 10. Message parsing (RX detail)

Из FUSB302 `fusb302_pd_message_receive()` возвращает структуру:
```
Fusb302PdMsg {
    sop_type      : Sop,
    header        : u16,
    objects       : [u32; 7],
    object_count  : u8,    // совпадает с header.ndo
}
```

PRL парсит header в типизированную структуру и передаёт в PE.

---

## 11. PRL ↔ PE API

### 11.1 PE → PRL (commands)

| Команда                       | Эффект                                                         |
|:------------------------------|:---------------------------------------------------------------|
| `tx_request(msg)`             | Поставить в очередь TX. Может быть отложено SinkTx-логикой.    |
| `cancel_tx`                   | Отменить pending TX (если в WAITING_FOR_TX_RESULT — TX_FLUSH). |
| `send_hard_reset`             | Прервать всё, послать Hard Reset через FUSB302.                |
| `set_sink_tx_pretend_ng`      | Когда мы Source: HOST_CUR=10b (SinkTxNG).                       |
| `set_sink_tx_ok`              | Когда мы Source: HOST_CUR=11b (SinkTxOk).                       |
| `prl_reset(reason)`           | Сбросить PRL counters. reason ∈ { soft_reset_sent, soft_reset_received, hard_reset_sent, hard_reset_received, disconnect }. |

### 11.2 PRL → PE (events)

| Событие                       | Когда                                                          |
|:------------------------------|:---------------------------------------------------------------|
| `MessageReceived(msg)`        | После успешного RX + duplicate check.                          |
| `MessageSent(header)`         | I_TXSENT.                                                      |
| `MessageFailed(reason)`       | I_RETRYFAIL / cancelled / hard_reset_during_tx.                |
| `HardResetReceived`           | I_HARDRST.                                                     |
| `HardResetSent`               | I_HARDSENT.                                                    |
| `CollisionResolved`           | Информационно, после I_COLLISION → retry (необязательно слать). |

---

## 12. Хранимое состояние PRL

```
struct PRLState {
    // TX
    tx_state            : { IDLE, CONSTRUCTING, WAITING_FOR_TX_RESULT },
    tx_message_id_counter : u3,
    pending_tx          : Option<TxRequest>,

    // RX
    stored_rx_message_id : Option<u3>,

    // PD revision
    peer_spec_rev       : SpecRev,
    our_spec_rev        : SpecRev,

    // SinkTx (только когда мы Sink)
    sink_tx_ok          : bool,
    sink_tx_timer       : Option<FuriEventLoopTimer>,
}
```

---

## 13. Открытые места

### 13.1 Sequence в случае одновременного TX и RX

Сценарий: PE говорит «отправь Request», в этот момент партнёр шлёт
Source_Capabilities снова. FUSB302 поднимает `I_COLLISION` — наш TX
abort-ится; параллельно `I_GCRCSENT` — наш RX обработан. Нужно: отдать
RX в PE первым (новые Source_Caps могут означать другой набор PDO);
после PE-обработки, если PE всё ещё хочет отправить Request, повторить
TX с новыми параметрами.

Решение: при `I_COLLISION`, не делать automatic retry — отдать в PE
оригинальную TX-команду как failed с `reason=collision`. PE сама решит
retry или нет.

Альтернатива: автоматический retry в PRL после следующего `I_GCRCSENT`.
Спека PRL рекомендует первый вариант — пусть PE сама решает. **Так и делаем.**

### 13.2 PRL и Extended Messages на приёме

Если прилетит Extended Message — PE отвечает Not_Supported. PRL по
сравнению с не-Extended обрабатывает одинаково: парсит header
(`extended=1`), кладёт payload в objects (даже если это chunked данные
— нам всё равно), отдаёт в PE. PE проверяет `header.extended==1` →
Not_Supported. Никакой специальной chunking-receive-логики.

### 13.3 SOP' / SOP''

`CONTROL1.ENSOP1=0, ENSOP2=0` — игнорим cable-plug сообщения.
`ENSOP1DB=0, ENSOP2DB=0` — debug cable тоже. Если случайно прилетит
(не должно при выключенных bits) — игнор в PRL_Rx step 6.

### 13.4 BIST mode

Если PE запросил `BIST` с Carrier Mode 2: PRL не делает ничего особенного —
PE сама пишет `CONTROL1.BIST_MODE2=1`, ждёт BISTContModeTimer (30-60 мс),
выключает.

`CONTROL3.BIST_TMODE=1` — отдельный режим где RX FIFO очищается после
GoodCRC: мы это **не используем** в v1.

### 13.5 PRL и Disconnect

При сигнале от Type-C SM «detached»:
1. Отменить pending TX.
2. PRL_Tx → IDLE.
3. Сбросить все counters (как при reset).
4. Очистить SinkTx-state.

---

## 14. Что дальше

[`pe-sm.md`](pe-sm.md) — самый большой документ. Policy Engine для
Source + Sink + PR_Swap + DR_Swap + Hard Reset orchestration. PRL и
Type-C SM ему обслуживают.
