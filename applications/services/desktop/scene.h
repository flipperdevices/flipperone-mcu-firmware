#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Scene Scene;

typedef struct {
    void (*on_alloc)(Scene* scene, void* context);
    void (*on_enter)(Scene* scene, void* app);
    void (*on_exit)(Scene* scene, void* app);
    bool (*on_event)(Scene* scene, uint32_t event, void* data);
} SceneCallbacks;

Scene* scene_alloc(const SceneCallbacks* callbacks, void* context);

View* scene_get_view(Scene* scene);

void scene_enter(Scene* scene, void* app);

void scene_exit(Scene* scene, void* app);

bool scene_event(Scene* scene, uint32_t event, void* data);

void scene_set_data(Scene* scene, void* data);

void* scene_get_data(Scene* scene);

#ifdef __cplusplus
}
#endif
