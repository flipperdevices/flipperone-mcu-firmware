# Policy Engine (PE) State Machine

Документ описывает Policy Engine для DRP-порта (Source + Sink) с PR_Swap
и DR_Swap, без VDM, без EPR/AVS/PPS, на PD 3.0. PE — самый
содержательный компонент библиотеки.

Источники:
- *USB Power Delivery Specification r3.2 v1.2* — глава 8 (Policy Engine).
- [`pd-scope.md`](pd-scope.md) — какие messages/timers/counters мы используем.
- [`prl-sm.md`](prl-sm.md) — что PRL предоставляет PE.
- [`fusb302.md`](fusb302.md) — для понимания того, что mapping в железе.
- [`type-c-sm.md`](type-c-sm.md) — что PE получает от Type-C и что ей шлёт.
- [`commands.md`](commands.md) — как PE реагирует на запросы PPM.

---

## 1. Обзор

### 1.1 Что делает PE

- Управляет PD-контрактом: что мы предлагаем как Source, что мы запрашиваем как Sink.
- Реагирует на входящие PD-сообщения от партнёра.
- Инициирует PR_Swap / DR_Swap / Get_Sink_Cap / Get_Source_Cap / Soft_Reset / Hard_Reset.
- Орchestrирует Hard Reset recovery (VBUS bounce, переход в дефолтное состояние).
- Считает HardResetCounter, проверяет NoResponseTimer.
- Принимает решения policy: какой PDO offered, какой запрошен, accept/reject/wait запросов swap.

### 1.2 Что делает Type-C SM (а не PE)

- CC detection, attach, debounce.
- Polarity lock.
- VBUS на/откл через внешний switch (PE говорит «включи»; Type-C делает).
- Track power_role / data_role (PE говорит «новая роль»; Type-C обновляет SWITCHES1).
- Перевод между Attached.SRC / Attached.SNK при PR_Swap.

### 1.3 Что делает PRL (а не PE)

- MessageID counter, duplicate detection.
- TX / RX через FUSB302.
- SinkTx collision avoidance.
- Soft_Reset counter semantics.

### 1.4 Высокоуровневая структура PE

Канонические group-ы состояний PE из спеки (мы реализуем подмножество):

```
┌─────────────────────────────────────────────────────────────────────┐
│                              Common                                 │
│  (Initial, Soft_Reset send/recv, Not_Supported send, BIST send)    │
└─────────────────────────────────────────────────────────────────────┘
        ┌────────────────────┐        ┌────────────────────┐
        │   Source states    │        │    Sink states     │
        │  PE_SRC_Startup    │        │   PE_SNK_Startup   │
        │  PE_SRC_Discovery  │        │   PE_SNK_Discovery │
        │  PE_SRC_Send_Caps  │        │   PE_SNK_Eval_Caps │
        │  PE_SRC_Negotiate  │        │   PE_SNK_Select    │
        │  PE_SRC_Transition │        │   PE_SNK_Transition│
        │  PE_SRC_Ready  ◄───┼───┐    │   PE_SNK_Ready ◄───┼───┐
        │  PE_SRC_Hard_Reset │   │    │   PE_SNK_Hard_Reset│   │
        │  ...               │   │    │   ...              │   │
        └────────────────────┘   │    └────────────────────┘   │
                                 │                              │
                                 ▼                              ▼
        ┌──────────────────────────────────────────────────────────────┐
        │              PR_Swap (SRC↔SNK) states                        │
        │  - Send_Swap, Evaluate_Swap, Accept, Transition_to_off,      │
        │    Assert_Rd/Rp, Wait_Source_On, Send_PS_RDY                 │
        ├──────────────────────────────────────────────────────────────┤
        │              DR_Swap (DFP↔UFP) states                        │
        │  - Send_Swap, Evaluate_Swap, Accept, Change_to_DFP/UFP       │
        └──────────────────────────────────────────────────────────────┘
```

При attach Type-C SM → PE_SRC_Startup или PE_SNK_Startup. Дальше PE
ведёт state machine до `PE_*_Ready` — стабильное состояние с
explicit-контрактом.

В отсутствие partner-а PE находится в специальном состоянии `PE_Detached`
(§1.5 ниже) — там нет ни Source, ни Sink логики; ждём `attached` event.

---

## 1.5 PE_Detached

**Вход**: после `PPM_RESET`; после detach (event `detached` от Type-C SM).

**Действия**:
- Очистить весь contract-state (`current_rdo = None`, `in_explicit_contract = false`).
- Очистить кэш partner caps.
- Сбросить `HardResetCounter = 0`, `CapsCounter = 0`.
- Не отменять policy-state-флаги (target_power_level, accept_pr_swap и т.п.) — они задаются PPM-ом и применяются на следующем attach.
- Уведомить PPM: `connect_change`, `power_op_mode_change=USB Default`.

**События**:
- `Type-C SM: attached(role, …)` → переход в `PE_SRC_Startup` или `PE_SNK_Startup` в зависимости от роли.
- `PPM: pending commands` (SET_PDR, SET_UOR, set_power_level, etc.) → принять и сохранить; применятся при следующем attach.

В этом состоянии PE **не** реагирует на FUSB302-сообщения (партнёра нет —
их не должно быть; если что-то прилетело — лог + ignore).

---

## 2. Source-side states

### 2.1 PE_SRC_Startup

**Вход**: от Type-C SM, при переходе в `Attached.SRC` (или после PR_Swap → мы стали Source).

**Действия**:
1. Сбросить PRL (через `prl_reset(reason=attach_or_pr_swap)`).
2. Установить `CapsCounter = 0` (опциональный счётчик, см. [`pd-scope.md`](pd-scope.md) §7).
3. Установить `HardResetCounter = 0`.
4. Если это первое подключение после attach: запустить `tFirstSourceCap` (max 250 мс) — после tFirstSourceCap начинаем слать Source_Capabilities.
5. Если это после PR_Swap: запустить `SwapSourceStartTimer` (tSwapSourceStart, ≥20 мс) — пауза, чтобы партнёр был готов.

**Переход**: по истечении таймера → `PE_SRC_Send_Capabilities`.

### 2.2 PE_SRC_Send_Capabilities

**Вход**: из `PE_SRC_Startup`, `PE_SRC_Discovery`, после Hard Reset recovery.

**Действия**:
1. Сконструировать `Source_Capabilities` message из наших PDOs (см. §8); учесть SET_POWER_LEVEL target (§8.4).
2. `HOST_CUR` уже = `10b` (SinkTxNG) с момента `Attached.SRC` entry (см. [`type-c-sm.md`](type-c-sm.md) §2.5 step 10). Подтверждать не нужно.
3. Отправить через `prl.tx_request(msg)`.
4. Запустить `SenderResponseTimer` (tSenderResponse, ~300 мс).
5. Inc `CapsCounter`.
6. После `I_TXSENT` (PRL событие MessageSent): установить `HOST_CUR = 11b` (SinkTxOk) — теперь sink может инициировать AMS.

**События**:
- `PRL: MessageReceived(Request)` → отменить SenderResponseTimer → `PE_SRC_Negotiate_Capability`.
- `PRL: MessageFailed(reason)` → `PE_SRC_Send_Soft_Reset` если retry-fail на первом attempt; `PE_SRC_Hard_Reset` если уже после soft-reset attempt.
- `SenderResponseTimer expired` → `PE_SRC_Hard_Reset`.
- `PRL: MessageReceived(<other>)` → разные ответы (см. §2.2.1).
- `PRL: HardResetReceived` → `PE_SRC_Hard_Reset_Received`.

#### 2.2.1 Обработка не-Request сообщений в Send_Capabilities

