#include <assert.h>    // [中文] 引入断言宏 assert（本文件当前没有直接使用，但常见于线程库模板/调试阶段保留）。
                       // [English] Include the assert macro for debugging (not directly used here, but commonly kept in thread library templates).

#include <signal.h>    // [中文] 引入信号相关声明；本文件未直接操作信号，但与 preemption 机制整体相关，项目里常一起包含。
                       // [English] Include signal-related declarations; not directly used here, but often included in a project that supports preemption.

#include <stddef.h>    // [中文] 引入标准类型/宏（如 NULL、size_t），本文件会用到 NULL。
                       // [English] Include standard definitions (e.g., NULL, size_t); this file uses NULL.

#include <stdint.h>    // [中文] 引入固定宽度整数类型（本文件当前未直接使用，模板常保留）。
                       // [English] Include fixed-width integer types (not directly used here; often kept in templates).

#include <stdio.h>     // [中文] 引入标准输入输出接口（本文件当前未直接使用，调试时可能会用 printf）。
                       // [English] Include standard I/O interfaces (not directly used here; may be useful for debugging prints).

#include <stdlib.h>    // [中文] 引入 malloc/free 等内存管理函数声明，本文件会分配线程控制块。
                       // [English] Include memory management declarations (malloc/free), used for TCB allocation.

#include <sys/time.h>  // [中文] 引入定时器相关声明（本文件未直接调用 setitimer，但与整个 uthread+preempt 项目相关）。
                       // [English] Include timer-related declarations (not directly used here, but part of the overall uthread+preempt project).

#include "private.h"   // [中文] 引入线程库内部接口（如上下文切换/栈分配函数、preempt 控制函数等内部声明）。
                       // [English] Include internal thread-library interfaces (context switch/stack allocation/preemption control, etc.).

#include "queue.h"     // [中文] 引入通用队列接口；本文件用队列维护就绪线程（ready queue）。
                       // [English] Include the generic queue interface; this file uses a queue as the ready queue.

#include "uthread.h"   // [中文] 引入 uthread 对外接口定义（uthread_run/uthread_create 等函数原型、类型定义）。
                       // [English] Include the public uthread interface (prototypes/types such as uthread_run, uthread_create, etc.).

