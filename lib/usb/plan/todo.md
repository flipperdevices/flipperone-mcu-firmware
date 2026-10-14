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

## 1. Обязательные UCSI-команды, которых нет 🟢

В [`commands.md`](commands.md) помечены MUST, в `ucsi_ppm_cmd.c` падают в
`default` и отвечают Not Supported. Хост у нас — стандартный Linux-драйвер
`ucsi`, и проверка по его исходникам (2026-10-14) показала, что ни одну из
них он при нашем GET_CAPABILITY не шлёт, поэтому для v1 они не блокируют:

- [ ] **CANCEL (0x02)** — драйвер шлёт только в ответ на CCI Busy, которого у
      нас нет (все хендлеры синхронные). Понадобится вместе с асинхронным
      завершением swap-команд.
- [ ] **SET_SINK_PATH (0x1C)** — драйвер не шлёт никогда.
- [ ] **SET_PDOS (0x1D)** — драйвер не шлёт никогда; полезно только для
      отладки без перепрошивки.
- [ ] **GET_CABLE_PROPERTY (0x11)** — драйвер шлёт только при `Cable Details`
      в bmOptionalFeatures, у нас бит снят. Без SOP' ответ был бы статичным.

Если понадобится полная спековая комплектность (другой OPM, сертификация),
все четыре небольшие.

## 2. Policy Engine 🟡

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
- [ ] 🟢 **Back-off Hard Reset после сброса порта источником.** Если detach
      случился в SnkWaitForCaps без единого кадра от источника, на следующих
      аттачах в окне ~10 с не слать Hard Reset по SinkWaitCapTimer, а сразу
      жить по Rp (type-c only); сброс по контракту. Контекст: Ugreen с двумя
      занятыми портами держит наш порт на 5 В / Rp 1.5 А без PD и
      передёргивает его каждые 3 с (1.63 с VBUS, 1.39 с без), пока второй
      порт занят; наши Hard Reset на цикл не влияют (проверено анализатором,
      с ними и без них одинаково), так что это только чтобы не слать
      бессмысленные Hard Reset в источник, который и так в беде. Проблему
      не решает — её с нашей стороны не решает ничто.
- [ ] 🟢 **GET_PDOS партнёра** отдаёт только кэш sink-стороны.
      Sink_Capabilities партнёра, когда мы source, не кэшируются; поля Type и
      Range игнорируются (`ucsi_ppm_cmd.c`, «partner PDOs aren't tracked
      yet»). Linux-драйвер шлёт GET_PDOS только при `PDO Details` в
      bmOptionalFeatures, у нас бит снят, так что сейчас команду никто не
      вызывает.
- [ ] 🟢 **Завершение SET_PDR / SET_UOR / SET_POWER_LEVEL по результату.**
      UCSI 3.0 §6.5.9–6.5.10: «if the swap fails for any reason, the command
      returns an error». Мы завершаем команду в момент отправки (или
      парковки) AMS. Linux-драйвер ucsi на CCI команды не смотрит вовсе:
      `ucsi_pr_swap`/`ucsi_dr_swap` ждут Connector Change со сменой роли до
      5 с (`UCSI_SWAP_TIMEOUT_MS`) и на Reject партнёра возвращают -ETIMEDOUT.
      Итог для хоста верный, только медленный, поэтому не для v1. Честное
      решение — асинхронное завершение (Busy → Command Completed с Error),
      `cmd_state` под это заложен; тогда драйвер получит ошибку сразу из
      `ucsi_role_cmd`.
- [ ] Устаревший TODO в `ucsi_ppm.c` («power_supply_ready handling, CCI event
      delivery») — проверить, что оба давно работают, и убрать.

## 3. Type-C 🟡

- [ ] 🟢 **Try.SRC / Try.SNK** ([`type-c-sm.md`](type-c-sm.md) §10.1) — выбор
      предпочтительной роли при встрече с другим DRP (ноутбук, DRP-зарядка).
      Для карманного устройства логичнее Try.SNK. Прежний симптом «ложный SRC
      и дёрганье OTG без партнёра» к этому отношения не имел и закрыт:
      проверка BC_LVL при коммите SRC не пускает OTG, а TOG_RD_ONLY=1 убрал
      петлю на Ra пустого e-marked кабеля.
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
- VDM / Alt Mode / Discover Identity (структурные VDM — NAK; GET_CURRENT_CAM
  отвечает «ни в одном», настоящая реализация — вторая стадия).
- SOP'/SOP'' и VCONN (не источаем).
- PPS / APDO, EPR, FR_Swap, BIST.
- tSrcHoldsBus (§7.31.13.2) — необязательная оптимизация Rp.