Если в окне ожидания Request пришло что-то другое:
- `Get_Source_Cap` → re-send Source_Capabilities (стандартное поведение).
- `Get_Sink_Cap` → ответить `Sink_Capabilities`, остаться в Send_Capabilities.
- `Soft_Reset` → `PE_SRC_Soft_Reset`.
- `DR_Swap`, `PR_Swap`, `VCONN_Swap` → spec говорит accept или wait or reject; safest = `Wait` (мы в середине AMS).
- Любое Extended / VDM / unsupported → `PE_Send_Not_Supported`, остаться.

### 2.3 PE_SRC_Discovery

**Вход**: после `PE_SRC_Send_Capabilities` если CapsCounter exceeded и sink не ответил (опционально), или для re-send Source_Capabilities по таймеру.

**Действия**:
1. Запустить `SourceCapabilityTimer` (tTypeCSendSourceCap, ~150 мс).

**События**:
- `SourceCapabilityTimer expired` → если `CapsCounter < nCapsCount (50)` → `PE_SRC_Send_Capabilities`. Иначе → `PE_SRC_Disabled` (или просто остаёмся в idle, периодически пробуя — реализационное решение; в v1 идём в Disabled чтобы не спамить).
- `PRL: MessageReceived(<any>)` → таймер отмена → `PE_SRC_Negotiate_Capability` (для Request) или соответствующий handler.

> **Упрощение в v1**: можно объединить `Discovery` и `Send_Capabilities` в один state с loop-таймером, не выделяя отдельный state. Это сэкономит 1 state. Решим при имплементации.

### 2.4 PE_SRC_Negotiate_Capability

**Вход**: получили `Request` (Data Message 0x02) от sink.

**Действия (synchronous, без таймеров)**:
1. Извлечь RDO из request (1 объект 32-битный).
2. Распарсить `Object Position` (биты 30:28) — какой из наших PDO выбран (1..7).
3. Если `Object Position == 0` или > числа наших PDO → invalid → `PE_SRC_Capability_Response` с Reject.
4. Извлечь `Operating Current` (биты 19:10) и `Maximum Operating Current` (биты 9:0).
5. Проверить: запрошенный ток ≤ Maximum Current выбранного PDO?
   - Если **да** и мы можем дать столько → Accept.
   - Если **нет** (mismatch flag в RDO bit 26 может быть = 1, сообщающий «мне нужно больше») → Reject или Accept с пометкой Capability Mismatch у себя.
6. Если PE не уверена или временно не может (что нетривиально — обычно мы готовы) → Wait.

**Переход**:
- Decision = Accept → отправить Accept control message → `PE_SRC_Transition_Supply`.
- Decision = Reject → `PE_SRC_Capability_Response` (отправляет Reject).
- Decision = Wait → `PE_SRC_Capability_Response` (отправляет Wait); sink будет retry через `SinkRequestTimer`.

### 2.5 PE_SRC_Transition_Supply

**Вход**: после Accept на Request.

**Действия**:
1. Отправлен Accept (через PRL).
2. Подождать I_TXSENT.
3. **Транзиция мощности** на новое значение (включить нужное напряжение). У нас Fixed PDO — это означает переключить external switch на новый voltage rail (или PWM PSU на новое значение). Это **внешняя инфраструктура**; PE зовёт callback `power_supply_set(voltage, current)`.
4. Запустить `PSTransitionTimer` (tPSTransition SPR, nom 500 мс).
5. После того как power_supply подтвердил, что voltage settled (callback `power_supply_ready()` или мониторинг через FUSB302 MDAC): отправить `PS_RDY` (Control message 0x06).

**События**:
- `power_supply_ready` → отправить `PS_RDY` → ждать `I_TXSENT` → отменить PSTransitionTimer → `PE_SRC_Ready`.
- `PSTransitionTimer expired` (power_supply не успел) → `PE_SRC_Hard_Reset`.
- `PRL: HardResetReceived` → `PE_SRC_Hard_Reset_Received`.

### 2.6 PE_SRC_Ready

**Вход**: после `PE_SRC_Transition_Supply`, после Reject/Wait response, или после успешной PR_Swap→SRC.

**Действия**:
- Состояние «explicit contract установлен». Sink довольный.
- Опубликовать в PPM: контракт установлен, `Power Operation Mode = PD` в `GET_CONNECTOR_STATUS`.
- Запустить `SourceEPRKeepAliveTimer`? Нет — мы EPR не делаем.
- Опционально: `tACTempUpdate` (500 мс) — для temperature monitoring; не делаем в v1.

**События**:
- `PRL: MessageReceived(Request)` (renegotiation) → `PE_SRC_Negotiate_Capability`.
- `PRL: MessageReceived(Get_Source_Cap)` → `PE_SRC_Send_Capabilities`.
- `PRL: MessageReceived(Get_Sink_Cap)` → отправить Sink_Capabilities (мы DRP, можем быть sink-ом тоже — отправляем наши sink-caps), остаться.
- `PRL: MessageReceived(DR_Swap)` → `PE_DRS_DFP_UFP_Evaluate_Swap` (см. §5).
- `PRL: MessageReceived(PR_Swap)` → `PE_PRS_SRC_SNK_Evaluate_Swap` (см. §4).
- `PRL: MessageReceived(VCONN_Swap)` → отправить Reject (мы VCONN не источаем).
- `PRL: MessageReceived(Soft_Reset)` → `PE_SRC_Soft_Reset`.
- `PRL: MessageReceived(<Extended/VDM/Data_Reset/Get_Country_Codes/...>)` → `PE_Send_Not_Supported`, остаться в Ready.
- `PRL: HardResetReceived` → `PE_SRC_Hard_Reset_Received`.
- `PE_internal: send_pr_swap_request` (от PPM, см. [`commands.md`](commands.md) §2.10 SET_PDR) → `PE_PRS_SRC_SNK_Send_Swap`.
- `PE_internal: send_dr_swap_request` (от PPM, см. [`commands.md`](commands.md) §2.9 SET_UOR) → `PE_DRS_DFP_UFP_Send_Swap`.
- `PE_internal: send_get_sink_cap` (от PPM, для GET_PDOS с Partner=1) → `PE_SRC_Get_Sink_Cap`.
- `PE_internal: source_caps_changed` (от PPM, SET_PDOS обновил наши PDO) → `PE_SRC_Send_Capabilities` (renegotiate).
- `PE_internal: set_power_level` (от PPM) → renegotiate с новым target → `PE_SRC_Send_Capabilities`.

### 2.7 PE_SRC_Capability_Response

**Вход**: после Negotiate с decision Reject или Wait.

**Действия**:
1. Отправить Reject или Wait control message.
2. Ждать I_TXSENT.

**Переход**:
- I_TXSENT → `PE_SRC_Ready` (мы сохранили предыдущий контракт). Sink будет retry через SinkRequestTimer (tSinkRequest, ≥100 мс).
- I_RETRYFAIL → `PE_SRC_Send_Soft_Reset`.

### 2.8 PE_SRC_Get_Sink_Cap

**Вход**: от Ready, по запросу PPM (GET_PDOS с Partner=1).

**Действия**:
1. Отправить `Get_Sink_Cap` control message.
2. Запустить SenderResponseTimer.

**События**:
- `PRL: MessageReceived(Sink_Capabilities)` → сохранить в local cache → return success в PPM → `PE_SRC_Ready`.
- `PRL: MessageReceived(Not_Supported)` → partner не PD-capable как sink (странно для DRP-partner) → fail в PPM → `PE_SRC_Ready`.
- `SenderResponseTimer expired` → `PE_SRC_Send_Soft_Reset`.

### 2.9 PE_SRC_Send_Soft_Reset

**Вход**: при protocol error (timeout, retry fail, unexpected message в неподходящем state).

**Действия**:
1. `prl.prl_reset(reason=soft_reset_sent)` — обнулить MessageIDCounter.
2. Отправить Soft_Reset control message (MessageID будет = 0 после reset).
3. Запустить SenderResponseTimer.

