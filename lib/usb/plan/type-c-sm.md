# Type-C State Machine

Документ описывает Type-C state machine для нашего DRP-порта. Учитывает,
что FUSB302 в режиме `TOGGLE=1` берёт на себя цикл Unattached →
detection → settle, а нам остаётся «остаток» — debounce, VBUS-логика,
переходы в Attached и обратно.

Источники:
- *USB Type-C Cable and Connector Specification* (свободно доступная)
- [`fusb302.md`](fusb302.md) — что FUSB302 делает в железе.
- [`pd-scope.md`](pd-scope.md) — что Type-C SM сообщает PE и что от неё ждёт.
- [`architecture.md`](architecture.md) — L3 = LPM, Type-C SM — её часть.

---

## 1. Обзор и обоснование скоупа

Канонический Type-C DRP state machine из спеки имеет ~15 состояний.
Большая часть из них покрывается FUSB302 `TOGGLE` и не требует
программной реализации:

| Каноническое состояние   | Где реализуется                                            |
|:-------------------------|:-----------------------------------------------------------|
| Unattached.SNK           | **FUSB302 TOGGLE** — Rd на CC1, потом Rd на CC2            |
| Unattached.SRC           | **FUSB302 TOGGLE** — Rp на CC1, потом Rp на CC2            |
| AttachWait.SNK (нач. debounce) | Частично FUSB302 (порог), финальный debounce — софт   |
| AttachWait.SRC (нач. debounce) | Частично FUSB302 (порог), финальный debounce — софт   |
| Try.SRC / TryWait.SNK    | **Не делаем** (опционально по спеке)                        |
| Try.SNK / TryWait.SRC    | **Не делаем**                                              |
| DebugAccessory.SRC       | **Не делаем** (FUSB302 settles, мы игнорируем)             |
| AudioAccessory           | FUSB302 detect, мы остаёмся «прибитыми» (без PD)           |
| Unsupported.Accessory    | **Не делаем** (мы DRP, support both ролей)                 |

В итоге наша программная Type-C SM имеет **7 состояний**:

```
                         ┌──────────────┐
                         │   Disabled   │
                         └──────┬───────┘
                                │ init / start
                                ▼
                         ┌──────────────┐     ┌─────────────────┐
                         │   Toggling   │◄────┤ ErrorRecovery   │
                         └──────┬───────┘     └────────▲────────┘
              I_TOGDONE         │                      │ tErrorRecovery
        ┌───────────────────────┼────────────────┐     │ expired
        │                       │                │     │
        │ TOGSS=SRCon*          │ TOGSS=SNKon*   │     │ CONNECTOR_RESET
        │                       │                │     │ (Hard)
        ▼                       ▼                ▼     │
┌────────────────┐      ┌────────────────┐  ┌──────────────┐
│ AttachWait.SRC │      │ AttachWait.SNK │  │ AudioAccess. │
└───────┬────────┘      └───────┬────────┘  └──────────────┘
        │ tCCDebounce           │ tCCDebounce
        │ + Rd persists         │ + VBUS appeared
        ▼                       ▼
┌────────────────┐      ┌────────────────┐
│ Attached.SRC   │      │ Attached.SNK   │
└───────┬────────┘      └───────┬────────┘
        │ Rd gone               │ VBUS gone
        │ (tCCDebounce)         │ (immediate)
        ▼                       ▼
       ... → Toggling          ... → Toggling
```

(PR_Swap и DR_Swap внутри Attached.* — детали в §7, §8.)

---

## 2. Состояния

### 2.1 Disabled

**Когда**: после init до первого "start", после PPM_RESET, при намеренной
остановке (например, OPM шлёт SET_CCOM с Disabled bit).

**Что делает**:
- TOGGLE = 0
- Все CC-терминации сняты (`SWITCHES0.PDWN1=PDWN2=PU_EN1=PU_EN2=0`)
- VBUS source-switch выключен
- PE остановлен
- AUTO_CRC выключен

**Выход**: команда «start» от выше (PPM init / SET_CCOM с разрешённым режимом) → **Toggling**.

### 2.2 Toggling

**Когда**: ищем партнёра.