/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这个文件实现了本次作业中的“用户级线程（uthread）核心调度模块”。
 * 它是整个线程库的中心：负责创建线程、保存线程状态、在不同线程之间切换上下文、
 * 管理 ready queue（就绪队列），并提供 block/unblock 接口给 semaphore 等同步模块使用。
 *
 * 【这个文件在整个作业里的作用（非常重要）】
 * 1) 它是线程库的“调度核心”
 *    - uthread_yield() 决定从哪个线程切换到哪个线程
 *    - uthread_run() 负责初始化线程系统并驱动调度循环
 *    - uthread_exit() 负责线程退出时的切换逻辑
 *
 * 2) 它和其他模块协同工作
 *    - queue.c：提供 ready queue 的底层队列操作（enqueue/dequeue）
 *    - preempt.c：定时触发 uthread_yield() 实现抢占式调度
 *    - sem.c：通过 uthread_block()/uthread_unblock() 实现等待/唤醒线程
 *
 * 3) 它维护线程控制块（TCB）和线程状态
 *    - 每个线程有自己的上下文 context（寄存器状态）
 *    - 每个线程有自己的栈 stack
 *    - 每个线程有状态 state（READY / RUNNING / BLOCKED / EXITED）
 *    这些信息决定线程是否能运行、是否在等待、是否已结束。
 *
 * 【本文件实现的关键能力】
 * - 当前线程获取：uthread_current()
 * - 主动让出 CPU：uthread_yield()
 * - 线程退出：uthread_exit()
 * - 创建线程：uthread_create()
 * - 启动线程系统：uthread_run()
 * - 阻塞当前线程：uthread_block()
 * - 唤醒阻塞线程：uthread_unblock()
 *
 * 【整体调度思路】
 * - 用一个全局 ready queue 保存“可运行但当前未运行”的线程
 * - current_thread 指向当前正在运行的线程
 * - yield / block / exit 时，从 ready queue 取下一个线程切换过去
 * - preempt 模块会周期性调用 yield，从而实现时间片轮转
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This file implements the core user-level thread (uthread) scheduling module.
 * It is the center of the thread library: it creates threads, tracks thread
 * state, switches contexts, manages the ready queue, and provides block/unblock
 * operations used by synchronization modules such as semaphores.
 *
 * [Why this file matters in the overall assignment]
 * 1) It is the scheduler core of the thread library
 *    - uthread_yield() chooses and switches to the next runnable thread
 *    - uthread_run() initializes the threading system and runs the scheduler loop
 *    - uthread_exit() handles thread termination and switching
 *
 * 2) It works together with other modules
 *    - queue.c: provides queue operations for the ready queue
 *    - preempt.c: periodically triggers uthread_yield() for preemptive scheduling
 *    - sem.c: uses uthread_block()/uthread_unblock() for waiting/wakeup
 *
 * 3) It manages thread control blocks (TCBs) and states
 *    - Each thread has a context (register state)
 *    - Each thread has a stack
 *    - Each thread has a state (READY / RUNNING / BLOCKED / EXITED)
 *    These determine whether and how a thread can run.
 *
 * [Key capabilities implemented here]
 * - Get current thread: uthread_current()
 * - Voluntary yield: uthread_yield()
 * - Thread exit: uthread_exit()
 * - Thread creation: uthread_create()
 * - Start threading system: uthread_run()
 * - Block current thread: uthread_block()
 * - Unblock a thread: uthread_unblock()
 *
 * [Overall scheduling model]
 * - A global ready queue stores runnable threads that are not currently running
 * - current_thread points to the currently running thread
 * - yield / block / exit pick the next thread from the ready queue and switch
 * - preempt periodically calls yield to implement time slicing
 * ============================================================================
 */

/* macros for thread state */
#define READY 0    // [中文] 线程状态宏：READY 表示线程已准备好运行（通常在 ready queue 中等待调度）。 
                   // [English] Thread state macro: READY means runnable (typically waiting in the ready queue).
#define RUNNING 1  // [中文] 线程状态宏：RUNNING 表示线程当前正在 CPU 上执行。 
                   // [English] Thread state macro: RUNNING means the thread is currently executing.
#define EXITED 2   // [中文] 线程状态宏：EXITED 表示线程已结束执行，不应再被调度。 
                   // [English] Thread state macro: EXITED means the thread has finished and should not be scheduled again.
#define BLOCKED 3  // [中文] 线程状态宏：BLOCKED 表示线程因等待资源/条件而阻塞，不在 ready queue 中。 
                   // [English] Thread state macro: BLOCKED means the thread is waiting and not runnable.

/*
 * [中文] 线程控制块（TCB, Thread Control Block）。
 * 每个用户线程对应一个 TCB，里面保存调度与执行所需的最核心信息。
 *
 * [English] Thread Control Block (TCB).
 * Each user thread has one TCB storing the core information needed for scheduling
 * and execution.
 */
struct uthread_tcb {
    uthread_ctx_t context; // [中文] 线程上下文（寄存器集合等），上下文切换时会保存/恢复这里。 
                           // [English] Thread context (register set, etc.); saved/restored during context switches.

    void *stack;           // [中文] 指向该线程栈空间的指针（由 uthread_ctx_alloc_stack 分配）。 
                           // [English] Pointer to this thread's stack memory (allocated by uthread_ctx_alloc_stack).

    int state;             // [中文] 线程当前状态（READY/RUNNING/BLOCKED/EXITED），供调度与同步逻辑判断。 
                           // [English] Current thread state (READY/RUNNING/BLOCKED/EXITED), used by scheduler/sync logic.
};

