#include <signal.h>     // [中文] 引入信号处理相关接口与类型声明（如 sigaction、sigset_t、SIGVTALRM、sigprocmask）。 
                        // [English] Include signal-handling interfaces and types (e.g., sigaction, sigset_t, SIGVTALRM, sigprocmask).

#include <stdbool.h>    // [中文] 引入 bool、true、false 的定义，本文件用 bool 控制是否启用抢占。
                        // [English] Include bool, true, and false definitions; this file uses bool to enable/disable preemption.

#include <stddef.h>     // [中文] 引入标准类型（如 NULL、size_t），这里主要是为了 NULL 等基础定义。
                        // [English] Include standard definitions (e.g., NULL, size_t); mainly for basic types/macros like NULL.

#include <stdint.h>     // [中文] 引入固定宽度整数类型定义（本文件中未直接使用，常见于项目模板保留）。
                        // [English] Include fixed-width integer types (not directly used here; commonly kept in project templates).

#include <stdio.h>      // [中文] 引入 perror 的声明，用于系统调用失败时报错输出。
                        // [English] Include perror declaration for reporting system call errors.

#include <stdlib.h>     // [中文] 引入 exit 的声明，用于发生严重错误时退出程序。
                        // [English] Include exit declaration for terminating the program on fatal errors.

#include <sys/time.h>   // [中文] 引入 setitimer 和 struct itimerval 的声明，用于配置定时器触发抢占信号。
                        // [English] Include setitimer and struct itimerval declarations for configuring timer-based preemption.

#define _POSIX_SOURCE 1 // [中文] POSIX 功能宏（用于启用某些 POSIX 接口声明；通常更推荐放在系统头文件之前）。
                        // [English] POSIX feature macro (used to expose some POSIX APIs; typically placed before system headers).

#include "private.h"    // [中文] 引入线程库内部接口（如 uthread_yield、preempt_disable/enable 的内部声明等）。
                        // [English] Include internal thread-library interfaces (e.g., uthread_yield and internal runtime APIs).

#include "uthread.h"    // [中文] 引入 uthread 对外接口定义；这里主要用于与线程库整体集成。
                        // [English] Include public uthread interfaces; mainly for integration with the thread library.

