#include "rpc_i.h"
#include <input/input.h>
#include <input_touch/input_touch.h>

#define TAG "RpcInput"

/* Map protobuf Button → system InputKey.
 * Proto numbering: OK=0, BACK=1, KEY_1=2, KEY_2=3, POWER=4,
 *                  KEY_4=5, KEY_5=6, SW=7, DOWN=8, RIGHT=9,
 *                  LEFT=10, UP=11, PTT=12
 */
static const InputKey proto_button_to_inputkey[] = {
    [Flipper_One_Input_Button_OK] = InputKeyOk,
    [Flipper_One_Input_Button_BACK] = InputKeyBack,
    [Flipper_One_Input_Button_KEY_1] = InputKey1,
    [Flipper_One_Input_Button_KEY_2] = InputKey2,
    [Flipper_One_Input_Button_POWER] = InputKey3,
    [Flipper_One_Input_Button_KEY_4] = InputKey4,
    [Flipper_One_Input_Button_KEY_5] = InputKey5,
    [Flipper_One_Input_Button_SW] = InputKeySw,
    [Flipper_One_Input_Button_DOWN] = InputKeyDown,
    [Flipper_One_Input_Button_RIGHT] = InputKeyRight,
    [Flipper_One_Input_Button_LEFT] = InputKeyLeft,
    [Flipper_One_Input_Button_UP] = InputKeyUp,
    [Flipper_One_Input_Button_PTT] = InputKeyPtt,
};

/* Map protobuf ButtonAction → system InputType */
static const InputType proto_action_to_inputtype[] = {
    [Flipper_One_Input_ButtonAction_PRESS] = InputTypePress,
    [Flipper_One_Input_ButtonAction_RELEASE] = InputTypeRelease,
};

void rpc_input_handler_callback(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(context);
    furi_assert(message);
    furi_assert(message->which_content == Flipper_One_Rpc_RpcMessage_button_event_tag);

    const Flipper_One_Input_ButtonEvent* evt = &message->content.button_event;

    if(evt->button >= COUNT_OF(proto_button_to_inputkey)) {
        FURI_LOG_E(TAG, "Unknown button %d", evt->button);
        return;
    }
    if(evt->action >= COUNT_OF(proto_action_to_inputtype)) {
        FURI_LOG_E(TAG, "Unknown action %d", evt->action);
        return;
    }

    InputEvent event = {
        .key = proto_button_to_inputkey[evt->button],
        .type = proto_action_to_inputtype[evt->action],
        .sequence = 0,
    };
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Injecting %s %s", input_get_key_name(event.key), input_get_type_name(event.type));
#endif

    FuriPubSub* input_events = furi_record_open(RECORD_INPUT_EVENTS);
    furi_pubsub_publish(input_events, &event);
    furi_record_close(RECORD_INPUT_EVENTS);
}

/* Map protobuf TouchType → system InputTouchType */
static const InputTouchType proto_touch_to_inputtype[] = {
    [Flipper_One_Input_TouchType_START] = InputTouchTypeStart,
    [Flipper_One_Input_TouchType_MOVE] = InputTouchTypeMove,
    [Flipper_One_Input_TouchType_END] = InputTouchTypeEnd,
};

void rpc_touch_handler_callback(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(context);
    furi_assert(message);
    furi_assert(message->which_content == Flipper_One_Rpc_RpcMessage_touch_event_tag);

    const Flipper_One_Input_TouchEvent* evt = &message->content.touch_event;

    if(evt->type >= COUNT_OF(proto_touch_to_inputtype)) {
        FURI_LOG_E(TAG, "Unknown touch type %d", evt->type);
        return;
    }

    InputTouchEvent event = {
        .type = proto_touch_to_inputtype[evt->type],
        .x = evt->x,
        .y = evt->y,
        .pressure = evt->pressure,
    };
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Injecting touch type=%d x=%ld y=%ld p=%ld", event.type, event.x, event.y, event.pressure);
#endif
    FuriPubSub* touch_events = furi_record_open(RECORD_INPUT_TOUCH_EVENTS);
    furi_pubsub_publish(touch_events, &event);
    furi_record_close(RECORD_INPUT_TOUCH_EVENTS);
}