static struct uthread_tcb *current_thread = NULL; // [中文] 全局指针：指向当前正在运行的线程 TCB。 
                                                  // [English] Global pointer to the currently running thread's TCB.

static queue_t queue;                              // [中文] 全局就绪队列（ready queue）：保存可运行线程（通常不含 current_thread）。 
                                                  // [English] Global ready queue: stores runnable threads (usually excluding current_thread).

/*
 * uthread_current
 * ----------------------------------------------------------------------------
 * [中文] 返回当前正在运行线程的 TCB 指针。
 * 常用于其他模块（例如 sem.c）获取“自己是谁”（当前线程），然后将其加入等待队列。
 *
 * 用法：
 *   struct uthread_tcb *self = uthread_current();
 *
 * [English] Return the TCB pointer of the currently running thread.
 * Often used by other modules (e.g., sem.c) to identify the current thread.
 */
struct uthread_tcb *uthread_current(void) { return current_thread; } // [中文] 直接返回全局 current_thread。 [English] Return the global current_thread pointer directly.

/*
 * uthread_yield
 * ----------------------------------------------------------------------------
 * [中文] 当前线程主动让出 CPU（或被 preemption 定时器间接触发）。
 *
 * 逻辑概览：
 * 1) 关闭抢占，保护调度关键区
 * 2) 保存当前线程指针 current
 * 3) 从 ready queue 取出下一个线程 next
 * 4) 把当前线程重新放回 ready queue（表示“我还没退出，只是让出 CPU”）
 * 5) 切换 current_thread 到 next，并做上下文切换
 *
 * 注意：
 * - 这里的 current 是“让出 CPU 的线程”，而 current_thread 全局变量会被更新为 next
 * - 如果没有可运行线程（next == NULL），代码当前直接 return（注意：此路径下没有执行 preempt_enable）
 *
 * [English] Yield the CPU from the current thread (either voluntarily or via preemption).
 *
 * Outline:
 * 1) Disable preemption to protect scheduler critical section
 * 2) Save the current thread pointer
 * 3) Dequeue the next runnable thread from the ready queue
 * 4) Re-enqueue the current thread (it is yielding, not exiting)
 * 5) Update current_thread and switch context
 *
 * Note:
 * - current is the thread that is yielding
 * - If next == NULL, this code returns immediately (and in this implementation,
 *   preempt_enable() is not reached on that path)
 */
void uthread_yield(void)
{
    preempt_disable(); // [中文] 进入调度关键区前先关闭抢占，避免 ready queue / current_thread 在切换中被打断。 
                       // [English] Disable preemption before scheduler critical section to avoid interruption during queue/current updates.

    struct uthread_tcb *current = current_thread; // [中文] 保存“当前正在运行、准备让出 CPU 的线程”指针。 
                                                  // [English] Save the pointer to the currently running thread that is about to yield.

    struct uthread_tcb *next;                     // [中文] 声明 next，用来接收 ready queue 里下一个要运行的线程。 
                                                  // [English] Declare next to receive the next runnable thread from the ready queue.

    queue_dequeue(queue, (void **)&next);         // [中文] 从就绪队列取出一个线程作为下一个运行者（FIFO）。 
                                                  // [English] Dequeue one thread from the ready queue as the next thread to run (FIFO).

    queue_enqueue(queue, current);                // [中文] 把当前线程重新放回 ready queue，因为它只是 yield，不是退出/阻塞。 
                                                  // [English] Re-enqueue the current thread because it is yielding, not exiting or blocking.

    if (next == NULL) {                           // [中文] 若队列里没有取到下一个线程（next 为 NULL），则无法切换。 
                                                  // [English] If no next thread was dequeued (next == NULL), there is no thread to switch to.

        return;                                   // [中文] 直接返回（注意：这一条路径没有 preempt_enable，属于代码行为上的细节）。 
                                                  // [English] Return immediately (note: this path does not reach preempt_enable in this code).
    }

    current_thread = next;                        // [中文] 更新全局 current_thread，使 next 成为“当前线程”。 
                                                  // [English] Update global current_thread so next becomes the current thread.

    current->state = READY;                       // [中文] 把让出 CPU 的线程状态设为 READY（可再次被调度）。 
                                                  // [English] Mark the yielding thread as READY (runnable again).

    next->state = RUNNING;                        // [中文] 把即将运行的线程状态设为 RUNNING。 
                                                  // [English] Mark the selected next thread as RUNNING.

    uthread_ctx_switch(&current->context, &next->context); // [中文] 执行上下文切换：保存 current 寄存器状态，恢复 next 寄存器状态。 
                                                           // [English] Perform context switch: save current context and restore next context.

    preempt_enable();                             // [中文] 切换返回后恢复抢占（通常当该线程以后再次被切回时会执行到这里）。 
                                                  // [English] Re-enable preemption after returning from the context switch.
}

