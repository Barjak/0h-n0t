#include "CSError.h"
#define MAKE_STR(A, B) A##B
#define CONCAT(STEM, TYPE) MAKE_STR(STEM, TYPE)
#define pln printf("%s %i\n", __FILE__, __LINE__)
#define INITIAL_Q_SIZE 63
#define LOAD_FACTOR 0.5
#define SCALE_FACTOR 1.5

#define QNODE CONCAT(QNode_, QTYPENAME)
struct QNODE {
        QTYPE data;
        struct QNODE * prev;
        struct QNODE * next;
        struct QNODE * q_prev;
};
#define QUEUESET CONCAT(QueueSet_, QTYPENAME)
struct QUEUESET {
        unsigned table_size, table_size_threshold;
        unsigned n_entries;
        struct QNODE * begin;
        struct QNODE * end;
        struct QNODE ** table;
};

#define QNODE_CREATE CONCAT(QNode_create_,QTYPENAME)

static inline
struct QNODE * QNODE_CREATE(QTYPE data)
{
        struct QNODE * node = malloc(sizeof(struct QNODE));
        if (!node) { goto bad_alloc1; }
        node->data = data;
        node->next = NULL;
        node->prev = NULL;
        node->q_prev = NULL;
        return node;
bad_alloc1:
        return NULL;
}

static inline
struct QUEUESET * CONCAT(QueueSet_create_,QTYPENAME)()
{
        struct QUEUESET * Q = malloc(sizeof(struct QUEUESET));
        if (!Q) {
                goto bad_alloc1;
        }
        *Q = (struct QUEUESET){
                .begin      = NULL,
                .end        = NULL,
                .n_entries  = 0,
                .table_size = INITIAL_Q_SIZE,
                .table_size_threshold = INITIAL_Q_SIZE * LOAD_FACTOR,
                .table      = NULL};
        Q->table = malloc(Q->table_size * sizeof(struct QNODE*));
        if (!Q->table) {
                goto bad_alloc2;
        }
        for (unsigned i = 0; i < Q->table_size; i++) {
                Q->table[i] = NULL;
        }
        return Q;
bad_alloc2:
        free(Q);
bad_alloc1:
        return NULL;
}

static inline void CONCAT(Queueset_data_, QTYPENAME)(struct QUEUESET * Q)
{
        unsigned empty_count = 0;
        unsigned max_length = 0;
        for (unsigned i = 0; i < Q->table_size; i++) {
                if (Q->table[i] != NULL) {
                        unsigned x = 0;
                        struct QNODE * n = Q->table[i];
                        while (n) {
                                x++;
                                n = n->next;
                        }
                        // printf("%u ", x);
                        max_length = x > max_length ? x : max_length;
                } else {
                        // printf("0 ");
                        empty_count++;
                }
        }
        printf("\n");
        printf("size %u empty %u max_len %u\n", Q->n_entries, empty_count, max_length);
}
#define QUEUESET_EXPAND CONCAT(QNode_expand_,QTYPENAME)
static inline void QUEUESET_EXPAND(struct QUEUESET * Q, unsigned new_size)
{
        struct QNODE ** new_table = malloc(new_size * sizeof(struct QNODE*));

        if (!new_table) {
                goto bad_alloc1;
        }
        for (unsigned i = 0; i < new_size; i++) {
                new_table[i] = NULL;
        }
        for (unsigned i = 0; i < Q->table_size; i++) {
                struct QNODE * current = Q->table[i];
                while (current) {
                        struct QNODE * next = current->next;
                        unsigned i = HASH_FUN(&current->data) % new_size;
                        if (new_table[i]) {
                                new_table[i]->prev = current;
                        }
                        current->next = new_table[i];
                        current->prev = NULL;
                        new_table[i] = current;

                        current = next;
                }
        }
        free(Q->table);
        Q->table = new_table;
        Q->table_size = new_size;

        return;

        // free(new_table);
bad_alloc1:
        return;
}

static inline
CSError CONCAT(QueueSet_insert_, QTYPENAME)(struct QUEUESET * Q, QTYPE data)
{
        unsigned index = HASH_FUN(&data) % Q->table_size;
        struct QNODE * node;
        if (Q->table[index] == NULL) {
                node = QNODE_CREATE(data);
                if (!node) {
                        goto bad_alloc1;
                }
                Q->table[index] = node;
        } else {
                struct QNODE * current = Q->table[index];
                while (current) {
                        // printf("%lu\n", current);
                        if (EQ_FUN(&data, &current->data)) {
                                goto duplicate_found;
                        }
                        current = current->next;
                }
                node = QNODE_CREATE(data);
                if (!node) {
                        goto bad_alloc1;
                }
                Q->table[index]->prev = node;
                node->next = Q->table[index];
                Q->table[index] = node;
        }
        if (Q->begin) {
                Q->begin->q_prev = node;
                Q->begin = node;
        } else {
                Q->begin = node;
                Q->end = node;
        }
        Q->n_entries++;
        if (Q->n_entries > Q->table_size_threshold) {
                QUEUESET_EXPAND(Q, Q->table_size * SCALE_FACTOR);
                Q->table_size_threshold = Q->table_size * LOAD_FACTOR;
                // printf("%u %u\n", Q->n_entries, Q->table_size);
        }
        assert(Q->end);
duplicate_found:
        return NO_FAILURE;
bad_alloc1:
        return FAIL_ALLOC;
}


static inline
struct QNODE * CONCAT(Queueset_search_,QTYPENAME)(struct QUEUESET * Q, QTYPE data)
{
        unsigned index = HASH_FUN(&data) % Q->table_size;
        struct QNODE * current = Q->table[index];
        while (current) {
                if (EQ_FUN(&current->data, &data)) {
                        break;
                }
                current = current->next;
        }
        return current;
}
#define QUEUESET_DELETE CONCAT(QueueSet_delete_,QTYPENAME)
static inline CSError QUEUESET_DELETE(struct QUEUESET * Q, struct QNODE * node)
{
        if (node->next) {
                node->next->prev = node->prev;
        }
        if (node->prev) {
                node->prev->next = node->next;
        } else {
                unsigned index = HASH_FUN(&node->data) % Q->table_size;
                Q->table[index] = node->next;
        }
        free(node);
        return NO_FAILURE;
}

#define QUEUESET_POP CONCAT(QueueSet_pop_,QTYPENAME)

static inline
CSError QUEUESET_POP(struct QUEUESET * Q, QTYPE * data)
{
        struct QNODE * node = Q->end;
        if (!node) {
                // printf(">>%u\n", Q->n_entries);
                assert(Q->n_entries == 0);
                goto empty_queue;
        }
        if (node->q_prev) {
                Q->end = node->q_prev;
        } else {
                Q->begin = NULL;
                Q->end = NULL;
        }
        QTYPE x = node->data;
        QUEUESET_DELETE(Q, node);
        if (data) {
                *data = x;
        }
        Q->n_entries--;
        return NO_FAILURE;
empty_queue:
        return FAILURE;
}

static inline
void CONCAT(QueueSet_destroy_,QTYPENAME)(struct QUEUESET * Q)
{
        if (!Q) {
                return;
        }
        while (Q->n_entries != 0) {
                QUEUESET_POP(Q, NULL);
        }
        free(Q->table);
        free(Q);
}
#undef QUEUESET_POP
#undef QNODE_CREATE
#undef QUEUESET_DELETE
#undef QNODE_CREATE
#undef QUEUESET
#undef QNODE
#undef INITIAL_Q_SIZE
#undef LOAD_FACTOR
#undef SCALE_FACTOR

#undef pln
