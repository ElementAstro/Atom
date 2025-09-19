#include <stdio.h>
#include <stdlib.h>

typedef struct LNode {
    int data;
    struct LNode *next;
} LNode, *LinkList;

#define MAXSIZE 100
typedef struct {
    int data[MAXSIZE];
    int length;
} SqList;

LinkList CreateList(int a[], int n) {
    LinkList L = (LinkList)malloc(sizeof(LNode));
    L->next = NULL;
    LNode *r = L;

    for (int i = 0; i < n; i++) {
        LNode *p = (LNode *)malloc(sizeof(LNode));
        p->data = a[i];
        p->next = NULL;
        r->next = p;
        r = p;
    }
    return L;
}

void PrintList(LinkList L) {
    LNode *p = L->next;
    if (!p) {
        return;
    }
    while (p != NULL) {
        printf("%d ", p->data);
        p = p->next;
    }
    printf("\n");
}

void PrintSqList(SqList L) {
    if (L.length == 0) {
        printf("空\n");
        return;
    }
    for (int i = 0; i < L.length; i++) {
        printf("%d ", L.data[i]);
    }
    printf("\n");
}

// 3
void DeleteAllX(LinkList L, int x) {
    LNode *p = L->next;
    LNode *pre = L;

    while (p != NULL) {
        if (p->data == x) {
            pre->next = p->next;
            free(p);
            p = pre->next;
        } else {
            pre = p;
            p = p->next;
        }
    }
}

// 5
void MoveMinToFront(LinkList L) {
    if (L->next == NULL || L->next->next == NULL)
        return;

    LNode *m = L->next;
    LNode *mp = L;
    LNode *p = L->next;
    LNode *pre = L;

    while (p != NULL) {
        if (p->data < m->data) {
            m = p;
            mp = pre;
        }
        pre = p;
        p = p->next;
    }

    if (mp == L)
        return;

    mp->next = m->next;

    m->next = L->next;
    L->next = m;
}

// 7
void Intersection(SqList A, SqList B, SqList *C) {
    C->length = 0;
    int i = 0, j = 0;

    while (i < A.length && j < B.length) {
        if (A.data[i] == B.data[j]) {
            C->data[C->length++] = A.data[i];
            i++;
            j++;
        } else if (A.data[i] < B.data[j]) {
            i++;
        } else {
            j++;
        }
    }
}

// 10
void MoveMaxToBack(LinkList L) {
    if (L->next == NULL || L->next->next == NULL)
        return;

    LNode *max_node = L->next;
    LNode *max_pre = L;
    LNode *p = L->next;
    LNode *pre = L;

    while (p != NULL) {
        if (p->data > max_node->data) {
            max_node = p;
            max_pre = pre;
        }
        pre = p;
        p = p->next;
    }

    if (max_node->next == NULL)
        return;

    max_pre->next = max_node->next;

    p = L;
    while (p->next != NULL) {
        p = p->next;
    }

    max_node->next = NULL;
    p->next = max_node;
}

int main() {
    int a3[] = {1, 1, 4, 5, 1, 4};
    LinkList L3 = CreateList(a3, 6);
    PrintList(L3);
    DeleteAllX(L3, 2);
    PrintList(L3);

    int a5[] = {5, 3, 8, 1, 6};
    LinkList L5 = CreateList(a5, 5);
    PrintList(L5);
    MoveMinToFront(L5);
    PrintList(L5);

    SqList A, B, C;
    A.data[0] = 1;
    A.data[1] = 3;
    A.data[2] = 5;
    A.data[3] = 7;
    A.length = 4;
    B.data[0] = 3;
    B.data[1] = 4;
    B.data[2] = 5;
    B.data[3] = 6;
    B.data[4] = 7;
    B.length = 5;

    PrintSqList(A);
    PrintSqList(B);

    Intersection(A, B, &C);
    PrintSqList(C);

    int a10[] = {2, 9, 4, 1, 7};
    LinkList L10 = CreateList(a10, 5);
    PrintList(L10);
    MoveMaxToBack(L10);
    PrintList(L10);

    return 0;
}