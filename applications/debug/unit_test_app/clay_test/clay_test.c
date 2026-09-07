/**
 * @file clay_test.c
 * @brief Tests for the vendored Clay fork's element-id hashmap lifecycle
 *        (services/gui/clay.h).
 *
 * The element-id hashmap (layoutElementsHashMapInternal) reclaims entries
 * whose id has not been declared for a few frames (generation-based eviction,
 * the same scheme Clay__MeasureTextCached uses), so id churn between
 * frames/screens must never permanently exhaust it. Historically the map was
 * append-only for the whole session and churning ids broke lookups until
 * reboot; the eviction-focused tests below were written against that defect
 * (TDD red) and now guard the eviction mechanism against regressions - every
 * test here must always pass.
 *
 * Each test runs against its own small Clay context so overflow is cheap to
 * reach; the GUI service's context is parked (and its redraw thread blocked
 * via gui_lock) for the duration of a test and restored afterwards, because
 * Clay's current-context pointer is a plain global shared with the GUI thread.
 */

#include "../unit_tests.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <gui/clay_helper.h>
#include <string.h>

/* Small on purpose: the id hashmap trips its capacity check at
 * TEST_MAX_ELEMENT_ID_COUNT - 1 entries, one of which is Clay's own
 * "Clay__RootContainer". */
#define TEST_MAX_ELEMENT_COUNT    32
#define TEST_MAX_ELEMENT_ID_COUNT 16

static Gui* test_gui;
static Clay_Context* gui_clay_context;
static int32_t gui_max_element_count;
static int32_t gui_max_element_id_count;
static int32_t gui_max_word_count;
static void* test_arena_memory;

static int err_total;
static int err_capacity;
static int err_duplicate;
static Clay_ErrorData err_last;

static void clay_test_error_handler(Clay_ErrorData error_data) {
    err_total++;
    if(error_data.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) err_capacity++;
    if(error_data.errorType == CLAY_ERROR_TYPE_DUPLICATE_ID) err_duplicate++;
    err_last = error_data;
}

static void clay_test_setup(void) {
    test_gui = furi_record_open(RECORD_GUI);
    gui_lock(test_gui);

    gui_clay_context = Clay_GetCurrentContext();
    gui_max_element_count = Clay_GetMaxElementCount();
    gui_max_element_id_count = Clay_GetMaxElementIdCount();
    gui_max_word_count = Clay_GetMaxMeasureTextCacheWordCount();

    /* With no current context the setters write the defaults, which both
     * Clay_MinMemorySize() and Clay_Initialize() read; with the GUI context
     * still current they would mutate the live GUI context instead. */
    Clay_SetCurrentContext(NULL);
    Clay_SetMaxElementCount(TEST_MAX_ELEMENT_COUNT);
    Clay_SetMaxElementIdCount(TEST_MAX_ELEMENT_ID_COUNT);

    uint32_t arena_size = Clay_MinMemorySize();
    test_arena_memory = malloc(arena_size);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(arena_size, test_arena_memory);
    Clay_Initialize(arena, (Clay_Dimensions){240, 320}, (Clay_ErrorHandler){clay_test_error_handler, NULL});

    err_total = 0;
    err_capacity = 0;
    err_duplicate = 0;
    memset(&err_last, 0, sizeof(err_last));
}

static void clay_test_teardown(void) {
    Clay_SetCurrentContext(NULL);
    Clay_SetMaxElementCount(gui_max_element_count);
    Clay_SetMaxElementIdCount(gui_max_element_id_count);
    Clay_SetMaxMeasureTextCacheWordCount(gui_max_word_count);
    Clay_SetCurrentContext(gui_clay_context);

    free(test_arena_memory);
    test_arena_memory = NULL;

    gui_unlock(test_gui);
    furi_record_close(RECORD_GUI);
    test_gui = NULL;
}

/* No text elements anywhere in these layouts, so no measure-text function is
 * needed. */
static void clay_test_declare_box(Clay_ElementId id, float width) {
    CLAY(
        id,
        {
            .layout = {.sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(10)}},
        }){};
}

static void clay_test_declare_float_on(Clay_ElementId id, uint32_t parent_id) {
    CLAY(
        id,
        {
            .layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}},
            .floating =
                {
                    .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                    .parentId = parent_id,
                },
        }){};
}

