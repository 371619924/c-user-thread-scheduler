#include <stdint.h>   // [中文] 引入标准整数类型定义（如 uint32_t 等）。本文件里虽然暂时没直接用到，但通常项目模板会保留。 
                      // [English] Include fixed-width integer types (e.g., uint32_t). Not directly used here, but often kept in project templates.

#include <stdlib.h>   // [中文] 引入内存分配与释放函数声明（malloc, free）。
                      // [English] Include declarations for memory management functions (malloc, free).

#include <string.h>   // [中文] 引入字符串/内存操作函数声明（如 memset）。本文件当前未使用，但模板可能保留。
                      // [English] Include declarations for string/memory utilities (e.g., memset). Not used here currently, but often kept.

#include "queue.h"    // [中文] 引入队列接口头文件，里面定义了 queue_t、queue_func_t 以及函数原型。
                      // [English] Include the queue interface header, which defines queue_t, queue_func_t, and function prototypes.

/*
 * ============================================================================
 * [中文总注释 / This File in the Whole Assignment]
 *
 * 这个文件实现了本次作业中的“通用队列（generic queue）”模块，底层使用单向链表
 * （singly linked list）。队列中存放的是 void* 指针，因此它可以保存任意类型数据
 * 的地址（具体类型由调用者自己管理和保证）。
 *
 * 【这个文件在整个作业里的作用（很重要）】
 * 1) 这是本次 uthread / synchronization 作业的基础数据结构模块之一。
 *    - 后续线程库（uthread）和同步原语（比如 semaphore）通常都需要“排队”功能：
 *      例如 ready queue（就绪线程队列）、wait queue（阻塞线程队列）等。
 *    - 这个 queue.c 提供的就是这些“排队行为”的底层支持。
 *
 * 2) 这个文件通常是你在作业早期阶段先完成和测试的模块（配合 apps/queue_tester.c）。
 *    - 因为队列逻辑正确与否，会直接影响后面线程调度、阻塞/唤醒是否正确。
 *    - 如果 queue 有 bug，后面 uthread / sem 的错误会很难排查。
 *
 * 3) 这个模块是“可复用”的基础组件：
 *    - queue_enqueue / queue_dequeue 支持 FIFO 行为（先进先出）
 *    - queue_delete 支持按数据指针删除某个元素（常用于从队列中移除指定线程）
 *    - queue_iterate 支持遍历并对每个元素调用回调函数（常用于测试/批量操作）
 *    - queue_length 支持 O(1) 获取当前长度
 *
 * 【本文件内部维护的核心状态】
 * - head：队头指针（出队从这里取）
 * - tail：队尾指针（入队加到这里）
 * - length：当前队列长度（元素个数）
 *
 * 【设计说明】
 * - 队列节点（queue_item）只保存 void* data，不负责 data 指向内容的内存管理。
 * - queue_destroy 只释放“队列对象本身”，不会释放用户数据。
 * - 数据匹配/删除使用“指针地址比较”（data1 == data2），不是按内容比较。
 * - queue_iterate 在调用回调前保存 next 指针，以便在回调可能修改队列时更安全地遍历。
 *
 *
 * [English Summary / Role in the Assignment]
 *
 * This file implements the generic queue module for the assignment using a
 * singly linked list. The queue stores void* pointers, so it can hold addresses
 * of any data type (the caller is responsible for the actual data type).
 *
 * [Why this file matters in the overall assignment]
 * 1) It is a foundational data-structure module for the uthread/synchronization
 *    project.
 *    - Later parts of the thread library (uthread) and synchronization
 *      primitives (e.g., semaphores) typically need queueing behavior, such as
 *      a ready queue (runnable threads) or wait queues (blocked threads).
 *    - This queue.c provides the underlying queue operations for that behavior.
 *
 * 2) It is usually completed and tested early (with apps/queue_tester.c).
 *    - Correct queue behavior is critical because later scheduling and
 *      blocking/wakeup logic depends on it.
 *    - If the queue is buggy, debugging uthread/semaphore code becomes much
 *      harder.
 *
 * 3) It is a reusable utility module:
 *    - queue_enqueue / queue_dequeue implement FIFO behavior
 *    - queue_delete removes a specific item by data pointer
 *    - queue_iterate applies a callback to each element
 *    - queue_length returns the size in O(1)
 *
 * [Core state maintained by this file]
 * - head: front node (dequeue removes from here)
 * - tail: back node (enqueue appends here)
 * - length: current number of elements
 *
 * [Design notes]
 * - Nodes (queue_item) store only void* data and do not own/free user data.
 * - queue_destroy frees only the queue object itself, not the user data.
 * - Matching/deletion uses pointer equality (data1 == data2), not deep compare.
 * - queue_iterate saves the next pointer before calling the callback so
 *   iteration is safer if the callback modifies the queue.
 * ============================================================================
 */

