#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_pwm.h>
#include <drivers/display/display_jd9853_spi.h>
#include <drivers/display/display_jd9853_reg.h>
#include <furi_hal_resources.h>
#include <drivers/ws2812/ws2812.h>
#include <input/input.h>

#include "clay_render.h"

#define TAG "GuiTest"

typedef enum {
    Power,
    Unknown,
    WiFi,
    Lan2,
    Lan1,
    USBPlug,
    USBWatt1,
    USBWatt2,
    USBWatt3,
    USBWatt4,
    BatteryCenter,
    BatteryOutline,
    BatteryWatt1,
    BatteryWatt2,
    BatteryWatt3,
    BatteryWatt4,
    Max,
} LedType;

static void gui_handle_clay_errors(Clay_ErrorData errorData) {
    FURI_LOG_E(TAG, "Clay error: %s", errorData.errorText.chars);
}

Clay_Color COLOR_WHITE = {255, 255, 255, 255};
Clay_Color COLOR_BLACK = {0, 0, 0, 255};

void RenderHeaderButton(Clay_String text) {
    CLAY_AUTO_ID(
        {.layout =
             {.padding =
                  {
                      8,
                      8,
                      4,
                      4,
                  }},
         .backgroundColor = COLOR_BLACK,
         .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
        CLAY_TEXT(
            text,
            CLAY_TEXT_CONFIG({
                .fontId = FontButton,
                .textColor = COLOR_WHITE,
            }));
    }
}

typedef struct {
    Clay_String title;
    Clay_String contents;
    // LCDBitmap* image;
} Document;

typedef struct {
    Document* documents;
    uint32_t length;
} DocumentArray;

#define MAX_DOCUMENTS 3
Document documentsRaw[MAX_DOCUMENTS];

DocumentArray documents = {.length = MAX_DOCUMENTS, .documents = documentsRaw};

void ClayVideoDemoPlaydate_Initialize(void) {
    documents.documents[0] = (Document){.title = CLAY_STRING("Squirrels"),
                                        // .image = pd->graphics->loadBitmap("star.png", NULL),
                                        .contents = CLAY_STRING(
                                            "The Secret Life of Squirrels: Nature's Clever Acrobats\n"
                                            "Squirrels are often overlooked creatures, dismissed as mere park "
                                            "inhabitants or backyard nuisances. Yet, beneath their fluffy tails "
                                            "and twitching noses lies an intricate world of cunning, agility, "
                                            "and survival tactics that are nothing short of fascinating. As one "
                                            "of the most common mammals in North America, squirrels have adapted "
                                            "to a wide range of environments from bustling urban centers to "
                                            "tranquil forests and have developed a variety of unique behaviors "
                                            "that continue to intrigue scientists and nature enthusiasts alike.\n"
                                            "\n"
                                            "Master Tree Climbers\n"
                                            "At the heart of a squirrel's skill set is its impressive ability to "
                                            "navigate trees with ease. Whether they're darting from branch to "
                                            "branch or leaping across wide gaps, squirrels possess an innate "
                                            "talent for acrobatics. Their powerful hind legs, which are longer "
                                            "than their front legs, give them remarkable jumping power. With a "
                                            "tail that acts as a counterbalance, squirrels can leap distances of "
                                            "up to ten times the length of their body, making them some of the "
                                            "best aerial acrobats in the animal kingdom.\n"
                                            "But it's not just their agility that makes them exceptional "
                                            "climbers. Squirrels' sharp, curved claws allow them to grip tree "
                                            "bark with precision, while the soft pads on their feet provide "
                                            "traction on slippery surfaces. Their ability to run at high speeds "
                                            "and scale vertical trunks with ease is a testament to the "
                                            "evolutionary adaptations that have made them so successful in their "
                                            "arboreal habitats.\n"
                                            "\n"
                                            "Food Hoarders Extraordinaire\n"
                                            "Squirrels are often seen frantically gathering nuts, seeds, and "
                                            "even fungi in preparation for winter. While this behavior may seem "
                                            "like instinctual hoarding, it is actually a survival strategy that "
                                            "has been honed over millions of years. Known as \"scatter "
                                            "hoarding,\" squirrels store their food in a variety of hidden "
                                            "locations, often burying it deep in the soil or stashing it in "
                                            "hollowed-out tree trunks.\n"
                                            "Interestingly, squirrels have an incredible memory for the "
                                            "locations of their caches. Research has shown that they can "
                                            "remember thousands of hiding spots, often returning to them months "
                                            "later when food is scarce. However, they don't always recover every "
                                            "stash some forgotten caches eventually sprout into new trees, "
                                            "contributing to forest regeneration. This unintentional role as "
                                            "forest gardeners highlights the ecological importance of squirrels "
                                            "in their ecosystems.\n"
                                            "\n"
                                            "The Great Squirrel Debate: Urban vs. Wild\n"
                                            "While squirrels are most commonly associated with rural or wooded "
                                            "areas, their adaptability has allowed them to thrive in urban "
                                            "environments as well. In cities, squirrels have become adept at "
                                            "finding food sources in places like parks, streets, and even "
                                            "garbage cans. However, their urban counterparts face unique "
                                            "challenges, including traffic, predators, and the lack of natural "
                                            "shelters. Despite these obstacles, squirrels in urban areas are "
                                            "often observed using human infrastructure such as buildings, "
                                            "bridges, and power lines as highways for their acrobatic "
                                            "escapades.\n"
                                            "There is, however, a growing concern regarding the impact of urban "
                                            "life on squirrel populations. Pollution, deforestation, and the "
                                            "loss of natural habitats are making it more difficult for squirrels "
                                            "to find adequate food and shelter. As a result, conservationists "
                                            "are focusing on creating squirrel-friendly spaces within cities, "
                                            "with the goal of ensuring these resourceful creatures continue to "
                                            "thrive in both rural and urban landscapes.\n"
                                            "\n"
                                            "A Symbol of Resilience\n"
                                            "In many cultures, squirrels are symbols of resourcefulness, "
                                            "adaptability, and preparation. Their ability to thrive in a variety "
                                            "of environments while navigating challenges with agility and grace "
                                            "serves as a reminder of the resilience inherent in nature. Whether "
                                            "you encounter them in a quiet forest, a city park, or your own "
                                            "backyard, squirrels are creatures that never fail to amaze with "
                                            "their endless energy and ingenuity.\n"
                                            "In the end, squirrels may be small, but they are mighty in their "
                                            "ability to survive and thrive in a world that is constantly "
                                            "changing. So next time you spot one hopping across a branch or "
                                            "darting across your lawn, take a moment to appreciate the "
                                            "remarkable acrobat at work a true marvel of the natural world.\n")};
    documents.documents[1] = (Document){.title = CLAY_STRING("Lorem Ipsum"),
                                        // .image = pd->graphics->loadBitmap("star.png", NULL),
                                        .contents = CLAY_STRING(
                                            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
                                            "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
                                            "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
                                            "aliquip ex ea commodo consequat. Duis aute irure dolor in "
                                            "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
                                            "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
                                            "culpa qui officia deserunt mollit anim id est laborum.")};
    documents.documents[2] = (Document){.title = CLAY_STRING("Vacuum Instructions"),
                                        // .image = pd->graphics->loadBitmap("star.png", NULL),
                                        .contents = CLAY_STRING(
                                            "Chapter 3: Getting Started - Unpacking and Setup\n"
                                            "\n"
                                            "Congratulations on your new SuperClean Pro 5000 vacuum cleaner! In "
                                            "this section, we will guide you through the simple steps to get "
                                            "your vacuum up and running. Before you begin, please ensure that "
                                            "you have all the components listed in the \"Package Contents\" "
                                            "section on page 2.\n"
                                            "\n"
                                            "1. Unboxing Your Vacuum\n"
                                            "Carefully remove the vacuum cleaner from the box. Avoid using sharp "
                                            "objects that could damage the product. Once removed, place the unit "
                                            "on a flat, stable surface to proceed with the setup. Inside the "
                                            "box, you should find:\n"
                                            "\n"
                                            "    The main vacuum unit\n"
                                            "    A telescoping extension wand\n"
                                            "    A set of specialized cleaning tools (crevice tool, upholstery "
                                            "brush, etc.)\n"
                                            "    A reusable dust bag (if applicable)\n"
                                            "    A power cord with a 3-prong plug\n"
                                            "    A set of quick-start instructions\n"
                                            "\n"
                                            "2. Assembling Your Vacuum\n"
                                            "Begin by attaching the extension wand to the main body of the "
                                            "vacuum cleaner. Line up the connectors and twist the wand into "
                                            "place until you hear a click. Next, select the desired cleaning "
                                            "tool and firmly attach it to the wand's end, ensuring it is "
                                            "securely locked in.\n"
                                            "\n"
                                            "For models that require a dust bag, slide the bag into the "
                                            "compartment at the back of the vacuum, making sure it is properly "
                                            "aligned with the internal mechanism. If your vacuum uses a bagless "
                                            "system, ensure the dust container is correctly seated and locked in "
                                            "place before use.\n"
                                            "\n"
                                            "3. Powering On\n"
                                            "To start the vacuum, plug the power cord into a grounded electrical "
                                            "outlet. Once plugged in, locate the power switch, usually "
                                            "positioned on the side of the handle or body of the unit, depending "
                                            "on your model. Press the switch to the \"On\" position, and you "
                                            "should hear the motor begin to hum. If the vacuum does not power "
                                            "on, check that the power cord is securely plugged in, and ensure "
                                            "there are no blockages in the power switch.\n"
                                            "\n"
                                            "Note: Before first use, ensure that the vacuum filter (if your "
                                            "model has one) is properly installed. If unsure, refer to \"Section "
                                            "5: Maintenance\" for filter installation instructions.")};
}

Clay_String test = CLAY_STRING("Test!");

void ClayVideoDemoPlaydate_CreateLayout(int selectedDocumentIndex) {
    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};

    Clay_BorderElementConfig contentBorders = {
        .color = COLOR_BLACK,
        .width = {
            .top = 1,
            .left = 1,
            .right = 1,
            .bottom = 1,
        }};

    // Build UI here
    CLAY(
        CLAY_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding =
                 {
                     4,
                     4,
                     4,
                     3,
                 },
             .childGap = 4,
         }}) {
        // Child elements go inside braces
        CLAY(
            CLAY_ID("HeaderBar"),
            {
                .layout =
                    {.sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                     .childGap = 8,
                     .childAlignment =
                         {
                             .y = CLAY_ALIGN_Y_CENTER,
                         }},
            }) {
            // Header buttons go here
            CLAY(
                CLAY_ID("FileButton"),
                {.layout =
                     {.padding =
                          {
                              8,
                              8,
                              4,
                              4,
                          }},
                 .backgroundColor = COLOR_BLACK,
                 .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(
                    CLAY_STRING("File"),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontButton,
                        .textColor = COLOR_WHITE,
                    }));
            }
            RenderHeaderButton(CLAY_STRING("Edit"));
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {
            }
            RenderHeaderButton(CLAY_STRING("Upload"));
            RenderHeaderButton(CLAY_STRING("Media"));
            RenderHeaderButton(CLAY_STRING("Close"));
        }

        CLAY(
            CLAY_ID("LowerContent"),
            {
                .layout =
                    {
                        .sizing = layoutExpand,
                        .childGap = 4,
                    },
            }) {
            CLAY(
                CLAY_ID("Sidebar"),
                {.border = contentBorders,
                 .cornerRadius = CLAY_CORNER_RADIUS(4),
                 .layout = {
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .padding = CLAY_PADDING_ALL(4),
                     .childGap = 4,
                     .sizing = {.width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_GROW(0)}}}) {
                for(int i = 0; i < documents.length; i++) {
                    Document document = documents.documents[i];
                    Clay_LayoutConfig sidebarButtonLayout = {.sizing = {.width = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(6)};

                    if(i == selectedDocumentIndex) {
                        CLAY_AUTO_ID({.layout = sidebarButtonLayout, .backgroundColor = COLOR_BLACK, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                            CLAY_TEXT(
                                document.title,
                                CLAY_TEXT_CONFIG({
                                    .fontId = FontButton,
                                    .textColor = COLOR_WHITE,
                                }));
                        }
                    } else {
                        CLAY_AUTO_ID({
                            .layout = sidebarButtonLayout,
                            .backgroundColor = (Clay_Color){0, 0, 0, Clay_Hovered() ? 120 : 0},
                            .cornerRadius = CLAY_CORNER_RADIUS(4),
                            .border = contentBorders,
                        }) {
                            CLAY_TEXT(
                                document.title,
                                CLAY_TEXT_CONFIG({
                                    .fontId = FontButton,
                                    .textColor = COLOR_BLACK,
                                }));
                        }
                    }
                }
            }

            CLAY(
                CLAY_ID("MainContent"),
                {.border = contentBorders,
                 .cornerRadius = CLAY_CORNER_RADIUS(4),
                 .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
                 .layout = {
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 8,
                     .padding = CLAY_PADDING_ALL(6),
                     .sizing = layoutExpand,
                 }}) {
                Document selectedDocument = documents.documents[selectedDocumentIndex];
                // CLAY_AUTO_ID(
                //     {.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 4, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM,}}}) {
                //     CLAY_TEXT(selectedDocument.title, CLAY_TEXT_CONFIG({.fontId = FontBody, .textColor = COLOR_BLACK,}));
                //     // CLAY_AUTO_ID(
                //     //     {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(32), .height = CLAY_SIZING_FIXED(30)}},
                //     //      .image = {.imageData = selectedDocument.image, .sourceDimensions = {32, 30}}}) {
                //     // }
                // }
                CLAY_TEXT(
                    selectedDocument.contents,
                    CLAY_TEXT_CONFIG({
                        .fontId = FontBody,
                        .textColor = COLOR_BLACK,
                    }));
            }
        }
    }
}