**События**:
- `PRL: MessageReceived(Accept)` → success → `PE_SRC_Send_Capabilities` (restart AMS).
- `PRL: MessageFailed(retry_fail)` или `SenderResponseTimer expired` → `PE_SRC_Hard_Reset`.
- `PRL: MessageReceived(<other>)` → ignore (мы в soft reset).
- `PRL: HardResetReceived` → `PE_SRC_Hard_Reset_Received`.

### 2.10 PE_SRC_Soft_Reset (responder)

**Вход**: получили Soft_Reset.

**Действия**:
1. `prl.prl_reset(reason=soft_reset_received)`.
2. Отправить Accept.
3. После I_TXSENT → `PE_SRC_Send_Capabilities` (заново шлём caps).

### 2.11 PE_SRC_Hard_Reset

**Вход**: при unrecoverable protocol error (после retry на Soft_Reset; timeout критичный; PE policy).

**Действия**:
1. Inc `HardResetCounter`.
2. Если `HardResetCounter > nHardResetCount (2)` → `PE_SRC_Disabled` (partner unresponsive; запустить NoResponseTimer — см. §12).
3. Иначе:
   - `prl.send_hard_reset()` — асинхронно через FUSB302.
4. Ждать `PRL: HardResetSent` (или `I_HARDSENT`).

**Переход**:
- `PRL: HardResetSent` → `PE_SRC_Transition_to_default`.
- `PRL: MessageFailed(<>)` если send_hard_reset не сработал — ретрить или Disabled.

### 2.12 PE_SRC_Hard_Reset_Received

**Вход**: получили Hard Reset BMC signal от партнёра.

**Действия**: PRL уже сбросил counters. Дальше как при Hard Reset, который мы инициировали.

**Переход**: → `PE_SRC_Transition_to_default`.

### 2.13 PE_SRC_Transition_to_default

**Вход**: после Hard Reset (initiated или received).

**Действия**:
1. Снять VBUS (выключить внешний switch) → ждать vSafe0V (мониторинг через MDAC; tPSHardReset 25-35 мс — пауза, чтобы убедиться).
2. Реинициализировать data role (DR_Swap default — DFP для source).
3. Поднять VBUS обратно до vSafe5V (внешний switch ON; ждать через MDAC).
4. Уведомить PPM: контракт сброшен, переходим в no-contract.
5. После vSafe5V → `PE_SRC_Startup` (для повторной discovery).

---

## 3. Sink-side states

### 3.1 PE_SNK_Startup

**Вход**: от Type-C SM, при переходе в `Attached.SNK`, или после PR_Swap → мы стали Sink.

**Действия**:
1. `prl.prl_reset(reason=attach_or_pr_swap)`.
2. Установить `HardResetCounter = 0`.
3. Sink path enabled (см. [`commands.md`](commands.md) §2.26 SET_SINK_PATH).

**Переход**: → `PE_SNK_Discovery`.

### 3.2 PE_SNK_Discovery (== Wait_For_Capabilities)

**Действия**:
1. Запустить `SinkWaitCapTimer` (tTypeCSinkWaitCap, ~465 мс).

**События**:
- `PRL: MessageReceived(Source_Capabilities)` → отменить timer → `PE_SNK_Evaluate_Capability`.
- `SinkWaitCapTimer expired` → `PE_SNK_Hard_Reset`.
- `PRL: MessageReceived(<other>)` → spec говорит игнорировать до получения Source_Caps. Реально — отвечаем Not_Supported на что попало.
- `PRL: HardResetReceived` → `PE_SNK_Hard_Reset_Received`.

### 3.3 PE_SNK_Evaluate_Capability

**Действия (synchronous)**:
1. Распарсить Source_Capabilities — массив PDO (1..7).
2. Извлечь revision из header → обновить `peer_spec_rev` / `our_spec_rev` (см. [`prl-sm.md`](prl-sm.md) §8).
3. Применить **selection policy** (см. §11): какой PDO выбираем? Какой запрашиваем ток?
4. Заполнить RDO.

**Переход**: → `PE_SNK_Select_Capability`.

### 3.4 PE_SNK_Select_Capability

**Действия**:
1. Отправить `Request` (data message 0x02) с RDO.
2. Запустить SenderResponseTimer.

**События**:
- `PRL: MessageReceived(Accept)` → отменить timer → `PE_SNK_Transition_Sink`.
- `PRL: MessageReceived(Reject)` → отменить timer → если был explicit contract → `PE_SNK_Ready` (keeping old); если нет — `PE_SNK_Wait_For_Capabilities` (но обычно spec считает это hard reset).
- `PRL: MessageReceived(Wait)` → отменить timer → `PE_SNK_Ready` (keeping old); запустить `SinkRequestTimer` (tSinkRequest, ≥100 мс) для retry.
- `SenderResponseTimer expired` → `PE_SNK_Hard_Reset` (или Send_Soft_Reset как промежуточный).
- `PRL: HardResetReceived` → `PE_SNK_Hard_Reset_Received`.

### 3.5 PE_SNK_Transition_Sink

**Действия**:
1. Запустить `PSTransitionTimer` (tPSTransition SPR, nom 500 мс).
2. Ждать `PS_RDY` от Source (= source готов с новой мощностью).

**События**:
- `PRL: MessageReceived(PS_RDY)` → отменить timer → новый контракт active → `PE_SNK_Ready`. Уведомить PPM (NegotiatedPowerLevelChange).
- `PSTransitionTimer expired` → `PE_SNK_Hard_Reset`.
- `PRL: HardResetReceived` → `PE_SNK_Hard_Reset_Received`.

### 3.6 PE_SNK_Ready

**Вход**: контракт установлен.

**Действия**:
- Уведомить PPM: `RDO active`, `Power Operation Mode = PD`.

**События**:
- `PRL: MessageReceived(Source_Capabilities)` → unsolicited (source сам решил обновить) → `PE_SNK_Evaluate_Capability`.
- `PRL: MessageReceived(Get_Sink_Cap)` → отправить Sink_Capabilities → `PE_SNK_Give_Sink_Cap`.
- `PRL: MessageReceived(Get_Source_Cap)` → spec: мы Sink, так что отправить Not_Supported (мы не Source).
- `PRL: MessageReceived(DR_Swap)` → `PE_DRS_UFP_DFP_Evaluate_Swap`.
- `PRL: MessageReceived(PR_Swap)` → `PE_PRS_SNK_SRC_Evaluate_Swap`.
- `PRL: MessageReceived(VCONN_Swap)` → Reject.
- `PRL: MessageReceived(Soft_Reset)` → `PE_SNK_Soft_Reset`.
- `PRL: MessageReceived(<Extended/VDM/...>)` → `PE_Send_Not_Supported`.
- `PRL: HardResetReceived` → `PE_SNK_Hard_Reset_Received`.
- `PE_internal: send_pr_swap_request` → `PE_PRS_SNK_SRC_Send_Swap`.
- `PE_internal: send_dr_swap_request` → `PE_DRS_UFP_DFP_Send_Swap`.
- `PE_internal: send_get_source_cap` (от PPM для GET_PDOS Partner=1) → `PE_SNK_Get_Source_Cap`.
- `PE_internal: sink_caps_changed` (от PPM SET_PDOS) → отправить Request с новым target → `PE_SNK_Select_Capability`.
- `PE_internal: set_power_level` → renegotiate → `PE_SNK_Select_Capability`.

### 3.7 PE_SNK_Get_Source_Cap

**Действия**:
1. Отправить `Get_Source_Cap`.
2. Запустить SenderResponseTimer.

