# Firmware target descriptor API.
#
# A target is described by targets/<name>/target.cmake, which only declares data:
# which target it is based on, and which sources and include directories make up
# the firmware. A descriptor must not create CMake targets — keeping it pure data
# is what makes inheritance a matter of editing lists.
#
# Paths without a fw_target_ prefix are relative to the repository root, like
# everywhere else in the project. The fw_target_ ones are relative to the target
# directory and are looked up along the inheritance chain: for a target f2 whose
# descriptor starts with fw_base(f100) the search path is [targets/f2,
# targets/f100], child first, which means
#
#   * overriding a file means dropping it into the child under the same relative
#     path — no descriptor change needed, the child's copy wins;
#   * a header in the child's config/ shadows the parent's by include order, while
#     everything the child did not override keeps coming from the parent;
#   * fw_target_remove() is only needed to drop an inherited file entirely.
#
# Include directories keep the order in which they are declared, and the base's
# entries always come first, so the include order of a derived target matches the
# target it is based on. Sources end up sorted, so their declaration order is
# irrelevant.

include_guard(GLOBAL)

set(FW_TARGETS_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(FW_ROOT_DIR "${FW_TARGETS_DIR}" DIRECTORY)

# ---------------------------------------------------------------------------
# Declaration API — for use inside targets/<name>/target.cmake
# ---------------------------------------------------------------------------

# Inherit from another target. Must be the first statement in a descriptor: the
# base is loaded in place, so everything after it is applied on top.
macro(fw_base _fw_name)
    if(NOT EXISTS "${FW_TARGETS_DIR}/${_fw_name}/target.cmake")
        _fw_available_targets(_fw_available)
        message(FATAL_ERROR
            "Unknown firmware target '${_fw_name}'. Available: ${_fw_available}")
    endif()

    if("${_fw_name}" IN_LIST FW_OVERLAY_NAMES)
        message(FATAL_ERROR
            "Cyclic target inheritance: ${FW_OVERLAY_NAMES} -> ${_fw_name}")
    endif()

    list(APPEND FW_OVERLAY_NAMES "${_fw_name}")
    list(APPEND FW_OVERLAY_DIRS "${FW_TARGETS_DIR}/${_fw_name}")
    include("${FW_TARGETS_DIR}/${_fw_name}/target.cmake")
endmacro()

# Sources shared by every target. Globs are relative to the repository root.
macro(fw_sources)
    foreach(_fw_pattern ${ARGV})
        list(APPEND FW_SOURCE_ENTRIES "root:${_fw_pattern}")
    endforeach()
endmacro()

# Target-specific sources. Globs are relative to the target directory and are
# looked up along the inheritance chain, so the child's copy of a file wins.
macro(fw_target_sources)
    foreach(_fw_pattern ${ARGV})
        list(APPEND FW_SOURCE_ENTRIES "target:${_fw_pattern}")
    endforeach()
endmacro()

# Drop shared sources (globs relative to the repository root).
macro(fw_remove)
    foreach(_fw_pattern ${ARGV})
        list(APPEND FW_REMOVALS "${_fw_pattern}")
    endforeach()
endmacro()

# Drop inherited target-specific sources (globs relative to the target
# directory). Only needed to remove a file outright — replacing one happens by
# shadowing.
macro(fw_target_remove)
    foreach(_fw_pattern ${ARGV})
        list(APPEND FW_TARGET_REMOVALS "${_fw_pattern}")
    endforeach()
endmacro()

# Include directories shared by every target, relative to the repository root.
macro(fw_includes)
    foreach(_fw_dir ${ARGV})
        list(APPEND FW_INCLUDE_ENTRIES "root:${_fw_dir}")
    endforeach()
endmacro()

# Target-specific include directories, relative to the target directory. Each one
# expands to every directory in the inheritance chain that actually has it, child
# first.
macro(fw_target_includes)
    foreach(_fw_dir ${ARGV})
        list(APPEND FW_INCLUDE_ENTRIES "target:${_fw_dir}")
    endforeach()
endmacro()

# ---------------------------------------------------------------------------
# Consumption API — for use in the top-level CMakeLists.txt
# ---------------------------------------------------------------------------

# Load a target descriptor and everything it is based on.
macro(fw_load _fw_target)
    set(FW_OVERLAY_NAMES "")
    set(FW_OVERLAY_DIRS "")
    set(FW_SOURCE_ENTRIES "")
    set(FW_INCLUDE_ENTRIES "")
    set(FW_REMOVALS "")
    set(FW_TARGET_REMOVALS "")
    fw_base("${_fw_target}")
endmacro()

# Turn the loaded declarations into FW_SOURCES and FW_INCLUDES.
function(fw_resolve)
    set(_sources "")
    set(_taken "")

    foreach(_entry IN LISTS FW_SOURCE_ENTRIES)
        if(_entry MATCHES "^target:(.+)$")
            set(_pattern "${CMAKE_MATCH_1}")
            foreach(_dir IN LISTS FW_OVERLAY_DIRS)
                file(GLOB_RECURSE _hits
                    RELATIVE "${_dir}" CONFIGURE_DEPENDS "${_dir}/${_pattern}")
                foreach(_rel IN LISTS _hits)
                    # First directory to provide a relative path wins, and the
                    # child is always searched before the target it is based on.
                    if(NOT "${_rel}" IN_LIST _taken)
                        list(APPEND _taken "${_rel}")
                        list(APPEND _sources "${_dir}/${_rel}")
                    endif()
                endforeach()
            endforeach()
        elseif(_entry MATCHES "^root:(.+)$")
            file(GLOB_RECURSE _hits
                CONFIGURE_DEPENDS "${FW_ROOT_DIR}/${CMAKE_MATCH_1}")
            list(APPEND _sources ${_hits})
        endif()
    endforeach()

    foreach(_pattern IN LISTS FW_TARGET_REMOVALS)
        set(_prefixed "")
        foreach(_dir IN LISTS FW_OVERLAY_DIRS)
            list(APPEND _prefixed "${_dir}/${_pattern}")
        endforeach()
        _fw_remove_matching(_sources "${_prefixed}" "fw_target_remove(${_pattern})")
    endforeach()

    foreach(_pattern IN LISTS FW_REMOVALS)
        _fw_remove_matching(_sources "${FW_ROOT_DIR}/${_pattern}"
            "fw_remove(${_pattern})")
    endforeach()

    # file(GLOB) sorts and deduplicates its whole result, so do the same here:
    # what gets built stays independent of the order the patterns were declared in.
    list(SORT _sources)
    list(REMOVE_DUPLICATES _sources)

    set(_includes "")
    foreach(_entry IN LISTS FW_INCLUDE_ENTRIES)
        if(_entry MATCHES "^target:(.+)$")
            set(_rel "${CMAKE_MATCH_1}")
            foreach(_dir IN LISTS FW_OVERLAY_DIRS)
                if(IS_DIRECTORY "${_dir}/${_rel}")
                    list(APPEND _includes "${_dir}/${_rel}")
                endif()
            endforeach()
        elseif(_entry MATCHES "^root:(.+)$")
            list(APPEND _includes "${FW_ROOT_DIR}/${CMAKE_MATCH_1}")
        endif()
    endforeach()

    set(FW_SOURCES "${_sources}" PARENT_SCOPE)
    set(FW_INCLUDES "${_includes}" PARENT_SCOPE)
endfunction()

# Locate a file along the inheritance chain, child first. Used for the few
# per-target files the top-level CMakeLists refers to by name, such as
# linker_symbols.ld.
function(fw_find_file _rel _out)
    foreach(_dir IN LISTS FW_OVERLAY_DIRS)
        if(EXISTS "${_dir}/${_rel}")
            set(${_out} "${_dir}/${_rel}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "'${_rel}' not found in any of the target directories: ${FW_OVERLAY_DIRS}")
endfunction()

# ---------------------------------------------------------------------------
# Internals
# ---------------------------------------------------------------------------

function(_fw_available_targets _out)
    file(GLOB _descriptors "${FW_TARGETS_DIR}/*/target.cmake")
    set(_names "")
    foreach(_descriptor IN LISTS _descriptors)
        get_filename_component(_dir "${_descriptor}" DIRECTORY)
        get_filename_component(_name "${_dir}" NAME)
        list(APPEND _names "${_name}")
    endforeach()
    set(${_out} "${_names}" PARENT_SCOPE)
endfunction()

# Drop every entry of the list named by _list_var that matches one of _globs.
# Removing nothing is an error: it means the pattern is stale or misspelled,
# which would otherwise pass silently and quietly change what gets built.
function(_fw_remove_matching _list_var _globs _context)
    set(_kept "")
    set(_matched FALSE)

    foreach(_path IN LISTS ${_list_var})
        set(_hit FALSE)
        foreach(_glob IN LISTS _globs)
            _fw_glob_to_regex("${_glob}" _regex)
            if(_path MATCHES "${_regex}")
                set(_hit TRUE)
                break()
            endif()
        endforeach()
        if(_hit)
            set(_matched TRUE)
        else()
            list(APPEND _kept "${_path}")
        endif()
    endforeach()

    if(NOT _matched)
        message(FATAL_ERROR "${_context} matched no sources")
    endif()

    set(${_list_var} "${_kept}" PARENT_SCOPE)
endfunction()

# '*' spans directory separators, matching the recursive globbing used above.
function(_fw_glob_to_regex _glob _out)
    string(REGEX REPLACE "([.+^$()])" "\\\\\\1" _regex "${_glob}")
    string(REPLACE "?" "." _regex "${_regex}")
    string(REPLACE "*" ".*" _regex "${_regex}")
    set(${_out} "^${_regex}$" PARENT_SCOPE)
endfunction()
