#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"

#include "xfs_block_ramdisk.h"
#include "xfs_core.h"
#include "xfs_format.h"
#include "xfs_file.h"
#include "xfs_directory.h"

#include "uart.h"

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
#include "xtrace.h"
#include "xrtos_tick.h"

#define TRACE_BUF_BYTES 16384U
static uint8_t s_trace_buf[TRACE_BUF_BYTES];
static xTRACE_Context_t s_trace_ctx;

static xRETURN_t trace_write(void *ctx, const uint8_t *buf, size_t len, size_t *written)
{
    (void)ctx;
    for (uint32_t i = 0U; i < len; i++)
    {
        uart_trace_putb(buf[i]);
    }
    *written = len;
    return xRETURN_OK;
}
static const xTRACE_Transport_t s_trace_transport = {.write = trace_write};

static xTRACE_Time_t rtos_tick_fn(void *ctx)
{
    (void)ctx;
    return (xTRACE_Time_t)xRTOS_Tick_Get();
}
#endif // QEMU_TRACE_ENABLED

// ---- xFS test state --------------------------------------------------------

#define RAMDISK_SECTOR_SIZE  512U
#define RAMDISK_SECTOR_COUNT 128U
#define RAMDISK_TOTAL_BYTES  (RAMDISK_SECTOR_SIZE * RAMDISK_SECTOR_COUNT)

static uint8_t s_disk[RAMDISK_TOTAL_BYTES];
static xFS_RAMDisk_Context_t s_ramdisk;
static xFS_Context_t s_fs;

static uint32_t s_pass;
static uint32_t s_fail;