**Конфигурация FUSB302**:
- `POWER = 0x0F` (все домены)
- `CONTROL2 = { TOGGLE=1, MODE=01b (DRP), TOG_SAVE_PWR=01b (40 мс idle), TOG_RD_ONLY=0 }`
- `CONTROL0.HOST_CUR = 11b` (3A advertised как source)
- `MASKA.M_TOGDONE = 0` (открыто)
- `SWITCHES0` — игнорируем; TOGGLE сам выставит.
- `SWITCHES1.AUTO_CRC = 0` (рано)

**События, на которые реагируем**:
- `Fusb302EventToggleDone { togss }` → перейти в:
  - `AttachWait.SRC` если `togss ∈ {001, 010}` (SRC on CC1/CC2)
  - `AttachWait.SNK` если `togss ∈ {101, 110}` (SNK on CC1/CC2)
  - `AudioAccessory` если `togss = 111`
  - иначе остаёмся (corrupted state — лог + ignore).

**Запомнить полярность**: CC = CC1 или CC2 — определяется из TOGSS, сохраняется в Type-C state.

### 2.3 AttachWait.SRC

**Когда**: TOGGLE settled с нашей ролью = Source. Партнёр **пока что**
имеет Rd, но мы должны убедиться, что это устойчиво (debounce
`tCCDebounce` = 100-200 мс).

**Что делает**:
- TOGGLE = 0 (стоп — больше не нужен).
- **Не сбрасываем** PDWN/PU_EN полностью — FUSB302 уже settled на правильных терминациях после TOGGLE; мы только **подтверждаем** их и дописываем MEAS_CC.
- Lock полярность: `SWITCHES0.MEAS_CC{1|2}=1`, противоположный MEAS = 0.
- TX direction: `SWITCHES1.TX_CC{1|2}=1`, противоположный TX = 0.
- Подтвердить PullUp: `SWITCHES0.PU_EN{1|2}=1` (на правильном CC), противоположный PU_EN = 0; PDWN1 = PDWN2 = 0.
- HOST_CUR соответствует тому, что мы advertised (default = 3A).
- Запустить `CCDebounceTimer` (tCCDebounce, ~150 мс).
- Маска `I_COMP_CHNG=0` — следить, если Rd исчезнет до истечения дебаунса.
- `MEASURE.MDAC` — порог между «Rd present» и «no Rd» (≈ 200 мВ — нижняя граница BC_LVL=01).

**События**:
- `CCDebounceTimer expired` → если `STATUS0.BC_LVL` всё ещё показывает Rd (≠ 00) → **Attached.SRC**.
- `I_COMP_CHNG` + BC_LVL = 00 → Rd ушёл → **Toggling** (партнёр отвалился до attach).
- `CONNECTOR_RESET` → **ErrorRecovery**.

### 2.4 AttachWait.SNK

**Когда**: TOGGLE settled с нашей ролью = Sink. Партнёр имеет Rp, ждём
появления **VBUS** для подтверждения attach.

**Что делает**:
- TOGGLE = 0.
- **Не сбрасываем** PDWN/PU_EN полностью — FUSB302 уже settled на правильных терминациях; подтверждаем.
- Lock полярность: `MEAS_CC{1|2}=1`, противоположный = 0.
- Подтвердить PullDown: `SWITCHES0.PDWN1=PDWN2=1`, PU_EN1=PU_EN2=0.
- Маска `I_VBUSOK=0` (главный сигнал sink-attach).
- Маска `I_BC_LVL=0` (если source меняет advertised current).
- Запустить `CCDebounceTimer`.

**События**:
- `Fusb302EventVbusChanged { vbus_ok=1 }` + `STATUS0.BC_LVL` ≠ 00 (Rp ещё присутствует) → **Attached.SNK** (даже до истечения CCDebounce, согласно spec — VBUS — самый надёжный сигнал sink-attach).
- `CCDebounceTimer expired` + BC_LVL = 00 → партнёр отвалился → **Toggling**.
- `CCDebounceTimer expired` + BC_LVL ≠ 00, но VBUS не пришёл → ждём ещё (VBUS может прийти позже на dead-battery sources); опционально — переход в **Toggling** через `tTypeCSinkWaitCap`. По умолчанию — продолжаем ждать VBUS бесконечно, пока CC не уходит.
- `I_COMP_CHNG` + BC_LVL = 00 → Rp ушёл → **Toggling**.
- `CONNECTOR_RESET` → **ErrorRecovery**.

### 2.5 Attached.SRC

**Когда**: мы — source, партнёр прицеплен.