/* 
 * [中文] 定义链表节点（队列中的一个元素）。
 * 每个节点保存：
 *   - data: 用户传入的数据地址（void*，可指向任意类型）
 *   - next: 指向下一个节点
 *
 * [English] Define a linked-list node (one element in the queue).
 * Each node stores:
 *   - data: user-provided data address (void*, can point to any type)
 *   - next: pointer to the next node
 */
typedef struct queue_item {
    void *data;               // [中文] 当前节点保存的数据指针。 [English] Data pointer stored in this node.
    struct queue_item *next;  // [中文] 指向下一个节点；若为 NULL 表示这是最后一个节点。 [English] Next node pointer; NULL means this is the last node.
} queue_item;

/*
 * [中文] 队列对象本体（在 queue.h 中通常通过 typedef struct queue* queue_t 暴露为不透明类型）。
 * 字段说明：
 *   - head   : 队头节点（dequeue 从这里删除）
 *   - tail   : 队尾节点（enqueue 加到这里）
 *   - length : 当前元素个数，便于 O(1) 返回长度
 *
 * [English] The queue object itself (typically exposed as an opaque type in queue.h,
 * e.g., typedef struct queue* queue_t).
 * Fields:
 *   - head   : front node (dequeue removes from here)
 *   - tail   : back node (enqueue appends here)
 *   - length : current number of elements for O(1) length queries
 */
struct queue {
    queue_item *head;  // [中文] 队头指针。 [English] Pointer to the head/front node.
    queue_item *tail;  // [中文] 队尾指针。 [English] Pointer to the tail/back node.
    int length;        // [中文] 队列长度（元素数量）。 [English] Queue length (number of elements).
};

/*
 * queue_create
 * ----------------------------------------------------------------------------
 * [中文] 创建一个新的空队列。
 * 用法：
 *   queue_t q = queue_create();
 *   if (q == NULL) { ... } // 内存分配失败
 *
 * 返回值：
 *   - 成功：返回新队列指针（queue_t）
 *   - 失败：返回 NULL（通常是 malloc 失败）
 *
 * [English] Create a new empty queue.
 * Usage:
 *   queue_t q = queue_create();
 *   if (q == NULL) { ... } // allocation failed
 *
 * Return:
 *   - Success: pointer to the new queue (queue_t)
 *   - Failure: NULL (usually malloc failure)
 */
queue_t queue_create(void)
{
    struct queue *new_queue = malloc(sizeof(*new_queue)); 
    // [中文] 为队列结构体分配内存。sizeof(*new_queue) 等价于 sizeof(struct queue)，写法更稳健（类型改名时不易出错）。
    // [English] Allocate memory for the queue structure. sizeof(*new_queue) is equivalent to sizeof(struct queue) and is a robust style.

    if (new_queue == NULL) { 
        // [中文] 检查内存分配是否失败。
        // [English] Check whether memory allocation failed.
        return NULL;         
        // [中文] 分配失败，返回 NULL 给调用者。
        // [English] Allocation failed; return NULL to the caller.
    }

    new_queue->head = NULL;   
    // [中文] 新队列一开始没有元素，所以队头为空。
    // [English] A new queue has no elements, so head is NULL.

    new_queue->tail = NULL;   
    // [中文] 新队列一开始没有元素，所以队尾也为空。
    // [English] A new queue has no elements, so tail is also NULL.

    new_queue->length = 0;    
    // [中文] 初始化队列长度为 0。
    // [English] Initialize queue length to 0.

    return new_queue;         
    // [中文] 返回创建好的空队列。
    // [English] Return the newly created empty queue.
}