typedef enum {
    GuiTestMessageTypeInputEvent = 0,
} GuiTestMessageType;

typedef struct {
    GuiTestMessageType type;
    union {
        InputEvent input_event;
    };
} GuiTestMessage;

static void gui_test_input_events_callback(const void* value, void* context) {
    furi_check(value);
    furi_check(context);

    FuriMessageQueue* queue = context;
    const InputEvent* event = value;

    GuiTestMessage message;
    message.type = GuiTestMessageTypeInputEvent;
    message.input_event = *event;

    furi_message_queue_put(queue, &message, FuriWaitForever);
}

void gui_test_create_keypad_button(Clay_String text, bool inverted) {
    CLAY_AUTO_ID(
        {.border = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}},
         .layout =
             {
                 .padding = {8, 8, 4, 4},
                 .sizing = {.width = CLAY_SIZING_FIXED(40)},
                 .childAlignment =
                     {
                         .y = CLAY_ALIGN_Y_CENTER,
                         .x = CLAY_ALIGN_X_CENTER,
                     },
             },
         .backgroundColor = inverted ? COLOR_WHITE : COLOR_BLACK,
         .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
        CLAY_TEXT(
            text,
            CLAY_TEXT_CONFIG({
                .fontId = FontButton,
                .textColor = inverted ? COLOR_BLACK : COLOR_WHITE,
            }));
    }
}

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool ok;
} KeypadState;