#define CHECK(label, expr)                                                                                                                 \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xRETURN_xFS_t _r = (expr);                                                                                                         \
        if (_r == xRETURN_xFS_OK)                                                                                                          \
        {                                                                                                                                  \
            uart_puts("  PASS: " label "\n");                                                                                              \
            s_pass++;                                                                                                                      \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            uart_puts("  FAIL: " label " (err=");                                                                                          \
            uart_puti((uint32_t)_r);                                                                                                       \
            uart_puts(")\n");                                                                                                              \
            s_fail++;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

#define CHECK_TRUE(label, expr)                                                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        bool _v = (expr);                                                                                                                  \
        if (_v)                                                                                                                            \
        {                                                                                                                                  \
            uart_puts("  PASS: " label "\n");                                                                                              \
            s_pass++;                                                                                                                      \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            uart_puts("  FAIL: " label "\n");                                                                                              \
            s_fail++;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

// ---- Phase 1: Format and mount ---------------------------------------------

static void phase1_format_mount(void)
{
    uart_puts("--- Phase 1: Format and Mount ---\n");

    xFS_Format_Config_t cfg;
    CHECK("Format_Config_Default", xFS_Format_Config_Default(&cfg, RAMDISK_SECTOR_COUNT));

    CHECK("Format_FAT32", xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &s_ramdisk, &cfg));

    CHECK("Init", (xRETURN_xFS_t)xFS_Init(&s_fs, &gxFS_RAMDisk_Driver, &s_ramdisk));
#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xFS_Trace_Init(&s_fs, &s_trace_ctx);
#endif

    CHECK("Mount", (xRETURN_xFS_t)xFS_Mount(&s_fs));

    xFS_Volume_Info_t info;
    CHECK("Volume_Get_Info", (xRETURN_xFS_t)xFS_Volume_Get_Info(&s_fs, &info));

    uart_puts("  Volume: ");
    uart_puts(info.label);
    uart_puts("  Clusters free=");
    uart_puti(info.free_clusters);
    uart_puts("/");
    uart_puti(info.total_clusters);
    uart_puts("\n");
}

// ---- Phase 2: File create / write / read -----------------------------------

static void phase2_file_write_read(void)
{
    uart_puts("--- Phase 2: File Write/Read ---\n");

    static const char payload[] = "Hello, xFS on Cortex-R5!";
    xFS_File_t f;

    CHECK("File_Create", (xRETURN_xFS_t)xFS_File_Create(&s_fs, &f, "/HELLO.TXT"));

    uint32_t written = 0U;
    CHECK("File_Write", (xRETURN_xFS_t)xFS_File_Write(&f, (const uint8_t *)payload, (uint32_t)sizeof(payload) - 1U, &written));
    CHECK_TRUE("Write count matches", written == (uint32_t)(sizeof(payload) - 1U));

    CHECK("File_Close (write)", (xRETURN_xFS_t)xFS_File_Close(&f));

    // Re-open and read back
    CHECK("File_Open", (xRETURN_xFS_t)xFS_File_Open(&s_fs, &f, "/HELLO.TXT"));

    static uint8_t rbuf[64];
    uint32_t bytes_read = 0U;
    memset(rbuf, 0, sizeof(rbuf));
    CHECK("File_Read", (xRETURN_xFS_t)xFS_File_Read(&f, rbuf, written, &bytes_read));
    CHECK_TRUE("Read count matches", bytes_read == written);
    CHECK_TRUE("Content verified", memcmp(rbuf, payload, (size_t)bytes_read) == 0);

    CHECK("File_Close (read)", (xRETURN_xFS_t)xFS_File_Close(&f));
}

// ---- Phase 3: Large file spanning two clusters -----------------------------

static void phase3_large_file(void)
{
    uart_puts("--- Phase 3: Large File (multi-cluster) ---\n");

    // 700 bytes spans 2 clusters of 512 bytes each
    static uint8_t wbuf[700];
    for (uint32_t i = 0U; i < 700U; i++)
    {
        wbuf[i] = (uint8_t)(i & 0xFFU);
    }

    xFS_File_t f;
    CHECK("File_Create (large)", (xRETURN_xFS_t)xFS_File_Create(&s_fs, &f, "/LARGE.BIN"));

    uint32_t written = 0U;
    CHECK("File_Write (700 bytes)", (xRETURN_xFS_t)xFS_File_Write(&f, wbuf, 700U, &written));
    CHECK_TRUE("Write count 700", written == 700U);
    CHECK("File_Close", (xRETURN_xFS_t)xFS_File_Close(&f));

    // Read back
    static uint8_t rbuf[700];
    memset(rbuf, 0, sizeof(rbuf));
    CHECK("File_Open (large)", (xRETURN_xFS_t)xFS_File_Open(&s_fs, &f, "/LARGE.BIN"));
    uint32_t bytes_read = 0U;
    CHECK("File_Read (700 bytes)", (xRETURN_xFS_t)xFS_File_Read(&f, rbuf, 700U, &bytes_read));
    CHECK_TRUE("Read count 700", bytes_read == 700U);
    CHECK_TRUE("Content verified", memcmp(rbuf, wbuf, 700U) == 0);
    CHECK("File_Close (large)", (xRETURN_xFS_t)xFS_File_Close(&f));
}

// ---- Phase 4: File seek ----------------------------------------------------

static void phase4_seek(void)
{
    uart_puts("--- Phase 4: File Seek ---\n");

    xFS_File_t f;
    CHECK("File_Open for seek", (xRETURN_xFS_t)xFS_File_Open(&s_fs, &f, "/HELLO.TXT"));

    CHECK("File_Seek to 7", (xRETURN_xFS_t)xFS_File_Seek(&f, 7U));

    uint32_t pos = 0U;
    CHECK("File_Tell", (xRETURN_xFS_t)xFS_File_Tell(&f, &pos));
    CHECK_TRUE("Position is 7", pos == 7U);

    uint8_t buf[4];
    uint32_t nread = 0U;
    CHECK("File_Read 4 bytes at offset 7", (xRETURN_xFS_t)xFS_File_Read(&f, buf, 4U, &nread));
    // "Hello, xFS on Cortex-R5!" - offset 7 = "xFS "
    CHECK_TRUE("Content at offset 7", (nread == 4U) && (buf[0] == 'x') && (buf[1] == 'F') && (buf[2] == 'S') && (buf[3] == ' '));

    CHECK("File_Close (seek)", (xRETURN_xFS_t)xFS_File_Close(&f));
}

// ---- Phase 5: Directory create and list ------------------------------------

static void phase5_directory(void)
{
    uart_puts("--- Phase 5: Directories ---\n");

    CHECK("Directory_Create /LOGS", (xRETURN_xFS_t)xFS_Directory_Create(&s_fs, "/LOGS"));

    // Create a file inside the directory
    xFS_File_t f;
    CHECK("File_Create /LOGS/A.TXT", (xRETURN_xFS_t)xFS_File_Create(&s_fs, &f, "/LOGS/A.TXT"));
    uint32_t w = 0U;
    uint8_t d = 0xABU;
    CHECK("File_Write 1 byte", (xRETURN_xFS_t)xFS_File_Write(&f, &d, 1U, &w));
    CHECK("File_Close /LOGS/A.TXT", (xRETURN_xFS_t)xFS_File_Close(&f));

    // Flush to ensure all directory entries are committed before listing
    CHECK("xFS_Sync", (xRETURN_xFS_t)xFS_Sync(&s_fs));

    // List root directory - should see HELLO.TXT, LARGE.BIN, LOGS
    xFS_Directory_t dir;
    CHECK("Directory_Open /", (xRETURN_xFS_t)xFS_Directory_Open(&s_fs, &dir, "/"));

    uint32_t entry_count = 0U;
    bool has_entry = false;
    xFS_Directory_Entry_t entry;
    while (true)
    {
        xRETURN_xFS_t r = (xRETURN_xFS_t)xFS_Directory_Read(&dir, &entry, &has_entry);
        if ((r != xRETURN_xFS_OK) || !has_entry)
        {
            break;
        }
        uart_puts("  entry: ");
        uart_puts(entry.name);
        uart_puts(entry.is_directory ? " [DIR]\n" : " [FILE]\n");
        entry_count++;
    }
    CHECK_TRUE("Root has 3 entries", entry_count == 3U);
    CHECK("Directory_Close /", (xRETURN_xFS_t)xFS_Directory_Close(&dir));

    // List /LOGS - should see A.TXT
    CHECK("Directory_Open /LOGS", (xRETURN_xFS_t)xFS_Directory_Open(&s_fs, &dir, "/LOGS"));
    entry_count = 0U;
    has_entry = false;
    while (true)
    {
        xRETURN_xFS_t r = (xRETURN_xFS_t)xFS_Directory_Read(&dir, &entry, &has_entry);
        if ((r != xRETURN_xFS_OK) || !has_entry)
        {
            break;
        }
        entry_count++;
    }
    CHECK_TRUE("/LOGS has at least 1 entry", entry_count >= 1U);
    CHECK("Directory_Close /LOGS", (xRETURN_xFS_t)xFS_Directory_Close(&dir));
}

// ---- Phase 6: Delete -------------------------------------------------------

static void phase6_delete(void)
{
    uart_puts("--- Phase 6: Delete ---\n");

    CHECK("File_Delete /HELLO.TXT", (xRETURN_xFS_t)xFS_File_Delete(&s_fs, "/HELLO.TXT"));

    // Verify it's gone
    xFS_File_t f;
    xRETURN_xFS_t r = (xRETURN_xFS_t)xFS_File_Open(&s_fs, &f, "/HELLO.TXT");
    CHECK_TRUE("File_Open after delete returns NOT_FOUND", r == xRETURN_xERR_xFS_NOT_FOUND);

    // Delete /LOGS/A.TXT then /LOGS
    CHECK("File_Delete /LOGS/A.TXT", (xRETURN_xFS_t)xFS_File_Delete(&s_fs, "/LOGS/A.TXT"));
    CHECK("Directory_Delete /LOGS", (xRETURN_xFS_t)xFS_Directory_Delete(&s_fs, "/LOGS"));
}

// ---- xRTOS task entry ------------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[128];
static xRTOS_Task_Context_t s_test_ctx;
static uint32_t s_test_stack[512];

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
    }
}

