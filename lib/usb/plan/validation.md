# Validation: End-to-End сценарии

Документ проходит по типичным PD-сценариям с прослеживанием event-flow
через все слои (OPM → L1 transport → L2 PPM-core → L3 LPM → L4 FUSB302)
и фиксирует найденные пробелы / противоречия / открытые вопросы дизайна.

Это валидация **до** имплементации — на бумаге; чтобы зафиксировать
дыры в дизайне, когда их ещё дёшево закрыть.

Связанные документы (которые этот док валидирует):
- [`commands.md`](commands.md), [`architecture.md`](architecture.md),
- [`pd-scope.md`](pd-scope.md), [`fusb302.md`](fusb302.md),
- [`type-c-sm.md`](type-c-sm.md), [`prl-sm.md`](prl-sm.md), [`pe-sm.md`](pe-sm.md).

---

## 0. Методология

Для каждого сценария:
1. Описываю **последовательность шагов**: что и в каком порядке.
2. На каждом шаге фиксирую: **слой**, **event/action**, **регистр FUSB302 или state**.
3. В конце сценария — **🟡 Findings**: пробелы, ambiguity, конфликты с другими сценариями.

Все найденные findings собираются в §10 «Summary of findings» с указанием,
какой доку требует апдейта.

---

## 1. Cold start: PPM init без партнёра

### 1.1 Последовательность

1. **Caller** вызывает `lib_init()` → создаётся LPM (один порт), запускается FuriEventLoop.
2. **L2 PPM-core**: инициализирует регистровый файл — `VERSION = 0x0300`, `CCI = 0`, `MESSAGE_IN = 0`, `MESSAGE_OUT = 0`.
3. **L3 LPM**: запускает Type-C SM в `Disabled`.
4. **L4 FUSB302**:
   - `RESET.SW_RESET = 1` — полный hardware reset.
   - `POWER = 0x0F` — все домены.
   - Маски открыть только для `I_TOGDONE`.
   - `CONTROL0.INT_MASK = 0` — глобальная маска снята.
5. **Type-C SM**: получает «start» от LPM → переходит в `Toggling`:
   - `CONTROL2 = {TOGGLE=1, MODE=01b, TOG_SAVE_PWR=01b}`.
   - `CONTROL0.HOST_CUR = 11b` (3 А advertised, для source-side detection).
6. **L2 PPM-core**: ждёт первого запроса от OPM.

### 1.2 OPM поднимается и опрашивает PPM (нормальный init OPM-driver-а)

Типичный init Linux ucsi-driver:
1. Читает `VERSION` (offset 0..2). Видит `0x0300` → понимает что это UCSI 3.0.
2. Отправляет `PPM_RESET` (opcode 0x01).
   - L1: получает write в CONTROL → детектит opcode = 0x01.
   - L2: обрабатывает synchronously. SW-сторона:
     - `prl_reset(reason=ppm_reset)`.
     - Type-C SM: → `Disabled` → `Toggling` (повторная инициализация).
     - L4: ещё один SW_RESET? Или PD_RESET достаточно?
   - L2 ставит `CCI.Reset Completed = 1`, alert наружу.
3. Отправляет `SET_NOTIFICATION_ENABLE` с битмаской.
4. Отправляет `GET_CAPABILITY` (opcode 0x06).
   - L2: возвращает MESSAGE_IN с our PPM caps:
     - `bmAttributes` — DRP support, PD support, USB Type-C current support.
     - `bNumConnectors = 1`.
     - `bmOptionalFeatures` — наш bitmap (см. §1.6 commands.md).
     - `bNumAltModes = 0` (мы не делаем AM в v1).
     - `bcdPDVersion = 0x0300`.
5. Отправляет `GET_CONNECTOR_CAPABILITY` (для коннектора 1).
   - L2 запрашивает у L3: `lpm_get_connector_capability()`.
   - L3 возвращает: Operation Mode = DRP+USB2 (или какой у нас фактически), Provider+Consumer flags, Swap to DFP/UFP/SRC/SNK = все 1, etc.
6. Отправляет `GET_CONNECTOR_STATUS`.
   - L2 запрашивает у L3: `lpm_get_connector_status()`.
   - L3 возвращает: Connect Status = 0 (no partner attached), Power Operation Mode = USB Default, и т.п.

### 1.3 🟡 Findings

- **F1.1**: **`PPM_RESET` обязан резет L4 (FUSB302)?** В архитектуре написано «PPM_RESET сбрасывает себя и L3 (включая FUSB302 reset)» — значит SW_RESET. Но тогда мы теряем `CONTROL2.TOGGLE` и т.д. Должны заново сконфигурировать L4 после reset → это **тот же путь, что cold start**. ОК, но **нужно явно зафиксировать порядок**: PPM_RESET = SW_RESET → re-init full sequence.
- **F1.2**: **OPM может прислать команду до того, как Type-C settled.** Например, `GET_CONNECTOR_STATUS` в первые миллисекунды после `lib_init`. В этот момент TOGGLE крутится, partner не найден. Ответ должен быть «Connect Status = 0», без ошибок. **Это покрывается** в [`pe-sm.md`](pe-sm.md) — PE в no-contract идле просто отвечает «не подключено».
- **F1.3**: **`bNumAltModes` в `GET_CAPABILITY`** — мы заявили 0 в v1. Но в `bmOptionalFeatures` (см. [`commands.md`](commands.md) §1.6) bit 2 «Alternate mode details supported» — если выставлен, OPM может запросить AM-команды. **Решение**: bit 2 = 0 в v1 (всё, что отвечаем NS). Зафиксировать в имплементации.
- **F1.4**: **Кто пишет `VERSION` в регистровый файл — L2 или L1?** Архитектура говорит L2 («VERSION → L2 (один раз на старте)»). ОК.
- **F1.5**: **Race: OPM пишет CONTROL до того как `VERSION` валиден.** Если caller вызвал `lib_init()` но регистры ещё не заполнены, OPM прочитает 0 → не сможет работать. **Решение**: `lib_init()` синхронно заполняет VERSION перед возвратом; OPM не должен начинать общение до этого. Это invariant для caller.