/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这个文件实现了本次 uthread 作业中的“抢占式调度（preemption）”支持模块。
 * 它的核心作用是：使用操作系统提供的虚拟定时器（ITIMER_VIRTUAL）周期性发送 SIGVTALRM
 * 信号，在信号处理函数中调用 uthread_yield()，从而让线程库即使在线程不主动让出 CPU
 * 的情况下，也能进行调度切换（时间片轮转的基础）。
 *
 * 【这个文件在整个作业里的作用（非常重要）】
 * 1) 它让线程库从“协作式调度（cooperative）”升级为“可抢占调度（preemptive）”
 *    - 如果没有这个模块，线程必须自己调用 uthread_yield() 才会切换
 *    - 有了这个模块，定时器会按固定频率中断并触发切换，提高公平性
 *
 * 2) 它保护关键区，避免线程库内部数据结构在中断时被破坏
 *    - preempt_disable() / preempt_enable() 通过屏蔽/解除 SIGVTALRM 来控制“是否允许抢占”
 *    - 在线程队列操作、信号量操作、调度器关键路径中，通常都要临时关闭抢占
 *
 * 3) 它与其他模块（queue.c / sem.c / uthread.c）协同工作
 *    - uthread.c 负责调度与上下文切换
 *    - queue.c 负责 ready/wait 队列
 *    - sem.c 在关键区内调用 preempt_disable/enable 保证原子性
 *    - preempt.c 则负责“定时触发 yield”的机制
 *
 * 【本文件实现的核心功能】
 * - preempt_start(bool): 启动抢占（安装信号处理器 + 启动虚拟定时器）
 * - preempt_stop():      停止抢占（恢复旧信号处理器 + 恢复旧定时器）
 * - preempt_disable():   屏蔽 SIGVTALRM（临界区保护）
 * - preempt_enable():    解除对 SIGVTALRM 的屏蔽
 * - alarm_handler():     SIGVTALRM 到达时执行，内部调用 uthread_yield()
 *
 * 【关键设计点】
 * - 使用 ITIMER_VIRTUAL：只在进程执行用户态代码时计时（更贴合用户级线程调度）
 * - 使用 SIGVTALRM：虚拟时间片到期时发送的信号
 * - 在 handler 中先 preempt_disable 再 uthread_yield，再 preempt_enable：
 *   避免 handler/调度过程被再次抢占，降低重入风险
 * - 保存 old_sa / old_timer：退出时恢复原来的系统配置，避免影响程序其他部分
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This file implements preemption support for the uthread assignment.
 * Its core job is to use a virtual timer (ITIMER_VIRTUAL) to periodically send
 * SIGVTALRM, and in the signal handler call uthread_yield(). This enables the
 * thread library to context-switch even when threads do not voluntarily yield,
 * which is the basis of time-sliced scheduling.
 *
 * [Why this file matters in the overall assignment]
 * 1) It upgrades the thread library from cooperative scheduling to preemptive
 *    scheduling.
 *    - Without this module, threads switch only when they explicitly yield.
 *    - With this module, a timer interrupt periodically triggers scheduling.
 *
 * 2) It protects critical sections from asynchronous interruption.
 *    - preempt_disable()/preempt_enable() block/unblock SIGVTALRM.
 *    - Queue operations, semaphore operations, and scheduler internals often
 *      use these functions to prevent races/corruption.
 *
 * 3) It works together with queue.c / sem.c / uthread.c
 *    - uthread.c handles scheduling/context switching
 *    - queue.c maintains ready/wait queues
 *    - sem.c uses preemption control for atomic semaphore operations
 *    - preempt.c provides the timer-driven yield mechanism
 *
 * [Core functions implemented here]
 * - preempt_start(bool): start preemption (install handler + start timer)
 * - preempt_stop():      stop preemption (restore old handler + timer)
 * - preempt_disable():   block SIGVTALRM (critical section protection)
 * - preempt_enable():    unblock SIGVTALRM
 * - alarm_handler():     runs on SIGVTALRM and calls uthread_yield()
 *
 * [Key design points]
 * - ITIMER_VIRTUAL counts only when the process runs in user mode
 * - SIGVTALRM is the signal delivered on virtual timer expiration
 * - The handler disables preemption before yielding to reduce re-entrancy risk
 * - old_sa / old_timer are saved and restored to avoid affecting other code
 * ============================================================================
 */

/*
 * [中文] 定义抢占频率：100Hz 表示每秒触发 100 次定时器信号（约每 10ms 一次）。
 * [English] Define preemption frequency: 100Hz means 100 timer interrupts per second (~10ms each).
 */
#define HZ 100

static bool preempt_active = false; // [中文] 标记抢占机制当前是否启用；false 表示 preempt_disable/enable 应该什么都不做。 
                                    // [English] Tracks whether preemption is currently active; when false, preempt_disable/enable become no-ops.

static sigset_t vtalrm_set;         // [中文] 保存“只包含 SIGVTALRM”的信号集合，用于 sigprocmask 屏蔽/解除屏蔽该信号。 
                                    // [English] A signal set containing SIGVTALRM, used by sigprocmask to block/unblock preemption signal.

static struct sigaction old_sa;     // [中文] 保存启动抢占前原有的 SIGVTALRM 处理器配置，方便 preempt_stop 时恢复。 
                                    // [English] Stores the previous SIGVTALRM signal action so preempt_stop can restore it.

static struct itimerval old_timer;  // [中文] 保存启动抢占前原有的虚拟定时器配置，方便 preempt_stop 时恢复。 
                                    // [English] Stores the previous virtual timer configuration for restoration in preempt_stop.

/*
 * alarm_handler
 * ----------------------------------------------------------------------------
 * [中文] SIGVTALRM 的信号处理函数。每次时间片到期，内核会调用它。
 * 它的作用是触发一次 uthread_yield()，让调度器有机会切换线程。
 *
 * 为什么这里要调用 preempt_disable()/preempt_enable()？
 * - 信号处理函数本身是异步触发的
 * - 如果在 handler 或 yield 过程中再次收到抢占信号，可能导致重入问题
 * - 因此先暂时屏蔽 SIGVTALRM，再 yield，最后恢复
 *
 * 参数：
 *   signum - 触发该 handler 的信号编号（这里应为 SIGVTALRM）
 *
 * [English] Signal handler for SIGVTALRM. It runs when a time slice expires.
 * Its job is to trigger uthread_yield() so the scheduler can switch threads.
 *
 * Why disable/enable preemption here?
 * - Signal handlers are asynchronous
 * - A nested SIGVTALRM during handler/yield could cause re-entrancy issues
 * - So we block SIGVTALRM before yielding, then unblock afterward
 */
