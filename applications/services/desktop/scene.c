#include "scene.h"

struct Scene {
    const SceneCallbacks* callbacks;
    View* view;
    void* data;
};

Scene* scene_alloc(const SceneCallbacks* callbacks, void* context) {
    Scene* scene = malloc(sizeof(Scene));
    scene->callbacks = callbacks;
    scene->data = NULL;

    scene->view = view_alloc();
    view_set_enabled(scene->view, false);

    if(scene->callbacks->on_alloc) {
        scene->callbacks->on_alloc(scene, context);
    }
    return scene;
}

View* scene_get_view(Scene* scene) {
    furi_check(scene);

    return scene->view;
}

bool scene_event(Scene* scene, uint32_t event, void* data) {
    furi_check(scene);

    if(scene->callbacks->on_event) {
        return scene->callbacks->on_event(scene, event, data);
    }
    return false;
}

void scene_enter(Scene* scene, void* app) {
    furi_check(scene);

    if(scene->callbacks->on_enter) {
        scene->callbacks->on_enter(scene, app);
    }

    view_set_enabled(scene->view, true);
}

void scene_exit(Scene* scene, void* app) {
    furi_check(scene);

    view_set_enabled(scene->view, false);

    if(scene->callbacks->on_exit) {
        scene->callbacks->on_exit(scene, app);
    }
}

void scene_set_data(Scene* scene, void* data) {
    furi_check(scene);
    scene->data = data;
}

void* scene_get_data(Scene* scene) {
    furi_check(scene);
    return scene->data;
}
