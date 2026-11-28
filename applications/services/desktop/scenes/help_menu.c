#include <assets.h>
#include <gui/gui.h>
#include <gui/modules/popup_menu.h>
#include "../scene.h"
#include "../desktop_i.h"

#define TAG "DesktopHelpMenu"

typedef enum {
    HelpMenuItemQuickStart = 0,
    HelpMenuItemPorts,
    HelpMenuItemTroubleshooting,
} HelpMenuItem;

typedef struct {
    Desktop* desktop;
    PopupMenu* menu;
} HelpMenuData;

static void help_menu_item_selected(size_t item_id, void* context) {
    Desktop* desktop = context;
    if(item_id == POPUP_MENU_EXIT_ID) {
        desktop_send_scene_event(desktop, DesktopSceneEventTypeToggleHelpMenu, desktop);
    } else {
        // TODO: the help pages
        FURI_LOG_I(TAG, "Item %zu", item_id);
    }
}

static void help_menu_on_alloc(Scene* scene, void* context) {
    HelpMenuData* scene_data = malloc(sizeof(HelpMenuData));
    scene_data->desktop = context;
    scene_set_data(scene, scene_data);

    View* view = scene_get_view(scene);
    furi_check(view);

    scene_data->menu = popup_menu_alloc(view);
    // It opens over the home screen, whose keys are the tabs
    const DesktopHomeTab* tab = &desktop_home_tab_help;
    popup_menu_set_softkey(scene_data->menu, PopupMenuSoftkeyStyleHomeTab, tab->index, tab->label, tab->key);
    popup_menu_set_title(scene_data->menu, "Flipper One - Help");
    popup_menu_set_icon(scene_data->menu, &popup_menu_help_icon);
    popup_menu_set_align(scene_data->menu, PopupMenuAlignLeft);
    popup_menu_set_callback(scene_data->menu, help_menu_item_selected, scene_data->desktop);
    popup_menu_add_item(scene_data->menu, "TBD Quick start", HelpMenuItemQuickStart);
    popup_menu_add_item(scene_data->menu, "TBD Ports", HelpMenuItemPorts);
    popup_menu_add_item(scene_data->menu, "TBD Troubleshooting", HelpMenuItemTroubleshooting);
}

static void help_menu_on_enter(Scene* scene, void* app) {
    UNUSED(app);
    HelpMenuData* scene_data = scene_get_data(scene);
    furi_check(scene_data);

    popup_menu_set_position(scene_data->menu, HelpMenuItemQuickStart);
    popup_menu_set_visible(scene_data->menu, true);
}

const SceneCallbacks scene_help_menu_callbacks = {
    .on_alloc = help_menu_on_alloc,
    .on_enter = help_menu_on_enter,
    .on_exit = NULL,
    .on_event = NULL,
};