**Entry actions**:
1. Включить **внешний** VBUS power switch (GPIO).
2. Запустить `VbusOnTimer` (tVbusON ≈ 275 мс).
3. Настроить event-driven мониторинг vSafe5V: `set_compare_threshold_mv(4500, above)` — выставит `MEASURE.MEAS_VBUS=1`, `MEASURE.MDAC` для 4500 мВ scaled, маска `I_COMP_CHNG=0`. Когда VBUS пересечёт 4500 мВ снизу-вверх → `Fusb302EventCompChanged` → step 4.
4. По достижении vSafe5V (или таймауту):
   - VbusOnTimer expired без vSafe5V → **ErrorRecovery**.
   - vSafe5V reached → отменить VbusOnTimer, продолжить:
5. `RESET.PD_RESET = 1` (PD-логика FUSB302 в дефолт).
6. `prl.prl_reset(reason=attach)` — обнулить software MessageIDCounter и stored_rx_message_id.
7. `SWITCHES1` — выставить `POWER_ROLE=1` (Source), `DATA_ROLE=1` (DFP — DRP-default), `SPEC_REV=01b` (PD 2.0; обновим до PD 3.0 после получения первого сообщения партнёра с rev=10b — см. [`pd-scope.md`](pd-scope.md) §1).
8. `SWITCHES1.AUTO_CRC = 1`.
9. `CONTROL3 = { AUTO_RETRY=1, N_RETRIES=10b (=2 retries), AUTO_SOFTRESET=0, AUTO_HARDRESET=0 }`.
10. **`CONTROL0.HOST_CUR = 10b`** (1.5A advertised = SinkTxNG) — sink не должен инициировать AMS до того как мы отправим Source_Caps. PE поднимет в `11b` (3.0A advertised = SinkTxOk) после успешного TX Source_Capabilities.
11. Маски: открыть `I_TXSENT, I_RETRYFAIL, I_HARDRST, I_HARDSENT, I_GCRCSENT, I_BC_LVL, I_COMP_CHNG, I_VBUSOK`.
12. Сообщить PE: **Source startup** — пусть начинает отправлять `Source_Capabilities`.

**Run / события**:
- `Fusb302EventMessageRx` → отдать в PRL → PE.
- `Fusb302EventMessageTxOk / TxFail / HardRcv / HardSent` → отдать в PRL → PE.
- `Fusb302EventBcLvlChanged` → партнёр-sink сменил current request (через Rp) — обычно не важно при PD-контракте.
- `Fusb302EventCompChanged` + BC_LVL = 00 → Rd ушёл (партнёр отвалился). Запустить `DetachDebounceTimer = tCCDebounce`. Если по истечении BC_LVL всё ещё = 00 → **detach**:
  - Выключить VBUS source switch.
  - Опционально включить discharge resistor для быстрого вSafe0V.
  - Сообщить PE: **Detach**.
  - → **Toggling** (через short pause = tVbusOFF max 650 мс на discharge).
- `Fusb302EventVbusChanged { vbus_ok=0 }` неожиданно (например, short-circuit) → → **ErrorRecovery**.
- PE говорит «PR_Swap success» → внутри Attached: переходим в `Attached.SNK` без disconnect (см. §7).
- `CONNECTOR_RESET (Hard)` → **ErrorRecovery**.
- `CONNECTOR_RESET (Data)` → PE делает Data Reset (если поддержан); Type-C state не меняется. Так как мы Data_Reset не делаем (NS — см. [`pd-scope.md`](pd-scope.md) §1.1) — отвечаем NS, остаёмся.

### 2.6 Attached.SNK

**Когда**: мы — sink, партнёр прицеплен и даёт VBUS.

**Entry actions**:
1. Если есть **внешний** sink path switch — включить (по умолчанию включён; см. `SET_SINK_PATH` в [`commands.md`](commands.md) §2.26).
2. `RESET.PD_RESET = 1`.
3. `prl.prl_reset(reason=attach)` — обнулить software counters.
4. `SWITCHES1 = { POWER_ROLE=0 (Sink), DATA_ROLE=0 (UFP — DRP-default), SPEC_REV=01b }`.
5. `SWITCHES1.AUTO_CRC = 1`.
6. `CONTROL3` — как в SRC.
7. Маски — как в SRC.
8. Запустить `SinkWaitCapTimer` (tTypeCSinkWaitCap ≈ 465 мс — см. [`pd-scope.md`](pd-scope.md) §6.2). Если за это время не пришёл Source_Capabilities → Hard Reset → **ErrorRecovery** (или, если HardResetCounter < nHardResetCount, реtry Hard Reset).
9. Сообщить PE: **Sink startup**.

