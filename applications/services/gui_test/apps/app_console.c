
#include <gui_test/app_common.h>
#include <drivers/display/display_jd9853_reg.h>

#define TAG "ConsoleApp"

#define MULTI_LINE_STRING(...) #__VA_ARGS__

Clay_String boot_log = CLAY_STRING(MULTI_LINE_STRING(
-- Journal begins at Thu 2022-02-10 03:35:38 UTC, ends at Wed 2023-07-05 21:06:45 UTC. -- \n
Apr 30 17:24:08 ls-debian-11 kernel: Linux version 5.10.0-22-cloud-amd64 (debian-kernel@lists.debian.org) (gcc-10 (Debian 10.2.1-6) 10.2.1 20210110, GNU ld (GNU Binutils for Debian) 2.35.2) #1 SMP Debian 5.10.178-3 (2023-04-22) \n
Apr 30 17:24:08 ls-debian-11 kernel: Command line: BOOT_IMAGE=/boot/vmlinuz-5.10.0-22-cloud-amd64 root=UUID=c9ac07d3-8239-4f4c-b6f9-352f3d4f90d4 ro quiet elevator=noop console=tty console=ttyS0 net.ifnames=0 crashkernel=384M-:128M \n
Apr 30 17:24:08 ls-debian-11 kernel: x86/fpu: Supporting XSAVE feature 0x001: 'x87 floating point registers' \n
Apr 30 17:24:08 ls-debian-11 kernel: x86/fpu: Supporting XSAVE feature 0x002: 'SSE registers' \n
Apr 30 17:24:08 ls-debian-11 kernel: x86/fpu: Supporting XSAVE feature 0x004: 'AVX registers' \n
Apr 30 17:24:08 ls-debian-11 kernel: x86/fpu: xstate_offset[2]:  576, xstate_sizes[2]:  256 \n
Apr 30 17:24:08 ls-debian-11 kernel: x86/fpu: Enabled xstate features 0x7, context size is 832 bytes, using 'standard' format. \n
Apr 30 17:24:08 ls-debian-11 kernel: BIOS-provided physical RAM map: \n
Apr 30 17:24:08 ls-debian-11 kernel: BIOS-e820: [mem 0x0000000000000000-0x000000000009dfff] usable \n
Apr 30 17:24:08 ls-debian-11 kernel: BIOS-e820: [mem 0x000000000009e000-0x000000000009ffff] reserved \n
Apr 30 17:24:08 ls-debian-11 kernel: BIOS-e820: [mem 0x00000000000e0000-0x00000000000fffff] reserved \n
May 14 20:51:32 ls-debian-11 systemd[1]: Reached target Unmount All Filesystems. \n
May 14 20:51:32 ls-debian-11 systemd[1]: systemd-fsck@dev-disk-by\\x2duuid-d90e3189\\x2d12cd\\x2d4d16\\x2d8fe9\\x2dcf362b5f267d.service: Succeeded. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Stopped File System Check on /dev/disk/by-uuid/d90e3189-12cd-4d16-8fe9-cf362b5f267d. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Removed slice system-systemd\\x2dfsck.slice. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Stopped target Local File Systems (Pre). \n
May 14 20:51:32 ls-debian-11 systemd[1]: systemd-tmpfiles-setup-dev.service: Succeeded. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Stopped Create Static Device Nodes in /dev. \n
May 14 20:51:32 ls-debian-11 systemd[1]: systemd-sysusers.service: Succeeded. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Stopped Create System Users. \n
May 14 20:51:32 ls-debian-11 systemd[1]: systemd-remount-fs.service: Succeeded. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Stopped Remount Root and Kernel File Systems. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Reached target Shutdown. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Reached target Final Step. \n
May 14 20:51:32 ls-debian-11 systemd[1]: systemd-reboot.service: Succeeded. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Finished Reboot. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Reached target Reboot. \n
May 14 20:51:32 ls-debian-11 systemd[1]: Shutting down. \n
May 14 20:51:32 ls-debian-11 systemd-shutdown[1]: Syncing filesystems and block devices. \n
May 14 20:51:32 ls-debian-11 systemd-shutdown[1]: Sending SIGTERM to remaining processes... \n
May 14 20:51:32 ls-debian-11 systemd-journald[198]: Journal stopped \n
));

typedef struct {
    int32_t scroll_x_offset;
    int32_t scroll_y_offset;
    float touch_y_start;
    float touch_y_current;
    float touch_x_start;
    float touch_x_current;
} ConsoleAppState;

static Clay_Vector2 console_get_scroll_offset() {
    Clay_Vector2 scroll = Clay_GetScrollOffset();
    return scroll;
}

