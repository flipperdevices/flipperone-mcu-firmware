project("assets")

file(GLOB IMAGE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.png
    # ${CMAKE_CURRENT_SOURCE_DIR}/phrase/*.png
    # ${CMAKE_CURRENT_SOURCE_DIR}/screensaver/*.png
)

if(CMAKE_HOST_WIN32)
    set(ASSETS_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/python/build.cmd)
else()
    set(ASSETS_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/python/build.sh)
endif()

set(ASSETS_SOURCES)

foreach(IMAGE_SOURCE ${IMAGE_SOURCES})
    get_filename_component(IMAGE_NAME ${IMAGE_SOURCE} NAME_WE)
    set(ASSETS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${IMAGE_NAME}.c")

    add_custom_command(
        OUTPUT ${ASSETS_SOURCE}
        COMMAND ${ASSETS_COMMAND} --ofmt C --cf L8 -o ${CMAKE_CURRENT_BINARY_DIR} ${IMAGE_SOURCE}
        DEPENDS ${IMAGE_SOURCE} ${ASSETS_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
        COMMENT "Generating C file for ${IMAGE_NAME}"
    )

    list(APPEND ASSETS_SOURCES ${ASSETS_SOURCE})
endforeach()

add_library(assets STATIC ${ASSETS_SOURCES})

target_include_directories(assets PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(assets lvgl)