/*
 * uthread_exit
 * ----------------------------------------------------------------------------
 * [中文] 当前线程结束执行并退出。
 *
 * 与 uthread_yield 的区别：
 * - yield 会把当前线程重新放回 ready queue
 * - exit 不会重新入队，而是把状态标记为 EXITED
 *
 * 逻辑：
 * 1) 保存当前线程 current
 * 2) 从 ready queue 取一个 next
 * 3) current 标记为 EXITED（表示不再参与调度）
 * 4) 若有 next，则切换到 next
 *
 * [English] Terminate the current thread.
 *
 * Difference from uthread_yield:
 * - yield re-enqueues the current thread
 * - exit marks it EXITED and does not re-enqueue it
 *
 * Outline:
 * 1) Save current thread
 * 2) Dequeue next runnable thread
 * 3) Mark current as EXITED
 * 4) Switch to next if one exists
 */
void uthread_exit(void)
{
    struct uthread_tcb *current = current_thread; // [中文] 保存当前即将退出的线程。 
                                                  // [English] Save the current thread that is about to exit.

    struct uthread_tcb *next;                     // [中文] 声明 next，用于接收下一个可运行线程。 
                                                  // [English] Declare next to hold the next runnable thread.

    queue_dequeue(queue, (void **)&next);         // [中文] 从 ready queue 取出下一个线程作为切换目标。 
                                                  // [English] Dequeue the next runnable thread from the ready queue.

    current->state = EXITED;                      // [中文] 将当前线程状态标记为 EXITED，表示它已完成、不应再次调度。 
                                                  // [English] Mark the current thread as EXITED so it will not be scheduled again.

    if (next == NULL) {                           // [中文] 如果没有其他可运行线程，当前实现直接返回（线程库可能随后由 run 循环结束）。 
                                                  // [English] If no next runnable thread exists, this implementation returns directly.

        return;                                   // [中文] 无线程可切换时返回。 
                                                  // [English] Return when there is no thread to switch to.
    }

    current_thread = next;                        // [中文] 更新当前线程指针为 next。 
                                                  // [English] Update current_thread to next.

    next->state = RUNNING;                        // [中文] next 即将开始运行，状态设为 RUNNING。 
                                                  // [English] Mark next as RUNNING because it is about to execute.

    uthread_ctx_switch(&current->context, &next->context); // [中文] 切换上下文到 next；当前 EXITED 线程不会再主动参与调度。 
                                                           // [English] Switch context to next; the EXITED thread will not be re-enqueued.
}