/*
 * queue_destroy
 * ----------------------------------------------------------------------------
 * [中文] 销毁队列对象本身（只释放队列结构，不释放队列里 data 指向的用户数据）。
 * 注意：只有在队列为空时才允许销毁，否则返回 -1。
 *
 * 用法：
 *   if (queue_length(q) == 0) {
 *       queue_destroy(q);
 *   }
 *
 * 参数：
 *   queue - 要销毁的队列
 *
 * 返回值：
 *   - 0  : 成功销毁
 *   - -1 : queue 为 NULL，或者队列非空
 *
 * [English] Destroy the queue object itself (frees only the queue structure,
 * not the user data pointed to by queued void* pointers).
 * Note: The queue must be empty; otherwise returns -1.
 *
 * Usage:
 *   if (queue_length(q) == 0) {
 *       queue_destroy(q);
 *   }
 *
 * Parameter:
 *   queue - queue to destroy
 *
 * Return:
 *   - 0  : success
 *   - -1 : queue is NULL or not empty
 */
int queue_destroy(queue_t queue)
{
    if (queue == NULL) {  
        // [中文] 若传入空指针，无法销毁，直接报错。
        // [English] If the queue pointer is NULL, cannot destroy; return error.
        return -1;
    }

    if (queue->length != 0) { 
        // [中文] 按作业要求，只有“空队列”才能销毁；非空则返回 -1。
        // [English] Per the spec, only an empty queue can be destroyed; non-empty returns -1.
        return -1;
    }

    free(queue);           
    // [中文] 释放队列结构体本身的内存。
    // [English] Free the memory for the queue structure itself.

    return 0;              
    // [中文] 返回 0 表示销毁成功。
    // [English] Return 0 to indicate success.
}

/*
 * queue_enqueue
 * ----------------------------------------------------------------------------
 * [中文] 入队操作：把一个数据指针追加到队尾（FIFO 的“进入”）。
 * 这是尾插法，时间复杂度 O(1)（因为维护了 tail 指针）。
 *
 * 用法：
 *   int x = 42;
 *   queue_enqueue(q, &x);
 *
 * 参数：
 *   queue - 目标队列
 *   data  - 要入队的数据指针（不能为 NULL，按本实现规则）
 *
 * 返回值：
 *   - 0  : 入队成功
 *   - -1 : queue 为 NULL、data 为 NULL，或内存分配失败
 *
 * [English] Enqueue operation: append a data pointer at the tail (FIFO "push").
 * This is O(1) because the queue keeps a tail pointer.
 *
 * Usage:
 *   int x = 42;
 *   queue_enqueue(q, &x);
 *
 * Parameters:
 *   queue - target queue
 *   data  - data pointer to enqueue (must not be NULL in this implementation)
 *
 * Return:
 *   - 0  : success
 *   - -1 : queue is NULL, data is NULL, or allocation fails
 */
int queue_enqueue(queue_t queue, void *data)
{
    if (queue == NULL || data == NULL) { 
        // [中文] 参数检查：队列为空 or 数据指针为空都不合法（按本作业接口约定）。
        // [English] Validate inputs: NULL queue or NULL data is invalid by this implementation/spec.
        return -1;
    }

    queue_item *item = malloc(sizeof(queue_item)); 
    // [中文] 为新节点分配内存，用来存放这次入队的元素。
    // [English] Allocate memory for a new node representing the enqueued element.

    if (item == NULL) { 
        // [中文] 如果节点分配失败，返回错误。
        // [English] If node allocation fails, return an error.
        return -1;
    }

    item->data = data;      
    // [中文] 把调用者传入的数据地址存入新节点。
    // [English] Store the caller-provided data pointer in the new node.

    item->next = NULL;      
    // [中文] 新节点将作为队尾，所以它的 next 应该是 NULL。
    // [English] The new node will be the tail, so its next pointer should be NULL.

    if (queue->tail == NULL) {      
        // [中文] 如果 tail 是 NULL，说明队列当前为空（head 也应为空）。
        // [English] If tail is NULL, the queue is currently empty (head should also be NULL).

        queue->tail = item;         
        // [中文] 空队列入队后，新节点既是队尾。
        // [English] In an empty queue, the new node becomes the tail.

        queue->head = item;         
        // [中文] 空队列入队后，新节点同时也是队头。
        // [English] In an empty queue, the new node also becomes the head.
    } else {
        queue->tail->next = item;   
        // [中文] 非空队列：先把“原队尾”的 next 指向新节点（把新节点接到链表末尾）。
        // [English] Non-empty queue: link the current tail's next to the new node.

        queue->tail = item;         
        // [中文] 更新 tail，让新节点成为新的队尾。
        // [English] Update tail so the new node becomes the new tail.
    }

    queue->length++;         
    // [中文] 队列元素个数加 1。
    // [English] Increment the queue length.

    return 0;               
    // [中文] 返回 0 表示入队成功。
    // [English] Return 0 to indicate enqueue success.
}