---

## 2. Attach как Source

### 2.1 Последовательность

1. **L4 FUSB302**: TOGGLE крутится, partner подключается (Rd-Rd на CC1).
2. FUSB302 settles на CC1 как Source. `STATUS1A.TOGSS = 001`. Поднимает `I_TOGDONE`.
3. **L4 ISR → L3**: callback → publish `Fusb302EventToggleDone { togss=001 }`.
4. **Type-C SM** (state=`Toggling`): получает событие → переходит в `AttachWait.SRC`:
   - `CONTROL2.TOGGLE = 0` (stop toggling).
   - `SWITCHES0.MEAS_CC1 = 1`, `MEAS_CC2 = 0`.
   - `SWITCHES1.TX_CC1 = 1`, `TX_CC2 = 0`.
   - `SWITCHES0.PU_EN1 = 1`, `PU_EN2 = PDWN1 = PDWN2 = 0`.
   - `CONTROL0.HOST_CUR = 11b` (3 А — мы Source).
   - Маска: `I_COMP_CHNG = 0` (открыто).
   - `MEASURE.MDAC` — порог для Rd detect (~200 мВ).
   - Запускает `CCDebounceTimer` (150 мс).
5. **150 мс passes** без событий → `CCDebounceTimer expires` → проверяет `STATUS0.BC_LVL`.
6. Если BC_LVL ≠ 00 → Rd всё ещё там → переход в `Attached.SRC`:
   - **Type-C SM entry actions** (`Attached.SRC`):
     1. Включить VBUS (внешний switch GPIO).
     2. Подождать vSafe5V — через `MEASURE.MEAS_VBUS=1, MDAC=0x0C (~5040 мВ), STATUS0.COMP=1` → vSafe5V.
     3. `RESET.PD_RESET = 1`.
     4. `SWITCHES1 = {POWER_ROLE=1, DATA_ROLE=1, SPEC_REV=01b, AUTO_CRC=1}`.
     5. `CONTROL3 = {AUTO_RETRY=1, N_RETRIES=10b, AUTO_SOFTRESET=0, AUTO_HARDRESET=0}`.
     6. Маски: открыть `I_TXSENT, I_RETRYFAIL, I_HARDRST, I_HARDSENT, I_GCRCSENT, I_BC_LVL, I_COMP_CHNG, I_VBUSOK`.
   - **Type-C SM → PE**: event `attached(role=Source, data_role=DFP, polarity=CC1, partner_rp_or_rd=null)`.
7. **PE**: получает `attached`, состояние → `PE_SRC_Startup`:
   - HardResetCounter = 0, CapsCounter = 0.
   - Запускает `FirstSourceCapTimer` (tFirstSourceCap, 250 мс).
8. **250 мс passes** → `FirstSourceCapTimer expires` → переход в `PE_SRC_Send_Capabilities`:
   - Сконструировать Source_Capabilities (наши 4 PDO).
   - `set_sink_tx_pretend_ng` → `CONTROL0.HOST_CUR = 10b` (1.5 А — SinkTxNG).
   - `prl.tx_request(msg)`.
   - Запускает `SenderResponseTimer` (300 мс).
9. **PRL** (`PE_SRC_Send_Capabilities` → `PRL_Tx.CONSTRUCTING`):
   - Заполняет MessageID = 0 (tx_message_id_counter = 0 после reset).
   - Header: NDO=4, msg_type=0x01 (Source_Capabilities), PowerRole=1, DataRole=1, SpecRev=01b.
   - Buffer: `SYNC1 SYNC1 SYNC1 SYNC2 PACKSYM|(2+16) hdr_lo hdr_hi obj0..obj3 JAMCRC EOP TXOFF TXON`.
   - `fusb302_pd_tx_flush` (non-blocking).
   - Write FIFO.
   - → `WAITING_FOR_TX_RESULT`.
10. **L4 FUSB302**: TX BMC encoding, partner получает Source_Capabilities, отправляет GoodCRC.
11. **L4 FUSB302**: `I_TXSENT` → ISR → event `MessageTxOk`.
12. **PRL**: получает → inc `tx_message_id_counter` (= 1) → notify PE `MessageSent` → → `IDLE`.
13. **PE** (state=`PE_SRC_Send_Capabilities`): получает `MessageSent` → `set_sink_tx_ok` → `CONTROL0.HOST_CUR = 11b`. Ждёт partner Request.
14. **L4 FUSB302**: partner отправляет `Request` (с RDO выбора 20V/3A). FUSB302 принимает, auto-GoodCRC высылает (`I_GCRCSENT`).
15. **L4 ISR → L3**: event `MessageRx { sop=SOP, header, objects=[rdo] }` после `I_GCRCSENT`.
16. **PRL_Rx**: парсит header, MessageID = 0 (partner-side counter). `stored_rx_message_id = 0`. Forward → PE.
17. **PE**: получает RxMessage(Request) → отменяет SenderResponseTimer → переход в `PE_SRC_Negotiate_Capability`:
    - Парсит RDO: obj_pos=4 (20V), operating_current=300 (3A), max_current=300, mismatch=0.
    - Проверяет: PDO #4 = 20V/3A. operating ≤ max → **Accept**.
18. **PE** → `PE_SRC_Transition_Supply`:
    - Отправить `Accept` (control message 0x03).
    - PRL → CONSTRUCTING (MessageID=1) → write FIFO.
