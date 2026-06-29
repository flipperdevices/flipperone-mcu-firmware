#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Scene Scene;

typedef struct {
    void (*on_alloc)(Scene* scene, void* context);
    void (*on_free)(Scene* scene);
    void (*on_enter)(Scene* scene, void* app);
    void (*on_exit)(Scene* scene, void* app);
} SceneCallbacks;

Scene* scene_alloc(const SceneCallbacks* callbacks, void* context);

void scene_free(Scene* scene);

View* scene_get_view(Scene* scene);

void scene_enter(Scene* scene, void* app);

void scene_exit(Scene* scene, void* app);

void scene_set_data(Scene* scene, void* data);

void* scene_get_data(Scene* scene);

#ifdef __cplusplus
}
#endif