**Run / события**:
- `Fusb302EventMessageRx` (Source_Capabilities) → отмена SinkWaitCapTimer; PE обрабатывает.
- `Fusb302EventVbusChanged { vbus_ok=0 }` → партнёр отключил VBUS → **detach**:
  - Сообщить PE: **Detach**.
  - → **Toggling**.
- `Fusb302EventBcLvlChanged` → source изменил advertised current (без PD-контракта это значит обновление Type-C-current; с PD-контрактом партнёр обычно не меняет Rp).
- `Fusb302EventCompChanged` — менее надёжный сигнал чем VBUS; используем для подтверждения.
- PE говорит «PR_Swap success» → переходим в `Attached.SRC`.
- `CONNECTOR_RESET (Hard)` → **ErrorRecovery**.

### 2.7 AudioAccessory

**Когда**: FUSB302 settles с TOGSS=111 (Ra/Ra обоих CC).

**Что делает**: ничего полезного. PE не запускаем. Просто отмечаем
состояние; PPM сообщает в `Connector Partner Type = 6` (Audio Adapter
Accessory).

**Выход**: мониторим `I_COMP_CHNG`. Если хотя бы один из CC уходит из
Ra-диапазона (а это значит accessory отключён) → **Toggling**.

### 2.8 ErrorRecovery

**Когда**: фатальная ошибка / Hard Reset на source-side с потерей контракта
/ tVbusON timeout / CONNECTOR_RESET (Hard).

**Что делает**:
1. Снять все CC-терминации (`SWITCHES0 = 0`).
2. Выключить VBUS (если был SRC).
3. PD_RESET, RX/TX flush.
4. Запустить `ErrorRecoveryTimer = tErrorRecovery` (≥25 мс; используем 250 мс — безопаснее).

**События**:
- `ErrorRecoveryTimer expired` → **Toggling**.

---

## 3. Хранимое состояние

Type-C SM хранит:

| Поле                | Тип                          | Источник                            | Использование                              |
|:--------------------|:-----------------------------|:------------------------------------|:-------------------------------------------|
| `state`             | enum                         | сама SM                             | текущее состояние                          |
| `cc_orientation`    | `{None, CC1, CC2}`           | из TOGSS                            | для `GET_CONNECTOR_STATUS.Orientation`, переключение SWITCHES0/SWITCHES1 |
| `power_role`        | `{Source, Sink}`             | из TOGSS / PR_Swap                  | для PD-header (auto-GoodCRC) и PPM API     |
| `data_role`         | `{DFP, UFP}`                 | DR_Swap или дефолт от `power_role`  | для PD-header и PPM API                    |
| `partner_type`      | enum (DFP, UFP, audio, etc)  | вычисляется при attach              | `GET_CONNECTOR_STATUS.ConnectorPartnerType`|
| `partner_rp`        | `{Default, 1.5A, 3.0A}`      | sink-side: из `STATUS0.BC_LVL`      | Type-C-current detection для sink          |
| `attach_at_ms`      | timestamp                    | `furi_get_tick()`                   | для статистики, дебаг                      |

---

## 4. Таймеры

| Таймер                | tName / значение      | Где запускается        | Истечение                                       |
|:----------------------|:----------------------|:-----------------------|:------------------------------------------------|
| `CCDebounceTimer`     | tCCDebounce, 100-200 мс (150) | AttachWait.* entry  | Подтвердить attach → Attached.*                |
| `PDDebounceTimer`     | tPDDebounce, 10-20 мс  | (опционально для specific transitions) | —                                  |
| `DetachDebounceTimer` | tCCDebounce              | при Rd gone в Attached.SRC | Если Rd не вернулся → detach                |
| `ErrorRecoveryTimer`  | tErrorRecovery, 250 мс  | ErrorRecovery entry    | → Toggling                                      |
| `VbusOnTimer`         | tVbusON, 275 мс         | Attached.SRC entry     | Если VBUS не поднялся → ErrorRecovery           |
| `VbusOffTimer`        | tVbusOFF, 650 мс        | После detach как SRC   | Гарантирует discharge перед след. cycle         |

