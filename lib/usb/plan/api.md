# API библиотеки UCSI-PPM (L1/L2 наружу)

Документ фиксирует **публичный C-API** библиотеки и контракты HAL-инжектов,
на которых она работает. Внутренние API между слоями (L2↔L3↔L4) описаны
в [`architecture.md`](architecture.md) §6–7 и здесь **не** дублируются.

Связанные документы:
- [`architecture.md`](architecture.md) — слои, потоки данных, концепции.
- [`commands.md`](commands.md) — UCSI-команды (внешний контракт OPM↔PPM).
- [`pd-scope.md`](pd-scope.md) — какие PD-сообщения / PDO в скоупе.

Допущения (зафиксированы в `architecture.md` §1):
- Один коннектор на инстанс.
- Транспорт — байтовый regfile (UCSI Table 4-1), независимый от шины.
- Язык — C99 (без C11-атомиков, без VLA).
- Single-context cooperative; не реентрантно.

---

## 0. Структура заголовков

Один зонтичный заголовок для пользователя библиотеки + внутренние:

```
lib/usb/include/
  ucsi_ppm.h           — публичный API (этот документ)
  ucsi_ppm_config.h    — типы конфигурации, PDO-helpers
  ucsi_ppm_hal.h       — сигнатуры HAL-callback-ов
  ucsi_ppm_errors.h    — коды ошибок
```

`ucsi_ppm.h` подключает остальные. Пользователю достаточно одного
`#include "ucsi_ppm.h"`.

---

## 1. Соглашения

- **Префикс** всех публичных идентификаторов: `ucsi_ppm_` для функций,
  `UcsiPpm` для типов (camelcase в типах — соответствие стилю проекта
  Furi-подобному, см. остальные `lib/`).
- **Заголовки** оборачиваются `extern "C"` для возможного C++ caller-а.
- **Memory ownership**: библиотека **не вызывает** `malloc`. Все буферы —
  caller-provided или статические внутри библиотеки (см. §2.1).
- **Возврат**: функции с возможной ошибкой возвращают `UcsiPpmStatus`
  (enum). Геттеры — возвращаемое значение.
- **Bool**: `bool` из `<stdbool.h>`.
- **Время**: только миллисекунды, `uint32_t`, монотонно растущее.

### 1.1 Константы версий спецификаций

Зафиксированы compile-time. Caller не может их переопределить — это
функция от выбранного скоупа (см. [`pd-scope.md`](pd-scope.md)).

```c
#define UCSI_PPM_VERSION_UCSI    0x0300  // UCSI 3.0 (regfile VERSION)
#define UCSI_PPM_VERSION_PD      0x0300  // PD 3.0 (commands.md §2.6 bcdPDVersion)
#define UCSI_PPM_VERSION_TYPEC   0x0200  // Type-C 2.0 (bcdUSBTypeCVersion)
#define UCSI_PPM_VERSION_BC      0x0000  // BC не реализуем (bcdBCVersion = 0)
#define UCSI_PPM_NUM_ALT_MODES   0       // v1: alt-modes отсутствуют (bNumAltModes)
#define UCSI_PPM_NUM_CONNECTORS  1       // bNumConnectors (architecture.md §1)
```

Версия публичного API библиотеки — отдельно:

```c
#define UCSI_PPM_API_VERSION_MAJOR 0
#define UCSI_PPM_API_VERSION_MINOR 1
#define UCSI_PPM_API_VERSION_PATCH 0
```

---

## 2. Базовые типы

### 2.1 Handle и владение памятью

Инстанс библиотеки — **opaque** для пользователя:

```c
typedef struct UcsiPpm UcsiPpm;
```

**Аллокация**: caller вызывает `ucsi_ppm_alloc()`; внутри библиотека
использует project-wide allocator (Furi `malloc`/`free` поверх FreeRTOS
heap). Структура `UcsiPpm` непрозрачна — размер не часть ABI.

```c
UcsiPpm* ucsi_ppm_alloc(void);
void     ucsi_ppm_free(UcsiPpm* ppm);
```

`ucsi_ppm_free` на не-`deinit`-нутом инстансе сам делает `deinit` перед
освобождением памяти. `ucsi_ppm_free(NULL)` — no-op.

### 2.2 Статус

```c
typedef enum {
    UcsiPpmStatusOk = 0,
    UcsiPpmStatusInvalidArg,        // NULL handle / out-of-range offset / etc.
    UcsiPpmStatusInvalidConfig,     // конфиг не прошёл валидацию (см. §5.1)
    UcsiPpmStatusNotInitialized,    // вызов до ucsi_ppm_init
    UcsiPpmStatusAlreadyInitialized,
    UcsiPpmStatusBusy,              // operation queued, не fatal
    UcsiPpmStatusHalError,          // I2C/GPIO callback вернул ошибку
    UcsiPpmStatusInternal,          // bug в библиотеке (assert-failure path)
} UcsiPpmStatus;
```

