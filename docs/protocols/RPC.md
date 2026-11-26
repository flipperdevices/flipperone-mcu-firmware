# RPC

The protobuf RPC that host tools use to stream the screen, inject input and drive a virtual display.

> [!NOTE]
> Not written yet. The sections follow [the contract template](README.md#writing-a-contract-document).

- **Counterpart:** host tools on a computer or on Linux.
- **Source of truth:** the messages in [`assets/proto`](../../assets/proto) (a submodule); the session in [`applications/services/rpc`](../../applications/services/rpc).

Planned contents:

- Starting and ending a session from the console
- Framing on the wire
- The messages: screen frames, input events, the virtual display
- Versioning of the `.proto` files