/*
 * queue_dequeue
 * ----------------------------------------------------------------------------
 * [中文] 出队操作：删除队头节点，并通过 *data 把被删除节点里的数据指针返回给调用者。
 * 这是头删法，时间复杂度 O(1)。
 *
 * 用法：
 *   void *out;
 *   if (queue_dequeue(q, &out) == 0) {
 *       // out 现在是原队头的数据指针
 *   }
 *
 * 参数：
 *   queue - 目标队列
 *   data  - 输出参数（void **），用于带回队头元素的数据指针
 *
 * 返回值：
 *   - 0  : 成功出队
 *   - -1 : queue 为 NULL、data 为 NULL、或队列为空
 *
 * [English] Dequeue operation: remove the head node and return its data pointer
 * via the output parameter *data. This is O(1).
 *
 * Usage:
 *   void *out;
 *   if (queue_dequeue(q, &out) == 0) {
 *       // out now points to the former head data
 *   }
 *
 * Parameters:
 *   queue - target queue
 *   data  - output parameter (void **) to receive the dequeued data pointer
 *
 * Return:
 *   - 0  : success
 *   - -1 : queue is NULL, data is NULL, or queue is empty
 */
int queue_dequeue(queue_t queue, void **data)
{
    if (queue == NULL || data == NULL || queue->head == NULL) { 
        // [中文] 参数检查 + 空队列检查：
        //        - queue 为 NULL：非法
        //        - data 为 NULL：无法回传结果
        //        - queue->head 为 NULL：队列为空，无法出队
        // [English] Validate input and empty-queue case:
        //           - NULL queue is invalid
        //           - NULL data output pointer cannot receive result
        //           - NULL head means the queue is empty
        return -1;
    }

    queue_item *prev_head = queue->head; 
    // [中文] 保存当前队头节点指针，稍后要用它取数据并释放它。
    // [English] Save the current head node pointer; we'll read its data and free it.

    *data = prev_head->data;            
    // [中文] 把队头节点里的数据指针写到调用者提供的输出参数里。
    // [English] Write the head node's data pointer into the caller's output parameter.

    queue->head = prev_head->next;      
    // [中文] 队头后移：新的 head 变成原队头的下一个节点。
    // [English] Advance head: the new head becomes the old head's next node.

    if (queue->head == NULL) {          
        // [中文] 如果移动后 head 变成 NULL，说明原队列只有一个元素，出队后队列变空。
        // [English] If head becomes NULL after advancing, the queue had one element and is now empty.

        queue->tail = NULL;             
        // [中文] 队列为空时，tail 也必须置为 NULL，避免悬空指针。
        // [English] When the queue is empty, tail must also be NULL to avoid a dangling pointer.
    }

    free(prev_head);                    
    // [中文] 释放原队头节点的内存（注意：只释放节点，不释放节点里的 data）。
    // [English] Free the old head node (but not the user data it points to).

    queue->length--;                    
    // [中文] 队列长度减 1。
    // [English] Decrement the queue length.

    return 0;                           
    // [中文] 返回 0 表示出队成功。
    // [English] Return 0 to indicate dequeue success.
}

/*
 * queue_delete
 * ----------------------------------------------------------------------------
 * [中文] 删除队列中“第一个（最早）匹配到的”某个数据指针（按指针地址比较，不按内容比较）。
 * 与 queue_dequeue 不同，它不是一定删队头，而是从头开始查找匹配的 data。
 *
 * 用法：
 *   int x, y;
 *   queue_enqueue(q, &x);
 *   queue_enqueue(q, &y);
 *   queue_delete(q, &x); // 删除队列中第一个 data == &x 的节点
 *
 * 参数：
 *   queue - 目标队列
 *   data  - 要删除的目标数据指针
 *
 * 返回值：
 *   - 0  : 找到并成功删除
 *   - -1 : queue 为 NULL、data 为 NULL、队列为空、或没找到
 *
 * [English] Delete the first (oldest) node whose data pointer matches the given
 * pointer (pointer equality, not deep content comparison).
 * Unlike queue_dequeue, this searches from the head and removes the first match.
 *
 * Usage:
 *   int x, y;
 *   queue_enqueue(q, &x);
 *   queue_enqueue(q, &y);
 *   queue_delete(q, &x); // removes first node with data == &x
 *
 * Parameters:
 *   queue - target queue
 *   data  - target data pointer to delete
 *
 * Return:
 *   - 0  : found and deleted successfully
 *   - -1 : queue/data invalid, queue empty, or not found
 */
