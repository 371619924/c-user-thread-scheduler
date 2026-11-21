# C User-Level Thread Scheduler

A Linux user-level threading library with round-robin scheduling, semaphores, and timer-driven preemption.

Team project for UC Davis ECS 150, Winter 2026, Project 2.2. This repository preserves the submitted source and applicable attribution.

## Build

```sh
make -C libuthread
make -C apps queue_tester.x uthread_hello.x sem_simple.x
```


The examples above are available in this snapshot. The original app Makefile also lists an example source file that was absent from the submission.

## Original project notes

================================================================================
Project 2.2 Overall File/Module Comment (English)
================================================================================

This project builds a user-level thread library for Linux in C. The final library
is meant to support:
1) a FIFO queue container (used internally by the thread system),
2) user-level thread creation and round-robin scheduling,
3) semaphore-based synchronization between threads,
4) optional timer/signal-based preemptive scheduling.

In short, this whole assignment is not just “writing a queue” or “writing one
thread function” — it is building the core pieces of a small user-thread runtime.

------------------------------------------------------------------------------
What this project is doing as a whole (big picture)
------------------------------------------------------------------------------
- The library (libuthread) provides the runtime and APIs.
- The apps/ programs are test applications that exercise each phase.
- The queue module is the base container used by other modules.
- The uthread module implements thread management and scheduling.
- The semaphore module implements synchronization using a waiter queue.
- The preemption module upgrades the scheduler from cooperative to timer-driven.
- The Makefile in libuthread builds the static library archive used by apps.

------------------------------------------------------------------------------
How this maps to your uploaded zip (your actual files)
------------------------------------------------------------------------------
Your zip is organized into two main parts:

1) libuthread/   (core library implementation)
   - queue.c          -> FIFO queue implementation (Phase 1 core)
   - uthread.c        -> user-level thread scheduler/runtime (Phase 2 core)
   - sem.c            -> semaphore implementation (Phase 3 core)
   - preempt.c        -> signal/timer preemption support (Phase 4 core)
   - context.c        -> provided low-level context helper (stack/context switch)
   - queue.h          -> queue API declaration
   - uthread.h        -> public thread API declaration
   - sem.h            -> semaphore API declaration
   - private.h        -> internal/private APIs used inside the library
   - Makefile         -> builds libuthread.a (static library)

2) apps/         (test programs / validation programs)
   - queue_tester.c   -> your queue unit tests (create/enqueue/dequeue/delete/
                         iterate/length/null cases, etc.)
   - uthread_hello.c  -> simple smoke test for thread creation/run
   - sem_simple.c     -> simple semaphore scheduling behavior test
   - sem_count.c      -> counting/semaphore alternation test
   - sem_buffer.c     -> producer/consumer style semaphore test
   - sem_prime.c      -> pipeline/prime sieve style semaphore test
   - test_preempt.c   -> preemption-related behavior test
   - Makefile         -> compiles and links test apps against libuthread.a

(Important practical note)
- apps/ are primarily for testing and demonstrating behavior.
- The main graded implementation work is in libuthread/ (especially the starred
  files from the starter spec: queue.c, uthread.c, sem.c, preempt.c, and
  libuthread/Makefile).

------------------------------------------------------------------------------
Project phases (what must be completed)
------------------------------------------------------------------------------
Phase 0: Preliminary setup
- Create/join the correct group repository and clone it locally.
- Understand the folder layout before coding.
- Avoid repo naming/group mistakes (the spec warns about penalties).

Phase 1: Queue API + libuthread Makefile
- Implement the FIFO queue in libuthread/queue.c according to queue.h.
- Keep queue operations O(1) except iterate/delete.
- Complete libuthread/Makefile so it builds libuthread.a.
- Start by building the library with queue.c, then add other .c files later.