**События**:
- `PRL: MessageReceived(Source_Capabilities)` → cache в PE для PPM → `PE_SNK_Evaluate_Capability` (партнёр может ожидать наш Request — re-negotiate).
- `PRL: MessageReceived(Not_Supported)` → partner not PD-source → fail в PPM → `PE_SNK_Ready`.
- `SenderResponseTimer expired` → `PE_SNK_Send_Soft_Reset`.

### 3.8 PE_SNK_Give_Sink_Cap

**Действия**:
1. Отправить `Sink_Capabilities` (Data message 0x04) с нашими sink PDO.
2. После I_TXSENT → `PE_SNK_Ready`.

### 3.9 PE_SNK_Send_Soft_Reset

Аналогично §2.9 (SRC) но в sink-режиме.

**Переход после Accept**: → `PE_SNK_Wait_For_Capabilities` (= Discovery), потому что после Soft_Reset Source перешлёт Source_Capabilities.

### 3.10 PE_SNK_Soft_Reset (responder)

Аналогично §2.10. После Accept → `PE_SNK_Wait_For_Capabilities`.

### 3.11 PE_SNK_Hard_Reset

**Действия**:
1. **Проверить `has_alternative_power_source()`** — если `false`, мы — единственный потребитель VBUS-питания от партнёра; Hard Reset нас вырубит. В этом случае:
   - **Не** делаем Hard Reset; вместо этого `PE_SNK_Disabled` с уведомлением PPM `error(Error Information bit 5 = Dead Battery)`.
   - Это deviation от spec (spec ожидает Hard Reset), но прагматичное решение для устройств без battery.
2. Иначе:
   - Inc `HardResetCounter`.
   - Если > nHardResetCount → `PE_SNK_Disabled` (запустить NoResponseTimer).
   - Иначе `prl.send_hard_reset()`.

**Переход**: `PRL: HardResetSent` → `PE_SNK_Transition_to_default`.

### 3.12 PE_SNK_Hard_Reset_Received

→ `PE_SNK_Transition_to_default`.

### 3.13 PE_SNK_Transition_to_default

**Действия**:
1. Ждать пока VBUS упадёт до vSafe0V (Source сейчас должен это сделать).
2. Sink path disable (защита).
3. Ждать VBUS вернётся до vSafe5V. Тайм-аут tPSHardReset + tSrcRecover (для дебатов; реально безопасно ≥800 мс).
4. Sink path enable обратно.
5. Уведомить PPM: контракт сброшен.
6. → `PE_SNK_Startup` (ждать новых Source_Caps).

---

## 4. Power Role Swap (PR_Swap)

### 4.1 Overview

PR_Swap — это два таймлайн-а: на стороне инициирующего и на стороне
отвечающего. Для DRP-порта мы должны реализовать обе стороны и для
обоих направлений (SRC→SNK и SNK→SRC). Это **8 различных state**:

| Направление | Роль       | States                                         |
|:------------|:-----------|:-----------------------------------------------|
| SRC → SNK   | Initiator (мы SRC) | Send_Swap, Wait_Accept, Transition_off, Assert_Rd, Wait_Source_On, Send_PS_RDY  |
| SRC → SNK   | Responder (мы SRC, партнёр инициирует) | Evaluate_Swap, Accept, Send_PS_RDY (мы говорим «отключаюсь»), Wait_PS_RDY, Assert_Rd, Wait_Source_On |
| SNK → SRC   | Initiator (мы SNK) | Send_Swap, Wait_Accept, Wait_PS_RDY, Assert_Rp, Source_On, Send_PS_RDY |
| SNK → SRC   | Responder (мы SNK, партнёр инициирует) | Evaluate_Swap, Accept, Wait_PS_RDY, Assert_Rp, Source_On, Send_PS_RDY |

В каноническом PE спека делит на ~12 явных PR_Swap-substate-ов. Здесь —
концептуальный плоский набор.

### 4.2 SRC → SNK (мы инициируем, мы изначально Source)

```
PE_SRC_Ready
   │ send_pr_swap_request
   ▼
PE_PRS_SRC_SNK_Send_Swap
   │ tx PR_Swap → SenderResponseTimer
   ▼ Accept received
PE_PRS_SRC_SNK_Transition_to_off
   │ Power supply OFF (callback external)
   │ wait vSafe0V (MDAC threshold)
   │ tPSSourceOff = 750-920 ms
   ▼ vSafe0V reached
PE_PRS_SRC_SNK_Assert_Rd
   │ Type-C SM: SWITCHES0.PDWN=1, PU_EN=0
   │ Update internal power_role = Sink
   ▼
PE_PRS_SRC_SNK_Wait_Source_On
   │ wait PS_RDY from new source
   │ tPSSourceOn = 390-480 ms
   ▼ PS_RDY received
PE_SNK_Startup  (теперь мы Sink)
   │ … standard sink flow → Wait_For_Caps
```

Если на любом шаге timeout / Reject / Hard Reset → отмена swap, попытка
восстановить state (Hard Reset чаще всего).

### 4.3 SRC → SNK (мы отвечаем, мы изначально Source, партнёр инициирует)

```
PE_SRC_Ready
   │ MessageReceived(PR_Swap)
   ▼
PE_PRS_SRC_SNK_Evaluate_Swap
   │ Decision: accept / reject / wait (policy)
   ▼ Accept decision
   │ tx Accept
   ▼
PE_PRS_SRC_SNK_Transition_to_off
   │ Power supply OFF, wait vSafe0V
   ▼
PE_PRS_SRC_SNK_Send_PS_RDY
   │ tx PS_RDY (= I'm done with source role)
   ▼
PE_PRS_SRC_SNK_Assert_Rd  
   │ ... (same path as initiator from here)
   ▼
PE_PRS_SRC_SNK_Wait_Source_On
   ▼ PS_RDY received from partner
PE_SNK_Startup
```

(Состояния `Transition_to_off` / `Assert_Rd` / `Wait_Source_On` —
общие для initiator-а и responder-а.)

### 4.4 SNK → SRC

Симметрично §4.2/§4.3 но в обратную сторону. Ключевая разница: мы
заканчиваем в `PE_SRC_Startup` (→ Send_Capabilities).

Не расписываю отдельно — структура та же, только Assert_Rp вместо Assert_Rd,
поднимаем VBUS вместо опускания.

### 4.5 PR_Swap evaluation policy

При получении PR_Swap (responder side) PE решает:
- **Reject** если:
  - `!accept_pr_swap` (PPM-policy флаг от `SET_PDR` bit 2 = 0 — OPM запретил accept).
  - Мы — `PE_SNK_Ready` и `!has_alternative_power_source()` (мы не сможем стать source без потери VBUS-питания).
- **Wait** если:
  - Мы не в `PE_*_Ready` (в середине AMS).
  - HardResetCounter > 0 (recovery в процессе).
- **Accept** в остальных случаях.

В v1 simplified: Accept если в PE_*_Ready и `accept_pr_swap`; Wait в Ready при HardResetCounter>0; Reject иначе.

### 4.5.1 PR_Swap failure handling (initiator side)

После отправки `PR_Swap` (мы initiator):

| Событие                                            | Действие                                                                |
|:---------------------------------------------------|:------------------------------------------------------------------------|
| `Accept` received                                  | Продолжаем по happy path (§4.2/§4.4).                                   |
| `Reject` received                                  | Swap не состоялся. Уведомить PPM: `SET_PDR` fail with Error Information `Port partner rejected swap`. → `PE_*_Ready` (остаёмся с прежней ролью). |
| `Wait` received                                    | Партнёр не готов сейчас. Запустить `SinkRequestTimer` (≥100 мс) для retry. Уведомить PPM: pending. → `PE_*_Ready`. |
| `SenderResponseTimer` expired                      | → `PE_*_Send_Soft_Reset`. После recovery — `PE_*_Ready`, swap не состоялся.|
| `PSSourceOffTimer` expired (VBUS не упал)          | См. §4.5.2 ниже.                                                        |
| `PSSourceOnTimer` expired (new source не подал VBUS) | См. §4.5.2 ниже.                                                      |
| `MessageRx(PR_Swap)` (collision: partner тоже шлёт)| Receiver-side имеет приоритет — abort our swap, отвечаем по responder path. |