19. **L4**: TX → GoodCRC → `I_TXSENT`. PRL: inc counter (=2), notify PE.
20. **PE**: получает MessageSent → транзиция power. Зовёт callback `power_supply_set(20V, 3A)`. Запускает `PSTransitionTimer` (500 мс).
21. **Caller (внешняя инфра)**: переключает PSU на 20V/3A. После settled зовёт `power_supply_ready()`.
22. **PE**: получает `power_supply_ready` → отправить `PS_RDY` (control message 0x06):
    - PRL → CONSTRUCTING (MessageID=2) → write FIFO.
23. **L4**: TX → GoodCRC → `I_TXSENT`. PRL: inc (=3), notify PE.
24. **PE**: получает MessageSent → отменяет PSTransitionTimer → переход в `PE_SRC_Ready`.
25. **PE → PPM (L2)**: publish event `negotiated_power_level_change`, `power_op_mode_change` (=PD).
26. **PPM (L2)**: добавляет в очередь Connector Status Change bits (Power Op Mode Change + Negotiated Power Level Change).
27. **PPM**: на ближайшем `tick` поднимает alert наружу (с CCI.ConnectorChangeIndicator = 1).
28. **OPM**: получает alert → читает CCI → видит ConnectorChangeIndicator = 1 → отправляет `GET_CONNECTOR_STATUS`.
29. **L2 → L3 → PE**: возвращает status с Connect Status=1, Power Op Mode=PD, RDO=current, Connector Status Change bitmap.
30. **OPM**: отправляет `ACK_CC_CI` с ConnectorChangeAcknowledge=1.
31. **PPM**: clear pending notification, return CCI.AcknowledgeCommand=1.

### 2.2 🟡 Findings

- **F2.1**: **Кто отвечает за пересечение порога vSafe5V в Type-C SM?** В §2.5 type-c-sm.md написано «Подождать пока VBUS дойдёт до vSafe5V (мониторинг через MDAC)». Это **polling** или event-driven? **Решение**: периодический check на event-loop tick (раз в 5 мс), пока vbus_on_timer не expire. Альтернатива: использовать `I_COMP_CHNG` с настроенным MDAC порогом 4.5В — proper event-driven. **Лучше второе.** Зафиксировать в type-c-sm.md / fusb302.md.
- **F2.2**: **`power_supply_set` callback семантика.** В §2.5 pe-sm.md упомянут callback но не зафиксирован signature. Это вызов **снаружи** библиотеки (caller). Сигнатура должна быть: `power_supply_set(voltage_mv, current_ma) → bool started`, и потом `power_supply_ready()` event обратно. Должен быть в L3↔caller-side API. Это **деталь имплементации**, но **архитектурно влияет**: значит у нас есть ещё одна категория callbacks (power-supply), которой нет в [`architecture.md`](architecture.md) §1.
- **F2.3**: **HOST_CUR=10b (SinkTxNG) перед Source_Caps.** Type-C SM в `Attached.SRC` entry выставляет HOST_CUR=11b. Потом PE в `PE_SRC_Send_Capabilities` опускает в 10b и поднимает обратно после `I_TXSENT`. Это race: между Attached.SRC entry и Send_Capabilities entry — `HOST_CUR=11b` (SinkTxOk), и sink может пробовать инициировать AMS. **Решение**: либо start с 10b (SinkTxNG) в Attached.SRC entry, либо чётко договориться, что initial Source_Caps идёт быстро. **Лучше первое**: type-c-sm.md §2.5 entry должна установить `HOST_CUR=10b` и менять PE только после I_TXSENT на Source_Caps. Зафиксировать.
- **F2.4**: **`PD_RESET=1` после attach: что именно сбрасывается в FUSB302?** Согласно spec: «Reset just the PD logic for both the PD transmitter and receiver» — TX/RX FIFO, internal MessageID-counter в железе. Это правильно. Но мы тоже хотим сбросить software MessageIDCounter — это делается в `prl.prl_reset()`. **Решение**: в Type-C SM `Attached.SRC` entry вызвать **обе**: `fusb302_pd_reset_logic()` и `prl.prl_reset(reason=attach)`. Сейчас в type-c-sm.md шаг 3 только первый. **Добавить** второй.
- **F2.5**: **MessageID counter после first attach.** Spec говорит «после reset MessageIDCounter=0», и первое сообщение должно быть с MessageID=0. У нас в шаге 9 — MessageID=0. ОК. ✅
- **F2.6**: **PSTransitionTimer и PS_RDY**: PE в шаге 20 запускает PSTransitionTimer **на 500 мс** до того как мы зовём callback. Если PSU справляется быстрее (например, 100 мс), мы PS_RDY шлём через 100 мс — это OK. Если медленнее 500 мс → Hard Reset. **Findings**: не зафиксировано, что PSTransitionTimer применим только в этом контексте (не путать с tSenderResponse). ОК — отдельные таймеры. ✅
- **F2.7**: **Notification flow**: PE → PPM publish. Кто конкретно «publish»? Должен быть internal API между L3 и L2 — у нас в pe-sm.md §15 есть «PE публикует в PPM (через events)». Но архитектура не зафиксировала **очередь событий PE→PPM**. **Решение**: явный `FuriMessageQueue` или callback. Зафиксировать в architecture.md.
- **F2.8**: **Connector Status Change bitmap аккумуляция.** PE публикует «negotiated_power_level_change», PPM должен установить bit 6 в Connector Status Change. Но **что если в это время PPM в `Busy` или `WaitForAck`?** Согласно architecture.md §4.2 — bitmap копится в LPM до следующего GET_CONNECTOR_STATUS. ОК.
- **F2.9** ✅ **Закрыт.** Кто ставит SPEC_REV в SWITCHES1: начальное значение пишет `ucsi_ppm_phy_enable_pd` (из `prl_our_spec_rev`, то есть 10b), а при понижении его перезаписывает PRL из `prl_observe_spec_rev`. Заводское значение регистра — 01b, и пока его никто не трогал, наши GoodCRC заявляли PD 2.0, а сообщения PE — PD 3.0. Направление в §8 исправлено: храповик идёт вниз, а не вверх.

