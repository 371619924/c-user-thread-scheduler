#include <stddef.h>   // [中文] 引入 size_t 的定义（无符号大小类型），本文件中 semaphore 的 count 使用 size_t。 
                      // [English] Include the definition of size_t, used for the semaphore count field.

#include <stdlib.h>   // [中文] 引入 malloc/free 的声明，用于创建和释放信号量对象。
                      // [English] Include declarations for malloc/free, used to allocate and free semaphore objects.

#include "private.h"  // [中文] 引入线程库内部接口（如 uthread_current/uthread_block/uthread_unblock/preempt_* 等内部函数声明）。
                      // [English] Include internal thread-library interfaces (e.g., uthread_current/uthread_block/uthread_unblock/preempt_*).
#include "queue.h"    // [中文] 引入通用队列接口，信号量用它维护等待线程队列（waiters）。
                      // [English] Include the generic queue interface; semaphores use it to maintain the waiters queue.
#include "sem.h"      // [中文] 引入信号量对外接口定义（sem_t、函数原型等）。
                      // [English] Include the public semaphore interface (sem_t and function prototypes).

/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这个文件实现了本次作业中的“信号量（semaphore）”同步模块，是 uthread 线程库里用于
 * 线程同步与互斥控制的核心组件之一。它建立在两个基础模块之上：
 * 1) uthread 线程管理/调度（阻塞、唤醒、当前线程获取、抢占开关）
 * 2) queue 通用队列（维护等待中的线程队列）
 *
 * 【这个文件在整个作业里的作用（很重要）】
 * 1) 它负责提供同步原语（Synchronization Primitive）
 *    - sem_down()：也叫 P / wait，尝试获取资源；资源不足时阻塞当前线程
 *    - sem_up()  ：也叫 V / signal，释放资源；如果有等待线程则唤醒一个
 *    这正是后续多线程协作（生产者-消费者、临界区互斥等）的基础。
 *
 * 2) 它把“线程调度”和“等待队列”连接起来
 *    - sem->count 表示当前可用资源数量
 *    - sem->waiters 保存因为资源不足而被阻塞的线程（TCB 指针）
 *    当资源释放时（sem_up），从 waiters 取出一个线程并唤醒。
 *
 * 3) 它是作业后续测试（尤其是同步相关测试）的关键模块
 *    - 如果 sem_down/sem_up 的逻辑或原子性处理错误，
 *      很容易出现死锁、丢唤醒、计数错误、线程卡住等问题。
 *    - 所以这里会配合 preempt_disable()/preempt_enable() 来保护关键区，
 *      避免在更新 count 或 waiters 队列时被时钟中断抢占。
 *
 * 【本文件内部维护的核心状态】
 * - count   : 当前可用资源数（非负）
 * - waiters : 等待该信号量资源的线程队列（队列中存放的是 struct uthread_tcb*）
 *
 * 【设计说明】
 * - sem_create() 创建信号量并初始化等待队列
 * - sem_destroy() 仅在没有等待线程时允许销毁
 * - sem_down() 在 count==0 时将当前线程加入 waiters 并阻塞；被唤醒后重新检查条件
 * - sem_up() 增加 count，并在有等待线程时唤醒一个线程
 * - 使用 while (不是 if) 检查 count==0，保证线程被唤醒后会重新确认条件
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This file implements the semaphore synchronization module for the uthread
 * assignment. It is a core synchronization primitive built on top of:
 * 1) the uthread scheduler/runtime (blocking, unblocking, current thread, preemption control)
 * 2) the generic queue module (to store waiting threads)
 *
 * [Why this file matters in the overall assignment]
 * 1) It provides the synchronization primitive used for thread coordination:
 *    - sem_down(): P / wait operation (acquire a resource or block)
 *    - sem_up()  : V / signal operation (release a resource and wake a waiter)
 *    This is essential for mutual exclusion and producer-consumer style patterns.
 *
 * 2) It connects scheduling and waiting queues:
 *    - sem->count tracks available resources
 *    - sem->waiters stores blocked threads waiting for the semaphore
 *    When a resource is released (sem_up), one waiting thread can be unblocked.
 *
 * 3) It is critical for synchronization tests:
 *    - Bugs here can cause deadlocks, lost wakeups, incorrect counts, or stuck threads.
 *    - preempt_disable()/preempt_enable() protect critical sections so updates to
 *      count and waiters are not interrupted by timer preemption.
 *
 * [Core state maintained in this file]
 * - count   : number of currently available resources
 * - waiters : queue of waiting threads (stores struct uthread_tcb* pointers)
 *
 * [Design notes]
 * - sem_create() allocates and initializes a semaphore and its wait queue
 * - sem_destroy() only succeeds when no threads are waiting
 * - sem_down() blocks the current thread when count==0, and rechecks after wakeup
 * - sem_up() increments count and wakes one waiting thread if present
 * - The count check uses while (not if) so the condition is rechecked after waking
 * ============================================================================
 */