Phase 2: uthread API (cooperative scheduling first)
- Implement most thread management in libuthread/uthread.c.
- Create a TCB (thread control block) structure to store:
  - thread context (register state)
  - stack pointer/stack allocation
  - thread state (READY/RUNNING/BLOCKED/EXITED)
- Implement the scheduler behavior around uthread_run(), create, yield, exit.
- Initially scheduling is non-preemptive (threads yield voluntarily).
- Use the provided context API (context.c/private.h) for stack/context handling.

Phase 3: Semaphore API
- Implement semaphores in libuthread/sem.c using sem.h.
- A semaphore tracks:
  - count (available resources)
  - waiters queue (blocked threads waiting for resource)
- sem_down() may block the current thread when count == 0.
- sem_up() releases a resource and may wake one waiting thread.
- Correctly handle the corner case where a thread wakes up but another thread
  takes the resource first (must re-check availability, not assume success).

Phase 4: Preemption
- Implement preemption support in libuthread/preempt.c (private/internal API).
- Set up:
  - SIGVTALRM signal handler
  - timer firing at 100 Hz
- The handler forces a yield to make scheduling fairer.
- Implement preempt_enable()/preempt_disable() to protect critical sections by
  blocking/unblocking SIGVTALRM.
- Use preemption transparently (user code should not need to call it directly).

------------------------------------------------------------------------------
What each big module does, and what its “branches” (subparts/functions) do
------------------------------------------------------------------------------

[A] libuthread/queue.c  (Queue module: foundational container)
Purpose:
- Provides a generic FIFO queue storing void* pointers.
- Used directly in queue tests and indirectly by uthread/sem modules.

Typical subparts/functions:
- queue_create()
  Creates an empty queue object and initializes head/tail/length.
- queue_destroy()
  Frees the queue object (only when empty).
- queue_enqueue()
  Appends one item to the tail in O(1).
- queue_dequeue()
  Removes the head item in O(1) and returns its data pointer.
- queue_delete()
  Removes a specific data pointer from the queue (search-based).
- queue_iterate()
  Walks through queued items and applies a callback function to each one.
- queue_length()
  Returns the current number of queued elements.

Internal design branch:
- queue_item (linked-list node) stores:
  - data (void*)
  - next pointer
- queue struct stores:
  - head
  - tail
  - length
This design is exactly what enables O(1) enqueue/dequeue.

[B] libuthread/uthread.c  (User thread runtime + scheduler)
Purpose:
- Implements the main user-level threading behavior.
- Manages which thread is currently running and which are ready/blocked.

Typical subparts/functions:
- struct uthread_tcb
  Per-thread metadata (context, stack, state).
- current_thread (global)
  Tracks the currently running thread.
- ready queue (global queue_t)
  Stores runnable threads waiting for CPU time.
- uthread_current()
  Returns the current TCB pointer (used internally by semaphores, etc.).
- uthread_run(preempt, func, arg)
  Library entry point:
  - initializes runtime
  - optionally starts preemption
  - creates initial user thread
  - runs scheduling loop until no ready threads remain
- uthread_create(func, arg)
  Allocates a TCB + stack, initializes context, enqueues new thread.
- uthread_yield()
  Cooperative scheduling step:
  - current thread voluntarily gives up CPU
  - scheduler switches to next READY thread
- uthread_exit()
  Marks current thread finished and switches away without re-queuing it.
- uthread_block()
  Moves current thread to BLOCKED state and switches to another READY thread.
- uthread_unblock(t)
  Marks a blocked thread READY and puts it back in the ready queue.

Design branch inside this module:
- Thread states (READY/RUNNING/BLOCKED/EXITED) control scheduling logic.
- Context switching is delegated to provided context helpers in context.c.

[C] libuthread/sem.c  (Semaphore synchronization module)
Purpose:
- Synchronizes threads accessing shared resources.
- Prevents unsafe concurrent access by controlling entry through a count.

Typical subparts/functions:
- struct semaphore
  Stores:
  - count   (how many resources are available)
  - waiters (queue of blocked threads)