static void alarm_handler(int signum)
{
    (void)signum;        // [中文] 显式忽略未使用参数，避免编译器发出 unused parameter 警告。 
                         // [English] Explicitly mark the parameter unused to avoid compiler warnings.

    preempt_disable();   // [中文] 进入 handler 后先屏蔽 SIGVTALRM，防止 handler/yield 过程再次被抢占（重入）。 
                         // [English] Block SIGVTALRM before yielding to avoid nested preemption/re-entrancy.

    uthread_yield();     // [中文] 主动让出 CPU，触发线程库调度器选择下一个可运行线程。 
                         // [English] Yield the CPU to the scheduler so another runnable thread may run.

    preempt_enable();    // [中文] handler 收尾时恢复 SIGVTALRM，使后续时间片中断继续生效。 
                         // [English] Re-enable SIGVTALRM so future timer-based preemption can continue.
}

/*
 * preempt_disable
 * ----------------------------------------------------------------------------
 * [中文] 临时关闭“抢占”功能（准确地说：屏蔽 SIGVTALRM 信号）。
 * 常用于线程库关键区：当代码在修改调度队列、TCB 状态、信号量等待队列等共享结构时，
 * 应避免被异步定时器信号打断。
 *
 * 用法：
 *   preempt_disable();
 *   // critical section
 *   preempt_enable();
 *
 * [English] Temporarily disable preemption by blocking SIGVTALRM.
 * Commonly used to protect critical sections when manipulating scheduler queues,
 * TCB states, semaphores, or other shared thread-library structures.
 */
void preempt_disable(void)
{
    if (!preempt_active) {                     // [中文] 如果抢占机制本来就没启动，就不需要做任何事情。 
                                               // [English] If preemption is not active, there is nothing to block.
        return;                                // [中文] 直接返回（no-op）。 
                                               // [English] Return immediately (no-op).
    }

    sigprocmask(SIG_BLOCK, &vtalrm_set, NULL); // [中文] 使用 sigprocmask 屏蔽 vtalrm_set 中的信号（即 SIGVTALRM）。 
                                               // [English] Block signals in vtalrm_set (SIGVTALRM) using sigprocmask.
}

/*
 * preempt_enable
 * ----------------------------------------------------------------------------
 * [中文] 重新启用“抢占”功能（准确地说：解除对 SIGVTALRM 的屏蔽）。
 * 通常与 preempt_disable() 成对使用，表示关键区结束。
 *
 * [English] Re-enable preemption by unblocking SIGVTALRM.
 * Typically paired with preempt_disable() to mark the end of a critical section.
 */
void preempt_enable(void)
{
    if (!preempt_active) {                       // [中文] 若抢占机制未启动，则无需解除屏蔽（本来就无效）。 
                                                 // [English] If preemption is not active, unblocking is unnecessary.
        return;                                  // [中文] 直接返回（no-op）。 
                                                 // [English] Return immediately (no-op).
    }

    sigprocmask(SIG_UNBLOCK, &vtalrm_set, NULL); // [中文] 解除对 SIGVTALRM 的屏蔽，让定时器信号可以再次送达。 
                                                 // [English] Unblock SIGVTALRM so timer signals can be delivered again.
}