/* ── Behavior that must keep working ────────────────────────────────────── */

/* Stable IDs re-declared every frame occupy a fixed number of hashmap slots. */
MU_TEST(clay_stable_ids_keep_hashmap_bounded) {
    int32_t len_after_first_frame = 0;

    for(int frame = 0; frame < 10; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(CLAY_ID("StableA"), 20);
        clay_test_declare_box(CLAY_ID("StableB"), 20);
        clay_test_declare_box(CLAY_ID("StableC"), 20);
        Clay_EndLayout();
        if(frame == 0) len_after_first_frame = Clay_GetLayoutElementHashMapLength();
    }

    mu_assert_int_eq(len_after_first_frame, Clay_GetLayoutElementHashMapLength());
    mu_assert_int_eq(0, err_total);
}

/* Two identical IDs in one frame report CLAY_ERROR_TYPE_DUPLICATE_ID with the
 * offending element fully identified (the diagnostics added to the fork). */
MU_TEST(clay_duplicate_id_reports_details) {
    Clay_ElementId dup = CLAY_ID("DupElement");

    Clay_BeginLayout();
    clay_test_declare_box(dup, 20);
    clay_test_declare_box(dup, 30);
    Clay_EndLayout();

    mu_assert_int_eq(1, err_duplicate);
    mu_check(err_last.elementId == dup.id);
    mu_check(err_last.elementBaseId == dup.baseId);
    mu_check(err_last.elementStringId.chars != NULL);
    mu_assert_int_eq((int)dup.stringId.length, (int)err_last.elementStringId.length);
}

/* CLAY_ID_LOCAL hashes against the element it is nested inside: the same
 * label under two different parents must not collide (regression test for the
 * Clay__GetParentElementId fix). */
MU_TEST(clay_local_id_scoped_to_parent) {
    for(int frame = 0; frame < 3; frame++) {
        Clay_BeginLayout();
        CLAY(
            CLAY_ID("LocalParentA"),
            {
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_FIXED(50)}},
            }) {
            clay_test_declare_box(CLAY_ID_LOCAL("Child"), 10);
        }
        CLAY(
            CLAY_ID("LocalParentB"),
            {
                .layout = {.sizing = {.width = CLAY_SIZING_FIXED(100), .height = CLAY_SIZING_FIXED(50)}},
            }) {
            clay_test_declare_box(CLAY_ID_LOCAL("Child"), 10);
        }
        Clay_EndLayout();
    }

    mu_assert_int_eq(0, err_duplicate);
    mu_assert_int_eq(0, err_total);
}

/* ── Regression tests for id-hashmap eviction ───────────────────────────── */

/* IDs that churn between frames (one new distinct ID per frame) must not
 * permanently exhaust the hashmap: entries whose elements are gone must be
 * reclaimed. Without eviction every distinct ID ever seen occupies a slot
 * forever and this overflows after TEST_MAX_ELEMENT_ID_COUNT frames. */
MU_TEST(clay_id_churn_does_not_exhaust_hashmap) {
    for(int frame = 0; frame < TEST_MAX_ELEMENT_ID_COUNT * 3; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(CLAY_ID("ChurnAnchor"), 20);
        clay_test_declare_box(CLAY_IDI("Churn", frame), 20);
        Clay_EndLayout();
    }

    mu_assert_int_eq(0, err_capacity);
    mu_assert_int_eq(0, err_total);
}

/* After a long session of churning IDs, a brand-new element must still get a
 * working hashmap entry. Without eviction Clay__AddHashMapItem returns NULL
 * once the map is full and the new element stays invisible to
 * Clay_GetElementData forever. */
MU_TEST(clay_new_ids_resolvable_after_long_session) {
    for(int frame = 0; frame < TEST_MAX_ELEMENT_ID_COUNT * 2; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(CLAY_IDI("Session", frame), 20);
        Clay_EndLayout();
    }

    /* A few frames so any lazily-reclaimed slots settle. */
    Clay_ElementId fresh = CLAY_ID("FreshAfterChurn");
    for(int frame = 0; frame < 3; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(fresh, 77);
        Clay_EndLayout();
    }

    Clay_ElementData data = Clay_GetElementData(fresh);
    mu_check(data.found);
    mu_assert_int_eq(77, (int)data.boundingBox.width);
}