UCSI-ошибки на уровне команд (Error Indicator + Error Information bitmap)
**не** возвращаются через `UcsiPpmStatus` — они уходят к OPM через CCI и
`GET_ERROR_STATUS`. См. [`architecture.md`](architecture.md) §9.

### 2.3 PDO и capabilities

PDO — 32-битная сущность (см. [`pd-scope.md`](pd-scope.md) §2):

```c
typedef uint32_t UcsiPpmPdo;
```

Helpers для конструирования (в `ucsi_ppm_config.h`) — чтобы caller не
паковал биты руками:

```c
UcsiPpmPdo ucsi_ppm_pdo_fixed_source(
    uint16_t voltage_mv,
    uint16_t max_current_ma,
    bool dual_role_power,
    bool unconstrained_power,
    bool usb_comms,
    bool dual_role_data
);

UcsiPpmPdo ucsi_ppm_pdo_fixed_sink(
    uint16_t voltage_mv,
    uint16_t max_current_ma,
    bool dual_role_power,
    bool higher_capability,
    bool unconstrained_power,
    bool usb_comms,
    bool dual_role_data
);
```

Source / Sink capabilities передаются массивом до 7 PDO:

```c
#define UCSI_PPM_MAX_PDOS 7

typedef struct {
    UcsiPpmPdo pdos[UCSI_PPM_MAX_PDOS];
    uint8_t    count;  // 1..7
} UcsiPpmPdoList;
```

### 2.4 Policy-энумы

```c
// Начальный CC operation mode (commands.md §2.8 SET_CCOM bits 0..3).
// Переопределяется через UCSI SET_CCOM в runtime.
typedef enum {
    UcsiPpmCcModeRpOnly,    // только Source — Rp терминации
    UcsiPpmCcModeRdOnly,    // только Sink — Rd терминации
    UcsiPpmCcModeDrp,       // DRP toggling (Rp ↔ Rd)
    UcsiPpmCcModeDisabled,  // терминаторы убраны; валиден только если supports_disabled_state == true
} UcsiPpmCcOperationMode;

// Какую роль анонсировать первой в DRP-цикле (type-c-sm.md).
typedef enum {
    UcsiPpmDrpFirstSrc,     // первый период tDRP — Rp
    UcsiPpmDrpFirstSnk,     // первый период tDRP — Rd
} UcsiPpmDrpFirstRole;

// Advertised Rp current до PD-контракта (fusb302.md HOST_CUR).
// После успешного PD-контракта значение не используется.
typedef enum {
    UcsiPpmRpCurrentUsbDefault,  // 80 µA — USB Default (500/900 мА)
    UcsiPpmRpCurrent1A5,         // 180 µA — Type-C 1.5A
    UcsiPpmRpCurrent3A,          // 330 µA — Type-C 3.0A
} UcsiPpmRpCurrent;
```

---

## 3. HAL callbacks (`ucsi_ppm_hal.h`)

Все callback-ы — обязательные, если не помечено `// optional`. Каждый
имеет `void* ctx` для произвольного состояния caller-а.

### 3.1 Time

```c
typedef uint32_t (*UcsiPpmTimeMsFn)(void* ctx);
```

Монотонные миллисекунды. Wrap-around обрабатывается внутри библиотеки
через unsigned-arithmetic-diff.

### 3.2 Alert sink

```c
typedef void (*UcsiPpmAlertFn)(void* ctx);
```

Дёргается **из контекста тика или write/IRQ-вызова**, когда у OPM есть
что прочитать (изменился CCI). Транспорту-конкретике (GPIO alert pin,
HID interrupt, RPC notification) — задача caller-а. Callback **не должен**
вызывать публичный API библиотеки рекуррентно (см. §8 концепция).

### 3.3 I²C HAL для FUSB302

```c
typedef UcsiPpmStatus (*UcsiPpmI2cWriteFn)(
    void* ctx,
    uint8_t i2c_addr,
    const uint8_t* data,
    size_t len
);

typedef UcsiPpmStatus (*UcsiPpmI2cReadFn)(
    void* ctx,
    uint8_t i2c_addr,
    uint8_t* data,
    size_t len
);
```

Блокирующие в v1. Любой возврат `!= Ok` транслируется L4→L3 как
«CC communication error», попадает в Error Information bitmap.

> **I²C-адрес**: FUSB302 имеет 4 варианта по версии чипа (0x22..0x25).
> Адрес — поле конфигурации (`UcsiPpmConfig.fusb302_i2c_addr`), не
> параметр callback-а. Callback получает уже подставленный адрес —
> это позволяет caller-у использовать тонкий wrapper над platform-HAL.