---

## 3. Attach как Sink

### 3.1 Последовательность

1. **L4**: TOGGLE → settles SNK on CC1 → `TOGSS=101`, `I_TOGDONE`.
2. **Type-C SM**: → `AttachWait.SNK`:
   - `CONTROL2.TOGGLE=0`.
   - `SWITCHES0.PDWN1=1, PDWN2=1, PU_EN1=PU_EN2=0`.
   - `MEAS_CC1=1`, `MEAS_CC2=0`.
   - Маска: `I_VBUSOK=0`, `I_BC_LVL=0`, `I_COMP_CHNG=0`.
   - CCDebounceTimer (150 мс).
3. **Partner подаёт VBUS** → `I_VBUSOK` → `STATUS0.VBUSOK=1`.
4. **Type-C SM**: получает `Fusb302EventVbusChanged { vbus_ok=1 }`, проверяет `STATUS0.BC_LVL` — Rp всё ещё там. Переход в `Attached.SNK`:
   - Sink path enable (если есть external — у нас может не быть).
   - `RESET.PD_RESET=1`.
   - `prl.prl_reset(reason=attach)`.
   - `SWITCHES1 = {POWER_ROLE=0, DATA_ROLE=0, SPEC_REV=01b, AUTO_CRC=1}`.
   - `CONTROL3 = {AUTO_RETRY=1, N_RETRIES=10b, AUTO_SOFTRESET=0, AUTO_HARDRESET=0}`.
   - Маски: как в Source.
   - → PE: event `attached(role=Sink, data_role=UFP, polarity=CC1, partner_rp=BC_LVL)`.
5. **PE**: → `PE_SNK_Startup`:
   - HardResetCounter = 0.
   - → `PE_SNK_Discovery`.
   - Запускает SinkWaitCapTimer (465 мс).
6. **L4**: partner отправляет Source_Capabilities → `I_GCRCSENT` → event `MessageRx`.
7. **PRL_Rx**: парсит header (MessageID=0, partner-side counter); `stored_rx_message_id=0`; SpecRev=10b → партнёр не старше нас, `our_spec_rev` остаётся 10b, `SWITCHES1.SPEC_REV` не трогаем. (Пришло бы 01b — понизились бы до него и переписали регистр.) Forward в PE.
8. **PE**: получает MessageRx(Source_Capabilities) → отменяет SinkWaitCapTimer → → `PE_SNK_Evaluate_Capability`:
   - Парсит массив PDOs.
   - Policy: выбрать 20V/3A (если в caps есть) или fall-back.
   - RDO = построен.
9. **PE** → `PE_SNK_Select_Capability`:
   - tx Request → PRL → FUSB302 TX.
   - SenderResponseTimer (300 мс).
10. **L4**: tx → GoodCRC → `I_TXSENT`. PRL: inc counter, notify PE.
11. **PE**: получает MessageSent. Ждёт Accept от source.
12. **L4**: partner отправляет Accept → `I_GCRCSENT` → MessageRx.
13. **PRL_Rx**: MessageID=1 (partner inc), stored=1, forward.
14. **PE**: получает Accept → отменяет SenderResponseTimer → `PE_SNK_Transition_Sink`:
    - Запустить PSTransitionTimer (500 мс).
    - Ждать PS_RDY.
15. **L4**: source поднимает VBUS до 20V, отправляет PS_RDY → MessageRx.
16. **PE**: получает PS_RDY → → `PE_SNK_Ready`:
    - publish в PPM: `negotiated_power_level_change`, `power_op_mode_change=PD`.
17. **PPM**: → CCI с Connector Change → alert → OPM читает GET_CONNECTOR_STATUS.

### 3.2 🟡 Findings

- **F3.1**: **`AttachWait.SNK` step 2 — выставлять PDWN до TOGGLE settle или после?** Type-C SM stop-ит TOGGLE и потом устанавливает PDWN — race conditions: после TOGGLE=0 хардвар pull-up/pull-down может быть в неопределённом состоянии 1-2 циклов. Spec говорит — после TOGGLE settle, FUSB302 уже **держит** правильные терминации (settled на Rd → PDWN active). **Решение**: при выходе из TOGGLE мы **не сбрасываем** SWITCHES0, а только дописываем MEAS_CC*. Уточнить в type-c-sm.md.
- **F3.2**: **`BC_LVL change` от Source во время Attached.SNK — что делать?** PE в Ready: «не важно при PD-контракте». Но spec PD 3.0 SinkTx использует BC_LVL для collision avoidance — PRL мониторит. Кто между PE и PRL получает событие? Согласно prl-sm.md §7 — PRL слушает BC_LVL для sink_tx_ok. **Решение**: L4 publish BcLvlChanged event; **слушают и PRL и Type-C SM** (Type-C — для GET_CONNECTOR_STATUS «Power Operation Mode» updates, PRL — для SinkTx). Это **broadcast**, не routing. Уточнить в architecture.md или в каждом из L3-компонентов.
- **F3.3**: **`SinkWaitCapTimer` на 465 мс может expire когда partner ещё не send-ит Source_Caps.** Например, partner — slow charger. Спека говорит: timeout → Hard Reset. Но это может быть **легитимно** — у некоторых старых charger-ов первый Source_Caps задерживается до 800 мс. **Решение**: тщательно тестировать с реальными devices; возможно увеличить timeout сверх spec. **Открытый вопрос** — fix позже на тестах.
- **F3.4**: **PD 3.0 partner advertise rev=10b в header, но first Source_Caps от него — `SpecRev=01b`?** Это типичный pattern: source advertised PD 3.0 в Type-C, но первое сообщение PD 2.0 header для compatibility. Наша логика в PRL §8 — стартуем с 2.0, апгрейд при первом 3.0 message. Если partner шлёт первое сообщение в 2.0, мы остаёмся в 2.0. Это **корректно** по spec §6.1.3.1. ✅

