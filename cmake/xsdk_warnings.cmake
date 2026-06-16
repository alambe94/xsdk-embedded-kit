include_guard(GLOBAL)

add_library(xe_sdk_warnings INTERFACE)
add_library(xsdk::warnings ALIAS xe_sdk_warnings)

if(MSVC)
    target_compile_options(xe_sdk_warnings INTERFACE /W4)
    if(xSDK_WARNINGS_AS_ERRORS)
        target_compile_options(xe_sdk_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(xe_sdk_warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(xSDK_WARNINGS_AS_ERRORS)
        target_compile_options(xe_sdk_warnings INTERFACE -Werror)
    endif()
endif()

function(xsdk_apply_target_warnings target)
    target_link_libraries(${target} PRIVATE
        xsdk::warnings
        xsdk::instrumentation
    )
endfunction()
