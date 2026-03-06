project("assets")

file(GLOB IMAGE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.png
)

set(ASSETS_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/scripts/image_convert.py)

if(CMAKE_HOST_WIN32)
    set(ASSETS_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/python/build.cmd)
else()
    set(ASSETS_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/python/build.sh)
endif()

set(IMAGES_HEADER "assets.h")
set(IMAGES "")

foreach(IMAGE_SOURCE ${IMAGE_SOURCES})
    get_filename_component(IMAGE_NAME ${IMAGE_SOURCE} NAME_WE)
    set(ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${IMAGE_NAME}.c")

    add_custom_command(
        OUTPUT ${ASSETS_SOURCE}
        COMMAND ${ASSETS_COMMAND} ${ASSETS_SCRIPT} ${IMAGE_SOURCE} ${ASSETS_SOURCE} ${IMAGE_NAME}
        DEPENDS ${IMAGE_SOURCE} ${ASSETS_COMMAND} ${ASSETS_SCRIPT} ${CMAKE_CURRENT_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
        COMMENT "Generating C file for ${IMAGE_NAME}"
    )

    list(APPEND ASSETS_SOURCES ${ASSETS_SOURCE})
    list(APPEND IMAGES "extern const Image ${IMAGE_NAME};")
endforeach()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${IMAGES_HEADER} "// This file is auto-generated. Do not edit.\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMAGES_HEADER} "#pragma once\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMAGES_HEADER} "#include <gui/image.h>\n\n")
foreach(IMAGE ${IMAGES})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMAGES_HEADER} "${IMAGE};\n")
endforeach()

add_library(assets STATIC ${ASSETS_SOURCES})

target_include_directories(assets PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# TODO: fixme
target_include_directories(assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../applications/services)