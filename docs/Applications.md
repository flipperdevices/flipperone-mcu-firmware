# Applications

> [!NOTE]
> Not written yet. Until then, start from
> [`applications/template/app.c`](../applications/template/app.c) and the registry in
> [`applications/applications.c`](../applications/applications.c).

Planned contents:

- Services, apps and CLI commands: what each is for and how each is registered
- The app lifecycle: `app_alloc`, `app_body`, `app_free`
- Records, event loops and message queues between services
- Memory rules: allocation never returns `NULL`, nothing from an ISR
- Adding a CLI command