Все таймеры — `FuriEventLoopTimer` типа `FuriEventLoopTimerTypeOnce`.

---

## 5. VBUS управление (external)

Type-C SM ответственна за внешний VBUS power switch (источаем) и
опционально discharge. Конкретные GPIO — за пределами этой библиотеки;
SM зовёт callback-и:
- `vbus_source_enable()` — поднимаем VBUS как source.
- `vbus_source_disable()` — опускаем.
- `vbus_discharge(on_off)` — опционально, для ускорения vSafe0V.

Состояния VBUS:
- **vSafe0V** — < 800 мВ (после discharge)
- **vSafe5V** — 4.75-5.5 В (default operating)
- **VBUS Higher** — > 5.5 В (PD-renegotiated voltage; 9V/15V/20V и т.д.)

Мониторинг VBUS:
- Грубо: `I_VBUSOK` (vVBUSthr ≈ 4.5 В).
- Точно: `MEASURE.MEAS_VBUS=1` + `MDAC[5:0]` пороги + `STATUS0.COMP`.

Type-C SM пользуется грубым мониторингом; точный мониторинг (для
проверки vSafe0V / vSafe5V на переходах PR_Swap) — в PE, через L4 API.

---

## 6. CONNECTOR_RESET handling

UCSI `CONNECTOR_RESET` (см. [`commands.md`](commands.md) §2.3) имеет
два режима:

- **Hard Reset** (Reset Type=0). Полная disconnect-connect последовательность.
  - **Действие Type-C SM**: переход в `ErrorRecovery` (см. §2.8).
- **Data Reset** (Reset Type=1). Только USB-data сброс, VBUS сохраняется.
  - **Действие Type-C SM**: в нашей конфигурации (не USB4) — отвечаем NS на CONNECTOR_RESET с Data Reset (CCI `Not Supported Indicator=1` + остаёмся в Attached).

---

## 7. PR_Swap и Type-C SM

PR_Swap — это **PD-протокольная** операция, но Type-C SM в ней
участвует. Последовательность (когда мы initial Source, партнёр initial
Sink):

