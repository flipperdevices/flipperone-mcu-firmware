---
applyTo: "applications/**/*.c,applications/**/*.h,lib/toolbox/**/*.c,lib/toolbox/**/*.h,lib/cli/**/*.c,lib/cli/**/*.h,lib/containers/**/*.c,lib/containers/**/*.h,lib/drivers/**/*.c,lib/drivers/**/*.h,lib/uart_pio/**/*.c,lib/uart_pio/**/*.h,lib/tusb/**/*.c,lib/tusb/**/*.h"
---

# C/C++ firmware review checks

`AGENTS.md` is the source of truth for layout, build and test commands; `TARGETS.md` covers board descriptors; `.clang-format` defines formatting. Refer to them instead of assuming a path or a name from this file. The rules below are the ones that recur most often and are meant to outlive any single file.

## Naming and structure

- Every function, including file-local helpers, carries its module prefix (`foo_start_timer`, not `start_timer`).
- The first parameter of a module function is the instance, always named `instance`; callback payloads are named `context`.
- Public types are opaque in the header (`typedef struct Foo Foo;`) and defined in the
`.c`. Implementation-only headers use an `_i.h` suffix.
- Callback typedefs are PascalCase ending in `Callback` or `Function`, and take `void* context` as their last parameter.
- Enums are anonymous `typedef enum` with module-prefixed PascalCase values and a `...Max` member kept last.
- Headers use `#pragma once`. Empty parameter lists are written `(void)`. Macros are UPPER_SNAKE with the module prefix and parenthesised values.

## Preconditions

- Public entry points open with `furi_check(...)`, file-local helpers with `furi_assert(...)`. `furi_check` always crashes; `furi_assert` compiles out unless `FURI_DEBUG` is set, so it must never guard anything that would corrupt state.
- Unreachable `default:` branches use `furi_crash("...")` rather than `break` or a silent return.

## Allocator

- The standard allocation functions are wrapped onto the RTOS heap. The wrapper never returns `NULL` — it crashes on exhaustion — returns zeroed memory, crashes on zero-size requests, and crashes when called from an interrupt context.
- Do not flag a missing `NULL` check after an allocation, and treat code that adds one as dead and misleading.
- Do not ask for explicit zeroing after an allocation; the memory is already zeroed.
- Flag any allocation reachable from an interrupt handler.
- `realloc` is a hand-rolled wrapper that copies the requested size out of a block of unknown old size, so growing a block over-reads. Flag new uses rather than fixing them.

## Ownership and lifecycle

- Services are permanent threads: the service entry point allocates, publishes its record and runs its event loop for the lifetime of the firmware. There is no stop or deinit path, so do not report missing service teardown as a leak.
- Apps follow an alloc / body / free triple — see the app template — and must release everything on every exit path.
- Drivers follow an `*_init` that returns `NULL` when the device does not answer, plus a matching `*_deinit`. `*_init` must undo partial initialisation, including pin direction, before returning `NULL`. A `NULL` from `*_init` is a probe failure, not an allocation failure.
- Records are created only after full initialisation, and open/close calls come in pairs.
- The single-exit idiom is `do { ... break; } while(false)`. There are no `goto` statements in first-party code — do not suggest one.

## Threading and interrupts

- Cross-thread calls marshal work to the owning thread: the caller queues a message and waits on the toolbox API lock, the owner unlocks it after handling. Flag service state touched without that handoff, or shared state guarded by an ad-hoc mutex.
- Interrupt handlers are marked as such and kept out of flash. They may write `volatile` fields and signal the owning thread through the event loop. Flag logging, allocation, blocking calls, bus transfers or direct event-loop invocation from an interrupt context.
- Shared register maps are accessed inside the project's critical-section helpers; flag accesses that skip them.
- Pubsub callbacks run on the publisher's thread and must forward to the owning thread instead of mutating service state directly.

## External input

- Data from bus frames, protobuf decodes, NVM, CLI arguments or intercom requests must be size-checked against an explicit maximum, type-checked, range-checked and fully consumed before use.
- Each distinct failure gets its own `FURI_LOG_E`/`FURI_LOG_W` message with the offending values. Files declare a `TAG` macro for that.

## Build

- Warnings are build errors. Do not suggest suppressing a new warning project-wide; the blanket suppressions in the top-level build file are a known workaround for vendored headers, not a pattern to extend.

## Vendored files inside this tree

- The GUI layout library and the on-device test framework header are third-party. Review their integration, not their style.
