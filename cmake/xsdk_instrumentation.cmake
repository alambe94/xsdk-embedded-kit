include_guard(GLOBAL)

add_library(xe_sdk_instrumentation INTERFACE)
add_library(xsdk::instrumentation ALIAS xe_sdk_instrumentation)

if(xSDK_SANITIZERS)
    if(MSVC)
        message(WARNING "xSDK_SANITIZERS has no effect on MSVC.")
    else()
        target_compile_options(xe_sdk_instrumentation INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(xe_sdk_instrumentation INTERFACE
            -fsanitize=address,undefined
        )
    endif()
endif()

if(xSDK_COVERAGE)
    if(MSVC)
        message(WARNING "xSDK_COVERAGE has no effect on MSVC.")
    else()
        target_compile_options(xe_sdk_instrumentation INTERFACE --coverage)
        target_link_options(xe_sdk_instrumentation INTERFACE --coverage)
    endif()
endif()
