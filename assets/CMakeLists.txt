project("assets")

# Configuration
set(PYTHON_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/python")
set(NANOPB_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/nanopb/generator/nanopb_generator.py")
set(NANOPB_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(NANOPB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/proto")
set(ASSETS_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/image_convert.py")
set(ANIMATION_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/animation_convert.py")
set(IMAGES_HEADER "${CMAKE_CURRENT_BINARY_DIR}/assets.h")

# Host interpreter for the generators below. Chosen once at configure and
# overridable with -DPython3_EXECUTABLE=..., instead of whatever python3 is first
# in PATH when the build happens to run. 3.10 is the floor of the pinned packages.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

# The venv lives in the build tree: a clean build starts from a clean venv and
# the source tree stays untouched.
set(VENV_DIR "${CMAKE_CURRENT_BINARY_DIR}/.venv")
if(CMAKE_HOST_WIN32)
    set(VENV_PYTHON "${VENV_DIR}/Scripts/python.exe")
else()
    set(VENV_PYTHON "${VENV_DIR}/bin/python")
endif()
set(ASSETS_COMMAND "${VENV_PYTHON}")

# Prepare the venv in a single prerequisite step. It is rebuilt from scratch
# when requirements.txt changes or another interpreter is configured (the marker
# below is only rewritten when its content changes). The stamp lives inside the
# venv, so deleting the venv by hand also triggers a rebuild.
set(VENV_REQUIREMENTS "${PYTHON_DIR}/requirements.txt")
set(VENV_STAMP "${VENV_DIR}/venv.stamp")
set(VENV_INTERPRETER "${CMAKE_CURRENT_BINARY_DIR}/venv_interpreter.txt")
file(GENERATE OUTPUT ${VENV_INTERPRETER} CONTENT "${Python3_EXECUTABLE}\n")
add_custom_command(
    OUTPUT  ${VENV_STAMP}
    # --clear: a venv left over from another interpreter is not reusable;
    # --copies: a symlinked venv breaks on macOS once the interpreter is updated
    COMMAND ${Python3_EXECUTABLE} -m venv --clear --copies ${VENV_DIR}
    COMMAND ${VENV_PYTHON} -m pip install --disable-pip-version-check -r ${VENV_REQUIREMENTS}
    COMMAND ${CMAKE_COMMAND} -E touch ${VENV_STAMP}
    DEPENDS ${VENV_REQUIREMENTS} ${VENV_INTERPRETER}
    COMMENT "Preparing Python virtual environment in ${VENV_DIR}"
    VERBATIM
)

# Nanopb .proto → .pb.h/.pb.c generation
file(GLOB PROTO_SOURCES CONFIGURE_DEPENDS "${NANOPB_PROTO_DIR}/*.proto")
foreach(PROTO_FILE ${PROTO_SOURCES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PB_HEADER "${NANOPB_OUTPUT_DIR}/${PROTO_NAME}.pb.h")
    set(PB_SOURCE "${NANOPB_OUTPUT_DIR}/${PROTO_NAME}.pb.c")

    add_custom_command(
        OUTPUT  ${PB_HEADER} ${PB_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NANOPB_OUTPUT_DIR}
        # -q drops the "Writing to ..." chatter; it would also silence the warning
        # about .options patterns that match nothing, so make those an error instead.
        COMMAND ${ASSETS_COMMAND} ${NANOPB_GENERATOR}
            --quiet --error-on-unmatched
            -D ${NANOPB_OUTPUT_DIR}
            -I ${NANOPB_PROTO_DIR}
            ${PROTO_FILE}
        DEPENDS
            ${PROTO_FILE}
            ${NANOPB_GENERATOR}
            ${VENV_STAMP}
        COMMENT "Generating ${PROTO_NAME}.pb.h/.pb.c from proto"
    )

    list(APPEND PROTO_GENERATED_SOURCES ${PB_SOURCE})
endforeach()

file(GLOB IMAGE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*/*.png")

# Generate a C source file for each image
foreach(IMAGE_SOURCE ${IMAGE_SOURCES})
    get_filename_component(IMAGE_NAME ${IMAGE_SOURCE} NAME_WE)
    set(ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${IMAGE_NAME}.c")

    add_custom_command(
        OUTPUT  ${ASSETS_SOURCE}
        COMMAND ${ASSETS_COMMAND} ${ASSETS_SCRIPT} ${IMAGE_SOURCE} ${ASSETS_SOURCE} ${IMAGE_NAME}
        DEPENDS
            ${IMAGE_SOURCE}
            ${ASSETS_SCRIPT}
            ${VENV_STAMP}
        COMMENT "Generating ${IMAGE_NAME}.c"
    )

    list(APPEND ASSETS_SOURCES ${ASSETS_SOURCE})
    list(APPEND IMAGE_NAMES    ${IMAGE_NAME})
endforeach()

# Each folder under animations/ is one animated image: its PNG frames in name
# order plus an optional meta.json (see scripts/animation_convert.py).
file(GLOB ANIMATION_DIRS LIST_DIRECTORIES true CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/animations/*")

foreach(ANIMATION_DIR ${ANIMATION_DIRS})
    if(NOT IS_DIRECTORY ${ANIMATION_DIR})
        continue()
    endif()
    get_filename_component(ANIMATION_NAME ${ANIMATION_DIR} NAME)
    set(ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${ANIMATION_NAME}.c")
    file(GLOB ANIMATION_INPUTS CONFIGURE_DEPENDS "${ANIMATION_DIR}/*.png" "${ANIMATION_DIR}/meta.json")

    add_custom_command(
        OUTPUT  ${ASSETS_SOURCE}
        COMMAND ${ASSETS_COMMAND} ${ANIMATION_SCRIPT} ${ANIMATION_DIR} ${ASSETS_SOURCE} ${ANIMATION_NAME}
        DEPENDS
            ${ANIMATION_INPUTS}
            ${ANIMATION_SCRIPT}
            ${ASSETS_SCRIPT}
            ${VENV_STAMP}
        COMMENT "Generating ${ANIMATION_NAME}.c"
    )

    list(APPEND ASSETS_SOURCES ${ASSETS_SOURCE})
    list(APPEND IMAGE_NAMES    ${ANIMATION_NAME})
endforeach()

# Generate assets.h with extern declarations for all images and animations
set(IMAGES_HEADER_CONTENT "#pragma once\n#include <gui/image.h>\n\n")
foreach(IMAGE ${IMAGE_NAMES})
    string(APPEND IMAGES_HEADER_CONTENT "extern const Image ${IMAGE};\n")
endforeach()
file(GENERATE OUTPUT ${IMAGES_HEADER} CONTENT "${IMAGES_HEADER_CONTENT}")

add_library(assets STATIC ${ASSETS_SOURCES} ${PROTO_GENERATED_SOURCES})
target_include_directories(assets PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${NANOPB_OUTPUT_DIR}
)
target_include_directories(assets PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../applications/services
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/nanopb
)