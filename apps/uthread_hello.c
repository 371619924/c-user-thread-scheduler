/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这是本次作业 apps/ 目录中的一个“最小功能测试程序（hello world test）”，用于验证
 * 你的 uthread 线程库是否已经具备最基本的运行能力。
 *
 * 【它在整个作业里的作用（很重要）】
 * 1) 这是 uthread 的入门测试 / 烟雾测试（smoke test）
 *    - 调用 uthread_run(...) 启动线程系统
 *    - 创建并运行一个最简单的线程函数 hello
 *    - 如果能正确打印 "Hello world!" 并正常返回，说明线程库主流程基本通了
 *
 * 2) 它用于快速检查线程创建与调度主流程是否工作
 *    - uthread_run() 能否成功初始化线程系统
 *    - uthread_create()（由 uthread_run 内部触发）能否创建线程
 *    - 调度器能否切换到 hello 线程并执行它
 *    - hello 执行结束后线程系统能否正常退出
 *
 * 3) 它是后续更复杂测试（queue / preempt / sem / 多线程）的前置验证
 *    - 如果这个最简单测试都不能通过，后面更复杂测试基本也会失败
 *    - 所以通常先跑通这个，再做复杂同步和抢占测试
 *
 * 【这个测试文件验证了什么】
 * - 单线程创建是否成功
 * - 线程入口函数是否被正确调用
 * - uthread_run(false, ...) 在“非抢占模式”下是否能正常运行
 * - 程序是否能正常结束返回
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This is a minimal "hello world" test program in the apps/ directory. It is
 * used as a basic smoke test for your uthread library.
 *
 * [Why this file matters in the overall assignment]
 * 1) It is an entry-level sanity check for the thread library
 *    - It calls uthread_run(...) to start the threading system
 *    - It runs a simple thread function (hello)
 *    - If it prints "Hello world!" and exits normally, the core flow works
 *
 * 2) It quickly validates the create/schedule/exit path
 *    - uthread_run() initializes correctly
 *    - thread creation works
 *    - the scheduler runs the hello thread
 *    - the thread returns and the system exits cleanly
 *
 * 3) It is a prerequisite before more complex tests
 *    - If this simple test fails, later queue/semaphore/preemption tests are
 *      likely to fail too
 *
 * [What this test checks]
 * - Single-thread creation
 * - Correct invocation of thread entry function
 * - Correct behavior of uthread_run(false, ...) in non-preemptive mode
 * - Clean program termination
 * ============================================================================
 */

/*
 * [中文] 原始文件注释：这是一个简单的 hello world 测试，用来测试单线程创建和正常返回。
 * [English] Original file comment: a simple hello world test for single-thread creation and successful return.
 */

/*
 * Simple hello world test
 *
 * Tests the creation of a single thread and its successful return.
 */

#include <stdbool.h> // [中文] 引入 bool / true / false 定义；这里会用到 false 传给 uthread_run。
                     // [English] Include bool/true/false definitions; this file uses false in uthread_run.

#include <stdio.h>   // [中文] 引入 printf 声明，hello 线程函数里要打印 "Hello world!"。
                     // [English] Include printf declaration; the hello thread prints "Hello world!".

#include <stdlib.h>  // [中文] 引入标准库声明（本文件当前未直接使用，但模板里常保留）。
                     // [English] Include standard library declarations (not directly used here, but commonly kept in templates).

#include <uthread.h> // [中文] 引入 uthread 线程库接口（如 uthread_run、线程函数类型定义等）。
                     // [English] Include the uthread library API (e.g., uthread_run and thread function types).

/*
 * hello
 * ----------------------------------------------------------------------------
 * [中文] 这是线程入口函数（thread entry function）。
 * 当 uthread_run 创建并调度这个线程时，这个函数会被执行。
 *
 * 参数：
 *   arg - 线程参数（这里不需要使用，所以传 NULL）
 *
 * 功能：
 *   打印一行 "Hello world!"
 *
 * [English] This is the thread entry function.
 * It runs when the uthread scheduler creates and schedules this thread.
 *
 * Parameter:
 *   arg - thread argument (unused here, so NULL is passed)
 *
 * Behavior:
 *   Prints "Hello world!"
 */
void hello(void *arg)
{
    (void)arg; // [中文] 显式忽略未使用参数，避免编译器警告（因为这个测试不需要线程参数）。
               // [English] Explicitly ignore the unused parameter to avoid compiler warnings.

    printf("Hello world!\n"); // [中文] 打印测试输出，表明线程函数被成功执行到了。
                              // [English] Print test output, showing the thread function ran successfully.
}

/*
 * main
 * ----------------------------------------------------------------------------
 * [中文] 测试程序入口。
 *
 * 逻辑：
 * 1) 调用 uthread_run(false, hello, NULL)
 *    - false：不启用抢占（cooperative 模式）
 *    - hello：线程入口函数
 *    - NULL：传给 hello 的参数（这里未使用）
 * 2) 若线程系统正常运行并结束，则 main 返回 0
 *
 * [English] Test program entry point.
 *
 * Behavior:
 * 1) Call uthread_run(false, hello, NULL)
 *    - false: disable preemption (cooperative mode)
 *    - hello: thread entry function
 *    - NULL: argument passed to hello (unused)
 * 2) Return 0 if the thread system runs and exits successfully
 */
int main(void)
{
    uthread_run(false, hello, NULL); // [中文] 启动用户线程系统（非抢占模式），运行 hello 线程。
                                     // [English] Start the user-thread system (non-preemptive mode) and run the hello thread.

    return 0; // [中文] 程序正常结束，返回 0 表示成功。
              // [English] Exit normally; return 0 indicates success.
}