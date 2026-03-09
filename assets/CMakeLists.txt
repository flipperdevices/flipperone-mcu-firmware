project("assets")

# Configuration
set(PYTHON_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/python")
set(ASSETS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/image_convert.py")
set(IMAGES_HEADER "${CMAKE_CURRENT_BINARY_DIR}/assets.h")

file(GLOB IMAGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/*.png")

if(CMAKE_HOST_WIN32)
    set(ASSETS_COMMAND "${PYTHON_DIR}/run_venv.cmd")
else()
    set(ASSETS_COMMAND "${PYTHON_DIR}/run_venv.sh")
endif()

# Generate a C source file for each image
foreach(IMAGE_SOURCE ${IMAGE_SOURCES})
    get_filename_component(IMAGE_NAME ${IMAGE_SOURCE} NAME_WE)
    set(ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${IMAGE_NAME}.c")

    add_custom_command(
        OUTPUT  ${ASSETS_SOURCE}
        COMMAND ${ASSETS_COMMAND} ${ASSETS_SCRIPT} ${IMAGE_SOURCE} ${ASSETS_SOURCE} ${IMAGE_NAME}
        DEPENDS
            ${IMAGE_SOURCE}
            ${ASSETS_COMMAND}
            ${ASSETS_SCRIPT}
        WORKING_DIRECTORY ${PYTHON_DIR}
        COMMENT "Generating ${IMAGE_NAME}.c"
    )

    list(APPEND ASSETS_SOURCES ${ASSETS_SOURCE})
    list(APPEND IMAGE_NAMES    ${IMAGE_NAME})
endforeach()

# Generate assets.h with extern declarations for all images
file(WRITE ${IMAGES_HEADER}
    "#pragma once\n"
    "#include <gui/image.h>\n\n"
)
foreach(IMAGE ${IMAGE_NAMES})
    file(APPEND ${IMAGES_HEADER} "extern const Image ${IMAGE};\n")
endforeach()

add_library(assets STATIC ${ASSETS_SOURCES})
target_include_directories(assets PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../applications/services)