---

## 4. PR_Swap: SRC → SNK (мы initiator)

### 4.1 Последовательность

Стартовое состояние: `PE_SRC_Ready`, explicit contract на 20V/3A.

1. **OPM** отправляет `SET_PDR` (opcode 0x0B, Power Direction Role bit 1 set — initiate swap to Sink).
2. **L2 PPM-core**: парсит → отправляет в L3 (PE): `pe_internal: send_pr_swap_request(target=Sink)`.
3. **PE**: проверяет — мы в `PE_SRC_Ready`? Да → → `PE_PRS_SRC_SNK_Send_Swap`:
   - tx `PR_Swap` (control msg 0x0A) → PRL.
   - SenderResponseTimer (300 мс).
4. **L4**: TX → GoodCRC → `I_TXSENT`.
5. **PRL**: inc counter, notify PE.
6. **L4**: partner отвечает Accept → MessageRx → PE.
7. **PE**: получает Accept → отменяет SenderResponseTimer → → `PE_PRS_SRC_SNK_Transition_to_off`:
   - Запустить PSSourceOffTimer (835 мс).
   - Вызвать callback `power_supply_disable()` (отключить наш source).
8. **External PSU**: понижает VBUS до vSafe0V. Мы мониторим через MDAC.
9. **Type-C SM / PE**: vbus_at_safe0v event → → `PE_PRS_SRC_SNK_Assert_Rd`:
   - `SWITCHES0.PDWN1=1, PU_EN1=0` (полярность CC1).
   - Update internal `power_role = Sink` в Type-C SM.
   - `SWITCHES1.POWER_ROLE = 0` (Sink).
10. **PE** → `PE_PRS_SRC_SNK_Wait_Source_On`:
    - Запустить PSSourceOnTimer (435 мс).
    - Ждать PS_RDY от нового источника (бывшего sink-а, теперь source-а).
11. **L4**: partner поднимает VBUS, отправляет PS_RDY → MessageRx.
12. **PE**: получает PS_RDY → отменяет PSSourceOnTimer → → `PE_SNK_Startup`:
    - HardResetCounter=0.
    - → `PE_SNK_Discovery` → SinkWaitCapTimer.
13. **L4**: partner отправляет Source_Capabilities → flow как в §3.

Параллельно публикуется в PPM: `power_direction_change` event, `partner_change` (нет — partner всё тот же).

### 4.2 🟡 Findings

- **F4.1**: **Кто отслеживает vSafe0V — Type-C SM или PE?** В pe-sm.md §4.2 шаг `Transition_to_off` упоминает мониторинг через MDAC. В type-c-sm.md §5 — VBUS measurement подробно описан и упоминается «Type-C SM пользуется грубым мониторингом; точный — в PE». Это **разделение неоднозначно** — для PR_Swap нам нужен **точный** мониторинг vSafe0V. **Решение**: PE напрямую через L4 API запрашивает MDAC threshold check; Type-C SM не вмешивается в PR_Swap PSSourceOff/On timing. Зафиксировать в pe-sm.md §4.2 и type-c-sm.md §5.
- **F4.2**: **`PE_PRS_SRC_SNK_Assert_Rd`: кто меняет SWITCHES0?** В §4.2 type-c-sm.md шаг — Type-C SM зовётся через `transition_pr_swap(new_role=Sink)`. ОК. Но **в pe-sm.md §14.1** этот вызов есть, а в type-c-sm.md §11 написано «Type-C SM команды в L4» включая `set Rp/Rd/host-cur`. Кто реально пишет регистры в этом шаге? **Решение**: PE зовёт `type_c_sm.transition_pr_swap()`, Type-C SM сама пишет SWITCHES0 и SWITCHES1. Уточнить.
- **F4.3**: **HOST_CUR после PR_Swap.** Мы были Source с HOST_CUR=11b. После PR_Swap мы Sink — HOST_CUR не имеет значения (мы PDWN). Должны ли мы записать HOST_CUR=00? Spec: «при PDWN HOST_CUR ignored». Можно не трогать. ✅
- **F4.4**: **HardResetCounter сбрасывается в PE_SNK_Startup после PR_Swap.** Это правильно — мы фактически в новой сессии. ✅
- **F4.5**: **Поведение, если PR_Swap fails (Reject от partner или timeout).** В pe-sm.md §4.2 написано только happy path. Что делать на Reject?
  - Reject → отменяем swap, остаёмся в `PE_SRC_Ready`. Уведомить PPM: SET_PDR fail с Error Information `Port partner rejected swap`.
  - Wait → ждём, retry через SinkRequestTimer? Wait в контексте PR_Swap означает «попробуй позже».
  - SenderResponseTimer expired → `PE_SRC_Send_Soft_Reset`.
  - PSSourceOffTimer expired (VBUS не упал) → `PE_SRC_Hard_Reset`. **Findings**: это плохо — мы initial source, hard reset без battery нас вырубит. См. F4.7.
  - PSSourceOnTimer expired (new source не подал VBUS) → Hard Reset. **Тоже плохо** — мы уже sink, нет VBUS, нет питания. См. F4.7.
- **F4.6**: **PR_Swap notification semantics.** Spec говорит «successful power role swap should not result in connector status change notification» (см. [`commands.md`](commands.md) §2.10). Однако мы публикуем `power_direction_change` event в PPM. Это противоречие? **Решение**: spec имеет в виду «не нужно ждать ACK», нотификация всё равно идёт чтобы OPM знал о новой роли. Уточнить ввод в pe-sm.md §15.
- **F4.7**: **Hard Reset из PSSourceOff/On expired без battery — критично.** Pe-sm.md §20.8 уже подняло этот вопрос. **Решение**: добавить флаг в PE state `has_alternative_power_source` (от caller); если без него — на PSSourceOff/PSSourceOn expired переход в `PE_*_Disabled` (graceful), а не Hard Reset. Это deviation от spec, но spec spec практический.

