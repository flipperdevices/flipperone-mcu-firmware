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

---

## 2. Базовые типы

### 2.1 Handle и владение памятью

Инстанс библиотеки — **opaque** для пользователя:

```c
typedef struct UcsiPpm UcsiPpm;
```

**Аллокация**: caller вызывает `ucsi_ppm_alloc()` — внутри библиотеки
аллокация делается через project-wide allocator (например, FreeRTOS
`pvPortMalloc` или статический пул; см. §10 «Открытые вопросы»).

```c
UcsiPpm* ucsi_ppm_alloc(void);
void     ucsi_ppm_free(UcsiPpm* ppm);
```

> **Альтернатива (статическая)**: `UcsiPpm` — раскрытая структура,
> размер известен compile-time. Это исключает любой malloc, но
> ломает ABI при изменении внутренностей. Решаем при выборе target
> платформы.

### 2.2 Статус

```c
typedef enum {
    UcsiPpmStatusOk = 0,
    UcsiPpmStatusInvalidArg,        // NULL handle / out-of-range offset / etc.
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
- **FUSB302 INT pin** — read-only. Запрашивается из тика для poll-fallback
  (если caller не дёргает `ucsi_ppm_notify_fusb302_irq`).
- **VBUS source enable** — write. Включает/выключает внешний VBUS-switch
  при роли Source.
- **VBUS discharge** — write, optional. Помогает быстрее упасть до vSafe0V
  при detach/Hard Reset.

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

После того как PSU отстоялся, caller обязан позвать:

```c
UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm);
```

(детали в §5.3). Это завершает `PSTransitionTimer` досрочно.

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
определён (см. §10 — открытый вопрос про формат).

---

## 4. Конфигурация

`UcsiPpmConfig` — read-only снимок, передаваемый в `init`. Библиотека
**копирует** все нужные поля внутрь; caller может освобождать структуру
сразу после возврата `init`.

```c
typedef struct {
    // Контекст для всех callback-ов (один общий).
    void* hal_ctx;

    // Обязательные callback-и.
    UcsiPpmTimeMsFn          time_ms;
    UcsiPpmAlertFn           alert;
    UcsiPpmI2cReadFn         i2c_read;
    UcsiPpmI2cWriteFn        i2c_write;
    UcsiPpmGpioReadFn        gpio_read_fusb302_int;
    UcsiPpmGpioWriteFn       gpio_write_vbus_source;
    UcsiPpmPowerSupplySetFn  power_supply_set;
    UcsiPpmHasAltPowerFn     has_alt_power;

    // Опциональные.
    UcsiPpmGpioWriteFn       gpio_write_vbus_discharge;  // NULL = нет discharge
    UcsiPpmLogFn             log;                        // NULL = silent

    // FUSB302.
    uint8_t fusb302_i2c_addr;  // 0x22..0x25

    // Initial capabilities — могут быть переписаны через SET_PDOS.
    UcsiPpmPdoList source_caps;
    UcsiPpmPdoList sink_caps;

    // Identity (PD R3.0 §6.4.4.3).
    // VID/PID/XID — для будущего Discover Identity (v1: используются только
    // как plumbing; реальный VDM мы не отправляем, см. pd-scope.md).
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;

    // Static UCSI flags (bmAttributes / bmOptionalFeatures
    // см. commands.md §2.1 GET_CAPABILITY).
    bool supports_set_uor;       // USB Role Swap
    bool supports_set_pdr;       // Power Role Swap (DRP)
    bool supports_get_pdos;
    bool supports_get_cable_property;
    bool supports_get_pd_message;
    bool supports_async_notification;
    // ... полный список — см. commands.md §2.1.
} UcsiPpmConfig;
```

> **Что НЕ в конфиге**: всё, что меняется в runtime через UCSI-команды —
> notification mask (`SET_NOTIFICATION_ENABLE`), текущая CC operation mode,
> текущая power/data role. Эти вещи живут в state библиотеки и сбрасываются
> в дефолт через `PPM_RESET`.

---

## 5. Жизненный цикл

### 5.1 Init / Deinit

```c
UcsiPpmStatus ucsi_ppm_init(UcsiPpm* ppm, const UcsiPpmConfig* config);
UcsiPpmStatus ucsi_ppm_deinit(UcsiPpm* ppm);
```

`init` делает:
1. Валидирует конфиг (обязательные callback-и не NULL; PDO #1 — Fixed 5V;
   `count >= 1` для source_caps/sink_caps).
2. Копирует поля во внутреннее состояние.
3. Заполняет regfile (VERSION=0x0300, CCI=0, MESSAGE_IN/OUT=0).
4. Поднимает L4 (FUSB302 power-on reset, базовая конфигурация регистров,
   маски прерываний).
5. Запускает L3 в начальное состояние Type-C SM (Unattached.SRC или
   Unattached.SNK в зависимости от DRP-policy).

После `init` библиотека **не делает ничего**, пока caller не позовёт
`tick` или `notify_*` — никаких background-операций (см.
[`architecture.md`](architecture.md) §8).

`deinit` отключает FUSB302 (Rd/Rp в Open), сбрасывает VBUS source через
GPIO, освобождает внутренние ресурсы. После `deinit` инстанс невалиден
до повторного `init`.

### 5.2 Tick

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

### 5.3 Async notifications от caller-а

```c
UcsiPpmStatus ucsi_ppm_notify_fusb302_irq(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_notify_power_supply_ready(UcsiPpm* ppm);
UcsiPpmStatus ucsi_ppm_notify_vbus_change(UcsiPpm* ppm);  // optional
```

- `notify_fusb302_irq` — caller дёрнул из ISR (или bottom-half) на INT
  pin. Эквивалентно «следующий тик прокачает FUSB302», но **не** ждёт
  следующего тика, а делает прокачку сразу. Реентрант-безопасный
  относительно `tick` **в той же thread** (см. §8).
- `notify_power_supply_ready` — PSU отстоялся после `power_supply_set`.
  Прерывает PSTransitionTimer ожидания.
- `notify_vbus_change` — опционально, если caller имеет внешний VBUS
  monitor вместо FUSB302-MDAC. В v1 — VBUS детектится через FUSB302,
  этот callback зарезервирован.

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

| API                               | Реентрант? | Из ISR? |
|:----------------------------------|:----------:|:-------:|
| `ucsi_ppm_alloc`/`free`           | нет        | нет     |
| `ucsi_ppm_init`/`deinit`          | нет        | нет     |
| `ucsi_ppm_tick`                   | нет        | да¹     |
| `ucsi_ppm_register_read`/`write`  | нет        | нет²    |
| `ucsi_ppm_notify_*`               | нет        | да      |
| `ucsi_ppm_get_*` (интроспект)     | нет        | нет     |

¹ Дёрнуть `tick` из ISR можно технически, но не рекомендуется — он
блокирует на I²C-операциях к FUSB302 (см. [`architecture.md`](architecture.md) §8).
Реальный способ — `notify_fusb302_irq` из ISR, основной `tick` в loop.

² Transport-callback-и (OPM-сторона) обычно живут в отдельной задаче
(HID-handler, I²C-slave-ISR). Если транспорт работает из ISR — caller
обязан либо очередить событие в task-context, либо обеспечить
mutual-exclusion с тиком.

**Caller обязан** сериализовать вызовы любой публичной функции (кроме
`notify_*`) одним мьютексом/одной задачей. `notify_*` спроектированы
быть **lock-free относительно ISR**: они только выставляют флаг в
atomic-переменной, реальная работа происходит в следующем `tick`.

> **Уточнение v1**: «atomic-флаг» — это `volatile uint32_t` + memory
> barrier через project HAL. Если требуется поддержка SMP — переходим
> на `_Atomic` (C11), это правка L2 без изменения публичного API.

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
    .gpio_read_fusb302_int   = my_gpio_int_read,
    .gpio_write_vbus_source  = my_vbus_set,
    .power_supply_set        = my_psu_set,
    .has_alt_power           = my_has_battery,
    .fusb302_i2c_addr        = 0x22,
    .source_caps = { .pdos = {
        ucsi_ppm_pdo_fixed_source(5000, 3000, true, false, true, true),
        ucsi_ppm_pdo_fixed_source(9000, 3000, true, false, true, true),
        ucsi_ppm_pdo_fixed_source(20000, 3000, true, false, true, true),
    }, .count = 3 },
    .sink_caps = { .pdos = {
        ucsi_ppm_pdo_fixed_sink(5000, 3000, true, false, false, true, true),
    }, .count = 1 },
    // ... identity, attributes ...
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

## 10. Открытые вопросы

Помечено `[OPEN]` — нужно решить до фиксации API.

1. **[OPEN] Memory model**: `alloc/free` через project allocator или
   полностью статический инстанс (один глобальный или caller-provided
   buffer). Влияет на ABI: если структура раскрытая, размер becomes part
   of API. Предложение по умолчанию — `alloc/free` с делегацией в project
   allocator; если в проекте принят статический allocation — переключим.
2. **[OPEN] Логирование**: формат `vprintf`-style vs structured key-value
   vs furi-log-style макросы. Предложение — `vprintf`-style для простоты,
   с возможностью отключить compile-time флагом.
3. **[OPEN] Интроспект-API**: какие именно структуры экспонируем (текущий
   §7 — минимум). Если `apps/self_check` потребует больше — расширим.
4. **[OPEN] Регистрация vendor-extensions**: UCSI Vendor Defined Command
   (см. [`architecture.md`](architecture.md) §10 п.3) — нужен ли в v1
   regsiter-callback или достаточно scope-stub-а. Предложение — отложить.
5. **[OPEN] Atomic-флаги для `notify_*` из ISR**: использовать platform
   HAL барьер или C11 `_Atomic`. Зависит от выбора компилятора и
   target-платформы.
6. **[OPEN] Версионирование API**: семвер в макросе
   `UCSI_PPM_API_VERSION` или git tag достаточно. Предложение — макрос
   `UCSI_PPM_VERSION_{MAJOR,MINOR,PATCH}` в `ucsi_ppm.h`, чтобы caller
   мог `#if`-гейтить новые поля конфига.

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