### 3.4 GPIO HAL

```c
typedef bool (*UcsiPpmGpioReadFn)(void* ctx);
typedef void (*UcsiPpmGpioWriteFn)(void* ctx, bool value);
```

Используются для:
- **FUSB302 INT pin** — read-only. **Optional fallback**: используется
  только если caller **не** проводил INT через прерывание/`notify_fusb302_irq`.
  При наличии IRQ-проводки этот callback не нужен (`NULL` в конфиге).
  При его отсутствии и отсутствии notify библиотека не увидит событий
  от FUSB302 → undefined behavior.
- **VBUS source enable** — write. Включает/выключает внешний VBUS-switch
  при роли Source. **Семантика `value`**: `true` означает «активировать
  switch» (на уровне HAL); физическую полярность инвертирует caller
  внутри callback-а. Библиотека не знает о active-high / active-low.
- **VBUS discharge** — write, optional. Помогает быстрее упасть до vSafe0V
  при detach/Hard Reset. Та же семантика `value` (`true` = активировать
  discharge).

Конкретное назначение каждого callback-а — поле в `UcsiPpmConfig` (§4).

### 3.5 Power supply controller

Для Source-ролей с поддержкой >5V PDO нужна возможность переключения
output-напряжения внешнего PSU.

```c
typedef UcsiPpmStatus (*UcsiPpmPowerSupplySetFn)(
    void* ctx,
    uint16_t voltage_mv,
    uint16_t current_limit_ma
);
```

Возврат `!= Ok` означает «этот voltage не поддерживается» — PE
отреагирует Reject или ограничит advertise-ed PDO. Если в системе только
vSafe5V Fixed — caller возвращает `Ok` только для voltage_mv==5000.

> **Контракт консистентности**: caller отвечает за то, чтобы любой
> voltage из `config.source_caps` (плюс runtime `SET_PDOS`) был
> успешно обрабатываемым в `power_supply_set`. Если caller рекламирует
> 20V PDO, а `power_supply_set(20000, ...)` возвращает error — PE
> вынужден Reject-ить уже принятый Request от Sink-а, что выглядит
> как баг порта снаружи. Библиотека не валидирует это в `init` —
> это caller-config-error.

После того как PSU отстоялся, caller обязан позвать:

```c
UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm);
```

(детали в §5.4). Это завершает `PSTransitionTimer` досрочно.

### 3.6 Альтернативный источник питания

```c
typedef bool (*UcsiPpmHasAltPowerFn)(void* ctx);
```

Возврат — есть ли в системе battery / другой USB-charger, способный
поддержать MCU при дропе VBUS. Используется PE для policy-решений
(Hard Reset как Sink; PR_Swap; drop VBUS как Source). См.
[`architecture.md`](architecture.md) §1.

### 3.7 Логирование (optional)

```c
typedef enum {
    UcsiPpmLogLevelTrace,
    UcsiPpmLogLevelDebug,
    UcsiPpmLogLevelInfo,
    UcsiPpmLogLevelWarn,
    UcsiPpmLogLevelError,
} UcsiPpmLogLevel;

typedef void (*UcsiPpmLogFn)(
    void* ctx,
    UcsiPpmLogLevel level,
    const char* module,  // "PE" | "PRL" | "TC" | "FUSB302" | "PPM"
    const char* fmt,
    va_list args
);
```

`NULL` callback = логирование выключено. Библиотека внутри использует
макросы, которые компилируются в no-op если `UCSI_PPM_LOG_DISABLE`
определён.

### 3.8 Контексты вызова callback-ов

| Callback                       | Контекст                | Может блокировать? |
|:-------------------------------|:------------------------|:------------------:|
| `time_ms`                      | tick / write / get      | нет — должен быть O(1) |
| `alert`                        | tick / register_write   | желательно нет¹    |
| `i2c_read` / `i2c_write`       | tick / init / deinit    | да (мс)            |
| `gpio_read_fusb302_int`        | tick (fallback)         | нет — должен быть O(1) |
| `gpio_write_vbus_*`            | tick / deinit           | нет (короткий switch) |
| `power_supply_set`             | tick                    | нет — должен быть async (PSU сам сообщит готовность через `notify_power_supply_ready`) |
| `has_alt_power`                | tick                    | нет — должен быть O(1) |
| `log`                          | любой task-context²     | желательно нет¹    |

¹ Не блокирует библиотеку, но удлиняет `tick` → влияет на latency
  PD-таймингов (см. [`pd-scope.md`](pd-scope.md) §6 — tSenderResponse
  500 мс, tPSTransition 500 мс и т.п. — запас большой, но не миллисекунды).