struct semaphore {
    size_t count;    // [中文] 当前信号量可用资源数量（token 数）。 [English] Number of available semaphore tokens/resources.
    queue_t waiters; // [中文] 等待该信号量的线程队列（保存线程 TCB 指针）。 [English] Queue of threads waiting on this semaphore (stores TCB pointers).
};

/*
 * sem_create
 * ----------------------------------------------------------------------------
 * [中文] 创建一个信号量对象，并初始化：
 * - count 为传入值（初始资源数）
 * - waiters 为一个空队列（用于存放等待线程）
 *
 * 用法：
 *   sem_t s = sem_create(1);   // 常见：1 表示二元信号量（类似 mutex）
 *   sem_t s2 = sem_create(3);  // 初始有 3 个资源
 *
 * 返回值：
 *   - 成功：返回信号量指针
 *   - 失败：返回 NULL（malloc 或 queue_create 失败）
 *
 * [English] Create a semaphore object and initialize:
 * - count with the given initial value
 * - waiters as an empty queue for blocked threads
 *
 * Usage:
 *   sem_t s = sem_create(1);   // common: binary semaphore (mutex-like)
 *   sem_t s2 = sem_create(3);  // 3 initial resources
 *
 * Return:
 *   - Success: semaphore pointer
 *   - Failure: NULL (malloc or queue_create fails)
 */
sem_t sem_create(size_t count)
{
    sem_t sem = malloc(sizeof(*sem)); 
    // [中文] 为 semaphore 结构体分配内存；sizeof(*sem) 写法更稳健。
    // [English] Allocate memory for the semaphore structure; sizeof(*sem) is a robust style.

    if (!sem) {                    
        // [中文] 如果分配失败，返回 NULL。
        // [English] If allocation fails, return NULL.
        return NULL;
    }

    sem->count = count;            
    // [中文] 设置初始资源数量（调用者传入）。
    // [English] Set the initial resource count from the caller.

    sem->waiters = queue_create(); 
    // [中文] 创建等待队列，用于存放在 sem_down 中阻塞的线程。
    // [English] Create the waiters queue for threads blocked in sem_down.

    if (!sem->waiters) {           
        // [中文] 如果等待队列创建失败，需要回收前面已经分配的 sem，避免内存泄漏。
        // [English] If creating the waiters queue fails, free sem to avoid a memory leak.
        free(sem);                 
        // [中文] 释放 sem 结构体内存。
        // [English] Free the semaphore structure.
        return NULL;               
        // [中文] 返回 NULL 表示创建失败。
        // [English] Return NULL to indicate creation failure.
    }

    return sem;                    
    // [中文] 创建成功，返回初始化好的信号量对象。
    // [English] Creation succeeded; return the initialized semaphore.
}

/*
 * sem_destroy
 * ----------------------------------------------------------------------------
 * [中文] 销毁信号量对象。
 * 注意：通常只有在没有等待线程时才允许销毁（本实现通过 waiters 队列长度判断）。
 *
 * 用法：
 *   if (sem_destroy(s) == 0) { ... }
 *
 * 参数：
 *   sem - 要销毁的信号量
 *
 * 返回值：
 *   - 0  : 成功销毁
 *   - -1 : sem 为 NULL，或仍有等待线程（waiters 非空）
 *
 * [English] Destroy a semaphore object.
 * Note: This implementation only allows destruction when no threads are waiting
 * (checked via the waiters queue length).
 *
 * Usage:
 *   if (sem_destroy(s) == 0) { ... }
 *
 * Parameter:
 *   sem - semaphore to destroy
 *
 * Return:
 *   - 0  : success
 *   - -1 : sem is NULL or there are still waiting threads
 */