- sem_create(count)
  Creates a semaphore with an initial count and an empty waiters queue.
- sem_destroy()
  Destroys the semaphore (typically only when no waiters remain).
- sem_down()
  “Acquire” operation:
  - if count > 0: decrement and continue
  - if count == 0: block current thread and wait
  - must be written carefully to handle wakeup race/corner case
- sem_up()
  “Release” operation:
  - increment count
  - wake one waiter (if any), making it eligible to run again

Design branch inside this module:
- Uses uthread_block()/uthread_unblock() from the private thread API.
- Uses a queue to keep waiters in an ordered waiting list.

[D] libuthread/preempt.c  (Preemption / timer interrupt simulation)
Purpose:
- Adds timer-driven scheduling so threads don’t have to cooperate perfectly.
- Makes the user-level scheduler more realistic and safer.

Typical subparts/functions:
- preempt state globals
  - preempt_active flag
  - SIGVTALRM signal mask set
  - saved old signal action
  - saved old timer configuration
- alarm_handler(int)
  Runs when timer fires; forces a yield.
- preempt_start(bool preempt)
  If preemption is enabled:
  - installs SIGVTALRM handler (using sigaction)
  - configures virtual timer (100 Hz)
- preempt_stop()
  Restores previous signal handler and timer setup.
- preempt_disable()
  Blocks SIGVTALRM to protect critical sections.
- preempt_enable()
  Unblocks SIGVTALRM after critical section is safe again.

Design branch inside this module:
- Critical sections in queue/thread/semaphore internals may need preemption
  disabled to avoid inconsistent shared state.

[E] libuthread/Makefile  (Build module)
Purpose:
- Builds the static library archive: libuthread.a
- This archive is linked by the apps/ test programs.

Subparts:
- compilation rules for .o files
- archive creation rule (ar) for libuthread.a
- default target should produce libuthread.a
- as phases progress, add queue.c / uthread.c / sem.c / preempt.c into the lib

[F] apps/ test programs  (Validation modules)
Purpose:
- These are not the core runtime implementation.
- They are test drivers to validate correctness phase by phase.

Subparts/examples:
- queue_tester.c
  Queue unit tests (normal cases + edge cases + iterate/delete callbacks).
- uthread_hello.c
  Basic “can the thread library start and run one thread?” smoke test.
- sem_simple.c / sem_count.c / sem_buffer.c / sem_prime.c
  Increasingly more realistic semaphore stress tests.
- test_preempt.c
  Preemption behavior check (timer-driven scheduling interactions).

------------------------------------------------------------------------------
How the modules depend on each other (important for understanding the project)
------------------------------------------------------------------------------
Dependency flow:
1) queue.c  -> foundational generic container
2) uthread.c -> uses queue for ready thread scheduling
3) sem.c    -> uses queue for waiters + uses uthread block/unblock
4) preempt.c -> interacts with uthread scheduler and protects internals
5) apps/    -> call public APIs (queue/uthread/sem) to test everything

So if queue is wrong, later phases (uthread/sem) often fail in confusing ways.
That’s why strong queue tests are very important.

------------------------------------------------------------------------------
What to keep in mind while writing comments/documentation for this project
------------------------------------------------------------------------------
- Distinguish public API vs private/internal helper:
  - Public: uthread.h, sem.h, queue.h
  - Private/internal: private.h, preempt internals, TCB internals
- Distinguish cooperative scheduling (yield/block) vs preemptive scheduling
  (timer interrupt via SIGVTALRM).
- Explain data-structure role, not just code syntax:
  e.g., “ready queue” vs “waiters queue”.
- Mention critical sections when preemption is enabled.
- Mention queue O(1) requirement when describing queue implementation.
- Mention apps as tests, not the primary library implementation.

================================================================================
项目 2.2 总体文件/模块注释（中文）
================================================================================