² `log` никогда не зовётся из ISR-контекста библиотеки (`notify_*`
  только выставляют флаги; логирование происходит когда `tick` их
  обнаруживает).

**Гарантия про `hal_ctx`**: caller обязан держать `hal_ctx` живым
весь lifetime инстанса (`init` → `deinit`). Библиотека сохраняет
указатель как есть и передаёт во все callback-и.

**Реентрантность снаружи**: библиотека **не вызывает** свой публичный
API из callback-ов. Caller волен из callback-а написать что-то в
queue / flag, но **не** должен вызывать `ucsi_ppm_*` рекурсивно
(см. §8).

> **Портабельность `va_list`**: на большинстве toolchain-ов (arm-none-eabi-gcc,
> clang) передача `va_list` через callback работает; на минимальных nano-libc
> сборках возможны проблемы. Альтернатива — pre-formatted строка фиксированного
> размера (`char buf[N]`); решаем при выборе target-libc.

---

## 4. Конфигурация

`UcsiPpmConfig` — read-only снимок, передаваемый в `init`. Библиотека
**копирует** все нужные поля внутрь; caller может освобождать структуру
сразу после возврата `init`.

```c
typedef struct {
    // ---- Контекст для всех callback-ов (один общий) -----------------
    void* hal_ctx;

    // ---- Обязательные callback-и ------------------------------------
    UcsiPpmTimeMsFn          time_ms;
    UcsiPpmAlertFn           alert;
    UcsiPpmI2cReadFn         i2c_read;
    UcsiPpmI2cWriteFn        i2c_write;
    UcsiPpmGpioWriteFn       gpio_write_vbus_source;
    UcsiPpmPowerSupplySetFn  power_supply_set;
    UcsiPpmHasAltPowerFn     has_alt_power;

    // ---- Опциональные callback-и (NULL допустимо) -------------------
    UcsiPpmGpioReadFn        gpio_read_fusb302_int;      // см. §3.4: NULL если caller обеспечивает notify_fusb302_irq
    UcsiPpmGpioWriteFn       gpio_write_vbus_discharge;  // NULL = discharge не поддерживается
    UcsiPpmLogFn             log;                        // NULL = silent

    // ---- FUSB302 ----------------------------------------------------
    uint8_t fusb302_i2c_addr;  // 0x22..0x25 (по версии чипа)

    // ---- Type-C initial policy (commands.md §2.8, type-c-sm.md) -----
    // Все три переопределяются в runtime: SET_CCOM (CC mode),
    // SET_PDR (роль), SET_POWER_LEVEL (косвенно — Rp current).
    UcsiPpmCcOperationMode  initial_cc_operation_mode;
    UcsiPpmDrpFirstRole     drp_advertise_first;
    UcsiPpmRpCurrent        source_rp_current;

    // ---- Initial PD capabilities (переписываются через SET_PDOS) ----
    UcsiPpmPdoList source_caps;  // count >= 1; PDO[0] обязан быть Fixed 5V
    UcsiPpmPdoList sink_caps;    // count >= 1; PDO[0] обязан быть Fixed 5V

    // ---- GET_CAPABILITY.bmAttributes (commands.md §2.6, Table 6-14) -
    bool supports_disabled_state;      // bit 0
    bool supports_battery_charging;    // bit 1 — для v1 false (pd-scope.md)
    bool supports_usb_pd;              // bit 2 — обычно true
    bool supports_typec_current;       // bit 6
    // bmPowerSource: минимум ОДИН из трёх должен быть true.
    bool power_source_ac;              // bit 8  — AC supply available
    bool power_source_other;           // bit 10 — иной (battery, solar и т.п.)
    bool power_source_vbus;            // bit 14 — питаемся от VBUS партнёра

    // ---- GET_CAPABILITY.bmOptionalFeatures (commands.md §1.6) -------
    bool supports_set_ccom;             // bit 0   — SET_CCOM
    // bit 1 (SET_POWER_LEVEL) — всегда 1 по spec, не конфигурируется
    bool supports_alt_mode_details;     // bit 2   — v1 false
    bool supports_alt_mode_override;    // bit 3   — v1 false
    bool supports_pdo_details;          // bit 4   — GET_PDOS
    bool supports_cable_details;        // bit 5   — GET_CABLE_PROPERTY
    bool supports_external_supply_notif;// bit 6   — внешний supply notify
    bool supports_pd_reset_notif;       // bit 7   — PD reset notification
    bool supports_get_pd_message;       // bit 8   — GET_PD_MESSAGE; v1 false
    bool supports_get_attention_vdo;    // bit 9   — v1 false (VDM нет)
    bool supports_fw_update_request;    // bit 10  — v1 false
    bool supports_negotiated_pl_notif;  // bit 11  — Negotiated Power Level Change notification
    bool supports_security_request;     // bit 12  — v1 false
    bool supports_set_retimer_mode;     // bit 13  — v1 false
    bool supports_chunking;             // bit 14  — chunking support

    // ---- GET_CONNECTOR_CAPABILITY (commands.md §2.7) ----------------
    // Operation Mode bits 0/1/2 (Rp/Rd/DRP) выводятся из source_caps
    // (есть → Provider) и sink_caps (есть → Consumer) + initial mode.
    bool connector_usb2_capable;       // bit 5 Operation Mode
    bool connector_usb3_capable;       // bit 6 Operation Mode
    // Provider / Consumer / Swap-флаги — выводятся из DRP policy и
    // содержания source/sink_caps; в конфиге не задаются явно.

} UcsiPpmConfig;
```

