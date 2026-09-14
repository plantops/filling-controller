cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# CI exports PROJECT_VER=#<fw run>. Bind it explicitly so esp_app_desc_t and
# both OP/DEV HMI report the same firmware number as the flash bundle VERSION.
if(DEFINED ENV{PROJECT_VER})
    set(PROJECT_VER "$ENV{PROJECT_VER}")
endif()

project(sp01_filling_controller)
