# xusb_common

Common USB definitions and shared helpers for the xSDK USB stacks (device and host).

**CMake target:** `xsdk::xusb_common`

---

## What It Provides

- **`include/xusb_std.h`** - Shared USB specification definitions, descriptors, standard request structures, and helper macros.
- **Shared test helpers** - Common mock and verification wrappers for USB transport testing.

---

## Integration

Typically, `xusb_common` is pulled in automatically as a dependency when linking `xsdk::xusbd` or `xsdk::xusbh`.

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xusbd)
```

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