static void test_entry(void *arg)
{
    (void)arg;

    uart_puts("\n--- xFS Cortex-R5 QEMU Smoke Test ---\n\n");

    // Initialise the RAM disk context (storage already BSS-zero at startup)
    s_ramdisk.storage = s_disk;
    s_ramdisk.sector_size = RAMDISK_SECTOR_SIZE;
    s_ramdisk.sector_count = RAMDISK_SECTOR_COUNT;

    s_pass = 0U;
    s_fail = 0U;

    phase1_format_mount();
    phase2_file_write_read();
    phase3_large_file();
    phase4_seek();
    phase5_directory();
    phase6_delete();

    uart_puts("\n=== xFS Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XFS TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XFS TESTS FAILED.\n");
    }

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xTRACE_Flush(&s_trace_ctx);
#endif

    for (;;)
    {
    }
}

int main(void)
{
    uart_init();

    xRTOS_Kernel_Init(&s_kernel, &xrtos_arm_r5_port_ops);

    xRTOS_Task_Config_t idle_cfg = {.task_id = xRTOS_IDLE_TASK_ID,
                                    .priority = xRTOS_IDLE_PRIORITY,
                                    .entry = idle_entry,
                                    .entry_arg = NULL,
                                    .stack_mem = s_idle_stack,
                                    .stack_words = 128U};
    xRTOS_Task_Create(&s_idle_ctx, &idle_cfg);

    xRTOS_Task_Config_t test_cfg = {
        .task_id = 1U, .priority = 2U, .entry = test_entry, .entry_arg = NULL, .stack_mem = s_test_stack, .stack_words = 512U};
    xRTOS_Task_Create(&s_test_ctx, &test_cfg);

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    {
        xTRACE_Config_t tcfg;
        tcfg.buffer = s_trace_buf;
        tcfg.capacity_bytes = TRACE_BUF_BYTES;
        tcfg.timestamp_fn = rtos_tick_fn;
        tcfg.timestamp_ctx = NULL;
        tcfg.timestamp_hz = 1000U;
        tcfg.is_enabled = true;
        (void)xTRACE_Init(&s_trace_ctx, &tcfg, &s_trace_transport, NULL);
        (void)xRTOS_Kernel_Trace_Init(&s_trace_ctx);
    }
#endif

    xRTOS_Kernel_Start();

    return 0;
}
