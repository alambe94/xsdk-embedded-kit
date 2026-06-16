# xrtos

Deterministic bitmap-scheduler RTOS with semaphore, mutex (Priority Inheritance), queue, event, notify, and software timer.

**CMake target:** `xsdk::xrtos`

---

## Design Principles

- **No heap usage.** All task, queue, mutex, semaphore, event, notification, and timer contexts are statically allocated by the application.
- **Deterministic Scheduling.** Uses a bitmap scheduler for $O(1)$ task scheduling, supporting up to 32 priorities.
- **Priority Inheritance.** Mutexes support Priority Inheritance (PI) to prevent priority inversion.
- **Stack Safety.** Supports stack watermark monitoring and canary checks (`xRTOS_STACK_CANARY`) to detect overflow.
- **CPU Profiling.** Built-in optional CPU stats logging per task.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xrtos)
```

The `xsdk::xrtos` target automatically pulls in `xsdk::xutil` and `xsdk::xtrace` dependencies.

---

## Usage - Task Creation

```c
#include "xrtos_core.h"
#include "xrtos_task.h"

// Define task stack size and allocate memory
#define MY_TASK_STACK_WORDS  256
static uint32_t g_my_task_stack[MY_TASK_STACK_WORDS];
static xRTOS_Task_Context_t g_my_task_ctx;

static void my_task_entry(void *arg)
{
    (void)arg;
    while (true) {
        // Task logic
        xRTOS_Task_Yield();
    }
}

void app_init(void)
{
    xRTOS_Task_Config_t cfg = {
        .task_id     = 1U,
        .priority    = 5U,
        .entry       = my_task_entry,
        .entry_arg   = NULL,
        .stack_mem   = g_my_task_stack,
        .stack_words = MY_TASK_STACK_WORDS,
        .name        = "MyTask",
    };

    xRETURN_t result = xRTOS_Task_Create(&g_my_task_ctx, &cfg);
    if (result == xRETURN_OK) {
        // Task successfully created
    }
}
```

---

## Usage - Mutex with Priority Inheritance

```c
#include "xrtos_mutex.h"

static xRTOS_Mutex_Context_t g_mutex;

void app_mutex_init(void)
{
    xRTOS_Mutex_Create(&g_mutex);
}

void shared_resource_access(void)
{
    if (xRTOS_Mutex_Lock(&g_mutex, xRTOS_TIMEOUT_INFINITE) == xRETURN_OK) {
        // Critical section
        xRTOS_Mutex_Unlock(&g_mutex);
    }
}
```

---

## Key Public APIs

| Feature | Function | Description |
|---|---|---|
| **Lifecycle** | `xRTOS_Init` | Initialize kernel scheduler state |
| | `xRTOS_Start` | Start the scheduler and launch the highest priority task |
| **Tasks** | `xRTOS_Task_Create` | Statically register and initialize a task |
| | `xRTOS_Task_Yield` | Cooperatively yield CPU to another ready task of same priority |
| | `xRTOS_Task_Exit` | Self-terminate the calling task |
| | `xRTOS_Task_Get_Stack_Watermark` | Check the maximum free stack depth |
| | `xRTOS_Task_Get_CPU_Stats` | Query task execution runtime statistics |
| **Mutexes** | `xRTOS_Mutex_Create` | Statically initialize a mutex with Priority Inheritance |
| | `xRTOS_Mutex_Lock` | Acquire a lock (blocking call with timeout support) |
| | `xRTOS_Mutex_Unlock` | Release a lock |
| **Semaphores** | `xRTOS_Sem_Create` | Initialize a counting semaphore |
| | `xRTOS_Sem_Post` | Post (signal) the semaphore |
| | `xRTOS_Sem_Wait` | Wait/acquire the semaphore |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