---

## 5. PR_Swap responder

### 5.1 Последовательность

Стартовое состояние: `PE_SRC_Ready`, partner шлёт `PR_Swap`.

1. **L4**: MessageRx(PR_Swap) → PRL → PE.
2. **PE**: получает PR_Swap → → `PE_PRS_SRC_SNK_Evaluate_Swap`:
   - Decision: Accept (мы DRP, в Ready).
3. **PE** → tx Accept → PRL → `I_TXSENT` → `PE_PRS_SRC_SNK_Send_PS_RDY`. Подождите — в pe-sm.md §4.3 написано «Transition_to_off → Send_PS_RDY». То есть сначала off, потом PS_RDY:
   - `power_supply_disable()`.
   - Мониторинг vSafe0V → PSSourceOffTimer (835 мс).
4. **vSafe0V reached** → tx PS_RDY (control msg 0x06).
5. **PRL** → TX → `I_TXSENT`.
6. **PE** → `PE_PRS_SRC_SNK_Assert_Rd` (как в §4.1 шаг 9).
7. **PE** → `PE_PRS_SRC_SNK_Wait_Source_On`:
   - Ждать PS_RDY от нового source (partner).
8. **Partner** поднимает VBUS, отправляет PS_RDY → `PE_SNK_Startup`.

### 5.2 🟡 Findings

- **F5.1**: **PR_Swap evaluation policy.** Pe-sm.md §4.5: «Reject если мы не DRP» / «Wait если временно не можем». Не зафиксировано: **кто говорит «временно не можем»?** Это policy PPM (например, SET_PDR с bit 2 = 0 — «не accept-ить swap-ы»). **Решение**: PE проверяет PPM-флаг `accept_pr_swap` перед Accept. Если caller выставил «не принимать» — Reject. Зафиксировать в pe-sm.md §4.5.
- **F5.2**: **Симметрия со §4.2: те же тайминги, та же сложность с battery-less Hard Reset.** Применяется F4.7.

---

## 6. Hard Reset triggered by protocol error

### 6.1 Последовательность

Стартовое состояние: `PE_SRC_Ready`. Partner внезапно перестал отвечать.

1. **OPM** или PE-internal action: requires partner-response (например, periodic Get_Source_Cap-ping для health check — в v1 не делаем; либо OPM-issued GET_PDOS с Partner=1).
2. **PE**: tx Get_Sink_Cap, ждёт SenderResponse → expire → `PE_SRC_Send_Soft_Reset`.
3. **PE**: tx Soft_Reset → нет Accept → SenderResponse expire → `PE_SRC_Hard_Reset`.
4. **PE**: inc HardResetCounter (=1). Меньше nHardResetCount (2) → `prl.send_hard_reset()`.
5. **L4 PRL**: `CONTROL3.SEND_HARD_RESET = 1`.
6. **L4**: BMC Hard Reset pattern → `I_HARDSENT` → event.
7. **PE**: получает HardResetSent → `PE_SRC_Transition_to_default`:
   - `power_supply_disable()`.
   - Мониторинг VBUS до vSafe0V (PSHardResetTimer 30 мс).
   - Data role → DFP (default).
   - `power_supply_enable(5V)`.
   - Мониторинг VBUS до vSafe5V.
   - → `PE_SRC_Startup` → → `PE_SRC_Send_Capabilities`.
8. Если partner всё ещё не отвечает (second Hard Reset на следующей попытке):
   - HardResetCounter становится 2 → exceeds nHardResetCount → `PE_SRC_Disabled`.
   - Запустить NoResponseTimer (5 с).
9. После NoResponseTimer expire — partner unresponsive. Type-C SM остаётся в Attached.SRC (VBUS включён, legacy Type-C only).
10. Publish в PPM: `error` event, Error Information = `PPM Policy Conflict` (или другой подходящий).

### 6.2 🟡 Findings

- **F6.1**: **HardResetCounter сбрасывается когда?** Pe-sm.md §17: «Successful attach, Hard Reset success, Soft Reset success». «Hard Reset success» — что это значит? Если Hard Reset → восстановили contract → reset counter? **Решение**: HardResetCounter обнуляется когда мы достигаем `PE_*_Ready` после Hard Reset. Уточнить в pe-sm.md §17.
- **F6.2**: **`PE_SRC_Disabled` после Hard Reset overflow — кто пробуждает в Startup опять?** Pe-sm.md §12: «Если в Disabled пришло сообщение от партнёра — переходим обратно в Startup». Это значит мы должны открыть `I_GCRCSENT` и `I_HARDRST` маски в Disabled. ✅ implicitly.
- **F6.3**: **VBUS bounce в Transition_to_default** влияет на partner-side: партнёр (sink) увидит detach, отправит свой Hard Reset? Spec: partner-sink должен **сохранять connection** во время Hard Reset (VBUS off briefly, потом back). Если partner timing-чувствителен, мы должны успеть bring VBUS обратно за tSrcRecover. **Открытый вопрос**: точные тайминги нужно сверить с spec USB Type-C.
- **F6.4**: **Sink-side Hard Reset (`PE_SNK_Hard_Reset`).** Мы отправляем Hard Reset BMC, но потом нам **самим** нужно дождаться VBUS off → on от source. Source может **не** отреагировать (если плохой charger, например). Тогда мы Disabled, sink path enable, ждём VBUS вообще. Это deadlock — без VBUS мы не работаем. **Решение**: после Hard Reset Sink-side, если VBUS не вернулся за `tSrcRecoverPSTransition` (~1 секунды), детектим detach (через `I_VBUSOK`). Это уже покрывается Type-C SM `Attached.SNK` exit на VBUS gone. ✅