### 4.5.2 PR_Swap critical timeouts: dead-battery handling

`PSSourceOffTimer` expired в `PE_PRS_SRC_SNK_Transition_to_off` означает,
что мы Source, выключили VBUS, но не дождались vSafe0V. Стандартный
recovery — `PE_*_Hard_Reset`, но это вырубит контракт целиком.

`PSSourceOnTimer` expired в `PE_PRS_SRC_SNK_Wait_Source_On` означает,
что мы уже Sink (CC переключены на Rd), но новый source (бывший sink)
не подал VBUS. Мы без питания — стандартный Hard Reset не поможет (его
нечего отправлять, мы dead).

**Policy для обоих случаев**:
- Если `has_alternative_power_source()` → нормальный Hard Reset через `PE_*_Hard_Reset`.
- Иначе:
  - PSSourceOffTimer expired: попытаться revert (включить VBUS обратно, остаться Source) → `PE_SRC_Ready`. Уведомить PPM: error `PPM Policy Conflict`.
  - PSSourceOnTimer expired: попытаться revert (вернуть Rp терминации, ждать VBUS из неоткуда — обычно невозможно) → `PE_*_Disabled`. Уведомить PPM: error.

Это deviation от spec; обоснование — без battery «честный» Hard Reset
эквивалентен power-off.

### 4.6 PR_Swap timers

| Timer                | Параметр              | Используется                                              |
|:---------------------|:----------------------|:----------------------------------------------------------|
| SenderResponseTimer  | tSenderResponse       | После tx PR_Swap, ждать Accept/Reject/Wait.               |
| PSSourceOffTimer     | tPSSourceOff (750-920 мс) | Initial source ждёт пока VBUS упадёт vSafe0V.         |
| PSSourceOnTimer      | tPSSourceOn (390-480 мс) | New sink ждёт пока new source поднимет VBUS, PS_RDY.   |
| SwapSourceStartTimer | tSwapSourceStart (≥20 мс) | New source перед первым Source_Capabilities (см. §2.1). |

### 4.7 Coordination с Type-C SM

PR_Swap **не** меняет Type-C SM state (Attached.* остаётся). Но Type-C
SM должна:
- Менять `SWITCHES1.POWER_ROLE` после swap.
- Менять `SWITCHES0` (PDWN/PU_EN) при role transition.

**Чёткое разделение**: PE дёргает Type-C SM через `transition_pr_swap(new_role)`,
**Type-C SM сама** пишет SWITCHES0 (PDWN/PU_EN) и SWITCHES1.POWER_ROLE.
PE не пишет SWITCHES* напрямую — это инкапсуляция Type-C SM-а.

PE **напрямую** через L4 делает только voltage measurement (MDAC threshold
для vSafe0V/vSafe5V detection) — см. §4.8.

### 4.8 vSafe0V / vSafe5V monitoring в PR_Swap

Когда PE в `Transition_to_off` (нам нужно дождаться vSafe0V для VBUS off):
1. PE зовёт L4 `set_compare_threshold_mv(800, below)` — MDAC выставлен на
   800 мВ (vSafe0V threshold), маска I_COMP_CHNG открыта.
2. Когда VBUS опускается ниже 800 мВ → `Fusb302EventCompChanged` → PE проверяет
   `STATUS0.COMP=0` → vSafe0V reached → переход в следующее состояние.
3. Параллельно работает PSSourceOffTimer (750-920 мс) — если COMP не сработал
   за это время → failure path (см. §4.5.2).

Аналогично для vSafe5V monitoring в `Wait_Source_On`.

### 4.9 Notification semantics для PR_Swap

[`commands.md`](commands.md) §2.10 говорит: «successful power role swap
should not result in connector status change notification». Это значит **не
требуется ждать ACK** от OPM-а; не нужно держать contract в pending state
до ACK_CC_CI.

Но мы **всё равно** publish-им event `power_direction_change` в PPM, потому что:
- OPM хочет знать о новой роли для UI.
- Connector Status Change bit 12 «Power Direction Changed» предназначен
  именно для этого.

То есть: нет «ждать ACK для applying изменения» (изменение уже applied),
но event для информирования OPM посылается.

---

## 5. Data Role Swap (DR_Swap)

Проще чем PR_Swap — без VBUS, без role swap, только data role flip.

### 5.1 Initiator (DFP→UFP, мы DFP)

```
PE_SRC_Ready (или PE_SNK_Ready)
   │ send_dr_swap_request
   ▼
PE_DRS_DFP_UFP_Send_Swap
   │ tx DR_Swap → SenderResponseTimer
   ▼ Accept received
PE_DRS_DFP_UFP_Change_to_UFP
   │ Type-C SM: SWITCHES1.DATA_ROLE = 0
   │ Update internal data_role = UFP
   ▼
back to PE_SRC_Ready / PE_SNK_Ready
```

Если Reject / Wait / timeout — остаёмся с прежним data role.

### 5.2 Responder

```
PE_*_Ready
   │ MessageReceived(DR_Swap)
   ▼
PE_DRS_*_Evaluate_Swap
   │ Decision: accept / reject (policy)
   ▼ Accept
PE_DRS_*_Accept_Swap
   │ tx Accept → I_TXSENT
   ▼
PE_DRS_*_Change_to_<new role>
   │ Type-C SM update
   ▼
back to PE_*_Ready
```

### 5.3 DR_Swap policy

- Accept если мы Dual-Role Data (`Dual-Role Data` bit в 5V PDO = 1; для нас всегда так).
- Reject если PPM запретил (через SET_UOR bit 2 = 0 — «не принимать swap requests»).
- Wait не используем.

---

## 6. Hard Reset orchestration

Hard Reset — это атомарный сброс PD-контракта. Используется при:
- Тайм-ауты на критичных PD-ответах.
- Retry на Soft_Reset не помог.
- PE policy decides recovery невозможна.
- Партнёр прислал Hard Reset.

### 6.1 Initiator side (мы инициируем)

```
Any state
   │ trigger_hard_reset
   ▼
PE_*_Hard_Reset
   │ inc HardResetCounter
   │ if HardResetCounter > nHardResetCount → PE_*_Disabled
   │ else:
   │   prl.send_hard_reset()
   ▼ I_HARDSENT
PE_*_Transition_to_default
   │ Source: VBUS off → vSafe0V → VBUS on → vSafe5V
   │ Sink: wait VBUS off, wait VBUS on
   │ Reset internal state (data role to default, etc.)
   ▼
PE_SRC_Startup или PE_SNK_Startup
```

### 6.2 Receiver side

```
Any state
   │ I_HARDRST (Hard Reset received)
   ▼
PE_*_Hard_Reset_Received
   │ (PRL уже сбросила counters)
   │ inc HardResetCounter — нет, спека: receive не инкрементирует counter; это initiator-метрика
   ▼
PE_*_Transition_to_default  (как в §6.1)
```

### 6.3 Hard Reset на стороне Source (детали Transition_to_default)

```
1. VBUS source switch OFF (external)
2. Wait vSafe0V (MEAS_VBUS + MDAC ≤ 800 mV) — tPSHardReset ≈ 30 мс
3. (Опционально) discharge resistor on
4. Reset data role to default (DRP source = DFP)
5. tPSHardResetReset (≥4500 us) — minimum delay
6. (Опционально) discharge resistor off
7. VBUS source switch ON
8. Wait vSafe5V (MDAC ≥ 4500 mV)
9. → PE_SRC_Startup
```

### 6.4 Hard Reset на стороне Sink