> **Что НЕ в конфиге**: всё, что меняется в runtime через UCSI-команды —
> notification mask (`SET_NOTIFICATION_ENABLE`), текущая CC operation mode
> (после `SET_CCOM`), текущая power/data role, accept_pr_swap/accept_dr_swap
> (после `SET_PDR`/`SET_UOR` bit 2). Эти вещи живут в state библиотеки и
> сбрасываются в дефолт через `PPM_RESET` / `ucsi_ppm_reset`.
>
> **Defaults после `init` / `reset`** (соответствуют PD R3.0 spec):
> - `notification mask` = 0 (все нотификации выключены, см.
>   commands.md §2.5);
> - `cc operation mode` = `initial_cc_operation_mode` из конфига;
> - `accept_pr_swap` = `true` (commands.md §2.10 SET_PDR: «по умолчанию —
>   принимать power swap-ы»);
> - `accept_dr_swap` = `true` (commands.md §2.9 SET_UOR bit 2 default);
> - power/data role — определяется по результату Type-C attach (для
>   `CcModeDrp` зависит от того, кто первым стал Source/Sink).

> **Identity (VID/PID/bcd_device)** в v1-конфиге **отсутствует**. PD VDM
> (Discover Identity) — `❌ NS` по [`pd-scope.md`](pd-scope.md) §1.2;
> мы отвечаем `Not_Supported` и identity не нужна. Поле вернётся в
> конфиг при добавлении VDM-фичи.

---

## 5. Жизненный цикл

### 5.1 Init / Deinit

```c
UcsiPpmStatus ucsi_ppm_init(UcsiPpm* ppm, const UcsiPpmConfig* config);
UcsiPpmStatus ucsi_ppm_deinit(UcsiPpm* ppm);
```

`init` делает:
1. Валидирует конфиг — возвращает `UcsiPpmStatusInvalidConfig` если:
   - обязательный callback NULL;
   - `fusb302_i2c_addr` вне 0x22..0x25;
   - `source_caps.count == 0` или `sink_caps.count == 0`;
   - `source_caps.pdos[0]` или `sink_caps.pdos[0]` не Fixed 5V;
   - `count > UCSI_PPM_MAX_PDOS`;
   - `initial_cc_operation_mode == CcModeDisabled` при `supports_disabled_state == false`;
   - ни один из `power_source_{ac,other,vbus}` не установлен в `true`
     (см. commands.md §2.6: «минимум один обязан быть»).
2. Копирует поля во внутреннее состояние.
3. Заполняет regfile (`VERSION=UCSI_PPM_VERSION_UCSI`, `CCI=0`,
   `MESSAGE_IN/OUT=0`).
4. Поднимает L4 (FUSB302 power-on reset, базовая конфигурация регистров,
   маски прерываний; Rp current = `source_rp_current`).
5. Запускает L3 в начальное состояние Type-C SM:
   - `CcModeRpOnly` → Unattached.SRC;
   - `CcModeRdOnly` → Unattached.SNK;
   - `CcModeDrp` → Unattached.SRC или Unattached.SNK по `drp_advertise_first`;
   - `CcModeDisabled` → Disabled.

После `init` библиотека **не делает ничего**, пока caller не позовёт
`tick` или `notify_*` — никаких background-операций (см.
[`architecture.md`](architecture.md) §8).

`deinit` отключает FUSB302 (Rd/Rp в Open), сбрасывает VBUS source через
GPIO, освобождает внутренние ресурсы. После `deinit` инстанс невалиден
до повторного `init`.

### 5.2 Reset

```c
UcsiPpmStatus ucsi_ppm_reset(UcsiPpm* ppm);
```

Тонкая обёртка над тем же кодом, что обрабатывает UCSI `PPM_RESET` opcode
(commands.md §2.1) — даёт caller-у способ сбросить состояние без
имитации OPM-write. Используется для self_check / автотестов.
Эквивалент `register_write` с CONTROL=PPM_RESET, но синхронный
(возврат после завершения сброса), без выставления CCI/alert.