/*
 * uthread_create
 * ----------------------------------------------------------------------------
 * [中文] 创建一个新的用户线程，并将其放入 ready queue（状态 READY）。
 *
 * 主要步骤：
 * 1) 分配新线程的 TCB
 * 2) 分配线程栈
 * 3) 初始化线程上下文（让它未来运行 func(arg)）
 * 4) 设置状态为 READY
 * 5) 进入关键区，把新线程入队到 ready queue
 *
 * 参数：
 *   func - 线程入口函数
 *   arg  - 传给线程入口函数的参数
 *
 * 返回值：
 *   - 0  : 创建成功
 *   - -1 : 任一步失败（内存分配/上下文初始化/入队失败）
 *
 * [English] Create a new user thread and enqueue it into the ready queue.
 *
 * Steps:
 * 1) Allocate a TCB
 * 2) Allocate a stack
 * 3) Initialize the thread context to run func(arg)
 * 4) Mark thread state READY
 * 5) Enqueue it into the ready queue with preemption disabled
 *
 * Parameters:
 *   func - thread entry function
 *   arg  - argument passed to the thread function
 *
 * Return:
 *   - 0  : success
 *   - -1 : failure in allocation/init/enqueue
 */
int uthread_create(uthread_func_t func, void *arg)
{
    struct uthread_tcb *new_tcb = malloc(sizeof(*new_tcb)); // [中文] 为新线程分配 TCB 内存。 
                                                            // [English] Allocate memory for a new thread TCB.

    if (new_tcb == NULL) {                                  // [中文] 检查 TCB 分配是否失败。 
                                                            // [English] Check if TCB allocation failed.
        return -1;                                          // [中文] 分配失败，返回错误。 
                                                            // [English] Return error on allocation failure.
    }

    new_tcb->stack = uthread_ctx_alloc_stack();             // [中文] 为新线程分配独立栈空间。 
                                                            // [English] Allocate a separate stack for the new thread.

    if (new_tcb->stack == NULL) {                           // [中文] 栈分配失败则无法创建线程。 
                                                            // [English] Cannot create thread if stack allocation fails.
        return -1;                                          // [中文] 返回错误（注意：当前代码路径未释放 new_tcb，这是现有代码行为）。 
                                                            // [English] Return error (note: this path does not free new_tcb in current code).
    }

    if (uthread_ctx_init(&new_tcb->context, new_tcb->stack, func, arg) == -1) { // [中文] 初始化线程上下文：设置栈与入口函数，使该线程未来从 func(arg) 开始执行。 
                                                                                  // [English] Initialize the thread context so it will start at func(arg) using the allocated stack.
        return -1;                                                                // [中文] 上下文初始化失败，返回错误（当前代码未清理已分配资源，这是现有行为）。 
                                                                                  // [English] Return error if context initialization fails (current code does not free allocated resources on this path).
    }

    new_tcb->state = READY;                               // [中文] 新线程初始状态设为 READY，表示可加入就绪队列等待调度。 
                                                          // [English] Mark the new thread READY so it can be scheduled.

    preempt_disable();                                    // [中文] 进入关键区：保护 ready queue 入队操作，避免被定时抢占打断。 
                                                          // [English] Disable preemption to protect ready-queue enqueue.

    int r = queue_enqueue(queue, new_tcb);                // [中文] 把新线程加入全局 ready queue。 
                                                          // [English] Enqueue the new thread into the global ready queue.

    preempt_enable();                                     // [中文] 离开关键区，恢复抢占。 
                                                          // [English] Re-enable preemption after queue operation.

    return (r == 1) ? -1 : 0;                             // [中文] 根据入队结果返回成功/失败（当前代码用 r==1 判断失败，这是原代码逻辑）。 
                                                          // [English] Return success/failure based on enqueue result (this keeps the original code logic using r==1).
}