```
1. Sink path: disable (защита, чтобы не съесть transient)
2. Wait VBUS off (mark detach if не вернётся в SinkWaitCapTimer)
3. Wait VBUS on (Source через 5-7 секунд должен поднять обратно)
4. Sink path: enable
5. → PE_SNK_Startup (Wait_For_Capabilities)
```

### 6.5 Hard Reset stress

После `nHardResetCount = 2` неудачных Hard Reset → `PE_*_Disabled`:
- Запустить `NoResponseTimer` (tNoResponse, 4.5-5.5 s).
- В Disabled — PD не работает; Type-C соединение остаётся.
- После tNoResponse: partner признан unresponsive; если мы Source — оставляем VBUS включённым (legacy Type-C only, без PD); если Sink — продолжаем drink.
- Если в Disabled пришло сообщение от партнёра — это спорный случай; spec recommends recovery — мы переходим обратно в Startup.

### 6.6 HardResetCounter reset semantics

`HardResetCounter` обнуляется в следующих случаях:
- При **attach** (entry в `PE_SRC_Startup` или `PE_SNK_Startup` после Type-C `attached` event) — счётчик новой сессии.
- При **успешном Hard Reset recovery**: после `PE_*_Transition_to_default` мы заходим в `PE_*_Startup` → counter обнуляется (это специальный случай attach-like flow).
- При **достижении `PE_*_Ready`** через нормальную negotiation после Hard Reset — это **подтверждение, что Hard Reset решил проблему**. (Реально совпадает с предыдущим пунктом, т.к. Ready достигается через Startup.)
- При **`PPM_RESET`** через UCSI.

**Не** обнуляется при:
- Soft_Reset (он легче, не означает «решена проблема»).
- Detach (PE state сбрасывается целиком при detach, см. §3.13 в [`type-c-sm.md`](type-c-sm.md) / `PE_Detached` ниже §3.0).

---

## 7. Soft Reset orchestration

См. §2.9 / §3.9 (initiate) и §2.10 / §3.10 (responder).

Trigger-ы для Soft_Reset (initiate):
- SenderResponseTimer expired on small operations (Get_Sink_Cap response not received, etc.).
- Some protocol error без необходимости полного Hard Reset.

Если Soft_Reset тоже не помог (retry-fail или responder timeout) → Hard Reset.

---

## 8. Source_Capabilities construction

### 8.1 Структура

`Source_Capabilities` — Data message 0x01 с 1..7 PDO. Format:
- PDO #1 — **обязательно** Fixed 5V (с DRP flags); см. [`pd-scope.md`](pd-scope.md) §2.1.
- PDO #2..N — дополнительные Fixed, Battery, Variable, PPS APDO в порядке: Fixed (по voltage), Battery, Variable, AVS APDO, PPS APDO.
  - В v1 у нас только Fixed.

### 8.2 Наши PDOs (default по [`pd-scope.md`](pd-scope.md) §2.1)

| # | Type  | V     | I max | Power |
|:-:|:------|:-----:|:-----:|:-----:|
| 1 | Fixed | 5 V   | 3 A   | 15 W  |
| 2 | Fixed | 9 V   | 3 A   | 27 W  |
| 3 | Fixed | 15 V  | 3 A   | 45 W  |
| 4 | Fixed | 20 V  | 3 A   | 60 W  |

Биты PDO #1 — special (Dual-Role Power, USB Comms, USB Suspend и т.п. —
см. [`pd-scope.md`](pd-scope.md) §2.1).

### 8.3 Источник PDO

PE хранит массив до 7 PDO в RAM. Изначально — компилируемая default-таблица
(из конфига библиотеки). PPM может перезаписать через `SET_PDOS` (см.
[`commands.md`](commands.md) §2.28) — после этого новый набор используется
в следующем Source_Capabilities.

### 8.4 SET_POWER_LEVEL влияет на формирование Source_Caps

`SET_POWER_LEVEL` (см. [`commands.md`](commands.md) §2.19) задаёт нашу
**максимально negotiable** мощность. PE использует этот target при
формировании Source_Capabilities:

- PDO с power > target — **исключаются** из массива (не отправляются партнёру).
- PDO с voltage > target voltage — исключаются (если target задаёт voltage).
- PDO с current > target_current — могут быть обрезаны до target_current или исключены.

**Пример**: stored source PDOs = [5V/3A, 9V/3A, 15V/3A, 20V/3A].
`SET_POWER_LEVEL(USB PD Max Power = 27 W)` → отбрасываем 15V/3A (45W) и
20V/3A (60W); advertised массив = [5V/3A, 9V/3A].

Это **renegotiation** trigger: после изменения PDO set → `PE_SRC_Send_Capabilities`,
sink выберет PDO из нового набора.

Reset target при: `PPM_RESET`, `CONNECTOR_RESET`, power cycle, detach
(см. [`commands.md`](commands.md) §2.19).

### 8.5 Re-send strategy

- При `attach` → tFirstSourceCap (250 мс) → отправить.
- В `PE_SRC_Discovery`: если sink не ответил → SourceCapabilityTimer (150 мс) → retry, до CapsCounter = nCapsCount (50).
- При unsolicited renegotiation (PPM: source_caps_changed, set_power_level) → отправить из Ready.
- На приём `Get_Source_Cap` → отправить.

---

## 9. RDO evaluation (Source side)

При получении Request:

```c
// Параметры RDO
obj_pos = (rdo >> 28) & 0x7;     // 1..7
operating_current = (rdo >> 10) & 0x3FF;  // в 10 mA
max_operating_current = rdo & 0x3FF;       // в 10 mA
capability_mismatch = (rdo >> 26) & 1;
no_usb_suspend = (rdo >> 24) & 1;
usb_comm = (rdo >> 25) & 1;
```

Логика:
1. `obj_pos == 0` или > N (наших PDO) → Reject (invalid).
2. Выбрать наш PDO #obj_pos.
3. Если PDO_Type != Fixed (в v1) — Reject (мы только Fixed offer).
4. `pdo_max_current = (pdo & 0x3FF)` в 10 mA.
5. Если `operating_current > pdo_max_current` → Reject (нельзя дать больше).
6. Если `max_operating_current > pdo_max_current` и `capability_mismatch == 0` → Reject (sink требует больше, чем мы advertised, но не пометил mismatch).
7. Если `max_operating_current > pdo_max_current` и `capability_mismatch == 1` → Accept (но запомнить, что mismatch — PPM может проинформировать host).
8. Иначе → Accept.

Wait response — не используем в v1 (мы всегда готовы немедленно).

---

## 10. Sink_Capabilities construction

Format аналогичный Source_Capabilities, но 5V PDO — sink-вариант (Higher
Capability bit, no Dual-Role Power-side fields; см. [`pd-scope.md`](pd-scope.md) §2.2).

### 10.1 Наши sink PDO (default)

| # | Type  | V    | I max  | Notes |
|:-:|:------|:----:|:------:|:------|
| 1 | Fixed | 5 V  | 3 A    | Mandatory; Higher Capability=1, DRP=1 |
| 2 | Fixed | 9 V  | 3 A    | Optional |
| 3 | Fixed | 15 V | 3 A    | Optional |
| 4 | Fixed | 20 V | 3 A    | Optional |

Точные значения зависят от платформы; задаются через PPM API.

### 10.2 Когда отправляем

- В ответ на `Get_Sink_Cap` (любой role state).
- Не отправляем unsolicited.

---

## 11. Source_Caps evaluation (Sink side)

При получении Source_Capabilities:

1. Распарсить массив PDOs.
2. Для каждого PDO определить: подходит ли (тип Fixed, voltage из нашего allowed set)?
3. **Policy**: какой выбрать?
   - В v1: **«ближайший к target из PPM»** — то, что PPM указал через `SET_POWER_LEVEL` (поле `USB PD Max Power`). Алгоритм:
     1. Если target задан → ищем Fixed PDO с power == target. Если нет — ближайший меньший. Если ничего нет → 5V (минимум) с `capability_mismatch = 1`.
     2. Если target не задан (`SET_POWER_LEVEL` не вызывался или сброшен) → дефолтная policy: **«highest watts available»**.
4. Если ничего из advertised не подходит → запросить 5V с `capability_mismatch = 1` (sink заявляет: «мне нужно больше, но я возьму что есть»).

**Sink-side SET_POWER_LEVEL flow**:
- PPM зовёт `set_power_level(target)`.
- PE сохраняет target в state.
- Если в `PE_SNK_Ready` → отправить новый `Request` с RDO под новый target → `PE_SNK_Select_Capability`.
- Source отвечает Accept → `PE_SNK_Transition_Sink` → PS_RDY → `PE_SNK_Ready` с новым контрактом.
- Уведомить PPM: `negotiated_power_level_change`.

### 11.1 RDO construction

```c
rdo = (obj_pos << 28) |
      (0 << 27) |                // GiveBack = 0 (не делаем)
      (capability_mismatch << 26) |
      (usb_comm << 25) |          // зависит от устройства
      (no_usb_suspend << 24) |
      (unchunked_ext << 23) |
      (epr_capable << 22) |        // = 0 (нет EPR)
      (operating_current << 10) |   // в 10 mA
      (max_operating_current << 0); // в 10 mA
```

В большинстве случаев `operating_current == max_operating_current`. Для
ad-hoc batt-mgmt можно установить max выше чем operating чтобы зарезервировать
budget на пиковые нагрузки.

---

## 12. NoResponseTimer и Disabled

После `HardResetCounter > nHardResetCount`:

```
PE_*_Hard_Reset (overflow)
   ▼
PE_*_Disabled
   │ запустить NoResponseTimer (4.5-5.5 s)
   ▼ timer expired
PE_*_Unresponsive  
   │ partner признан unresponsive
   │ PD больше не пытаемся
   │ остаёмся в Disabled пока не Detach
```

При Detach (от Type-C SM) → выйти из Disabled, перейти в normal flow на
следующем attach.

---

## 13. PE ↔ PRL API

### 13.1 PE → PRL

См. [`prl-sm.md`](prl-sm.md) §11.1. Основное:
- `tx_request(msg)` — отправить.
- `cancel_tx`.
- `send_hard_reset`.
- `prl_reset(reason)`.
- `set_sink_tx_ok / set_sink_tx_pretend_ng` (для управления HOST_CUR во время AMS).

### 13.2 PRL → PE

См. [`prl-sm.md`](prl-sm.md) §11.2:
- `MessageReceived(msg)`.
- `MessageSent(header)`.
- `MessageFailed(reason)`.
- `HardResetReceived`.
- `HardResetSent`.

---

## 14. PE ↔ Type-C SM API

### 14.1 PE → Type-C

- `transition_pr_swap(new_power_role)` — после PR_Swap.
- `update_data_role(new_data_role)` — после DR_Swap.
- `request_hard_reset_recovery()` — Type-C SM перейдёт в ErrorRecovery (с VBUS bounce).
- `vbus_source_enable / disable` — управление внешним switch (можно делать самим PE через L4, но удобнее через Type-C — она знает state).
- `vbus_measure(threshold) → above/below` — для определения vSafe0V / vSafe5V.

### 14.2 Type-C → PE

- `attached(role, data_role, polarity, partner_type)` → trigger PE_SRC_Startup или PE_SNK_Startup.
- `detached` → PE → idle / cleanup.
- `vbus_at_safe0v` / `vbus_at_safe5v` → для Hard Reset / PR_Swap.

---

## 15. PE ↔ PPM (LPM API) summary

См. [`commands.md`](commands.md) и [`architecture.md`](architecture.md) §6.

PPM запрашивает у PE:
- get_connector_status → power_role / data_role / partner_type / RDO / Power Op Mode.
- get_pdos (own или partner; source или sink; current / max / advertised).
- get_cable_property — мы возвращаем «no cable info» (e-marker discovery не делаем).
- set_power_level / set_pdos / set_uor / set_pdr / set_sink_path — изменения policy.

PE публикует в PPM (через events):
- attach / detach / partner_change.
- power_op_mode_change.
- pd_reset_complete.
- negotiated_power_level_change.
- supported_provider_capabilities_change.
- battery_charging_status_change.
- error.

---

## 16. Таймеры PE (сводка)

| Таймер                  | Параметр              | Используется                                        |
|:------------------------|:----------------------|:----------------------------------------------------|
| FirstSourceCapTimer     | tFirstSourceCap (250 мс) | После attach как Source, перед первым Source_Caps |
| SourceCapabilityTimer   | tTypeCSendSourceCap (150 мс) | Re-send Source_Caps                            |
| SinkWaitCapTimer        | tTypeCSinkWaitCap (465 мс) | Sink ждёт первого Source_Caps                     |
| SenderResponseTimer     | tSenderResponse (300 мс)   | После TX, ждать ответ                              |
| PSTransitionTimer       | tPSTransition (500 мс)     | Power supply transition после Accept               |
| PSHardResetTimer        | tPSHardReset (30 мс)       | Source: пауза после Hard Reset перед VBUS-up       |
| PSSourceOffTimer        | tPSSourceOff (835 мс)      | PR_Swap: ждать VBUS off                            |
| PSSourceOnTimer         | tPSSourceOn (435 мс)       | PR_Swap: ждать VBUS on                             |
| SwapSourceStartTimer    | tSwapSourceStart (20 мс)   | После PR_Swap, перед Source_Caps                   |
| SinkRequestTimer        | tSinkRequest (100 мс min)  | Sink: min delay между Request-ами                  |
| NoResponseTimer         | tNoResponse (5 s)          | После HardResetCounter overflow                    |

Все — `FuriEventLoopTimer` типа `Once`.

---

## 17. Counters PE

См. [`pd-scope.md`](pd-scope.md) §7.

| Counter             | Max         | Reset on                                          |
|:--------------------|:-----------:|:--------------------------------------------------|
| HardResetCounter    | 2           | Successful attach, Hard Reset success, Soft Reset success |
| CapsCounter         | 50          | Attach, Hard Reset                                |

---

## 18. Что **не делаем** в PE v1

(дублирую из [`pd-scope.md`](pd-scope.md) §8 для удобства):

- VDM, alt-modes (вся группа `PE_*_VDM_*` states).
- EPR, AVS, PPS (нет `PE_*_EPR_*`).
- Fast Role Swap (нет `PE_FRS_*`).
- VCONN_Swap (отвечаем Reject).
- Data_Reset (отвечаем Not_Supported — это USB4-only).
- BIST (опционально, см. [`pd-scope.md`](pd-scope.md) §5).
- Country messages.
- Battery messages.
- Security / FW Update.
- Extended messages (любые — Not_Supported).
- Chunking (PRL chunking-receiver не делаем).

---

## 19. Хранимое состояние PE