---

## 7. Renegotiation от SET_POWER_LEVEL

### 7.1 Последовательность (мы Source)

Стартовое состояние: `PE_SRC_Ready`, contract на 20V/3A.

1. **OPM** отправляет `SET_POWER_LEVEL` (opcode 0x14) с новым target (например, 9V/3A).
2. **L2 PPM**: → L3 PE: `pe_internal: set_power_level(target=9V/3A)`.
3. **PE**: проверяет — мы Source? Connected? Yes/yes → принимает.
   - Update internal target.
   - L2 → CCI с `Command Completed=1` (без ожидания контракта).
   - **Параллельно**: → `PE_SRC_Send_Capabilities` с тем же набором PDOs (потому что capabilities не изменились — изменился только target). Sink сам решит, какой PDO выбрать (обычно последний согласованный, но если PPM-target другой — sink не знает).
   - **Проблема**: SET_POWER_LEVEL это **наш** target, sink его не видит. Sink выбирает свой target из caps. Renegotiation на сервере **бессмысленна** без иной caps-структуры.
4. Альтернативная интерпретация: SET_POWER_LEVEL влияет на caps (например, ограничивает max current). Тогда мы **изменяем PDO** перед re-send:
   - Если target 9V/3A → caps стают [5V/3A, 9V/3A] (отбросив 15V и 20V).
   - Re-send Source_Capabilities.
   - Sink negotiate новый contract (наверняка 9V/3A).
5. **PPM** публикует `negotiated_power_level_change` после нового PS_RDY.

### 7.2 🟡 Findings

- **F7.1**: **Что SET_POWER_LEVEL делает на Source-side в реальности?** Read [`commands.md`](commands.md) §2.19: «set the maximum negotiable power level». Это **наш** floor — мы advertise caps не больше этого. **Решение**: SET_POWER_LEVEL обрезает наши PDO до target и re-send Source_Caps. Sink наверняка переключится. Уточнить в pe-sm.md §8 (Source_Capabilities construction) — учитывать SET_POWER_LEVEL target при формировании caps.
- **F7.2**: **Sink-side SET_POWER_LEVEL.** Мы Sink, хотим запросить меньше. Изменяем target → отправляем новый Request с нужным RDO → source Accept → транзиция. Это **proceeds without re-evaluation of Source_Caps** — просто новый Request. Зафиксировать в pe-sm.md §3.6 («pending set_power_level → re-send Request»).
- **F7.3**: **Кто учитывает SET_POWER_LEVEL при policy выбора PDO Sink-side?** Это policy в `PE_SNK_Evaluate_Capability` §3.3. **Решение**: добавить в evaluation step: «если pending_target есть, выбрать closest PDO к нему». Уточнить.

---

## 8. Detach

### 8.1 Последовательность (как Source)

1. **L4**: partner отключился → `STATUS0.BC_LVL` → 00 → `I_COMP_CHNG` → event.
2. **Type-C SM** (state=`Attached.SRC`): получает CompChanged → BC_LVL=00 → запускает `DetachDebounceTimer` (tCCDebounce 150 мс).
3. **150 мс** → BC_LVL ещё 00 → detach confirmed:
   - `power_supply_disable()`.
   - Discharge (опционально).
   - Уведомить PE: `detached`.
   - → `Toggling` (после tVbusOFF 650 мс на discharge — или сразу, если discharge done).
4. **PE**: получает detached → reset state → idle (no PE state for non-attached).
5. **PE** → PPM: `connect_change` event, `power_op_mode_change` (→ USB Default или какой fallback).
6. **PPM** → alert → OPM читает GET_CONNECTOR_STATUS → видит Connect Status=0.

### 8.2 🟡 Findings

- **F8.1**: **Type-C SM в detach из Attached.SRC — переход в Toggling или в Disabled?** Type-c-sm.md §2.5 говорит «→ Toggling». ОК.
- **F8.2**: **VBUS discharge time vs tVbusOFF.** Если у нас нет discharge resistor — VBUS падает медленно (зависит от capacitors). 650 мс может не хватить. **Решение**: либо ставить discharge externally, либо игнорировать (partner всё равно отключён, медленный discharge не критичен).
- **F8.3**: **PE state при detach.** В pe-sm.md нет явного `PE_Idle` или `PE_Detached` state. Сейчас implicit: PE «не в каком состоянии» когда Type-C SM не в Attached. **Решение**: добавить состояние `PE_Detached` или назвать как `PE_No_Connection`. Чтобы при следующем Attach явно был entry в Startup. Зафиксировать в pe-sm.md.
- **F8.4**: **Connector Status Change bit для Connect Change.** Bit 14 в Connector Status Change bitmap. Сбрасывается «когда OPM/PPM читает GET_CONNECTOR_STATUS Data Structure» (см. [`commands.md`](commands.md) §2.17 Table 6-44). ОК.

---

## 9. CONNECTOR_RESET от OPM (Hard)

### 9.1 Последовательность

Стартовое состояние: `Attached.SNK`, `PE_SNK_Ready`.

1. **OPM** отправляет `CONNECTOR_RESET` (opcode 0x03, Reset Type=0 Hard).
2. **L2 PPM**: → L3: `lpm_connector_reset(type=hard)`.
3. **PE**: → `PE_SNK_Hard_Reset` (initiator path).
4. **PE** → `prl.send_hard_reset()`.
5. **L4**: `CONTROL3.SEND_HARD_RESET=1` → BMC pattern → `I_HARDSENT`.
6. **PE**: получает HardResetSent → → `PE_SNK_Transition_to_default`.
7. Параллельно: L2 PPM уже **отправил** Command Completion как только начал процесс (см. [`commands.md`](commands.md) §2.3: «PPM shall send a command completion once it starts the Reset process»).
8. После Transition_to_default → `PE_SNK_Startup` → contract re-establish.
9. PE → PPM: Asynchronous notification (`pd_reset_complete` event).
10. PPM → CCI с Connector Change → alert → OPM read.