1. PE отправляет/принимает `PR_Swap`, получает Accept.
2. PE отправляет `PS_RDY` (мы — Source — говорим «готов отключить VBUS»).
3. Type-C SM: VBUS source-switch off; ждём пока VBUS упадёт ниже vSafe0V (через `MEASURE.MEAS_VBUS` + threshold ≈ 0.8 В).
4. PE отправляет `PS_RDY` второй раз (Source-side: «теперь я роль не source»; Sink-side: «готов».
5. После роли swap: новый Source (партнёр) поднимает VBUS.
6. Мы (теперь Sink) ждём `I_VBUSOK`. Если за `PSSourceOnTimer` (tPSSourceOn, ~435 мс) не пришёл → Hard Reset.
7. PE отправляет/принимает финальный `PS_RDY`.
8. Type-C SM transition: `Attached.SRC` → `Attached.SNK`. Конкретно:
   - Обновить `power_role = Sink`.
   - `SWITCHES1.POWER_ROLE = 0`.
   - `SWITCHES0`: pull-down on, pull-up off (но физически Rd/Rp на CC уже перестроены *PE-протоколом* через PS_RDY-handshake; FUSB302 это делает само через `SWITCHES0`).
9. PE отправляет `SourceCapabilities` (если мы стали Source) или ждёт их (если стали Sink).

Похожая логика для обратного направления (initial Sink → Source). Точные
детали — в [`pe-sm.md`](pe-sm.md). Type-C SM здесь — пассивный
исполнитель команд "now switch role".

---

## 8. DR_Swap

Проще PR_Swap — только меняется data-role, VBUS не трогается, Rp/Rd не
переключаются.

1. PE обрабатывает DR_Swap handshake.
2. Type-C SM: меняет `data_role` (DFP ↔ UFP) и `SWITCHES1.DATA_ROLE`.
3. Остаёмся в `Attached.*`.

---

## 9. Соглашения о ролях по умолчанию

После attach (без PD-контракта):
- Source-side: `data_role = DFP`.
- Sink-side: `data_role = UFP`.

После PD-контракта (через DR_Swap или нет):
- DRP-port должен поддерживать оба data-role на любом power-role. Source-as-UFP или Sink-as-DFP — валидно.

В PD-header при auto-GoodCRC мы должны корректно отражать **текущие**
`power_role` / `data_role`. После каждого swap — `SWITCHES1` обновлять.

---

## 10. Открытые/особые места

### 10.1 Try.SRC?

Try.SRC — опциональный канонический state, позволяет порту "предпочесть"
source-роль над sink-ролью при ambiguity. Полезно для laptop-style
устройств, чтобы при подключении кабеля к dock-у первым делом стать
host-ом.

Мы **не делаем** Try.SRC в v1. Это упрощение; добавить — отдельная задача
(потребует пары новых состояний и таймера `DRPTryTimer`).

### 10.2 Audio Accessory полнее

Если устройство должно поддерживать пропускание audio через CC —
требуется отдельная обработка. Сейчас мы только **детектим** Audio
Accessory и ничего не делаем.

### 10.3 Debug Accessory

FUSB302 settles `Debug Accessory` (Rd/Rd) с TOGSS подобно SRC. Мы не
делаем debug, но settles в SRC-role — значит можем влететь в
`AttachWait.SRC` ошибочно. На debounce увидим, что что-то странное (Rd
без обычного pull-up response от партнёра), но обработать корректно
помогут специфические BC_LVL-проверки. Откладываем — в v1 принимаем,
что debug accessory будет вести себя как обычный sink (с возможными
багами).

### 10.4 Dead Battery scenario

UCSI Connector Reset (Hard) при отсутствии другого питания и при
подключённом charger-е должен **fail** (см. [`commands.md`](commands.md)
§2.3). Это значит, перед `ErrorRecovery` нужно проверить — есть ли
альтернативный source питания? Если мы sink с charger-ом на VBUS и
батарея dead — Hard Reset обрубит наше питание.

Реализация: при CONNECTOR_RESET (Hard) с `power_role=Sink` и без external
power supply → fail с `Error Information = bit 5 (Dead Battery condition)`.
Логика «есть ли external power» — за пределами этой библиотеки (caller
говорит через config).

### 10.5 USB Type-C Error Recovery vs CONNECTOR_RESET

Spec различает:
- **Error Recovery** (Type-C-level) — внутренний механизм, активируется при protocol fail или Hard Reset.
- **CONNECTOR_RESET (Hard)** в UCSI — OPM-инициированный.

Действие одинаковое (`Disabled CC` на tErrorRecovery, затем Toggling), но
триггер разный. В нашей SM это одно и то же состояние `ErrorRecovery`.

### 10.6 Re-enter Toggling без TOGGLE

Альтернативно после detach можно **не** включать TOGGLE, а вручную
выставить Rp/Rd и мониторить через `I_COMP_CHNG`. Это даёт более
предсказуемый порядок (а не "ROL зависит от того, на каком CC сработал
TOGGLE первым"). Минус — больше софтового state-machine. Для DRP-портов
TOGGLE — стандартный путь, оставляем.

---

## 11. Связь с PE и L4

Type-C SM публикует в PE (через очередь LPM):
- `attached(role, data_role, polarity, partner_rp_or_rd)`
- `detached`
- `error_recovery_started`
- `vbus_at_safe0v` (для PR_Swap)
- `vbus_at_safe5v`

Type-C SM принимает от PE:
- `pr_swap_complete(new_power_role)`
- `dr_swap_complete(new_data_role)`
- `pe_requested_hard_reset` → Type-C SM посылает Hard Reset через FUSB302 → ErrorRecovery
- `pe_requested_disable` → Disabled

Type-C SM команды в L4 (через FUSB302 API):
- start/stop TOGGLE
- lock polarity
- set Rp/Rd/host-cur
- set power/data role в SWITCHES1
- enable/disable AUTO_CRC, AUTO_RETRY
- send Hard Reset
- PD_RESET
- VBUS measure через MDAC

---

## 12. Что дальше

[`prl-sm.md`](prl-sm.md) — Protocol Layer state machine. Будет тонкий,
потому что почти всё в FUSB302. Главные ответственности — `MessageIDCounter`,
коллизии, SinkTxTimer, Soft_Reset semantics.

[`pe-sm.md`](pe-sm.md) — Policy Engine. Большая часть содержательного
кода. Source + Sink + PR/DR_Swap + Hard Reset orchestration.