int sem_destroy(sem_t sem)
{
    if (!sem) {                    
        // [中文] 空指针非法，无法销毁。
        // [English] NULL semaphore pointer is invalid; cannot destroy.
        return -1;
    }

    if (queue_length(sem->waiters) != 0) { 
        // [中文] 如果等待队列非空，说明仍有线程在等待该信号量，按规范不应销毁。
        // [English] If the waiters queue is not empty, threads are still waiting; destruction is not allowed.
        return -1;
    }

    queue_destroy(sem->waiters);   
    // [中文] 销毁内部等待队列（此时队列应为空）。
    // [English] Destroy the internal waiters queue (it should be empty here).

    free(sem);                     
    // [中文] 释放 semaphore 结构体本身。
    // [English] Free the semaphore structure itself.

    return 0;                      
    // [中文] 返回 0 表示销毁成功。
    // [English] Return 0 to indicate success.
}

/*
 * sem_down
 * ----------------------------------------------------------------------------
 * [中文] 信号量的 P / wait 操作（获取资源）。
 *
 * 逻辑：
 * 1) 进入关键区（preempt_disable），防止在修改 count / waiters 时被抢占
 * 2) 如果 count == 0，说明没有可用资源：
 *    - 获取当前线程 TCB（uthread_current）
 *    - 放入 waiters 队列
 *    - 调用 uthread_block() 阻塞自己，等待 sem_up 唤醒
 * 3) 被唤醒后重新检查（while），直到 count > 0
 * 4) count--，消费一个资源
 * 5) 离开关键区（preempt_enable）
 *
 * 为什么用 while 而不是 if？
 * - 因为线程被唤醒后需要重新确认条件（count 是否真的可用）
 * - 这是同步原语里常见的“重新检查条件”写法
 *
 * 用法：
 *   sem_down(s); // 若无资源则阻塞，直到可获取
 *
 * 返回值：
 *   - 0  : 成功获取一个资源
 *   - -1 : sem 为 NULL，或等待入队失败（极少见，通常是内存问题）
 *
 * [English] Semaphore P / wait operation (acquire a resource).
 *
 * Behavior:
 * 1) Enter critical section (preempt_disable) to avoid races while updating
 *    count/waiters
 * 2) If count == 0 (no resource available):
 *    - get current thread TCB via uthread_current()
 *    - enqueue it into waiters
 *    - block the thread via uthread_block() until sem_up wakes it
 * 3) After wakeup, recheck the condition (while) until count > 0
 * 4) Decrement count to consume one resource
 * 5) Leave critical section (preempt_enable)
 *
 * Why while instead of if?
 * - The thread must recheck the condition after waking
 * - This is a standard synchronization pattern
 *
 * Usage:
 *   sem_down(s); // blocks if needed until a resource is available
 *
 * Return:
 *   - 0  : acquired successfully
 *   - -1 : sem is NULL, or enqueue to waiters fails
 */
int sem_down(sem_t sem)
{
    if (!sem) {                    
        // [中文] 参数检查：sem 不能为空。
        // [English] Validate input: sem must not be NULL.
        return -1;
    }

    preempt_disable();             
    // [中文] 关闭抢占（进入关键区），保证下面对 count 和 waiters 的操作具有原子性。
    // [English] Disable preemption (enter critical section) so updates to count and waiters are atomic.

    while (sem->count == 0) {      
        // [中文] 当资源数为 0 时，当前线程不能继续，需要进入等待队列并阻塞。
        // [English] While no resources are available, the current thread must wait.

        struct uthread_tcb *self = uthread_current(); 
        // [中文] 获取当前正在运行的线程的 TCB 指针（线程控制块）。
        // [English] Get the TCB pointer for the currently running thread.

        if (queue_enqueue(sem->waiters, self) == -1) { 
            // [中文] 将当前线程加入该信号量的等待队列。
            //        若入队失败（例如内存分配失败），无法安全等待，返回错误。
            // [English] Enqueue the current thread into the semaphore's waiters queue.
            //           If enqueue fails (e.g., allocation failure), return an error.

            preempt_enable();      
            // [中文] 发生错误前先恢复抢占状态，避免系统一直处于“关闭抢占”状态。
            // [English] Re-enable preemption before returning error to avoid leaving it disabled.

            return -1;             
            // [中文] 返回错误。
            // [English] Return error.
        }

        uthread_block();           
        // [中文] 阻塞当前线程并切换到其他线程运行。
        //        当未来某个 sem_up() 唤醒该线程后，它会从这里“返回继续执行”。
        // [English] Block the current thread and yield control to another thread.
        //           When a future sem_up() unblocks this thread, execution resumes here.
    }

    sem->count--;                  
    // [中文] 走到这里说明 count > 0，当前线程成功获取一个资源，计数减 1。
    // [English] Reaching here means count > 0; consume one resource by decrementing count.

    preempt_enable();              
    // [中文] 离开关键区，恢复抢占。
    // [English] Leave the critical section and re-enable preemption.

    return 0;                      
    // [中文] 返回 0 表示 sem_down 成功。
    // [English] Return 0 to indicate success.
}

