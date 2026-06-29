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

void scene_free(Scene* scene) {
    furi_check(scene);

    if(scene->callbacks->on_free) {
        scene->callbacks->on_free(scene);
    }
    view_free(scene->view);
    free(scene);
}

View* scene_get_view(Scene* scene) {
    furi_check(scene);

    return scene->view;
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
    furi_check(scene->data == NULL);
    scene->data = data;
}

void* scene_get_data(Scene* scene) {
    furi_check(scene);
    return scene->data;
}
