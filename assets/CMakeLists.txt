project("assets")

# Configuration
set(PYTHON_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/python")
set(NANOPB_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/nanopb/generator/nanopb_generator.py")
set(NANOPB_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(NANOPB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/proto")
set(ASSETS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/image_convert.py")
set(IMAGES_HEADER "${CMAKE_CURRENT_BINARY_DIR}/assets.h")

if(CMAKE_HOST_WIN32)
    set(ASSETS_COMMAND "${PYTHON_DIR}/run_venv.cmd")
else()
    set(ASSETS_COMMAND "${PYTHON_DIR}/run_venv.sh")
endif()

# Prepare the venv in a single prerequisite step: run_venv creates .venv and
# installs packages on first use, which is not safe to do from several
# generation commands racing in a parallel build.
set(VENV_STAMP "${CMAKE_CURRENT_BINARY_DIR}/venv.stamp")
add_custom_command(
    OUTPUT  ${VENV_STAMP}
    COMMAND ${ASSETS_COMMAND} --version
    COMMAND ${CMAKE_COMMAND} -E touch ${VENV_STAMP}
    DEPENDS ${ASSETS_COMMAND}
    WORKING_DIRECTORY ${PYTHON_DIR}
    COMMENT "Preparing Python virtual environment"
)

# Nanopb .proto → .pb.h/.pb.c generation
file(GLOB PROTO_SOURCES "${NANOPB_PROTO_DIR}/*.proto")
foreach(PROTO_FILE ${PROTO_SOURCES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PB_HEADER "${NANOPB_OUTPUT_DIR}/${PROTO_NAME}.pb.h")
    set(PB_SOURCE "${NANOPB_OUTPUT_DIR}/${PROTO_NAME}.pb.c")

    add_custom_command(
        OUTPUT  ${PB_HEADER} ${PB_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${NANOPB_OUTPUT_DIR}
        COMMAND ${ASSETS_COMMAND} ${NANOPB_GENERATOR}
            -D ${NANOPB_OUTPUT_DIR}
            -I ${NANOPB_PROTO_DIR}
            ${PROTO_FILE}
        DEPENDS
            ${PROTO_FILE}
            ${NANOPB_GENERATOR}
            ${ASSETS_COMMAND}
            ${VENV_STAMP}
        WORKING_DIRECTORY ${PYTHON_DIR}
        COMMENT "Generating ${PROTO_NAME}.pb.h/.pb.c from proto"
    )

    list(APPEND PROTO_GENERATED_SOURCES ${PB_SOURCE})
endforeach()

file(GLOB IMAGE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/*.png")

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
            ${VENV_STAMP}
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

add_library(assets STATIC ${ASSETS_SOURCES} ${PROTO_GENERATED_SOURCES})
target_include_directories(assets PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${NANOPB_OUTPUT_DIR}
)
target_include_directories(assets PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../applications/services
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/nanopb
)