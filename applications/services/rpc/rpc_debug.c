#include "rpc_i.h"

static const char* rpc_debug_button_name(Flipper_One_Input_Button btn) {
    switch(btn) {
    case Flipper_One_Input_Button_OK:
        return "OK";
    case Flipper_One_Input_Button_BACK:
        return "BACK";
    case Flipper_One_Input_Button_KEY_1:
        return "KEY_1";
    case Flipper_One_Input_Button_KEY_2:
        return "KEY_2";
    case Flipper_One_Input_Button_POWER:
        return "POWER";
    case Flipper_One_Input_Button_KEY_4:
        return "KEY_4";
    case Flipper_One_Input_Button_KEY_5:
        return "KEY_5";
    case Flipper_One_Input_Button_SW:
        return "SW";
    case Flipper_One_Input_Button_DOWN:
        return "DOWN";
    case Flipper_One_Input_Button_RIGHT:
        return "RIGHT";
    case Flipper_One_Input_Button_LEFT:
        return "LEFT";
    case Flipper_One_Input_Button_UP:
        return "UP";
    case Flipper_One_Input_Button_PTT:
        return "PTT";
    default:
        return "?";
    }
}

static const char* rpc_debug_encoding_name(Flipper_One_Frame_Encoding enc) {
    switch(enc) {
    case Flipper_One_Frame_Encoding_PLAIN:
        return "PLAIN";
    case Flipper_One_Frame_Encoding_RUN_LENGTH:
        return "RUN_LENGTH";
    case Flipper_One_Frame_Encoding_DEFLATE:
        return "DEFLATE";
    case Flipper_One_Frame_Encoding_DEFLATE_RUN_LENGTH:
        return "DEFLATE_RUN_LENGTH";
    default:
        return "?";
    }
}

void rpc_debug_print_data(const char* prefix, uint8_t* buffer, size_t size) {
    FuriString* str;
    str = furi_string_alloc();
    furi_string_reserve(str, 100 + size * 5);

    furi_string_cat_printf(str, "\r\n%s DEC(%zu): {", prefix, size);
    for(size_t i = 0; i < size; ++i) {
        furi_string_cat_printf(str, "%d, ", buffer[i]);
    }
    furi_string_cat_printf(str, "}\r\n");

    printf("%s", furi_string_get_cstr(str));
    furi_string_reset(str);
    furi_string_reserve(str, 100 + size * 3);

    furi_string_cat_printf(str, "%s HEX(%zu): {", prefix, size);
    for(size_t i = 0; i < size; ++i) {
        furi_string_cat_printf(str, "%02X", buffer[i]);
    }
    furi_string_cat_printf(str, "}\r\n\r\n");

    printf("%s", furi_string_get_cstr(str));
    furi_string_free(str);
}

void rpc_debug_print_message(const Flipper_One_Rpc_RpcMessage* message) {
    FuriString* str;
    str = furi_string_alloc();

    furi_string_cat_printf(str, "RpcMessage: {\r\n");

    switch(message->which_content) {
    case Flipper_One_Rpc_RpcMessage_frame_tag: {
        const Flipper_One_Frame_Frame* frame = &message->content.frame;
        furi_string_cat_printf(str, "\tframe: {\r\n");
        furi_string_cat_printf(str, "\t\twidth: %lu\r\n", (unsigned long)frame->width);
        furi_string_cat_printf(str, "\t\theight: %lu\r\n", (unsigned long)frame->height);
        furi_string_cat_printf(
            str, "\t\tencoding: %s\r\n", rpc_debug_encoding_name(frame->encoding));
        furi_string_cat_printf(
            str, "\t\tpixel_format: %s\r\n",
            frame->pixel_format == Flipper_One_Frame_PixelFormat_L8 ? "L8" : "?");
        if(frame->data) {
            furi_string_cat_printf(str, "\t\tdata: %u bytes\r\n", frame->data->size);
        } else {
            furi_string_cat_printf(str, "\t\tdata: (null)\r\n");
        }
        furi_string_cat_printf(str, "\t}\r\n");
        break;
    }
    case Flipper_One_Rpc_RpcMessage_button_event_tag: {
        const Flipper_One_Input_ButtonEvent* evt = &message->content.button_event;
        furi_string_cat_printf(str, "\tbutton_event: {\r\n");
        furi_string_cat_printf(
            str, "\t\tbutton: %s\r\n", rpc_debug_button_name(evt->button));
        furi_string_cat_printf(
            str, "\t\taction: %s\r\n",
            evt->action == Flipper_One_Input_ButtonAction_PRESS ? "PRESS" : "RELEASE");
        furi_string_cat_printf(str, "\t}\r\n");
        break;
    }
    default:
        furi_string_cat_printf(
            str, "\tUNKNOWN (which_content=%d)\r\n", message->which_content);
        break;
    }

    furi_string_cat_printf(str, "}\r\n");
    printf("%s", furi_string_get_cstr(str));

    furi_string_free(str);
}
