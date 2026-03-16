include(FetchContent)

function(add_git_dependency NAME)

    set(options)
    set(oneValueArgs
        GIT_REPOSITORY
        GIT_TAG
        GIT_SHALLOW
        WORK_DIR
    )

    cmake_parse_arguments(DEP "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT DEP_GIT_REPOSITORY)
        message(FATAL_ERROR "add_git_dependency missing GIT_REPOSITORY")
    endif()

    if(NOT DEP_GIT_TAG)
        message(FATAL_ERROR "add_git_dependency missing GIT_TAG")
    endif()

    if(NOT DEFINED DEP_GIT_SHALLOW)
        set(DEP_GIT_SHALLOW TRUE)
    endif()

    if(NOT DEFINED DEP_WORK_DIR)
        set(DEP_WORK_DIR ${PROJECT_BINARY_DIR}/${NAME})
        message(WARNING "add_git_dependency missing DEP_WORK_DIR, default ${DEP_WORK_DIR}")
    endif ()

    message(STATUS "[ThirdParty] ${NAME}")
    message(STATUS "  workdir: ${DEP_WORK_DIR}")
    message(STATUS "  repo: ${DEP_GIT_REPOSITORY}")
    message(STATUS "  tag : ${DEP_GIT_TAG}")
    message(STATUS "  shallow : ${DEP_GIT_SHALLOW}")

    # 检查源码目录是否已经存在且不为空（包含 .git 目录通常意味着已经下载）
    if(EXISTS "${DEP_WORK_DIR}/.git")
        message(STATUS "  Source directory ${DEP_WORK_DIR} already exists, skipping download.")
        FetchContent_Declare(
            ${NAME}
            SOURCE_DIR     ${DEP_WORK_DIR}
            BINARY_DIR     ${PROJECT_BINARY_DIR}/_deps/${NAME}-build
            SUBBUILD_DIR   ${PROJECT_BINARY_DIR}/_deps/${NAME}-subbuild
        )
    else()
        FetchContent_Declare(
            ${NAME}
            GIT_REPOSITORY ${DEP_GIT_REPOSITORY}
            GIT_TAG        ${DEP_GIT_TAG}
            GIT_SHALLOW    ${DEP_GIT_SHALLOW}
            SOURCE_DIR     ${DEP_WORK_DIR}
            BINARY_DIR     ${PROJECT_BINARY_DIR}/_deps/${NAME}-build
            SUBBUILD_DIR   ${PROJECT_BINARY_DIR}/_deps/${NAME}-subbuild
            CMAKE_ARGS
                -DCMAKE_INSTALL_PREFIX=${DEP_WORK_DIR}/install
        )
    endif()
    
    FetchContent_MakeAvailable(${NAME})
endfunction()