int queue_delete(queue_t queue, void *data)
{
    if (queue == NULL || data == NULL || queue->head == NULL) { 
        // [中文] 参数检查 + 空队列检查：任一条件不满足都无法删除。
        // [English] Validate inputs and empty queue case; deletion cannot proceed if any condition fails.
        return -1;
    }

    queue_item *prev_item = queue->head;    
    // [中文] prev_item 用来记录 current_item 的前一个节点，便于中间删除时改链。
    // [English] prev_item tracks the node before current_item to relink the list when deleting.

    queue_item *current_item = queue->head; 
    // [中文] current_item 从队头开始扫描，寻找 data 匹配的节点。
    // [English] current_item starts at the head and scans for a matching data pointer.

    if (data == current_item->data) {       
        // [中文] 特判：如果要删除的就是队头节点（头删场景）。
        // [English] Special case: target matches the head node (head removal case).

        queue->head = current_item->next;   
        // [中文] 把 head 往后移，跳过当前节点（即删除它）。
        // [English] Move head forward to skip the current node (removing it).

        if (queue->head == NULL) {          
            // [中文] 如果删除后 head 为 NULL，说明原队列只有一个节点，删除后变空。
            // [English] If head becomes NULL after deletion, the queue is now empty.

            queue->tail = NULL;             
            // [中文] 队列为空时，tail 也要设为 NULL。
            // [English] When the queue becomes empty, tail must also be set to NULL.
        }

        free(current_item);                 
        // [中文] 释放被删除的队头节点。
        // [English] Free the removed head node.

        queue->length--;                    
        // [中文] 队列长度减 1。
        // [English] Decrement the queue length.

        return 0;                           
        // [中文] 删除成功，返回 0。
        // [English] Deletion succeeded; return 0.
    }

    while (current_item != NULL && current_item->next != NULL) { 
        // [中文] 从第二个节点开始继续扫描：
        //        这里循环条件写 current_item != NULL && current_item->next != NULL，
        //        因为循环体里会先 current_item = current_item->next。
        // [English] Continue scanning from the second node onward:
        //           The condition uses current_item != NULL && current_item->next != NULL
        //           because the loop body advances current_item first.

        prev_item = current_item;           
        // [中文] 更新前驱指针，让 prev_item 指向“当前节点”（下一步 current 会前进）。
        // [English] Update predecessor pointer so prev_item tracks the node before the next current_item.

        current_item = current_item->next;  
        // [中文] current_item 前进一步，指向下一个待检查节点。
        // [English] Advance current_item to the next node to examine.

        if (data == current_item->data) {   
            // [中文] 找到匹配节点（按指针地址相等）。
            // [English] Found a match (pointer equality).

            prev_item->next = current_item->next; 
            // [中文] 跳过当前节点：让前驱节点直接指向当前节点的后继节点。
            // [English] Bypass the current node by linking predecessor directly to current_item->next.

            if (current_item->next == NULL) {    
                // [中文] 如果当前节点后继为 NULL，说明删的是尾节点。
                // [English] If current_item->next is NULL, we're deleting the tail node.

                queue->tail = prev_item;         
                // [中文] 删除尾节点后，前驱节点成为新的 tail。
                // [English] After deleting the tail, the predecessor becomes the new tail.
            }

            free(current_item);                 
            // [中文] 释放被删除的节点。
            // [English] Free the deleted node.

            queue->length--;                    
            // [中文] 队列长度减 1。
            // [English] Decrement the queue length.

            return 0;                           
            // [中文] 删除成功，返回 0。
            // [English] Deletion succeeded; return 0.
        }
    }

    return -1;  
    // [中文] 扫描完整个队列仍未找到匹配 data，返回 -1。
    // [English] Reached the end without finding a match; return -1.
}

