include(FetchContent)

function(add_git_dependency NAME)

    set(options)
    set(oneValueArgs
        GIT_REPOSITORY
        GIT_TAG
    )

    cmake_parse_arguments(DEP "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT DEP_GIT_REPOSITORY)
        message(FATAL_ERROR "add_git_dependency missing GIT_REPOSITORY")
    endif()

    if(NOT DEP_GIT_TAG)
        message(FATAL_ERROR "add_git_dependency missing GIT_TAG")
    endif()

    # 缓存路径
    set(FETCHCONTENT_BASE_DIR
        ${CMAKE_SOURCE_DIR}/third_party
        CACHE PATH "third party cache"
    )

    message(STATUS "[ThirdParty] ${NAME}")
    message(STATUS "  repo: ${DEP_GIT_REPOSITORY}")
    message(STATUS "  tag : ${DEP_GIT_TAG}")

    FetchContent_Declare(
        ${NAME}
        GIT_REPOSITORY ${DEP_GIT_REPOSITORY}
        GIT_TAG        ${DEP_GIT_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(${NAME})

endfunction()