static void gui_test_create_keypad_layout(KeypadState state) {
    Clay_Sizing layoutExpand = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
    Clay_BorderElementConfig contentBorders = {.color = COLOR_BLACK, .width = {.top = 1, .left = 1, .right = 1, .bottom = 1}};

    CLAY(
        CLAY_ID("OuterContainer"),
        {.backgroundColor = COLOR_WHITE,
         .layout = {
             .layoutDirection = CLAY_TOP_TO_BOTTOM,
             .sizing = layoutExpand,
             .padding = {4, 4, 4, 3},
             .childGap = 4,
         }}) {
        // Child elements go inside braces
        CLAY(
            CLAY_ID("HeaderBar"),
            {
                .layout =
                    {.sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                     .childGap = 8,
                     .childAlignment =
                         {
                             .y = CLAY_ALIGN_Y_CENTER,
                         }},
            }) {
            // Header buttons go here
            CLAY_AUTO_ID({.layout = {.padding = {8, 8, 4, 4}}, .backgroundColor = COLOR_BLACK, .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                CLAY_TEXT(
                    CLAY_STRING("KeypadTest"),
                    CLAY_TEXT_CONFIG({
                        .fontId = FontButton,
                        .textColor = COLOR_WHITE,
                    }));
            }
        }
        // CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {
        //             }
        CLAY(
            CLAY_ID("LowerContent"),
            {
                .layout =
                    {
                        .sizing = layoutExpand,
                        .childGap = 4,
                    },
            }) {
            CLAY(
                CLAY_ID("MainContent"),
                {
                    .border = contentBorders,
                    .cornerRadius = CLAY_CORNER_RADIUS(4),
                    .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()},
                    .layout =
                        {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .childGap = 8,
                            .padding = {6, 6, 6, 6},
                            .sizing = layoutExpand,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    gui_test_create_keypad_button(CLAY_STRING("Up"), state.up);
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    gui_test_create_keypad_button(CLAY_STRING("Left"), state.left);
                    gui_test_create_keypad_button(CLAY_STRING("Ok"), state.ok);
                    gui_test_create_keypad_button(CLAY_STRING("Right"), state.right);
                }
                CLAY_AUTO_ID({
                    .layout =
                        {
                            .sizing = {.height = CLAY_SIZING_FIXED(14), .width = CLAY_SIZING_GROW(0)},
                            .childGap = 8,
                            .childAlignment =
                                {
                                    .y = CLAY_ALIGN_Y_CENTER,
                                    .x = CLAY_ALIGN_X_CENTER,
                                },
                        },
                }) {
                    gui_test_create_keypad_button(CLAY_STRING("Down"), state.down);
                }
            }
        }
    }
}

static void gui_test_create_layout(int32_t layout_index, int32_t selected_document_index, KeypadState state) {
    if(layout_index == 0) {
        gui_test_create_keypad_layout(state);
    } else if(layout_index == 1) {
        ClayVideoDemoPlaydate_CreateLayout(selected_document_index);
    }
}

int32_t gui_test_app(void* p) {
    FURI_LOG_I(TAG, "Starting GUI Test App");

    Ws2812* ws2812 = ws2812_init(&gpio_status_led_line1, 1);
    ws2812_put_pixel_rgb(ws2812, 0, 10, 0, 0);

    DisplayJd9853SPI* display = display_jd9853_spi_init();
    display_jd9853_spi_backlight_set_brightness(display, 10);

    FuriMessageQueue* queue = furi_message_queue_alloc(32, sizeof(GuiTestMessage));

    FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* input_subscription = furi_pubsub_subscribe(input, gui_test_input_events_callback, queue);

    Clay_SetMaxElementCount(256);
    Clay_SetMaxMeasureTextCacheWordCount(1024);

    uint64_t totalMemorySize = Clay_MinMemorySize();
    FURI_LOG_I(TAG, "Clay allocation: %lluk", totalMemorySize / 1024);

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(arena, (Clay_Dimensions){JD9853_WIDTH, JD9853_HEIGHT}, (Clay_ErrorHandler){gui_handle_clay_errors});
    ClayVideoDemoPlaydate_Initialize();
    Clay_SetMeasureTextFunction(render_measure_text, NULL);

    int32_t layout_index = 0;
    int32_t selected_document_index = 0;
    int32_t scroll_offset = 0;

    KeypadState keypad_state = {0};

    while(1) {
        Clay_ResetMeasureTextCache();

        // Will be different for each renderer / environment
        // Optional: Update internal layout dimensions to support resizing
        Clay_SetLayoutDimensions((Clay_Dimensions){JD9853_WIDTH, JD9853_HEIGHT});
        // Optional: Update internal pointer position for handling mouseover / click / touch events - needed for scrolling & debug tools
        // Clay_SetPointerState((Clay_Vector2){0, 0}, false);
        // Optional: Update internal pointer position for handling mouseover / click / touch events - needed for scrolling and debug tools
        // Clay_UpdateScrollContainers(true, (Clay_Vector2){0, 0}, 1 / 60.f);

        // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
        Clay_BeginLayout();

        gui_test_create_layout(layout_index, selected_document_index, keypad_state);

        // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
        Clay_RenderCommandArray renderCommands = Clay_EndLayout();

        render_clear_buffer(0x00);

        // More comprehensive rendering examples can be found in the renderers/ directory
        for(int i = 0; i < renderCommands.length; i++) {
            Clay_RenderCommand* renderCommand = &renderCommands.internalArray[i];
            Clay_BoundingBox boundingBox = renderCommand->boundingBox;

            switch(renderCommand->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                render_rectangle(&boundingBox, &renderCommand->renderData.rectangle);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                render_border(&boundingBox, &renderCommand->renderData.border);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                render_text(&boundingBox, &renderCommand->renderData.text);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                render_image(&boundingBox, &renderCommand->renderData.image);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                render_scissor_start(&boundingBox);
            } break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                render_scissor_end();
            } break;
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
                furi_crash("Custom render commands are not supported");
            } break;
            }
        }

        display_jd9853_spi_write_buffer(display, JD9853_WIDTH, JD9853_HEIGHT, render_get_buffer(), JD9853_WIDTH * JD9853_HEIGHT);

        GuiTestMessage message;
        if(furi_message_queue_get(queue, &message, 1000 / 60) == FuriStatusOk) {
            switch(message.type) {
            case GuiTestMessageTypeInputEvent: {
                InputEvent event = message.input_event;
                if(event.key == InputKeyDown) {
                    if(event.type == InputTypePress) scroll_offset = -1;
                    if(event.type == InputTypeRelease) scroll_offset = 0;
                }
                if(event.key == InputKeyUp) {
                    if(event.type == InputTypePress) scroll_offset = 1;
                    if(event.type == InputTypeRelease) scroll_offset = 0;
                }
                if(event.type == InputTypePress && event.key == InputKeyOk) {
                    selected_document_index++;
                    if(selected_document_index >= documents.length) {
                        selected_document_index = 0;
                    }
                }
                if(event.key == InputKey1 && event.type == InputTypePress) {
                    layout_index = 0;
                }
                if(event.key == InputKey2 && event.type == InputTypePress) {
                    layout_index = 1;
                }

                if(event.type == InputTypePress || event.type == InputTypeRelease) {
                    if(event.key == InputKeyUp) {
                        keypad_state.up = event.type == InputTypePress;
                    }
                    if(event.key == InputKeyDown) {
                        keypad_state.down = event.type == InputTypePress;
                    }
                    if(event.key == InputKeyLeft) {
                        keypad_state.left = event.type == InputTypePress;
                    }
                    if(event.key == InputKeyRight) {
                        keypad_state.right = event.type == InputTypePress;
                    }
                    if(event.key == InputKeyOk) {
                        keypad_state.ok = event.type == InputTypePress;
                    }
                }
            } break;
            }
        }

        Clay_SetPointerState((Clay_Vector2){.x = JD9853_WIDTH / 2.0f, .y = JD9853_HEIGHT / 2.0f}, false);
        Clay_UpdateScrollContainers(false, (Clay_Vector2){0, 1.0f * scroll_offset}, 1 / 60.f);
    }

    return 0;
}