/*
 * uthread_run
 * ----------------------------------------------------------------------------
 * [中文] 启动整个用户线程系统（线程库入口）。
 *
 * 主要职责：
 * 1) 创建全局 ready queue
 * 2) 启动 preemption（或按参数选择不启用）
 * 3) 创建“主线程”的 TCB（current_thread）
 * 4) 可选地创建第一个用户线程 func(arg)
 * 5) 循环调度：只要 ready queue 非空，就不断调用 uthread_yield()
 * 6) 所有可运行线程结束后，停止 preemption 并返回
 *
 * 参数：
 *   preempt - 是否启用抢占式调度
 *   func    - 初始线程入口函数（可为 NULL）
 *   arg     - 初始线程参数
 *
 * [English] Start the entire user-thread system (main entry point of the library).
 *
 * Responsibilities:
 * 1) Create the global ready queue
 * 2) Start preemption (or not, based on argument)
 * 3) Create the "main/current" thread TCB
 * 4) Optionally create the first user thread func(arg)
 * 5) Run the scheduler loop while ready queue is not empty
 * 6) Stop preemption and return when no runnable threads remain
 */
int uthread_run(bool preempt, uthread_func_t func, void *arg)
{

    queue = queue_create();           // [中文] 创建全局 ready queue，用于保存可运行线程。 
                                      // [English] Create the global ready queue for runnable threads.

    if (!queue)                       // [中文] 如果队列创建失败，线程系统无法运行。 
                                      // [English] If queue creation fails, the thread system cannot run.
        return -1;                    // [中文] 返回错误（原代码保持无大括号风格）。 
                                      // [English] Return error (keeping the original no-braces style).

    preempt_start(preempt);           // [中文] 根据参数启动抢占机制；若 preempt=false，则该函数内部会选择不启用。 
                                      // [English] Start preemption according to the flag; if false, preemption stays disabled.

    current_thread = malloc(sizeof(*current_thread)); // [中文] 为“当前线程（主线程上下文壳）”分配 TCB。 
                                                      // [English] Allocate a TCB for the current/main thread placeholder.

    if (current_thread == NULL) {     // [中文] 检查主线程 TCB 分配是否失败。 
                                      // [English] Check if current thread TCB allocation failed.
        return -1;                    // [中文] 分配失败返回错误。 
                                      // [English] Return error on allocation failure.
    }

    current_thread->state = RUNNING;  // [中文] 初始化当前线程状态为 RUNNING（此时调用者线程就是正在运行的线程）。 
                                      // [English] Mark current thread as RUNNING since the caller is executing now.

    if (func != NULL) {               // [中文] 如果提供了初始线程入口函数，则创建第一个用户线程。 
                                      // [English] If an initial thread function is provided, create the first user thread.
        uthread_create(func, arg);    // [中文] 创建线程并放入 ready queue，等待调度运行。 
                                      // [English] Create the thread and enqueue it in the ready queue.
    }

    while (queue_length(queue) > 0) { // [中文] 调度主循环：只要 ready queue 里还有可运行线程，就继续调度。 
                                      // [English] Scheduler loop: continue while there are runnable threads in the ready queue.
        uthread_yield();              // [中文] 让当前线程/调度器切换到下一个线程，推动整个线程系统运行。 
                                      // [English] Yield/switch to another runnable thread to keep the system progressing.
    }

    preempt_stop();                   // [中文] 当没有可运行线程后，关闭抢占并恢复原始信号/定时器配置。 
                                      // [English] Stop preemption and restore original signal/timer state.

    return 0;                         // [中文] 返回 0 表示线程系统正常运行结束。 
                                      // [English] Return 0 to indicate normal completion.
}

/*
 * uthread_block
 * ----------------------------------------------------------------------------
 * [中文] 阻塞当前线程（常由同步原语，如 sem_down，在资源不足时调用）。
 *
 * 逻辑：
 * 1) 将当前线程状态设为 BLOCKED（注意：不放回 ready queue）
 * 2) 从 ready queue 取一个 next 线程
 * 3) 若没有 next，则恢复当前线程为 RUNNING 并返回（避免系统无线程可切换）
 * 4) 否则切换到 next
 *
 * [English] Block the current thread (commonly used by synchronization code,
 * e.g., sem_down when resources are unavailable).
 *
 * Outline:
 * 1) Mark current thread BLOCKED (do not re-enqueue it)
 * 2) Dequeue a next runnable thread from ready queue
 * 3) If none exists, restore current to RUNNING and return
 * 4) Otherwise switch to next
 */
