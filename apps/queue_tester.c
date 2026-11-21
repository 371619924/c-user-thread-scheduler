#include <assert.h>   // [中文] 引入 assert 宏（本文件当前没有直接使用，但测试文件模板里常保留）。 
                      // [English] Include the assert macro (not directly used here, but often kept in test templates).

#include <stdio.h>    // [中文] 引入标准输入输出函数（printf / fprintf），本文件大量用于打印测试信息。 
                      // [English] Include standard I/O functions (printf / fprintf), heavily used for test output.

#include <stdlib.h>   // [中文] 引入 exit 函数声明；测试断言失败时会用 exit(1) 立即终止。 
                      // [English] Include exit declaration; tests use exit(1) to stop immediately on failure.

#include <queue.h>    // [中文] 引入队列模块接口（queue_t、queue_create、queue_enqueue 等），这是被测试的对象。 
                      // [English] Include the queue module API (queue_t, queue_create, queue_enqueue, etc.), which is what this file tests.

/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这个文件是本次作业中 apps/ 目录下的“队列测试程序（queue tester）”，用于测试你在
 * libuthread/queue.c 中实现的通用队列功能是否正确。
 *
 * 【它在整个作业里的作用（非常重要）】
 * 1) 这是 queue 模块的功能验证程序（通常是 Phase 1 的重点）
 *    - 它会逐项测试 queue_create / destroy / enqueue / dequeue / delete /
 *      iterate / length 等接口是否符合要求。
 *    - 这些测试是你确认 queue.c 正确性的第一道关卡。
 *
 * 2) 它是后续 uthread / semaphore 模块的“前置保障”
 *    - 后面的线程调度（ready queue）和同步等待队列（waiters）都依赖 queue.c。
 *    - 如果 queue 实现有 bug，后面 uthread.c / sem.c 的错误会很难排查。
 *    - 所以先把这个 tester 跑通非常关键。
 *
 * 3) 它也用于你对照作业要求补充“自定义测试用例”
 *    - 上面前两个测试通常是作业给的样例测试
 *    - 下面是你自己补充的边界测试（NULL、空队列、删除中间元素、iterate + delete 等）
 *    - 这些额外测试可以帮助你覆盖更多 corner cases，提高通过率与稳定性。
 *
 * 【本文件的测试风格】
 * - 使用 TEST_ASSERT(...) 宏统一打印 PASS/FAIL
 * - 一旦某个断言失败，立即 exit(1) 停止测试，方便定位第一个错误
 * - 每个 test_* 函数测试一个功能点，main() 负责顺序调用
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This file is the queue test program in the apps/ directory. It is used to
 * verify whether your queue implementation in libuthread/queue.c is correct.
 *
 * [Why this file matters in the overall assignment]
 * 1) It is the functional validator for the queue module (often a key part of Phase 1)
 *    - It tests queue_create / destroy / enqueue / dequeue / delete / iterate /
 *      length against the assignment requirements.
 *    - It is the first checkpoint before moving on.
 *
 * 2) It is a prerequisite for later uthread/semaphore correctness
 *    - The scheduler ready queue and semaphore wait queues depend on queue.c.
 *    - A bug in queue often causes confusing failures later in uthread.c / sem.c.
 *    - So getting this tester to pass is essential.
 *
 * 3) It is also where you add your own custom test cases
 *    - The first few tests are usually provided examples
 *    - The rest are your additional edge/corner-case tests
 *    - These help improve confidence and coverage
 *
 * [Testing style used in this file]
 * - TEST_ASSERT(...) macro prints PASS/FAIL consistently
 * - On failure, the test exits immediately with exit(1)
 * - Each test_* function targets one feature; main() runs them in order
 * ============================================================================
 */

/* 
 * [中文] 定义统一的测试断言宏 TEST_ASSERT。
 * 作用：打印当前断言表达式，若为真打印 PASS，否则打印 FAIL 并退出程序。
 *
 * [English] Define a unified test assertion macro TEST_ASSERT.
 * It prints the asserted expression, shows PASS if true, otherwise FAIL and exits.
 */