/*
 * sem_up
 * ----------------------------------------------------------------------------
 * [中文] 信号量的 V / signal 操作（释放资源）。
 *
 * 逻辑：
 * 1) 进入关键区（preempt_disable）
 * 2) count++，表示释放一个资源
 * 3) 如果有等待线程（waiters 非空）：
 *    - 从 waiters 队列取出一个线程
 *    - 调用 uthread_unblock() 唤醒它
 * 4) 离开关键区（preempt_enable）
 *
 * 注意：
 * - 这里一次 sem_up() 最多唤醒一个等待线程（常见语义）
 * - 被唤醒线程在 sem_down() 中恢复执行后，会重新检查 count，并在合适时 count--
 *
 * 用法：
 *   sem_up(s); // 释放一个资源，并可能唤醒一个等待线程
 *
 * 返回值：
 *   - 0  : 成功
 *   - -1 : sem 为 NULL
 *
 * [English] Semaphore V / signal operation (release a resource).
 *
 * Behavior:
 * 1) Enter critical section (preempt_disable)
 * 2) Increment count to release one resource
 * 3) If there are waiters:
 *    - dequeue one waiting thread
 *    - unblock it via uthread_unblock()
 * 4) Leave critical section (preempt_enable)
 *
 * Notes:
 * - One sem_up() wakes at most one waiting thread (standard behavior)
 * - The woken thread resumes in sem_down(), rechecks the condition, and then
 *   decrements count when it acquires the resource
 *
 * Usage:
 *   sem_up(s); // release one resource; may wake one waiting thread
 *
 * Return:
 *   - 0  : success
 *   - -1 : sem is NULL
 */
int sem_up(sem_t sem)
{
    if (!sem) {                    
        // [中文] 参数检查：sem 不能为空。
        // [English] Validate input: sem must not be NULL.
        return -1;
    }

    preempt_disable();             
    // [中文] 关闭抢占，保护 count 和 waiters 的更新操作。
    // [English] Disable preemption to protect updates to count and waiters.

    sem->count++;                  
    // [中文] 释放一个资源：可用资源数加 1。
    // [English] Release one resource by incrementing the available count.

    if (queue_length(sem->waiters) > 0) {  
        // [中文] 如果存在等待线程，则尝试唤醒一个。
        // [English] If there are waiting threads, try to wake one.

        struct uthread_tcb *t = NULL;      
        // [中文] 用于接收从等待队列出队的线程 TCB 指针。
        // [English] Will hold the TCB pointer dequeued from the waiters queue.

        if (queue_dequeue(sem->waiters, (void **)&t) == 0 && t) { 
            // [中文] 从等待队列取出一个线程（FIFO 顺序），并确保取出的指针有效。
            //        这里把 &t 转成 (void **) 是因为 queue_dequeue 的接口是通用 void**。
            // [English] Dequeue one thread from the waiters queue (FIFO order) and ensure it's valid.
            //           &t is cast to (void **) because queue_dequeue is a generic void** API.

            uthread_unblock(t);    
            // [中文] 将该等待线程唤醒，使其重新进入可运行状态（通常会回到 ready queue）。
            // [English] Unblock the waiting thread, making it runnable again (typically re-added to ready queue).
        }
    }

    preempt_enable();              
    // [中文] 恢复抢占，离开关键区。
    // [English] Re-enable preemption and leave the critical section.

    return 0;                      
    // [中文] 返回 0 表示 sem_up 成功。
    // [English] Return 0 to indicate success.
}