/*
 * queue_iterate
 * ----------------------------------------------------------------------------
 * [中文] 遍历队列，对每个节点的数据调用一次回调函数 func(queue, data)。
 * 这个接口常用于批量处理队列元素，或者在测试中检查内容。
 *
 * 关键点：
 * - 在调用 func 前先保存 current_item->next 到 saved_next
 * - 这样即使 func 内部删除了 current_item，对遍历也更安全（不会因为 current_item 被释放而丢失后继）
 *
 * 用法（示例）：
 *   void print_item(queue_t q, void *data) { ... }
 *   queue_iterate(q, print_item);
 *
 * 参数：
 *   queue - 要遍历的队列
 *   func  - 回调函数，签名通常在 queue.h 里定义为 queue_func_t
 *
 * 返回值：
 *   - 0  : 遍历完成（即使队列为空也返回 0）
 *   - -1 : queue 或 func 为 NULL
 *
 * [English] Iterate through the queue and call the callback func(queue, data)
 * once for each node. Useful for bulk processing or testing.
 *
 * Key idea:
 * - Save current_item->next into saved_next before invoking func
 * - This makes iteration safer if func deletes the current node
 *
 * Usage example:
 *   void print_item(queue_t q, void *data) { ... }
 *   queue_iterate(q, print_item);
 *
 * Parameters:
 *   queue - queue to iterate
 *   func  - callback function (type queue_func_t, typically defined in queue.h)
 *
 * Return:
 *   - 0  : iteration completed (also returns 0 for an empty queue)
 *   - -1 : queue or func is NULL
 */
int queue_iterate(queue_t queue, queue_func_t func)
{
    if (queue == NULL || func == NULL) { 
        // [中文] 参数检查：队列或回调函数为空都无法遍历。
        // [English] Validate inputs: cannot iterate with a NULL queue or NULL callback.
        return -1;
    }

    if (queue->head == NULL) { 
        // [中文] 空队列是合法情况：没有元素可遍历，但不算错误，返回 0。
        // [English] Empty queue is a valid case: nothing to iterate, but not an error.
        return 0;
    }

    queue_item *current_item = queue->head; 
    // [中文] current_item 从队头开始，逐个访问节点。
    // [English] Start iteration at the head node.

    queue_item *saved_next = queue->head;   
    // [中文] saved_next 用来暂存“下一个节点”指针，防止回调修改/删除当前节点后无法继续遍历。
    // [English] saved_next temporarily stores the next node pointer so iteration can continue safely if the callback modifies/deletes the current node.

    while (current_item != NULL) {          
        // [中文] 只要当前节点存在，就继续遍历。
        // [English] Continue while there is a current node.

        saved_next = current_item->next;    
        // [中文] 在调用回调前，先保存 next 指针（这是这个函数的关键安全步骤）。
        // [English] Save the next pointer before invoking the callback (key safety step).

        func(queue, current_item->data);    
        // [中文] 调用回调函数，让调用者对当前节点的数据执行自定义操作。
        // [English] Invoke the callback so the caller can perform custom work on the current data.

        current_item = saved_next;          
        // [中文] 移动到下一个节点，继续遍历。
        // [English] Advance to the next node and continue.
    }

    return 0;  
    // [中文] 遍历完成，返回 0。
    // [English] Iteration finished successfully; return 0.
}

/*
 * queue_length
 * ----------------------------------------------------------------------------
 * [中文] 返回当前队列长度（元素个数）。
 * 因为队列结构里维护了 length 字段，所以这是 O(1) 操作。
 *
 * 用法：
 *   int n = queue_length(q);
 *
 * 参数：
 *   queue - 目标队列
 *
 * 返回值：
 *   - >= 0 : 队列长度
 *   - -1   : queue 为 NULL
 *
 * [English] Return the current queue length (number of elements).
 * This is O(1) because the queue tracks length directly.
 *
 * Usage:
 *   int n = queue_length(q);
 *
 * Parameter:
 *   queue - target queue
 *
 * Return:
 *   - >= 0 : queue length
 *   - -1   : queue is NULL
 */
int queue_length(queue_t queue)
{
    if (queue == NULL) { 
        // [中文] 空队列指针非法，返回 -1 表示错误。
        // [English] NULL queue pointer is invalid; return -1 to indicate error.
        return -1;
    }

    return queue->length; 
    // [中文] 直接返回维护好的长度字段（O(1)）。
    // [English] Return the maintained length field directly (O(1)).
}