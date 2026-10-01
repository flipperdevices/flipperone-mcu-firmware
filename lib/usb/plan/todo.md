# TODO до релиза UCSI v1

Что осталось доделать, чтобы связку `lib/usb` (UcsiPpm) + `lib/usb_v2` (UsbPd) +
`applications/services/pd` + UCSI-окно в `i2c_negotiator` можно было назвать
первой версией. Собрано 2026-10-01 по коду, планам и TODO-меткам; по мере
закрытия пунктов — удалять их отсюда, а не помечать выполненными.

Что уже проверено с реальным хостом: Linux `ucsi`-драйвер через окно 0x0500
в `i2c_intercom` поднимается, init-последовательность проходит, role swap
работает.

Легенда: 🔴 блокирует релиз, 🟡 нужно для честного v1, 🟢 можно после.

---

## 1. Обязательные UCSI-команды, которых нет 🔴

В [`commands.md`](commands.md) помечены MUST, в `ucsi_ppm_cmd.c` падают в
`default` и отвечают Not Supported:

- [ ] **CONNECTOR_RESET (0x03)** — Hard Reset / Error Recovery по команде хоста.
      Вместе с ним dead-battery политика ([`type-c-sm.md`](type-c-sm.md) §10.4,
      `validation.md` F4.7/F9.1): если мы sink без альтернативного питания —
      fail с Error Information bit 5. Хук `has_alt_power` в конфиге есть, но
      кроме чтения в `ucsi_ppm.c` нигде не используется.
- [ ] **CANCEL (0x02)** — хендлеры синхронные, отменять нечего, но ответ по
      спеке нужен вместо Not Supported.
- [ ] **SET_SINK_PATH (0x1C)**.
- [ ] **SET_PDOS (0x1D)** — хост сможет менять PDO-листы без прошивки.
- [ ] **GET_CABLE_PROPERTY (0x11)** — без SOP' отдаём статичный ответ.

## 2. Policy Engine 🟡

- [ ] **PR_Swap source→sink** — ни как инициатор, ни как респондер: на
      входящий PR_Swap отвечаем Reject (`ucsi_ppm_pe.c`, «PR_Swap / VCONN_Swap
      aren't implemented in v1»). Нужен для сценария «зарядили партнёра,
      теперь он заряжает нас» и для хоста, который просит питание. При
      реализации — внести в `ucsi_ppm_pe_src_ams_in_progress()` и пропустить
      через `pe_ams_start_allowed` (правило в [`prl-sm.md`](prl-sm.md) §7.2).
- [ ] **Request от Sink в PE_SRC_Ready** — ренеготиация по инициативе синка не
      обрабатывается, отвечаем Not_Supported. `pe_src_handle_request` вызывается
      только из SrcSendCapabilities.
- [ ] **Source-side SET_POWER_LEVEL** — сейчас отвергается с Invalid Params
      («PE-5»). Нужна переотправка Source_Capabilities из SrcReady; это наша
      source-AMS — через гейт и `src_ams_in_progress`.
- [ ] **Source_Capabilities во время нашей DR_Swap / PR_Swap** — уходят в
      Not_Supported. Правильно — отменить свою AMS и пойти в Evaluate.
      Редко, но при коллизии с повтором Source_Caps воспроизводимо.
- [ ] **Wait на Request** — PE_SNK_Select_Capability схлопывает Reject и Wait в
      Hard Reset. По спеке Wait → SinkRequestTimer (100 мс) и повтор.
- [ ] **GET_PDOS партнёра** отдаёт только кэш sink-стороны. Sink_Capabilities
      партнёра, когда мы source, не кэшируются; поля Type и Range игнорируются
      (`ucsi_ppm_cmd.c`, «partner PDOs aren't tracked yet»).
- [ ] **Завершение SET_PDR / SET_UOR / SET_POWER_LEVEL по результату.** UCSI
      3.0 §6.5.9–6.5.10: «if the swap fails for any reason, the command returns
      an error». Мы завершаем команду в момент отправки (или парковки) AMS;
      Reject партнёра и таймауты хост не видит. Сброс отложенной AMS уже
      поднимает CSC Error — это костыль, честное решение — асинхронное
      завершение (Busy → Command Completed), `cmd_state` под это заложен.
- [ ] Устаревший TODO в `ucsi_ppm.c` («power_supply_ready handling, CCI event
      delivery») — проверить, что оба давно работают, и убрать.

## 3. Type-C 🟡

- [ ] **Try.SRC** ([`type-c-sm.md`](type-c-sm.md) §10.1). Без него в DRP без
      партнёра чип раз в ~500 мс ложно садится в SRC (Ra / шум) и дёргает OTG
      вкл/выкл. Наблюдалось на bring-up, видно пользователю на устройстве.
- [ ] **VBUS discharge** — хук `gpio_write_vbus_discharge` объявлен в конфиге,
      в коде не используется. Стратегия — открытый вопрос `validation.md`
      F8.2. Либо реализовать, либо убрать из конфига.
- [ ] **Debug Accessory** принимается как обычный sink (§10.3). Осознанно, но
      проверить на реальном debug-кабеле хотя бы раз.

## 4. Интеграция и репозиторий 🟡

- [ ] **Merge `dev`** — ветка отстаёт на 22 коммита (на 2026-10-01). Ожидаемые
      конфликты: `lib/drivers/fusb302/fusb302.c` (правка e608d20 на dev, у нас
      файл удалён — оставлять удаление) и protobuf (211300a). Непотраченные
      `assets/proto/` и `lib/nanopb/` в рабочем дереве — оттуда же.
- [ ] **Идентичность в SCEDB** — VID/PID/версии захардкожены в `ucsi_ppm_pe.c`
      (`SCEDB_VID` и далее). Перенести в `UcsiPpmConfig`, когда появится
      второе место использования (SKEDB), или сразу.

## 5. Документация планов 🟢

- [ ] [`pd-scope.md`](pd-scope.md) таблицы сообщений устарели относительно
      кода: Get_Source_Cap_Extended, Get_Status, Source_Capabilities_Extended,
      Status помечены NS, но реализованы; Vendor_Defined помечен NS, а
      структурные VDM получают NAK.
- [ ] `validation.md` §10 — пройтись по findings и отметить закрытые
      (F2.3 отозван, F3.2 закрыт, F2.9 снят вместе с SPEC_REV в SWITCHES1).
- [ ] `fusb302.md` §9.2 — события и retry на коллизии теперь реализованы,
      сверить формулировки.

## 6. Что сознательно не делаем в v1

Чтобы не возвращаться к этому в обсуждениях:

- Chunking extended-сообщений (нечего слать длиннее 26 байт).
- VDM / Alt Mode / Discover Identity (структурные VDM — NAK).
- SOP'/SOP'' и VCONN (не источаем).
- PPS / APDO, EPR, FR_Swap, BIST.
- tSrcHoldsBus (§7.31.13.2) — необязательная оптимизация Rp.
