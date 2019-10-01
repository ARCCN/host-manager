target_sources(runos 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HostManager.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/HostManager.hpp
)

target_sources(runos_rest 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HostManagerRest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/include/HostManager.hpp
)