这个作业是在 C 语言里实现一个 Linux 的“用户级线程库”（user-level thread
library）。最终你做出来的不是单一功能，而是一整套小型运行时系统，包含：

1）FIFO 队列（线程库内部基础容器）
2）用户级线程创建与轮转调度（round-robin）
3）信号量同步（semaphore）
4）基于定时器/信号的抢占式调度（preemption）

也就是说，这个作业整体目标是“搭一个能跑线程、能切换线程、能同步线程、
还能定时打断线程的迷你线程库”。

------------------------------------------------------------------------------
这个作业整体在做什么（总作用）
------------------------------------------------------------------------------
- libuthread/ 是核心库：真正要实现的运行时逻辑在这里。
- apps/ 是测试程序：用来验证每一阶段是否正确。
- queue 模块是基础：后面的线程调度队列、信号量等待队列都依赖它。
- uthread 模块负责线程管理与调度。
- sem 模块负责线程同步（等待/唤醒）。
- preempt 模块负责“抢占式”调度（信号 + 定时器）。
- libuthread/Makefile 负责把这些代码打包成静态库 libuthread.a 给 apps 链接。

------------------------------------------------------------------------------
结合你这次上传的 zip（你的实际文件）来看
------------------------------------------------------------------------------
你的 zip 里主要也是两大块：

1) libuthread/（核心线程库）
   - queue.c       -> 队列实现（Phase 1 核心）
   - uthread.c     -> 用户级线程运行时/调度器（Phase 2 核心）
   - sem.c         -> 信号量实现（Phase 3 核心）
   - preempt.c     -> 抢占式调度实现（Phase 4 核心）
   - context.c     -> 已提供的上下文切换底层辅助代码
   - queue.h       -> 队列 API 声明
   - uthread.h     -> 线程库公开 API 声明
   - sem.h         -> 信号量 API 声明
   - private.h     -> 库内部用的私有 API/结构声明
   - Makefile      -> 生成 libuthread.a

2) apps/（测试程序）
   - queue_tester.c  -> 你的队列单元测试（含 null/length/delete/iterate 等）
   - uthread_hello.c -> 线程库最基础冒烟测试（能否创建并跑线程）
   - sem_simple.c    -> 简单信号量调度测试
   - sem_count.c     -> 计数/交替执行测试
   - sem_buffer.c    -> 生产者-消费者风格测试
   - sem_prime.c     -> 管道式 prime sieve 测试
   - test_preempt.c  -> 抢占式调度相关测试
   - Makefile        -> 编译并链接这些测试程序

（很重要）
- apps/ 主要是“测试”和“验证”用，不是核心实现模块。
- 真正作业主干是在 libuthread/ 里（尤其是 queue.c / uthread.c / sem.c /
  preempt.c / libuthread/Makefile）。

------------------------------------------------------------------------------
作业分阶段（所有要完成的部分）
------------------------------------------------------------------------------
Phase 0：前期准备
- 正确创建/加入组队仓库并 clone 到本地
- 先看懂目录结构再开始写代码
- 仓库命名或分组错误会有扣分风险（作业说明里明确提醒）

Phase 1：Queue API + libuthread Makefile
- 在 libuthread/queue.c 中实现 FIFO 队列（按 queue.h 的 API）
- 除了 iterate/delete 以外，其它操作要满足 O(1)
- 完成 libuthread/Makefile，生成 libuthread.a
- 一开始先把 queue.c 编进库，后面再逐步加其它模块

Phase 2：uthread API（先做非抢占式 cooperative）
- 在 libuthread/uthread.c 实现大部分线程管理逻辑
- 设计 TCB（线程控制块）保存：
  - 上下文（寄存器状态）
  - 栈信息
  - 线程状态（READY/RUNNING/BLOCKED/EXITED）
- 实现 uthread_run / create / yield / exit 等调度逻辑
- 这一阶段默认是非抢占式：线程要主动 yield 才会切走
- 使用已提供的 context API 做上下文初始化与切换