/* [中文] 宏定义开始：TEST_ASSERT(assert) 接收一个表达式参数。 
 * [English] Macro definition starts: TEST_ASSERT(assert) takes an expression.
 */
#define TEST_ASSERT(assert)                                                    \
    /* [中文] do { ... } while (0) 写法让宏像“单条语句”一样安全使用。          */ \
    /* [English] do { ... } while (0) makes the macro behave like one statement. */ \
    do {                                                                       \
        /* [中文] 打印正在测试的断言文本（#assert 会把表达式转成字符串）。      */ \
        /* [English] Print the assertion text (#assert stringifies the expression). */ \
        printf("ASSERT: " #assert " ... ");                                    \
        /* [中文] 判断断言表达式真假。                                          */ \
        /* [English] Evaluate whether the assertion expression is true.         */ \
        if (assert) {                                                          \
            /* [中文] 断言为真，打印 PASS。                                     */ \
            /* [English] If true, print PASS.                                   */ \
            printf("PASS\n");                                                  \
        } else {                                                               \
            /* [中文] 断言为假，打印 FAIL。                                     */ \
            /* [English] If false, print FAIL.                                  */ \
            printf("FAIL\n");                                                  \
            /* [中文] 失败后立即退出，方便定位第一个失败点。                    */ \
            /* [English] Exit immediately on failure to pinpoint the first issue. */ \
            exit(1);                                                           \
        }                                                                      \
    } while (0)                                                                \
    /* [中文] 宏结束。                                                          */ \
    /* [English] End of macro.                                                 */

/*
 * test_create
 * ----------------------------------------------------------------------------
 * [中文] 测试 queue_create() 能否成功创建队列，以及创建后 queue_destroy() 是否可销毁空队列。
 * [English] Test whether queue_create() successfully creates a queue and whether
 * queue_destroy() can destroy an empty queue.
 */
void test_create(void)
{
    queue_t q; // [中文] 声明一个队列句柄变量 q，用来接收 queue_create 返回值。 
               // [English] Declare a queue handle q to store the queue_create result.

    fprintf(stderr, "*** TEST create ***\n"); // [中文] 向标准错误输出当前测试名称，便于查看测试进度。 
                                              // [English] Print the test name to stderr for progress visibility.

    TEST_ASSERT((q = queue_create()) != NULL); // [中文] 调用 queue_create 并断言返回值非 NULL（创建成功）。 
                                               // [English] Call queue_create and assert the result is not NULL.

    queue_destroy(q); // [中文] 销毁刚创建的空队列，做基本清理。 
                      // [English] Destroy the newly created empty queue (basic cleanup).
}

/*
 * test_queue_simple
 * ----------------------------------------------------------------------------
 * [中文] 测试最基础的入队+出队流程，确认 FIFO 的最简单情况以及数据指针正确返回。
 * [English] Test a simple enqueue+dequeue flow to confirm basic FIFO behavior
 * and correct returned data pointer.
 */
void test_queue_simple(void)
{
    int data = 3, *ptr; // [中文] data 是测试数据，ptr 用于接收 dequeue 返回的数据指针。 
                        // [English] data is the test value; ptr receives the dequeued data pointer.

    queue_t q; // [中文] 声明队列变量 q。 
               // [English] Declare queue variable q.

    fprintf(stderr, "*** TEST queue_simple ***\n"); // [中文] 打印该测试标题。 
                                                    // [English] Print the test title.

    q = queue_create(); // [中文] 创建一个空队列。 
                        // [English] Create an empty queue.

    queue_enqueue(q, &data); // [中文] 把 data 的地址入队（队列里存放的是指针）。 
                             // [English] Enqueue the address of data (the queue stores pointers).

    queue_dequeue(q, (void **)&ptr); // [中文] 从队列出队一个元素，并把得到的指针写到 ptr。 
                                     // [English] Dequeue one element and write the returned pointer into ptr.

    TEST_ASSERT(ptr == &data); // [中文] 断言出队得到的指针就是入队时的 &data。 
                               // [English] Assert the dequeued pointer is exactly &data.

    queue_destroy(q); // [中文] 队列已空，销毁队列对象。 
                      // [English] The queue is empty now, so destroy it.
}

/* 
 * [中文] 以上通常是作业给的示例测试；下面开始是你自己补充的测试用例。
 * [English] The tests above are typically provided examples; the tests below are custom cases added by you.
 */

/*
 * test_nulls
 * ----------------------------------------------------------------------------
 * [中文] 测试各个接口在接收到 NULL 参数时是否按规范返回 -1（错误处理测试）。
 * [English] Test whether APIs correctly return -1 on NULL arguments (error handling).
 */
void test_nulls(void)
{
    fprintf(stderr, "*** TEST null queue in various functions ***\n"); // [中文] 打印该组测试标题。 
                                                                       // [English] Print the title for this test group.

    fprintf(stderr, "*** enqueue ***\n"); // [中文] 打印当前子测试：enqueue 的 NULL 情况。 
                                          // [English] Print subtest label: enqueue NULL case.
    TEST_ASSERT(queue_enqueue(NULL, NULL) == -1); // [中文] 断言 queue_enqueue(NULL, NULL) 返回 -1。 
                                                  // [English] Assert queue_enqueue(NULL, NULL) returns -1.

    fprintf(stderr, "*** dequeue ***\n"); // [中文] 打印当前子测试：dequeue 的 NULL 情况。 
                                          // [English] Print subtest label: dequeue NULL case.
    TEST_ASSERT(queue_dequeue(NULL, NULL) == -1); // [中文] 断言 queue_dequeue(NULL, NULL) 返回 -1。 
                                                  // [English] Assert queue_dequeue(NULL, NULL) returns -1.

    fprintf(stderr, "*** iterate ***\n"); // [中文] 打印当前子测试：iterate 的 NULL 情况。 
                                          // [English] Print subtest label: iterate NULL case.
    TEST_ASSERT(queue_iterate(NULL, NULL) == -1); // [中文] 断言 queue_iterate(NULL, NULL) 返回 -1。 
                                                  // [English] Assert queue_iterate(NULL, NULL) returns -1.

    fprintf(stderr, "*** length ***\n"); // [中文] 打印当前子测试：length 的 NULL 情况。 
                                         // [English] Print subtest label: length NULL case.
    TEST_ASSERT(queue_length(NULL) == -1); // [中文] 断言 queue_length(NULL) 返回 -1。 
                                           // [English] Assert queue_length(NULL) returns -1.
}

/*
 * test_length_enqueue
 * ----------------------------------------------------------------------------
 * [中文] 测试入队后长度是否正确变为 1。
 * [English] Test whether queue length becomes 1 after one enqueue.
 */
void test_length_enqueue(void)
{
    fprintf(stderr, "*** TEST queue length ***\n"); // [中文] 打印测试标题。 
                                                    // [English] Print the test title.

    int data = 3, *ptr; // [中文] data 用于入队，ptr 用于后续出队接收（便于清理队列）。 
                        // [English] data is enqueued; ptr receives dequeue result later for cleanup.

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建队列。 
                        // [English] Create a queue.

    queue_enqueue(q, &data); // [中文] 入队一个元素。 
                             // [English] Enqueue one item.

    TEST_ASSERT(queue_length(q) == 1); // [中文] 断言长度应为 1。 
                                       // [English] Assert that the queue length is 1.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队清理该元素，使队列变空。 
                                     // [English] Dequeue the item for cleanup so the queue becomes empty.

    queue_destroy(q); // [中文] 销毁空队列。 
                      // [English] Destroy the empty queue.
}

/*
 * test_length_dequeue
 * ----------------------------------------------------------------------------
 * [中文] 测试入队两个元素、出队一个元素后，长度是否正确变为 1。
 * [English] Test whether length becomes 1 after enqueuing two items and dequeuing one.
 */
void test_length_dequeue(void)
{
    int data = 3, *ptr; // [中文] 准备测试数据（这里重复使用同一个地址也没问题，测试的是队列行为）。 
                        // [English] Prepare test data (reusing the same address is fine here; this tests queue behavior).

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建队列。 
                        // [English] Create a queue.

    queue_enqueue(q, &data); // [中文] 第一次入队。 
                             // [English] First enqueue.
    queue_enqueue(q, &data); // [中文] 第二次入队（同一个指针也允许重复入队）。 
                             // [English] Second enqueue (same pointer can be queued again).

    queue_dequeue(q, (void **)&ptr); // [中文] 出队一个元素。 
                                     // [English] Dequeue one item.

    TEST_ASSERT(queue_length(q) == 1); // [中文] 断言此时队列长度应为 1。 
                                       // [English] Assert that the queue length is now 1.

    queue_destroy(q); // [中文] 注意：此时队列实际上还有一个元素；若 queue_destroy 规范要求必须空队列，这里会依赖实现/测试行为。 
                      // [English] Note: one element still remains here; if queue_destroy requires an empty queue, this depends on behavior/spec and may be a test issue.
}

/*
 * test_dequeue_empty
 * ----------------------------------------------------------------------------
 * [中文] 测试对空队列执行 dequeue 时是否返回 -1。
 * [English] Test whether dequeue on an empty queue returns -1.
 */
void test_dequeue_empty(void)
{
    fprintf(stderr, "*** TEST dequeue on empty list ***\n"); // [中文] 打印测试标题。 
                                                             // [English] Print the test title.

    int *ptr; // [中文] 声明接收指针（即使 dequeue 失败也需要传入一个地址变量）。 
              // [English] Declare output pointer variable (still needed as an argument even if dequeue fails).

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建空队列。 
                        // [English] Create an empty queue.

    TEST_ASSERT(queue_dequeue(q, (void **)&ptr) == -1); // [中文] 断言对空队列 dequeue 返回 -1。 
                                                        // [English] Assert dequeue on an empty queue returns -1.

    queue_destroy(q); // [中文] 销毁空队列。 
                      // [English] Destroy the empty queue.
}

/*
 * test_delete
 * ----------------------------------------------------------------------------
 * [中文] 综合测试 queue_delete：
 * - 删除中间元素
 * - 删除不存在元素
 * - 单元素删除
 * - 删除尾元素
 * 同时也顺便验证删除后的队列顺序和长度是否正确。
 *
 * [English] Comprehensive tests for queue_delete:
 * - delete middle element
 * - delete nonexistent element
 * - delete in single-element case
 * - delete tail element
 * Also validates resulting order and length.
 */
void test_delete(void)
{
    fprintf(stderr, "*** TEST delete on middle element***\n"); // [中文] 打印第一部分测试标题：删除中间元素。 
                                                               // [English] Print first subtest title: delete middle element.

    int x = 1, y = 2, z = 3, *ptr; // [中文] 准备 3 个不同数据，用于验证删除前后顺序。 
                                   // [English] Prepare 3 distinct values to verify order before/after deletion.

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建队列。 
                        // [English] Create a queue.

    queue_enqueue(q, &x); // [中文] 入队 x（队头）。 
                          // [English] Enqueue x (front).
    queue_enqueue(q, &y); // [中文] 入队 y（中间）。 
                          // [English] Enqueue y (middle).
    queue_enqueue(q, &z); // [中文] 入队 z（队尾）。 
                          // [English] Enqueue z (tail).

    TEST_ASSERT(queue_delete(q, &y) == 0); // [中文] 删除中间元素 y，断言成功返回 0。 
                                           // [English] Delete middle element y and assert success (0).

    TEST_ASSERT(queue_length(q) == 2); // [中文] 删除后长度应为 2。 
                                       // [English] Length should be 2 after deletion.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第一个元素，应当仍是 x。 
                                     // [English] Dequeue first item; it should still be x.
    TEST_ASSERT(ptr == &x); // [中文] 断言顺序正确：第一个是 x。 
                            // [English] Assert order is correct: first is x.

    queue_dequeue(q, (void **)&ptr); // [中文] 再出队一个，应当是 z（因为 y 被删掉了）。 
                                     // [English] Dequeue again; it should be z (since y was deleted).
    TEST_ASSERT(ptr == &z); // [中文] 断言第二个是 z。 
                            // [English] Assert the second is z.

    fprintf(stderr, "*** TEST delete on not found***\n"); // [中文] 打印第二部分测试标题：删除不存在元素。 
                                                          // [English] Print second subtest title: delete not found.

    TEST_ASSERT(queue_delete(q, &y) == -1); // [中文] 队列当前为空，删除 y 应失败并返回 -1。 
                                            // [English] Queue is empty now; deleting y should fail and return -1.

    queue_enqueue(q, &x); // [中文] 再入队一个 x，构造“队列非空但目标不存在”的情况。 
                          // [English] Enqueue x to create a non-empty queue where target y is absent.

    TEST_ASSERT(queue_delete(q, &y) == -1); // [中文] 队列里只有 x，删除 y 应返回 -1。 
                                            // [English] Queue contains only x, so deleting y should return -1.

    fprintf(stderr, "*** TEST delete on one element list***\n"); // [中文] 打印第三部分测试标题：单元素队列删除。 
                                                                 // [English] Print third subtest title: delete on single-element queue.

    TEST_ASSERT(queue_delete(q, &x) == 0); // [中文] 删除唯一元素 x，应成功。 
                                           // [English] Delete the only element x; should succeed.

    TEST_ASSERT(queue_length(q) == 0); // [中文] 删除后队列长度应为 0。 
                                       // [English] Length should be 0 after deleting the only item.

    fprintf(stderr, "*** TEST delete on one element tail***\n"); // [中文] 打印第四部分测试标题：删除尾元素。 
                                                                 // [English] Print fourth subtest title: delete tail element.

    queue_enqueue(q, &x); // [中文] 入队 x（将成为头）。 
                          // [English] Enqueue x (will be head).
    queue_enqueue(q, &y); // [中文] 入队 y（将成为尾）。 
                          // [English] Enqueue y (will be tail).

    TEST_ASSERT(queue_delete(q, &y) == 0); // [中文] 删除尾元素 y，应成功。 
                                           // [English] Delete tail element y; should succeed.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队剩下的元素，应当是 x。 
                                     // [English] Dequeue the remaining element; it should be x.

    TEST_ASSERT(ptr == &x); // [中文] 验证尾删后队列结构仍正确（剩余元素是 x）。 
                            // [English] Verify queue structure is correct after tail deletion (remaining item is x).

    queue_dequeue(q, (void **)&ptr); // [中文] 再 dequeue 一次（此处用于清理；若队列为空则实现应返回 -1，这里未断言）。 
                                     // [English] Dequeue once more for cleanup (if empty, implementation should return -1; not asserted here).

    queue_destroy(q); // [中文] 销毁队列对象。 
                      // [English] Destroy the queue object.
}

/*
 * iterator_inc
 * ----------------------------------------------------------------------------
 * [中文] 这是给 queue_iterate 使用的回调函数：把每个元素（int*）指向的值加 1。
 * 参数 q 在这个回调里不需要使用，所以用 (void)q 消除未使用警告。
 *
 * [English] Callback for queue_iterate: increments the integer pointed to by
 * each data item. q is unused here, so (void)q suppresses warnings.
 */
void iterator_inc(queue_t q, void *data)
{
    (void)q; // [中文] 显式忽略未使用的 q 参数，避免编译器警告。 
             // [English] Explicitly ignore unused q parameter to avoid warnings.

    int *a = (int *)data; // [中文] 把通用 void* 转回 int*，因为本测试存放的是 int 地址。 
                          // [English] Cast generic void* back to int* because this test stores int addresses.

    *a += 1; // [中文] 将该整数值加 1。 
             // [English] Increment the integer value by 1.
}

/*
 * iterator_inc_delete_2
 * ----------------------------------------------------------------------------
 * [中文] 另一个 iterate 回调：
 * - 如果当前值是 2，则在遍历过程中删除该元素
 * - 否则给该值加 1
 * 这个回调用来测试 queue_iterate 在“回调会删除当前元素”时是否还能安全遍历。
 *
 * [English] Another iterate callback:
 * - If the current value is 2, delete that element during iteration
 * - Otherwise increment it
 * This tests whether queue_iterate is safe when the callback deletes the current item.
 */
void iterator_inc_delete_2(queue_t q, void *data)
{
    int *a = (int *)data; // [中文] 将 data 转成 int*，以便读取/修改该整数值。 
                          // [English] Cast data to int* so the integer can be read/modified.

    if (*a == 2) { // [中文] 如果当前元素值为 2，则测试“遍历中删除当前元素”。 
                   // [English] If the current item value is 2, test deleting during iteration.

        queue_delete(q, data); // [中文] 从队列中删除当前这个数据指针对应的节点。 
                               // [English] Delete the node whose data pointer matches the current item.

    } else { // [中文] 否则（当前值不是 2），正常做加 1 操作。 
             // [English] Otherwise (not 2), just increment it.

        *a += 1; // [中文] 值加 1。 
                 // [English] Increment the value by 1.
    }
}

/*
 * test_iterate_simple
 * ----------------------------------------------------------------------------
 * [中文] 测试 queue_iterate 的基本功能（不删除元素），验证回调是否对每个元素都执行了一次。
 * [English] Test basic queue_iterate behavior (no deletion), verifying the
 * callback runs once on each element.
 */
void test_iterate_simple(void)
{
    fprintf(stderr, "*** TEST iterate with no deletion***\n"); // [中文] 打印测试标题。 
                                                               // [English] Print the test title.

    int x = 1, y = 2, z = 3, *ptr; // [中文] 准备三个整数，便于观察 iterate 后是否都加 1。 
                                   // [English] Prepare three ints to verify each is incremented.

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建队列。 
                        // [English] Create a queue.

    queue_enqueue(q, &x); // [中文] 入队 x。 
                          // [English] Enqueue x.
    queue_enqueue(q, &y); // [中文] 入队 y。 
                          // [English] Enqueue y.
    queue_enqueue(q, &z); // [中文] 入队 z。 
                          // [English] Enqueue z.

    queue_iterate(q, iterator_inc); // [中文] 遍历队列并对每个元素执行 iterator_inc（值 +1）。 
                                    // [English] Iterate through the queue and run iterator_inc on each item (+1).

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第一个元素（同时用于清理）。 
                                     // [English] Dequeue first item (also cleanup).
    TEST_ASSERT(x == 2); // [中文] 验证 x 已由 1 变成 2。 
                         // [English] Verify x changed from 1 to 2.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第二个元素。 
                                     // [English] Dequeue second item.
    TEST_ASSERT(y == 3); // [中文] 验证 y 已由 2 变成 3。 
                         // [English] Verify y changed from 2 to 3.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第三个元素。 
                                     // [English] Dequeue third item.
    TEST_ASSERT(z == 4); // [中文] 验证 z 已由 3 变成 4。 
                         // [English] Verify z changed from 3 to 4.

    queue_destroy(q); // [中文] 队列已空，销毁队列。 
                      // [English] Queue is empty now; destroy it.
}

/*
 * test_iterate_with_delete
 * ----------------------------------------------------------------------------
 * [中文] 测试 queue_iterate 在回调中删除元素的情况。
 * 这里回调会删除值为 2 的元素（y），其余元素加 1。
 * 用来验证：
 * - iterate 不会因为删除当前元素而崩溃
 * - 队列剩余元素顺序仍正确
 * - 长度更新正确
 *
 * [English] Test queue_iterate when the callback deletes an element.
 * The callback deletes the item whose value is 2 (y) and increments others.
 * This verifies:
 * - iterate remains safe during deletion
 * - remaining order is correct
 * - length is updated correctly
 */
void test_iterate_with_delete(void)
{
    fprintf(stderr, "*** TEST iterate with one deletion***\n"); // [中文] 打印测试标题。 
                                                                // [English] Print the test title.

    int x = 1, y = 2, z = 3, *ptr; // [中文] 准备三个元素，y 会在遍历中被删掉。 
                                   // [English] Prepare three items; y will be deleted during iteration.

    queue_t q; // [中文] 声明队列变量。 
               // [English] Declare queue variable.

    q = queue_create(); // [中文] 创建队列。 
                        // [English] Create a queue.

    queue_enqueue(q, &x); // [中文] 入队 x。 
                          // [English] Enqueue x.
    queue_enqueue(q, &y); // [中文] 入队 y（这个会在 iterate 中被删除）。 
                          // [English] Enqueue y (this one will be deleted during iterate).
    queue_enqueue(q, &z); // [中文] 入队 z。 
                          // [English] Enqueue z.

    queue_iterate(q, iterator_inc_delete_2); // [中文] 遍历队列：删除 y，其余元素 +1。 
                                             // [English] Iterate: delete y, increment others.

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第一个剩余元素，应为 x。 
                                     // [English] Dequeue first remaining item; should be x.
    TEST_ASSERT(x == 2); // [中文] 验证 x 已加 1（1 -> 2）。 
                         // [English] Verify x was incremented (1 -> 2).

    queue_dequeue(q, (void **)&ptr); // [中文] 出队第二个剩余元素，应为 z（因为 y 被删除）。 
                                     // [English] Dequeue second remaining item; should be z (since y was deleted).
    TEST_ASSERT(z == 4); // [中文] 验证 z 已加 1（3 -> 4）。 
                         // [English] Verify z was incremented (3 -> 4).

    TEST_ASSERT(queue_length(q) == 0); // [中文] 此时队列应已空（x 和 z 都已出队，y 在 iterate 时被删）。 
                                       // [English] Queue should now be empty (x/z dequeued, y deleted during iterate).

    queue_destroy(q); // [中文] 销毁空队列。 
                      // [English] Destroy the empty queue.
}

/*
 * main
 * ----------------------------------------------------------------------------
 * [中文] 测试程序入口，按顺序运行所有测试函数。
 * 如果某个 TEST_ASSERT 失败，程序会在对应位置直接 exit(1)。
 * 若 main 正常返回 0，说明所有测试都通过了。
 *
 * [English] Entry point of the test program. Runs all tests in sequence.
 * If any TEST_ASSERT fails, the program exits immediately with exit(1).
 * Returning 0 from main means all tests passed.
 */
int main(void)
{
    test_create(); // [中文] 测试队列创建/销毁空队列。 
                   // [English] Test queue creation and destroying an empty queue.

    test_nulls(); // [中文] 测试 NULL 参数错误处理。 
                  // [English] Test NULL-argument error handling.

    test_queue_simple(); // [中文] 测试最基本入队/出队。 
                         // [English] Test basic enqueue/dequeue.

    test_length_enqueue(); // [中文] 测试入队后长度变化。 
                           // [English] Test length after enqueue.

    test_length_dequeue(); // [中文] 测试出队后长度变化。 
                           // [English] Test length after dequeue.

    test_dequeue_empty(); // [中文] 测试空队列 dequeue 错误处理。 
                          // [English] Test dequeue on empty queue.

    test_delete(); // [中文] 测试 delete 的多种情况（中间/不存在/单元素/尾元素）。 
                   // [English] Test multiple delete scenarios (middle/not found/single/tail).

    test_iterate_simple(); // [中文] 测试 iterate 基本遍历与修改。 
                           // [English] Test basic iterate traversal and modification.

    test_iterate_with_delete(); // [中文] 测试 iterate 中删除元素的安全性。 
                                // [English] Test iterate safety when deleting during callback.

    return 0; // [中文] 所有测试通过，返回 0。 
              // [English] All tests passed; return 0.
}