static void console_layout(App* app) {
    furi_assert(app);
    ConsoleAppState state = *(ConsoleAppState*)app->state;

    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    CLAY(
        CLAY_APP_ID(TAG "OuterContainer"),
        {
            .backgroundColor = COLOR_WHITE,
            .layout =
                {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = layoutExpand,
                    .padding = {4, 4, 4, 3},
                    .childGap = 4,
                },
        }) {
        CLAY(
            CLAY_APP_ID("HeaderBar"),
            {
                .layout =
                    {
                        .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                        .childGap = 8,
                        .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                    },
            }) {
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}, .backgroundColor = COLOR_BLACK, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(
                    CLAY_STRING("Boot Log"),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontButton,
                        .textColor = COLOR_WHITE,
                    }));
            }
        }
        CLAY(
            CLAY_APP_ID("LowerContent"),
            {
                .layout =
                    {
                        .sizing = layoutExpand,
                        .childGap = 4,
                    },
            }) {
            CLAY(
                CLAY_APP_ID("MainContent"),
                {
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                    .clip = {.vertical = true, .horizontal = true, .childOffset = console_get_scroll_offset()},
                    .layout =
                        {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .childGap = 8,
                            .padding = {6, 6, 6, 6},
                            .sizing = layoutExpand,
                        },
                }) {
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.width = CLAY_SIZING_GROW(0)},
                        },
                }) {
                    CLAY_TEXT(
                        boot_log,
                        CLAY_TEXT_CONFIG({
                            .fontId = FontBody,
                            .textColor = COLOR_BLACK,
                            .wrapMode = CLAY_TEXT_WRAP_NEWLINES,
                        }));
                }
            }
        }
    }
}

static bool console_input(App* app, const GuiTestMessage* message) {
    furi_assert(app);
    furi_assert(message);

    ConsoleAppState* state = (ConsoleAppState*)app->state;
    bool handled = false;

    switch(message->type) {
    case GuiTestMessageTypeInputEvent: {
        InputEvent event = message->input_event;
        if(event.key == InputKeyDown) {
            if(event.type == InputTypePress) {
                state->scroll_y_offset = -1;
                handled = true;
            }
            if(event.type == InputTypeRelease) {
                state->scroll_y_offset = 0;
                handled = true;
            }
        }
        if(event.key == InputKeyUp) {
            if(event.type == InputTypePress) {
                state->scroll_y_offset = 1;
                handled = true;
            }
            if(event.type == InputTypeRelease) {
                state->scroll_y_offset = 0;
                handled = true;
            }
        }
        if(event.key == InputKeyRight) {
            if(event.type == InputTypePress) {
                state->scroll_x_offset = -1;
                handled = true;
            }
            if(event.type == InputTypeRelease) {
                state->scroll_x_offset = 0;
                handled = true;
            }
        }
        if(event.key == InputKeyLeft) {
            if(event.type == InputTypePress) {
                state->scroll_x_offset = 1;
                handled = true;
            }
            if(event.type == InputTypeRelease) {
                state->scroll_x_offset = 0;
                handled = true;
            }
        }
    } break;
    case GuiTestMessageTypeInputTouchEvent: {
        switch(message->input_touch_event.type) {
        case InputTouchTypeStart:
            state->touch_x_start = message->input_touch_event.x;
            state->touch_x_current = state->touch_x_start;
            state->touch_y_start = message->input_touch_event.y;
            state->touch_y_current = state->touch_y_start;
            handled = true;
            break;
        case InputTouchTypeMove:
            state->touch_x_current = message->input_touch_event.x;
            state->touch_y_current = message->input_touch_event.y;
            handled = true;
            break;
        case InputTouchTypeEnd:
            state->touch_x_start = -1;
            state->touch_y_start = -1;
            state->touch_x_current = state->touch_x_start;
            state->touch_y_current = state->touch_y_start;
            handled = true;
            break;
        }
    } break;
    }

    return handled;
}

static void console_scroll(App* app) {
    furi_assert(app);
    ConsoleAppState* state = (ConsoleAppState*)app->state;
    Clay_Vector2 scroll = {1.0f * state->scroll_x_offset, 1.0f * state->scroll_y_offset};

    if(state->touch_y_start >= 0) {
        float delta = state->touch_y_current - state->touch_y_start;
        scroll.y -= delta * 0.05f;
        state->touch_y_start = state->touch_y_current;
    }

    if(state->touch_x_start >= 0) {
        float delta = state->touch_x_current - state->touch_x_start;
        scroll.x -= delta * 0.05f;
        state->touch_x_start = state->touch_x_current;
    }

    Clay_SetPointerState((Clay_Vector2){.x = JD9853_WIDTH / 2.0f, .y = JD9853_HEIGHT / 2.0f}, false);
    Clay_UpdateScrollContainers(false, scroll, 1 / 60.f);
}

App app_console = {
    .state =
        &(ConsoleAppState){
            .scroll_x_offset = 0,
            .scroll_y_offset = 0,
        },
    .input = console_input,
    .render = console_layout,
    .scroll = console_scroll,
};