Phase 3：Semaphore API
- 在 libuthread/sem.c 实现信号量（按 sem.h）
- 信号量内部要维护：
  - count（可用资源数）
  - waiters（等待该资源的线程队列）
- sem_down() 可能会阻塞当前线程
- sem_up() 释放资源并唤醒等待线程
- 必须处理 corner case：
  被唤醒线程恢复执行时，资源可能已被别的线程先抢走，不能直接假设成功

Phase 4：Preemption（抢占式调度）
- 在 libuthread/preempt.c 实现抢占逻辑（私有 API）
- 用 SIGVTALRM + timer（100Hz）周期性触发
- 信号处理函数里强制当前线程 yield，提升公平性
- 实现 preempt_enable()/preempt_disable()，通过屏蔽/解除 SIGVTALRM 来保护
  临界区，避免库内部状态被抢占打坏
- 这个模块对“用户线程代码”应当是透明的（用户不需要自己调用这些内部函数）

------------------------------------------------------------------------------
每个大模块在做什么，以及模块里的“分支/子部分”在做什么
------------------------------------------------------------------------------

[A] libuthread/queue.c（队列模块：基础容器）
作用：
- 提供一个通用 FIFO 队列，元素类型是 void*（泛型指针）
- 既用于 queue 测试，也会被后续 uthread / sem 模块复用

主要子部分（函数）：
- queue_create()
  创建空队列，初始化 head/tail/length
- queue_destroy()
  销毁空队列对象（通常要求队列已空）
- queue_enqueue()
  O(1) 尾插入
- queue_dequeue()
  O(1) 头删除并返回数据指针
- queue_delete()
  删除指定 data 指针对应的队列项（需要查找）
- queue_iterate()
  遍历当前队列，对每个元素调用回调函数
- queue_length()
  返回当前队列长度

内部数据结构分支：
- queue_item（链表节点）：保存 data + next
- queue（队列对象）：保存 head / tail / length
这个“头尾指针 + 长度”的设计就是满足 O(1) enqueue/dequeue 的关键。

[B] libuthread/uthread.c（线程运行时 + 调度器）
作用：
- 实现用户级线程的核心行为
- 管理当前线程是谁、哪些线程可运行、哪些线程被阻塞

主要子部分（函数/结构）：
- struct uthread_tcb
  每个线程的控制块（上下文、栈、状态）
- current_thread（全局）
  记录当前正在运行的线程
- ready queue（全局 queue_t）
  存放 READY 状态、等待 CPU 的线程
- uthread_current()
  返回当前线程 TCB（给 sem 等内部模块使用）
- uthread_run(preempt, func, arg)
  线程库入口：
  - 初始化运行时
  - 按参数决定是否启动 preemption
  - 创建初始线程
  - 循环调度直到没有可运行线程
- uthread_create(func, arg)
  创建线程：分配 TCB、分配栈、初始化上下文、加入 ready queue
- uthread_yield()
  协作式调度切换：当前线程主动让出 CPU，切到下一个 READY 线程
- uthread_exit()
  当前线程退出，不再回到 ready queue
- uthread_block()
  当前线程进入 BLOCKED 状态，并切换到别的 READY 线程
- uthread_unblock(t)
  把被阻塞线程改回 READY 并重新入队

模块内部设计分支：
- 线程状态机（READY/RUNNING/BLOCKED/EXITED）决定调度行为
- 真正的寄存器/栈上下文切换由 context.c 提供的辅助函数完成

[C] libuthread/sem.c（信号量同步模块）
作用：
- 控制多个线程对共享资源的访问顺序
- 避免并发访问导致的错误

主要子部分（函数/结构）：
- struct semaphore
  内部保存：
  - count（资源数量）
  - waiters（等待线程队列）
- sem_create(count)
  创建信号量并设置初始 count
- sem_destroy()
  销毁信号量（通常等待队列要为空）
