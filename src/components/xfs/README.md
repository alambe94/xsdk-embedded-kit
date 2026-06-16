# xfs

FAT32 filesystem for embedded block devices.

**CMake target:** `xsdk::xfs`

---

## What It Provides

- FAT32 mount, read, and write over any block device
- Sector-level cache to reduce physical I/O
- Block device abstraction (`xFS_Block_Driver_t`) - plug in any storage
  backend: SD card, SPI flash, eMMC, or RAM disk
- No dynamic allocation in core filesystem paths
- A RAM disk reference port is included under `port/` for testing

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xfs)
```

---

## Usage

### 1. Implement a block device driver

```c
static xRETURN_t my_read(void *ctx, uint32_t lba, uint8_t *buf, uint32_t count)
{
    // read `count` sectors from `lba` into `buf`
}

static xRETURN_t my_write(void *ctx, uint32_t lba, const uint8_t *buf, uint32_t count)
{
    // write `count` sectors at `lba` from `buf`
}

static xRETURN_t my_flush(void *ctx)
{
    // flush write cache if applicable
}

static xFS_Block_Driver_t my_driver = {
    .init         = my_init,
    .read_sector  = my_read,
    .write_sector = my_write,
    .flush        = my_flush,
};
```

### 2. Initialize and mount

```c
static xFS_Context_t g_fs;

xFS_Init(&g_fs, &my_driver, &my_driver_ctx);
xFS_Mount(&g_fs);

// ... read and write files ...

xFS_Unmount(&g_fs);
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xFS_Init` | Bind a block driver to a filesystem context |
| `xFS_Trace_Init` | Attach or detach optional xTrace instrumentation |
| `xFS_Mount` | Parse the BPB and validate the FAT32 volume |
| `xFS_Unmount` | Flush caches and release the volume |

Directory and file entry APIs are in `include/xfs_fat32_directory.h` and
`include/xfs_fat32.h`.

---

## Pairing with xusb MSC

xfs pairs naturally with the `xusb` MSC class driver. The MSC driver calls
into an application-supplied storage backend; the application forwards those
calls to xfs. This gives a USB mass-storage device backed by any xfs-compatible
block device.

---

## License

MIT - see [LICENSE](../../../LICENSE).