### 9.2 🟡 Findings

- **F9.1**: **CONNECTOR_RESET (Hard) на Sink-side без battery.** Если мы единственный source питания (через VBUS) и делаем Hard Reset, VBUS дропается, мы умираем. Spec говорит fail with `Error Information bit 5 (Dead Battery)`. **Решение**: добавить policy-check перед Hard Reset как Sink: если нет альтернативного источника питания, fail команды CONNECTOR_RESET с ошибкой. Это уже в [`commands.md`](commands.md) §2.3 как «Если USB Type-C charger подключён и нет другого power source → fail». Зафиксировать в pe-sm.md.
- **F9.2**: **Command Completion отправляется до или после Hard Reset complete?** Spec §2.3 «once it starts». То есть сразу. OK — L2 PPM сразу шлёт CCI.Command Completed=1. Затем по завершению — отдельный async notification (`pd_reset_complete` bit в Connector Status Change). ✅
- **F9.3**: **CONNECTOR_RESET (Data) — мы NS-им.** ОК (см. [`commands.md`](commands.md) §2.3 — Not Supported Indicator=1).

---

## 10. Summary of findings

Findings собраны в порядке: документ-для-апдейта → finding.

### 10.1 [`architecture.md`](architecture.md)

- **F2.2** — категория callbacks «power-supply» (set_voltage, ready_signal) добавить в §1 «Внешние зависимости».
- **F2.7** — Зафиксировать механизм PE→PPM event-publishing (FuriMessageQueue или callback).
- **F3.2** — Уточнить broadcast events (BcLvlChanged идёт и в PRL, и в Type-C SM).
- **F4.1** — Чёткое разделение: PE может **напрямую** обращаться к L4 (через abstraction) для voltage measurement; не обязательно через Type-C SM.

### 10.2 [`commands.md`](commands.md)

- Никаких изменений напрямую — но §2.3 уже корректно описывает dead-battery fail; нужно только при имплементации не забыть.

### 10.3 [`pd-scope.md`](pd-scope.md)

- Возможно добавить пункт про tSrcRecover (для VBUS bounce timing в Hard Reset) — это в Type-C spec, не PD. Опциональный апдейт.

### 10.4 [`fusb302.md`](fusb302.md)

- **F2.1** — Использовать `I_COMP_CHNG` с MDAC порогом для vSafe5V detect вместо polling. Уточнить.
- **F4.1** — Добавить «PE может запрашивать MDAC threshold check напрямую» в §9.1.

### 10.5 [`type-c-sm.md`](type-c-sm.md)

- **F2.1** — Заменить polling vSafe5V на event-driven через I_COMP_CHNG в §2.5.
- **F2.3** — В `Attached.SRC` entry установить `HOST_CUR=10b` (SinkTxNG), не 11b. PE поднимет в 11b после I_TXSENT первого Source_Caps.
- **F2.4** — В `Attached.SRC` entry добавить `prl.prl_reset()` после `PD_RESET=1`.
- **F3.1** — Уточнить: при выходе из TOGGLE не сбрасываем PDWN/PU_EN, только добавляем MEAS_CC*.

### 10.6 [`prl-sm.md`](prl-sm.md)

- **F2.9** — Зафиксировать: PRL записывает `SWITCHES1.SPEC_REV` при изменении our_spec_rev (§8).

### 10.7 [`pe-sm.md`](pe-sm.md)

- **F4.5** — Зафиксировать handling Reject/Wait/timeout в PR_Swap (initiator path); явно расписать failure transitions.
- **F4.6** — Уточнить notification semantics для PR_Swap (publish event но не expectation на ACK).
- **F4.7 / F9.1** — Добавить policy-флаг `has_alternative_power_source`; на critical hard reset checks (PSSourceOff/On expired, CONNECTOR_RESET as sink) проверять перед Hard Reset.
- **F5.1** — Добавить policy-флаг `accept_pr_swap` (из SET_PDR bit 2); проверять перед Accept на PR_Swap.
- **F6.1** — Уточнить когда обнуляется HardResetCounter (на `PE_*_Ready` после Hard Reset path).
- **F7.1** — SET_POWER_LEVEL влияет на формирование Source_Caps; учитывать в §8.
- **F7.3** — Sink-side SET_POWER_LEVEL влияет на PDO selection policy в §11.
- **F8.3** — Добавить явное `PE_Detached` (или `PE_No_Connection`) state для post-detach idle.

### 10.8 Новые открытые вопросы

- **F3.3** — Реальные тайминги SinkWaitCapTimer (некоторые chargers медленнее spec). Откладывается на тестирование.
- **F6.3** — VBUS bounce timing в Hard Reset (tSrcRecover) — сверить с USB Type-C spec.
- **F8.2** — Стратегия discharge (external vs none) — design decision.

---

## 11. Outcome

Дизайн **в целом согласован**, но 20+ findings показывают, что без
прохождения сценариев на бумаге часть деталей упустилась бы. Большая
часть findings — это **уточнения** (где-то записать «PE делает X, не
Type-C SM», где-то «callback signature такая»), а не структурные
проблемы. Структурно дизайн выдержал.

Самые **существенные** изменения нужны в pe-sm.md (failure paths для PR_Swap,
dead-battery policy, явный Detached state) и в type-c-sm.md (event-driven
VBUS, HOST_CUR=10b в Attached.SRC).

Рекомендуемый следующий шаг — **внести finding-фиксы в соответствующие
документы** (это уже не «писать с нуля», а инкрементальный апдейт), потом
переходить к `api.md` (концептуально → конкретные C-сигнатуры).