```c
struct PEState {
    pe_substate     : enum,             // см. §2/§3/§4/§5
    
    // Контракт
    in_explicit_contract : bool,
    current_rdo     : Option<u32>,       // последний accepted RDO
    
    // Roles
    power_role      : { Source, Sink },
    data_role       : { DFP, UFP },
    
    // PDO data
    source_pdos     : [u32; 7],          // наши source caps (Fixed only в v1)
    source_pdo_count: u8,
    sink_pdos       : [u32; 7],          // наши sink caps
    sink_pdo_count  : u8,
    partner_source_pdos : Option<[u32; 7]>,  // кэш после Get_Source_Cap
    partner_source_pdo_count : u8,
    partner_sink_pdos : Option<[u32; 7]>,
    partner_sink_pdo_count : u8,
    
    // Counters (см. §17)
    hard_reset_counter  : u8,
    caps_counter        : u8,
    
    // Timers (см. §16) — FuriEventLoopTimer-ы
    timer_first_source_cap : FuriEventLoopTimer,
    timer_source_capability : FuriEventLoopTimer,
    timer_sink_wait_cap : FuriEventLoopTimer,
    timer_sender_response : FuriEventLoopTimer,
    timer_ps_transition : FuriEventLoopTimer,
    timer_ps_hard_reset : FuriEventLoopTimer,
    timer_ps_source_off : FuriEventLoopTimer,
    timer_ps_source_on : FuriEventLoopTimer,
    timer_swap_source_start : FuriEventLoopTimer,
    timer_sink_request : FuriEventLoopTimer,
    timer_no_response : FuriEventLoopTimer,
    
    // PPM-side requested ops (от UCSI команд)
    pending_pr_swap : Option<TargetRole>,
    pending_dr_swap : Option<TargetRole>,
    pending_get_partner_caps : Option<{ source_or_sink }>,
    pending_set_power_level : Option<u32>,

    // Policy flags (от PPM commands / caller)
    accept_pr_swap : bool,           // SET_PDR bit 2: принимать ли PR_Swap от partner
    accept_dr_swap : bool,           // SET_UOR bit 2: принимать ли DR_Swap от partner
    target_power_level : Option<PowerTarget>, // SET_POWER_LEVEL: max negotiable; влияет на Source_Caps и Sink Request
    sink_path_enabled : bool,        // SET_SINK_PATH; используется в Sink-side
    cc_operation_mode_lock : Option<CCMode>, // SET_CCOM: ограничение режима (Rp/Rd/DRP/Disabled)

    // Capability mismatch tracking
    last_request_had_mismatch : bool,  // помним если sink заявил capability mismatch — для PPM
}
```

**Источники policy флагов** (см. [`commands.md`](commands.md)):
- `accept_pr_swap`: устанавливается из `SET_PDR` (opcode 0x0B) bit 2. Default = true.
- `accept_dr_swap`: устанавливается из `SET_UOR` (opcode 0x09) bit 2. Default = true.
- `target_power_level`: из `SET_POWER_LEVEL` (opcode 0x14). Default = None (PPM determines).
- `sink_path_enabled`: из `SET_SINK_PATH` (opcode 0x1C). Default = true.
- `cc_operation_mode_lock`: из `SET_CCOM` (opcode 0x08). Default = None (DRP free).

**Сброс policy флагов**:
- `PPM_RESET` → все default.
- `CONNECTOR_RESET` → target_power_level cleared (per spec [`commands.md`](commands.md) §2.19); остальные default.
- `detach` → target_power_level cleared.
- power cycle → все default.

---

## 20. Открытые места

### 20.1 Optional Wait responses

В v1 мы Accept всё что в Ready и можем; никаких Wait. Если позже окажется,
что нам нужно «попросить sink/source подождать» (например, при пакетном
SET_PDOS) — добавим Wait логику. Сейчас — Accept или Reject.

### 20.2 Renegotiation flow

Когда PPM шлёт `SET_POWER_LEVEL` или `SET_PDOS` — мы делаем renegotiation
без disconnect. Detail:
- Source: → `PE_SRC_Send_Capabilities` (с новыми PDO/новым PDP).
- Sink: → отправить новый Request с другим target.

В этом сценарии sink уже получил предыдущий Source_Caps; новый PDO-set
может (теоретически) не пересекаться со старым. Тогда sink выберет
ближайшее. Если нужен **garantированный** disconnect-reconnect — Hard
Reset. В v1 — мягкая renegotiation, без Hard Reset.

### 20.3 Что делать если получили unsolicited Source_Capabilities в Ready (Sink-side)

Spec: must обрабатывать (Source может слать caps в любой момент). Мы:
- Reset SenderResponseTimer.
- Перейти в Evaluate_Capability.
- Отправить новый Request.

Это уже отражено в §3.6.

### 20.4 GoodCRC сам — это AMS или нет?

Per spec, GoodCRC сам по себе не считается AMS-частью. FUSB302
auto-GoodCRC — out of scope для PE. Мы не реагируем на него явно.

### 20.5 Дискуссия: на PD 2.0 партнёр vs PD 3.0 partner

PD 2.0 partner не понимает: Not_Supported (PD 3.0 message), Get_Status,
Get_PPS_Status, Get_Source_Cap_Extended, etc. Если мы шлём Not_Supported
PD 2.0-партнёру — он его не распознает и ответит Reject или ничего.

**Решение**: смотрим `peer_spec_rev`:
- PD 3.0 → шлём Not_Supported (нормально).
- PD 2.0 → шлём Reject вместо Not_Supported для control messages; ничего не шлём для data messages (просто игнор).

### 20.6 OPM-инициированные swaps (через PPM API)

Цепочка: UCSI `SET_UOR` (DR_Swap) → PPM → PE. PE из `Ready` → PRL.
Что если в момент SET_UOR мы уже в середине AMS (например, обрабатываем
Request от partner)? PE должна отложить swap до возврата в Ready.

Реализация: `pending_dr_swap` поле в state; пока не в Ready — не
переходим в DRS states.

### 20.7 Conflict: одновременные swap-requests

Что если мы шлём DR_Swap и одновременно получаем PR_Swap от partner?
Spec говорит: один из них должен быть приоритетным. Обычно — receiver-side
обработка имеет приоритет (то есть мы отвечаем на полученный PR_Swap, а
наш DR_Swap абортится).

Реализация: state-machine приоритеты — при получении message в Send_Swap-substate
— абортим наш swap, обрабатываем как responder.

### 20.8 PSSourceOff / PSSourceOn precision — РЕШЕНО

Эти таймеры важны для PR_Swap. tPSSourceOff = 750-920 ms — большое окно.
Если new source не подал VBUS за этот промежуток → стандартный recovery — Hard
Reset. Но без battery Hard Reset нас вырубит.

**Решение зафиксировано в §4.5.2**: используем флаг
`has_alternative_power_source()` от caller-а. Если есть alternative — Hard
Reset. Если нет — graceful revert (попытаться вернуться в прежнюю роль)
или Disabled с error notification.

---

## 21. Связь с другими документами

- [`commands.md`](commands.md) — UCSI commands, которые PPM зовёт у PE.
- [`pd-scope.md`](pd-scope.md) — что мы отвечаем какому message.
- [`prl-sm.md`](prl-sm.md) — детали TX/RX.
- [`type-c-sm.md`](type-c-sm.md) — что Type-C делает в PR_Swap.
- [`fusb302.md`](fusb302.md) — как через L4 PE дёргает FUSB302 для CONTROL3.SEND_HARD_RESET и т.п.
- [`architecture.md`](architecture.md) — слои.

---

## 22. Следующий шаг

После всех пяти plan-документов мы имеем полный концептуальный дизайн
библиотеки. Следующие документы (которые могут понадобиться по мере
имплементации):

- `api.md` — конкретные C signatures для всех слоёв (PPM, LPM-внутренние,
  Type-C, PRL, PE, FUSB302).
- `testing.md` — стратегия тестирования: что юнит-тестируется, что
  интеграционно, как делаем mocks для FUSB302.
- `logging.md` — формат логов, уровни, ключевые events для трассировки.

Но это уже implementation-фаза. Прежде чем идти туда — стоит
*валидировать* концептуальную модель: пройти по типичным PD-сценариям
(attach as source, attach as sink, PR_Swap, Hard Reset on protocol error,
renegotiation после SET_POWER_LEVEL) и убедиться, что все слои
работают согласованно. Это упражнение лучше делать на бумаге / в
обсуждении, до начала кода.