После `reset`:
- regfile очищен (VERSION/Reserved сохраняются);
- L3 в `Unattached.*` согласно `initial_cc_operation_mode`;
- L4 (FUSB302) полностью переинициализирован;
- notification mask сброшен в 0;
- accept_pr_swap / accept_dr_swap — в дефолты (accept).

### 5.3 Tick

```c
UcsiPpmStatus ucsi_ppm_tick(UcsiPpm* ppm);
```

Точка cooperative-многозадачности. Caller обязан дёргать «регулярно»
(см. [`architecture.md`](architecture.md) §5):
- хотя бы раз в **10–50 мс** в idle;
- сразу при сигнале IRQ FUSB302 (или вместо отдельного `notify_*`-вызова);
- сразу после `ucsi_ppm_register_write` (опционально — `register_write`
  сам прокачивает state-machine достаточно, чтобы CCI обновился; но более
  ленивый caller может полагаться только на тик).

Внутри тика происходит всё: прокачка FUSB302, продвижение Type-C/PRL/PE,
проверка таймаутов, доставка событий в CCI, alert наружу.

### 5.4 Async notifications от caller-а

```c
UcsiPpmStatus ucsi_ppm_notify_fusb302_irq(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm);
```

- `notify_fusb302_irq` — caller дёрнул из ISR (или bottom-half) на INT
  pin. Только выставляет atomic-флаг «требуется прокачка FUSB302» и
  возвращается. Реальное чтение регистров FUSB302 происходит в следующем
  `tick` (см. §8 про concurrency).
- `notify_power_supply_ready` — PSU отстоялся после `power_supply_set`.
  Выставляет atomic-флаг; PSTransitionTimer завершается на следующем
  тике досрочно. Альтернатива — caller не зовёт этот notify, и
  библиотека дожидается vSafe-уровня через FUSB302 MDAC. Зов notify
  **рекомендуется**, если caller имеет точный сигнал готовности PSU —
  это экономит I²C-трафик на MDAC-опрос.

> VBUS-уровень в v1 детектится через FUSB302 (`I_VBUSOK` + MDAC,
> см. [`fusb302.md`](fusb302.md) §1.8). Внешний VBUS-monitor не
> поддерживается; если в будущем понадобится — добавим `notify_vbus_change`
> в v2.

---

## 6. Транспорт OPM ↔ PPM (L1)

UCSI regfile (Table 4-1, 528 байт) — модель: caller пишет/читает байты
по offset-у, библиотека интерпретирует.

```c
UcsiPpmStatus ucsi_ppm_register_read(
    UcsiPpm* ppm,
    uint16_t offset,
    uint16_t length,
    uint8_t* buf
);

UcsiPpmStatus ucsi_ppm_register_write(
    UcsiPpm* ppm,
    uint16_t offset,
    uint16_t length,
    const uint8_t* buf
);
```

Контракт (см. [`architecture.md`](architecture.md) §2):
- `offset + length <= 528`; иначе `UcsiPpmStatusInvalidArg`.
- `register_read` всегда корректен (любой valid range). Чтение из RESERVED
  возвращает нули.
- `register_write` в OPM-read-only регионы (VERSION, CCI, MESSAGE_IN)
  возвращает `UcsiPpmStatusInvalidArg`. Запись в RESERVED — игнор (`Ok`,
  без эффекта).
- Запись, попавшая в `CONTROL` (offset 8..15) и включающая `offset == 8`
  с ненулевым значением байта — триггерит обработку команды.

Размер `length` не ограничен 1 байтом — caller может писать MESSAGE_OUT
целиком (255 байт) одной транзакцией.

> **Порядок записи MESSAGE_OUT → CONTROL**. Для команд с payload-ом
> (например, `SET_PDOS`) caller обязан **сначала** записать MESSAGE_OUT
> (offset 272..526), **потом** CONTROL (offset 8..15). L1 триггерит
> обработку команды по записи в CONTROL byte-0 (см. [`architecture.md`](architecture.md)
> §2), поэтому MESSAGE_OUT должен быть валиден к этому моменту.
> Обратный порядок — undefined behavior: библиотека прочитает старый
> MESSAGE_OUT.

---

## 7. Возможности интроспекции (debug-API)

Необязательное для production, но крайне полезное для отладки и
самотестирования (`apps/self_check`):