/*
 * preempt_start
 * ----------------------------------------------------------------------------
 * [中文] 启动抢占式调度支持。
 *
 * 主要步骤：
 * 1) 如果参数 preempt == false：不启用抢占，直接标记关闭并返回（兼容作业测试模式）
 * 2) 初始化 vtalrm_set（构造一个只包含 SIGVTALRM 的信号集）
 * 3) 安装 SIGVTALRM 的信号处理函数 alarm_handler（并保存旧处理器到 old_sa）
 * 4) 配置 ITIMER_VIRTUAL 定时器为周期性触发（100Hz），并保存旧定时器到 old_timer
 * 5) 调用 preempt_enable()，确保 SIGVTALRM 未被屏蔽
 *
 * 参数：
 *   preempt - true 表示启用抢占；false 表示禁用抢占（协作式模式）
 *
 * [English] Start preemptive scheduling support.
 *
 * Main steps:
 * 1) If preempt == false, leave preemption disabled and return
 * 2) Initialize vtalrm_set (a signal set containing SIGVTALRM only)
 * 3) Install alarm_handler for SIGVTALRM (save previous action into old_sa)
 * 4) Configure ITIMER_VIRTUAL as a periodic timer (100Hz), saving old timer in old_timer
 * 5) Call preempt_enable() to ensure SIGVTALRM is unblocked
 */
void preempt_start(bool preempt)
{
    if (!preempt) {                    // [中文] 如果调用者要求“不启用抢占”（例如某些测试/模式），直接关闭标记并返回。 
                                       // [English] If the caller requests no preemption, keep it disabled and return.
        preempt_active = false;        // [中文] 明确设置为未启用状态。 
                                       // [English] Explicitly mark preemption as inactive.
        return;                        // [中文] 不安装 handler、不启动 timer。 
                                       // [English] Do not install handler or start timer.
    }

    preempt_active = true;             // [中文] 标记抢占机制已启用；此后 preempt_disable/enable 会真正操作信号屏蔽。 
                                       // [English] Mark preemption as active so preempt_disable/enable take effect.

    sigemptyset(&vtalrm_set);          // [中文] 先把 vtalrm_set 初始化为空信号集。 
                                       // [English] Initialize vtalrm_set as an empty signal set.

    sigaddset(&vtalrm_set, SIGVTALRM); // [中文] 将 SIGVTALRM 加入信号集，之后用于屏蔽/解除屏蔽该信号。 
                                       // [English] Add SIGVTALRM to the set used for blocking/unblocking preemption.

    struct sigaction sa;               // [中文] 定义新的信号处理配置结构，用来安装我们的 handler。 
                                       // [English] Define a sigaction structure to install our signal handler.

    sa.sa_handler = alarm_handler;     // [中文] 指定 SIGVTALRM 到来时执行的处理函数。 
                                       // [English] Set the handler function to run on SIGVTALRM.

    sigemptyset(&sa.sa_mask);          // [中文] 初始化 handler 执行期间额外屏蔽的信号集合为空。 
                                       // [English] Initialize the handler's mask (extra blocked signals during handler) to empty.

    sigaddset(&sa.sa_mask, SIGVTALRM); // [中文] 在 handler 执行期间额外屏蔽 SIGVTALRM，避免 handler 重入。 
                                       // [English] Block SIGVTALRM while the handler is running to avoid re-entrancy.

    sa.sa_flags = SA_RESTART;          // [中文] 使用 SA_RESTART：让某些被信号中断的系统调用自动重启（减少 EINTR 问题）。 
                                       // [English] Use SA_RESTART so some interrupted system calls are automatically restarted.

    if (sigaction(SIGVTALRM, &sa, &old_sa) == -1) { // [中文] 安装新 handler，并把旧配置保存到 old_sa；失败返回 -1。 
                                                    // [English] Install the new handler and save the previous one into old_sa.
        perror("sigaction");           // [中文] 输出系统错误信息，帮助调试（例如权限/参数问题）。 
                                       // [English] Print the system error message for debugging.
        exit(1);                       // [中文] 这是基础运行机制，失败后通常无法继续，直接退出程序。 
                                       // [English] This is fundamental runtime setup; exit if it fails.
    }

    struct itimerval timer;            // [中文] 定义定时器配置结构（当前值 + 周期间隔）。 
                                       // [English] Define timer configuration (current value + repeating interval).

    timer.it_interval.tv_sec = 0;      // [中文] 周期的“秒”部分设为 0。 
                                       // [English] Set the seconds part of the repeating interval to 0.

    timer.it_interval.tv_usec = 1000000 / HZ; // [中文] 周期的“微秒”部分 = 1 秒 / HZ；HZ=100 时为 10000 微秒（10ms）。 
                                              // [English] Set microseconds interval to 1,000,000 / HZ; at HZ=100 this is 10ms.

    timer.it_value = timer.it_interval; // [中文] 设置首次触发时间为同样的间隔值（即启动后约 10ms 先触发一次）。 
                                        // [English] Set the initial expiration to the same interval (first signal after ~10ms).

    if (setitimer(ITIMER_VIRTUAL, &timer, &old_timer) == -1) { // [中文] 启动虚拟定时器，并保存旧定时器配置到 old_timer。 
                                                                // [English] Start the virtual timer and save the previous timer configuration in old_timer.
        perror("setitimer");           // [中文] 输出定时器设置失败的系统错误信息。 
                                       // [English] Print system error message if timer setup fails.
        exit(1);                       // [中文] 无法设置抢占定时器时，线程库无法按预期工作，直接退出。 
                                       // [English] Exit because timer-driven preemption cannot function without this.
    }

    preempt_enable();                  // [中文] 确保 SIGVTALRM 未被屏蔽（某些初始化路径中可能被屏蔽过），正式开始抢占。 
                                       // [English] Ensure SIGVTALRM is unblocked so preemption can start firing.
}