void uthread_block(void)
{

    struct uthread_tcb *current = current_thread; // [中文] 保存当前线程指针；该线程将被置为 BLOCKED。 
                                                  // [English] Save the current thread pointer; it will be marked BLOCKED.

    struct uthread_tcb *next = NULL;              // [中文] 初始化 next 为 NULL，用于接收 ready queue 中的下一个线程。 
                                                  // [English] Initialize next to NULL; it will hold the next runnable thread.

    current->state = BLOCKED;                     // [中文] 将当前线程状态置为 BLOCKED，表示它暂时不能运行。 
                                                  // [English] Mark the current thread as BLOCKED (not runnable).

    if (queue_dequeue(queue, (void **)&next) == -1 || next == NULL) { // [中文] 尝试从 ready queue 取一个线程；若失败或为空，则无法切换。 
                                                                       // [English] Try to dequeue a next runnable thread; if dequeue fails or next is NULL, cannot switch.

        current->state = RUNNING;                 // [中文] 没有其他线程可运行时，把当前线程状态恢复为 RUNNING（否则会“卡死”成 BLOCKED）。 
                                                  // [English] If no other thread can run, restore current state to RUNNING.

        return;                                   // [中文] 返回继续执行当前线程。 
                                                  // [English] Return and continue executing the current thread.
    }

    current_thread = next;                        // [中文] 更新全局当前线程为 next。 
                                                  // [English] Update global current_thread to next.

    next->state = RUNNING;                        // [中文] next 即将运行，状态设为 RUNNING。 
                                                  // [English] Mark next as RUNNING before switching.

    uthread_ctx_switch(&current->context, &next->context); // [中文] 执行上下文切换：当前线程被挂起，next 开始/恢复执行。 
                                                           // [English] Perform context switch: current thread is suspended, next runs.
}

/*
 * uthread_unblock
 * ----------------------------------------------------------------------------
 * [中文] 唤醒一个阻塞线程（常由 sem_up 调用）。
 *
 * 逻辑：
 * - 若传入线程指针为 NULL：直接返回
 * - 若线程当前状态是 BLOCKED：将其改为 READY 并放回 ready queue
 * - 若不是 BLOCKED：不做操作（避免重复入队）
 *
 * 参数：
 *   uthread - 要唤醒的线程 TCB 指针
 *
 * [English] Unblock a blocked thread (commonly called by sem_up).
 *
 * Behavior:
 * - If the pointer is NULL: return
 * - If state is BLOCKED: mark READY and enqueue into ready queue
 * - Otherwise: do nothing (prevents duplicate enqueue)
 *
 * Parameter:
 *   uthread - TCB pointer of the thread to unblock
 */
void uthread_unblock(struct uthread_tcb *uthread)
{
    if (uthread == NULL) {                     // [中文] 空指针检查：没有线程可唤醒时直接返回。 
                                               // [English] NULL check: if no thread is provided, do nothing.
        return;                                // [中文] 返回。 
                                               // [English] Return.
    }

    if (uthread->state == BLOCKED) {           // [中文] 只有真正处于 BLOCKED 状态的线程才需要被唤醒。 
                                               // [English] Only a thread in BLOCKED state should be unblocked.

        uthread->state = READY;                // [中文] 将线程状态改为 READY，表示它重新具备运行资格。 
                                               // [English] Mark the thread READY so it becomes runnable again.

        queue_enqueue(queue, uthread);         // [中文] 把线程重新放入全局 ready queue，等待调度器选中运行。 
                                               // [English] Enqueue the thread into the global ready queue for future scheduling.
    }
}