- sem_down()
  获取资源操作：
  - count > 0：减一并继续
  - count == 0：阻塞当前线程，进入等待队列
  - 必须正确处理被唤醒后资源已被抢走的情况
- sem_up()
  释放资源操作：
  - count 加一
  - 若有等待线程，唤醒一个线程让它后续可运行

模块内部设计分支：
- 依赖 uthread_block()/uthread_unblock() 实现阻塞与唤醒
- 依赖队列保存 waiters（等待线程）

[D] libuthread/preempt.c（抢占式调度模块）
作用：
- 给线程库增加“定时器驱动”的调度能力
- 避免线程不主动 yield 时一直霸占 CPU

主要子部分（函数/结构）：
- 全局状态
  - preempt_active（是否启用抢占）
  - SIGVTALRM 屏蔽集合
  - 旧 signal handler 备份
  - 旧 timer 配置备份
- alarm_handler(int)
  定时器信号到达时执行，触发 yield
- preempt_start(bool preempt)
  若启用抢占：
  - 安装 SIGVTALRM handler（必须用 sigaction）
  - 配置 100Hz 的虚拟定时器
- preempt_stop()
  恢复之前的 signal 和 timer 配置
- preempt_disable()
  屏蔽 SIGVTALRM，进入临界区保护
- preempt_enable()
  解除 SIGVTALRM 屏蔽，退出临界区保护

模块内部设计分支：
- 当库正在修改关键共享结构（如 ready queue、waiters queue、线程状态）
  时，需要暂时禁止 preemption，避免中间状态被打断。

[E] libuthread/Makefile（构建模块）
作用：
- 把 libuthread 下的实现文件编译并打包成静态库 libuthread.a
- apps/ 下测试程序会链接这个库来运行

子部分：
- .o 编译规则
- 静态库打包规则（ar）
- 默认目标应为 libuthread.a
- 随着阶段推进，把 queue / uthread / sem / preempt 逐步加入库

[F] apps/ 测试程序（验证模块）
作用：
- 不是核心实现，而是“验证你的库是否正确”的程序集合
- 每个测试对应不同阶段/功能点

子部分示例：
- queue_tester.c
  队列单元测试（正常流程 + 边界情况 + iterate/delete 回调）
- uthread_hello.c
  基础冒烟测试（能不能启动线程库并运行一个线程）
- sem_simple / sem_count / sem_buffer / sem_prime
  不同复杂度的信号量测试（同步、交替、生产者消费者、管道）
- test_preempt.c
  抢占式调度相关测试（定时器触发、切换行为等）

------------------------------------------------------------------------------
模块依赖关系（这个很重要，方便你写注释时把层次讲清楚）
------------------------------------------------------------------------------
依赖顺序基本是：
1) queue.c     -> 基础容器
2) uthread.c   -> 用 queue 做 ready queue
3) sem.c       -> 用 queue 做 waiters + 调用 uthread 的 block/unblock
4) preempt.c   -> 和 uthread 调度协作，并保护临界区
5) apps/       -> 调用公开 API 来测试整个库

所以 queue 写错，后面 uthread/sem 经常会连带出错（而且很难排查）。
这也是为什么作业强调 queue 阶段和测试覆盖率。

------------------------------------------------------------------------------
写这个作业“总注释”时建议强调的点（你后面可以继续用这套框架）
------------------------------------------------------------------------------
- 区分“公开 API”和“私有实现”：
  - 公开：queue.h / uthread.h / sem.h
  - 私有：private.h、TCB 细节、preempt 内部逻辑
- 区分“协作式调度”和“抢占式调度”
- 注释要写“这个结构在系统里扮演什么角色”，不只是语法解释
  （例如 ready queue 是调度队列，waiters queue 是同步等待队列）
- preemption 开启后要强调临界区保护（disable/enable）
- queue 模块要强调 O(1) 约束（除了 iterate/delete）
- apps 模块要强调“测试用途”，不是最终核心库实现