/*
 * preempt_stop
 * ----------------------------------------------------------------------------
 * [中文] 停止抢占式调度支持，并恢复程序在 preempt_start 之前的系统状态。
 *
 * 主要步骤：
 * 1) 如果抢占未启用，直接返回
 * 2) 先 preempt_disable()，防止停止过程中再被 SIGVTALRM 打断
 * 3) 恢复原来的 SIGVTALRM 处理器配置（old_sa）
 * 4) 恢复原来的虚拟定时器配置（old_timer）
 * 5) 标记 preempt_active = false
 *
 * 这样做的好处：
 * - 不会把线程库的信号处理器/定时器设置“污染”到程序其他部分
 * - 便于测试和资源清理
 *
 * [English] Stop preemption support and restore the previous process state.
 *
 * Main steps:
 * 1) Return immediately if preemption is not active
 * 2) Disable preemption during shutdown to avoid interruption
 * 3) Restore previous SIGVTALRM handler (old_sa)
 * 4) Restore previous virtual timer (old_timer)
 * 5) Mark preemption inactive
 *
 * This avoids leaving the program's signal/timer state altered after the thread
 * library stops.
 */
void preempt_stop(void)
{
    if (!preempt_active) {                      // [中文] 如果抢占本来就没开，停止操作无事可做。 
                                                // [English] If preemption is not active, there is nothing to stop.
        return;                                 // [中文] 直接返回。 
                                                // [English] Return immediately.
    }

    preempt_disable();                          // [中文] 在恢复 handler/timer 之前先屏蔽 SIGVTALRM，防止停止流程被中断。 
                                                // [English] Block SIGVTALRM before restoring state to avoid interruptions.

    if (sigaction(SIGVTALRM, &old_sa, NULL) == -1) { // [中文] 恢复 preempt_start 前的 SIGVTALRM 处理器配置。 
                                                     // [English] Restore the previous SIGVTALRM signal action saved in old_sa.
        perror("sigaction restore");            // [中文] 输出恢复失败的系统错误信息。 
                                                // [English] Print system error if restoring sigaction fails.
        exit(1);                                // [中文] 恢复失败通常视为严重错误，直接退出。 
                                                // [English] Treat restore failure as fatal and exit.
    }

    if (setitimer(ITIMER_VIRTUAL, &old_timer, NULL) == -1) { // [中文] 恢复之前的虚拟定时器配置（可能是关闭状态，也可能是别的设置）。 
                                                             // [English] Restore the previous virtual timer configuration from old_timer.
        perror("setitimer restore");            // [中文] 输出恢复定时器失败的系统错误信息。 
                                                // [English] Print system error if restoring timer fails.
        exit(1);                                // [中文] 恢复失败视为严重错误，直接退出。 
                                                // [English] Treat restore failure as fatal and exit.
    }

    preempt_active = false;                     // [中文] 最后标记抢占已关闭；此后 preempt_disable/enable 不再操作信号屏蔽。 
                                                // [English] Mark preemption inactive; future preempt_disable/enable become no-ops.
}