/* A screen transition must survive an id budget that fits either screen but
 * not both at once: the previous screen's ids stay in the map until the
 * end-of-frame sweep ages them out (3 frames), so mid-transition the map fills
 * up and Clay__AddHashMapItem must reclaim stale entries on demand instead of
 * failing. */
MU_TEST(clay_screen_transition_survives_tight_id_budget) {
    /* "Screen A": 10 ids + Clay's root = 11 of the 15 usable slots. */
    for(int frame = 0; frame < 2; frame++) {
        Clay_BeginLayout();
        for(int i = 0; i < 10; i++) {
            clay_test_declare_box(CLAY_IDI("ScreenA", i), 20);
        }
        Clay_EndLayout();
    }

    /* "Screen B": 10 fresh ids while all of screen A's are still resident -
     * the union (21) exceeds capacity, so this only passes if the map evicts
     * screen A's entries the moment it runs out of room. */
    for(int frame = 0; frame < 2; frame++) {
        Clay_BeginLayout();
        for(int i = 0; i < 10; i++) {
            clay_test_declare_box(CLAY_IDI("ScreenB", i), 40);
        }
        Clay_EndLayout();
    }

    mu_assert_int_eq(0, err_capacity);
    mu_assert_int_eq(0, err_total);

    Clay_ElementData data = Clay_GetElementData(CLAY_IDI("ScreenB", 9));
    mu_check(data.found);
    mu_assert_int_eq(40, (int)data.boundingBox.width);
}

/* A full hashmap must not freeze elements that were known before it filled.
 * Without eviction the capacity bail-out in Clay__AddHashMapItem also skipped
 * the existing-entry update path, so a known element's item kept a stale
 * layoutElement pointer into the per-frame element array; anything resolved
 * through the hashmap (here: a floating element sizing itself to its parent)
 * silently read whatever element occupied that slot in the current frame. */
MU_TEST(clay_floating_attachment_survives_full_hashmap) {
    Clay_ElementId tracked = CLAY_ID("Tracked");
    Clay_ElementId floater = CLAY_ID("Floater");

    /* Both get hashmap entries while there is still room. */
    for(int frame = 0; frame < 2; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(tracked, 50);
        clay_test_declare_float_on(floater, tracked.id);
        Clay_EndLayout();
    }

    /* Exhaust the id hashmap with churning IDs. */
    for(int frame = 0; frame < TEST_MAX_ELEMENT_ID_COUNT * 2; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(tracked, 50);
        clay_test_declare_box(CLAY_IDI("Noise", frame), 33);
        Clay_EndLayout();
    }

    /* Change the layout: an old-id decoy takes the element-array slot the
     * tracked element used to occupy, and the tracked element resizes. The
     * floating element (GROW sizing) must follow the tracked element's new
     * 120px width, not the decoy's 33px. */
    for(int frame = 0; frame < 2; frame++) {
        Clay_BeginLayout();
        clay_test_declare_box(CLAY_IDI("Noise", 0), 33);
        clay_test_declare_box(tracked, 120);
        clay_test_declare_float_on(floater, tracked.id);
        Clay_EndLayout();
    }

    Clay_ElementData data = Clay_GetElementData(floater);
    mu_check(data.found);
    mu_assert_int_eq(120, (int)data.boundingBox.width);
}

MU_TEST_SUITE(clay_hashmap_suite) {
    MU_SUITE_CONFIGURE(clay_test_setup, clay_test_teardown);

    MU_RUN_TEST(clay_stable_ids_keep_hashmap_bounded);
    MU_RUN_TEST(clay_duplicate_id_reports_details);
    MU_RUN_TEST(clay_local_id_scoped_to_parent);

    MU_RUN_TEST(clay_id_churn_does_not_exhaust_hashmap);
    MU_RUN_TEST(clay_new_ids_resolvable_after_long_session);
    MU_RUN_TEST(clay_screen_transition_survives_tight_id_budget);
    MU_RUN_TEST(clay_floating_attachment_survives_full_hashmap);
}

int run_minunit_clay_test(void) {
    MU_RUN_SUITE(clay_hashmap_suite);
    return MU_EXIT_CODE;
}