```c
typedef enum {
    UcsiPpmStateUnattached,
    UcsiPpmStateAttachWait,
    UcsiPpmStateAttachedSrc,
    UcsiPpmStateAttachedSnk,
    UcsiPpmStateErrorRecovery,
    UcsiPpmStateDisabled,
} UcsiPpmConnectorState;

UcsiPpmConnectorState ucsi_ppm_get_connector_state(const UcsiPpm* ppm);

typedef struct {
    bool       contract_in_place;
    uint16_t   voltage_mv;     // negotiated
    uint16_t   current_ma;     // negotiated
    bool       is_source;      // true = мы source, false = sink
    bool       is_dfp;         // true = DFP, false = UFP
} UcsiPpmContractInfo;

UcsiPpmStatus ucsi_ppm_get_contract(const UcsiPpm* ppm, UcsiPpmContractInfo* out);
```

Эти функции **не** проходят через regfile, не дёргают L4, не меняют
состояние. Только snapshot текущего внутреннего состояния. Для UCSI-OPM
эта информация уже доступна через `GET_CONNECTOR_STATUS` — но интроспект
быстрее и не требует имитации OPM.

> Открытый вопрос: гарантировать ли thread-safe snapshot. В v1 — нет,
> caller обязан дёргать в той же thread что и `tick`. См. §8.

---

## 8. Concurrency / контексты

Контракт (см. [`architecture.md`](architecture.md) §8):

| API                               | Из ISR? | Заметка                                              |
|:----------------------------------|:-------:|:-----------------------------------------------------|
| `ucsi_ppm_alloc`/`free`           | нет     | task-context only                                    |
| `ucsi_ppm_init`/`deinit`          | нет     | task-context only                                    |
| `ucsi_ppm_tick`                   | нет     | блокируется на I²C, использовать `notify_*` из ISR¹  |
| `ucsi_ppm_register_read`/`write`  | нет²    | task-context only (см. §6)                           |
| `ucsi_ppm_get_*` (интроспект)     | нет     | task-context only                                    |
| `ucsi_ppm_notify_fusb302_irq`     | **да**  | ISR-safe; lock-free относительно tick                |
| `ucsi_ppm_notify_power_supply_ready` | **да** | ISR-safe; lock-free относительно tick              |

¹ `tick` блокируется на I²C-операциях к FUSB302 (см.
  [`architecture.md`](architecture.md) §8). Из ISR — недопустимо.
  Правильный паттерн: на INT pin → `notify_fusb302_irq` → возврат из
  ISR. Основной `tick` идёт в task-context и прокачает FUSB302 на
  следующей итерации.

² Transport-callback-и (OPM-сторона) обычно живут в отдельной задаче
  (HID-handler, I²C-slave-ISR). Если транспорт работает из ISR — caller
  обязан очередить событие в task-context перед `register_read/write`.

**Контракт сериализации**:

- **Task-context API** (`tick`, `register_*`, `get_*`, `init`/`deinit`):
  caller обязан сериализовать одним мьютексом или вызывать только из
  одной задачи. Реентрантный вызов из callback-а наружу (например,
  alert) **запрещён**.
- **ISR-context API** (`notify_*`): ISR-safe; могут быть вызваны
  одновременно с `tick` в другой thread, либо рекурсивно через
  вложенные прерывания. Реализуются как atomic-set одного флага,
  без I²C, без блокировок.

> **Уточнение v1**: «atomic-флаг» реализуется через Furi/FreeRTOS
> primitives — `FuriEventFlag` или `FuriThreadFlag`. `notify_*` делает
> `furi_event_flag_set` (ISR-safe в FreeRTOS), `tick` опрашивает флаги
> через `furi_event_flag_get` или ждёт с zero-timeout. Это
> деталь имплементации L2; публичный API остаётся pure C99.

---

## 9. Пример использования (skeleton)

Иллюстрация порядка вызовов; не компилируется, не часть API:

```c
static UcsiPpmConfig config = {
    .hal_ctx = &my_hal_state,

    .time_ms                 = my_time_ms,
    .alert                   = my_opm_alert,
    .i2c_read                = my_i2c_read,
    .i2c_write               = my_i2c_write,
    .gpio_write_vbus_source  = my_vbus_set,
    .power_supply_set        = my_psu_set,
    .has_alt_power           = my_has_battery,

    .gpio_read_fusb302_int   = NULL,  // caller wires INT через notify_fusb302_irq
    .log                     = my_log_sink,

    .fusb302_i2c_addr        = 0x22,

    .initial_cc_operation_mode = UcsiPpmCcModeDrp,
    .drp_advertise_first       = UcsiPpmDrpFirstSrc,
    .source_rp_current         = UcsiPpmRpCurrent3A,

    .source_caps = { .pdos = {
        ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true),
        ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true),
        ucsi_ppm_pdo_fixed_source(20000, 3000, true, false, true, true),
    }, .count = 3 },
    .sink_caps = { .pdos = {
        ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true),
    }, .count = 1 },

    // bmAttributes
    .supports_disabled_state = true,
    .supports_usb_pd         = true,
    .supports_typec_current  = true,
    .power_source_other      = true,   // у нас battery
    .power_source_vbus       = true,   // можем питаться от VBUS

    // bmOptionalFeatures
    .supports_set_ccom         = true,
    .supports_pdo_details      = true,
    .supports_cable_details    = true,
    .supports_pd_reset_notif   = true,
    .supports_negotiated_pl_notif = true,

    // Connector capability
    .connector_usb2_capable  = true,
    .connector_usb3_capable  = false,
};

UcsiPpm* ppm = ucsi_ppm_alloc();
ucsi_ppm_init(ppm, &config);

// FUSB302 IRQ handler (вне библиотеки):
void on_fusb302_int_falling_edge(void) {
    ucsi_ppm_notify_fusb302_irq(ppm);
}

// PSU ready callback (вне библиотеки):
void on_psu_settled(void) {
    ucsi_ppm_notify_power_supply_ready(ppm);
}

// Main loop:
while (run) {
    ucsi_ppm_tick(ppm);
    sleep_ms(20);
}

// OPM transport (отдельная задача, например HID-handler):
void on_opm_write(uint16_t offset, uint16_t len, const uint8_t* buf) {
    // mutex с main loop опущен для краткости
    ucsi_ppm_register_write(ppm, offset, len, buf);
}

void on_opm_alert_from_lib(void* ctx) {
    // дёргаем транспорт наружу: "OPM, читай regfile"
    notify_host_via_hid_interrupt();
}
```

---

## 10. Решения по архитектуре API

Все ранее открытые вопросы зафиксированы. Список решений — для
последующих ревью и чтобы понимать «почему именно так».

- ✅ **Memory model**: `ucsi_ppm_alloc`/`free` поверх Furi malloc
   (FreeRTOS heap). Структура `UcsiPpm` opaque — размер не часть ABI.
   См. §2.1.
- ✅ **Версионирование**: макросы `UCSI_PPM_API_VERSION_*` и
   `UCSI_PPM_VERSION_*` для PD/Type-C/UCSI/BC — в §1.1.
- ✅ **Identity (VID/PID)**: убрана из v1-конфига (VDM не делаем; см. §4).
- ✅ **`notify_vbus_change`**: не вводим в v1 (VBUS детект через FUSB302).
- ✅ **Логирование callback signature**: `vprintf`-style; полная
   отключаемость через `UCSI_PPM_LOG_DISABLE`. На целевой Furi/arm-gcc
   `va_list` работает; альтернатива не требуется (см. §3.7).
- ✅ **GPIO INT pin polling**: optional fallback, `NULL` если caller
   обеспечил IRQ → `notify_fusb302_irq` (см. §3.4).
- ✅ **GPIO write polarity**: `value=true` означает «активировать»,
   физическую инверсию делает caller (см. §3.4).
- ✅ **Concurrency**: `tick`/`register_*` task-only; `notify_*` ISR-safe
   (см. §8).
- ✅ **Atomic-флаги для `notify_*`**: `FuriEventFlag`/`FuriThreadFlag`
   из Furi-runtime — это деталь имплементации L2, публичный API
   pure C99 (см. §8).
- ✅ **Интроспект-API**: в v1 — минимум (§7). Расширение откладываем
   до момента, когда `apps/self_check` или другой caller потребует
   больше. Thread-safety snapshot не гарантируем — caller дёргает в
   той же thread что и `tick`.
- ✅ **Vendor-extensions**: scope-stub (handler возвращает
   `Not_Supported` для VENDOR_DEFINED opcode). Регистрация
   user-handler-ов не входит в v1 API.
- ✅ **Public reset API**: `ucsi_ppm_reset` добавлен (§5.2).
- ✅ **BIST API**: отложено до compliance-сертификации; в v1 любой
   приём BIST → `Not_Supported`.

---

## 11. Что **не** в этом документе

Эти куски — отдельные документы (или будут добавлены):

- Внутренний API L2↔L3 (типизированные команды/события LPM) — в
  [`architecture.md`](architecture.md) §6.
- Внутренний API L3↔L4 (FUSB302-операции и события) — в
  [`architecture.md`](architecture.md) §7 и [`fusb302.md`](fusb302.md).
- Маршаллинг UCSI-команд (opcode → payload разметка) — в
  [`commands.md`](commands.md).
- State-machine деталей — в [`type-c-sm.md`](type-c-sm.md),
  [`prl-sm.md`](prl-sm.md), [`pe-sm.md`](pe-sm.md).
- Тест-стратегия и mock-точки — в [`validation.md`](validation.md).
