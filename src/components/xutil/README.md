# xutil

SDK-wide utility layer - portable types, assertions, and trace logging shared
across all other xSDK modules.

**CMake target:** `xsdk::xutil`

---

## What It Provides

- **`xRETURN_t`** - common return-code type used by all SDK APIs. Every SDK
  function returns `xRETURN_OK` on success or a module-specific error code on
  failure. No error is silently swallowed.
- **Trace / logging** - lightweight, compile-time-configurable log output.
  Each module sets its own log level (`0` = off, `1` = error codes only,
  `2` = full printf). The backend is a single function pointer that the
  application can redirect to UART, ITM, or any other sink.
- **Shared portable types** - stdint-based typedefs and helper macros used
  internally by all SDK modules.

---

## Integration

xutil is a dependency of all other xSDK modules. Linking any xSDK module
automatically pulls in xutil - no explicit `target_link_libraries` call is
needed unless you want xutil on its own.

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xutil)
```

---

## Status

xutil is the shared foundation for the SDK. The public API surface is small
and intentionally stable. Additional utilities (string helpers, memory
utilities, assert macros) will be added here as the SDK grows.

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
