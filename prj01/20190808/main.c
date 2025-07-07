#include "list.h"
#include "bitmap.h"
#include "hash.h"
#include "debug.h"
#include "hex_dump.h"
#include "limits.h"
#include "round.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>  
#include <unistd.h> 

// void dumpordelete(char arg[][30]);
void list_functions(char arg[][30]);
void hash_functions(char arg[][30]);
void bitmap_functions(char arg[][30]);

struct list_table* list_table_head = NULL;
struct hash_table* hash_table_head = NULL;
struct bitmap_table* bitmap_table_head = NULL;

int main() {
    char *line = NULL;
    size_t len = 0;
    ssize_t n;  // getline의 반환값
    while ((n = getline(&line, &len, stdin)) != -1) {
        if (strcmp("quit\n", line) == 0)
            break;
    
        if (n > 0 && line[n - 1] == '\n')  // 개행 문자가 있다면
            line[n - 1] = '\0';  // 개행 문자를 널 문자로 변경
        char arg[10][30] = {{'0'}};
        char *temp = strtok(line, " ");
        // Token화
        for (int i = 0; i < 10 && temp != NULL; i++) {
            strcpy(arg[i], temp); 
            temp = strtok(NULL, " ");
            // arg[0]: create
            // arg[1]: list, bitmap, hashtable
            // arg[2]: other parameters such as list0
            // arg[3]: bitmap size?
        }
        // create functions: list, hashtable, bitmap
        if (!strcmp(arg[0], "create") || !strcmp(arg[0], "delete") || !strcmp(arg[0], "dumpdata")){
            if(!strncmp(arg[1],"list", 4))
                list_functions(arg);
            else if(!strncmp(arg[1],"hash", 4))
                hash_functions(arg);
            else
                bitmap_functions(arg);
        }
        
        // other functions: list, hashtable, bitmap
        else {
            if(!strncmp(arg[0],"list",4))
                list_functions(arg);
            else if(!strncmp(arg[0],"hash",4))
                hash_functions(arg);
            else
                bitmap_functions(arg);
        }
    }
    free(line);
    return 0;
}

void list_functions(char arg[][30]) {
    // create
    // ex) create list list0
    if (!strcmp(arg[0], "create")){
        struct list_table* ptr;
        struct list_table *new = (struct list_table*)malloc(sizeof(struct list_table));
        if (new == NULL) return;
        new->list = (struct list*)malloc(sizeof(struct list));
        if (new->list == NULL) {
            free(new);
            return;
        }
        strcpy(new->name, arg[2]);
        new->right = NULL;
        list_init(new->list);
        if(!list_table_head)
            list_table_head = new; 
        else {
            ptr = list_table_head;
            while (1) {
               if (ptr->right == NULL) {
                    ptr->right = new;     
                    break;
                }
                ptr = ptr->right;
            }
        }
    }
        
    // dumpdata
    // ex) dumpdata list0
    else if (!strcmp(arg[0], "dumpdata")) {
        for(struct list_table* ptr = list_table_head; ptr != NULL; ptr = ptr->right){
            if(!strcmp(arg[1], ptr->name)){
                if(list_empty(ptr->list))
                    return;
                for(struct list_elem* element_ptr = list_begin(ptr->list); element_ptr != list_end(ptr->list); element_ptr = list_next(element_ptr)) {
                    struct list_item* item_ptr = list_entry(element_ptr, struct list_item, elem);
                    printf("%d ", item_ptr->data);
                }
                printf("\n");
                return;
            }
        }
    }
    
    // delete
    // ex) delete list0
    else if (!strcmp(arg[0], "delete")) {
        // printf("delete list function\n");
        for(struct list_table* ptr = list_table_head; ptr != NULL; ptr = ptr->right){
            // found it!!
            if(!strcmp(arg[1], ptr->name)){
                // item부터 Free
                // 여기서부터 ㅈㄴ 꼬이네
                while(!(list_empty(ptr->list))){
                    struct list_elem* del = list_pop_front(ptr->list);
                    struct list_item* item = list_entry(del, struct list_item, elem);
                    free(item);
                }

                free(ptr->list);

                // 만약 해당 list_table이 첫번째 노드였으면
                if(ptr == list_table_head){
                    list_table_head = ptr->right;
                    free(ptr); 
                }
                // 그외의 경우
                else{
                    struct list_table* prev = list_table_head;
                    while(1) {
                        if (prev->right == ptr)
                            break;
                        prev = prev->right;  
                    }
                    prev-> right = ptr->right;
                    free(ptr);
                }
                break;
            }
        }
    }
    
    // other functions
    else {
        // 우선 list_table을 찾는다.
        struct list_table* list_table_ptr;
        for(list_table_ptr = list_table_head; list_table_ptr != NULL; list_table_ptr = list_table_ptr->right){
            if(!strcmp(list_table_ptr->name, arg[1])) break;
        }
        if (list_table_ptr == NULL) return;
        struct list *list_ptr = list_table_ptr->list;  
        // list_max
        if (!strcmp(arg[0], "list_max")) {
            // list_max list0
            struct list_elem *max = list_max(list_ptr, list_less, NULL);
            struct list_item* max_item = list_entry(max, struct list_item, elem);
            printf("%d\n", max_item->data);
        }
        // list_min
        else if (!strcmp(arg[0], "list_min")) {
            // list_min list0
            struct list_elem *min = list_min(list_ptr, list_less, NULL);
            struct list_item* min_item = list_entry(min, struct list_item, elem);
            printf("%d\n", min_item->data);
        }
        // list_size
        else if (!strcmp(arg[0], "list_size")) {
            size_t size = list_size(list_ptr);
            printf("%zu\n", size);
        }
        // list_empty
        else if (!strcmp(arg[0], "list_empty")) {
            // list_empty list0
            if(list_empty(list_ptr))
                printf("true\n");
            else
                printf("false\n");
        }
        // list_front
        else if (!strcmp(arg[0], "list_front")) {
            // list_front list0
            struct list_elem *front_elem = list_front(list_ptr);
            struct list_item *front_item = list_entry(front_elem, struct list_item, elem);
		    printf("%d\n", front_item->data);
        }
        // list_back
        else if (!strcmp(arg[0], "list_back")) {
            // list_back list0
            struct list_elem *back_elem = list_back(list_ptr);
            struct list_item *back_item = list_entry(back_elem, struct list_item, elem);
		    printf("%d\n", back_item->data);
        }
        // list_insert
        else if (!strcmp(arg[0], "list_insert")) {
            // list_insert list0 5 6
            struct list_item* new_item = (struct list_item*)malloc(sizeof(struct list_item));
            if (new_item == NULL) return;
            new_item->data = atoi(arg[3]);

            struct list_elem *temp = list_head(list_ptr);
            int end = atoi(arg[2]);
            for(int j = 0; j <= end; j++)
                temp = temp->next;
            list_insert(temp, &new_item->elem);
        }
        // list_insert_ordered
        else if (!strcmp(arg[0], "list_insert_ordered")) {
            // list_insert_ordered list0 5
            struct list_item* new_item = (struct list_item*)malloc(sizeof(struct list_item));
            if (new_item == NULL) return;
            new_item->data = atoi(arg[2]);
            list_insert_ordered(list_ptr, &new_item->elem, list_less, NULL);
        }
        // list_remove
        else if (!strcmp(arg[0], "list_remove")) {
            // list_remove list0 0
            struct list_elem *remove_ptr = list_head(list_ptr);
            int pos = atoi(arg[2]);
            for(int i = 0; i <= pos; i++)
                remove_ptr = remove_ptr->next;
            list_remove(remove_ptr);    
            struct list_item *del_item = list_entry(remove_ptr, struct list_item, elem);
            free(del_item);
        }
        // list_reverse
        else if (!strcmp(arg[0], "list_reverse")) {
            // list_reverse list0
           list_reverse(list_ptr);
        }
        // list_swap
        else if (!strcmp(arg[0], "list_swap")){
            // list_swap list0 0 1
            int a = atoi(arg[2]);
            int b = atoi(arg[3]);
            struct list_elem *elem_a = list_head(list_ptr);
            for (int i = 0; i <= a; i++) {
                elem_a = elem_a->next;
            }
            struct list_elem *elem_b = list_head(list_ptr);
            for (int i = 0; i <= b; i++) {
                elem_b = elem_b->next;
            }
            list_swap(elem_a, elem_b);
        }
        // list_push_back
        else if (!strcmp(arg[0], "list_push_back")) {
            // list_push_back list0 1 했음
            struct list_item* new_item = (struct list_item*)malloc(sizeof(struct list_item));
            if (new_item == NULL) return;
            new_item->data=atoi(arg[2]);
            list_push_back(list_ptr, &new_item->elem);
        }
        // list_push_front
        else if (!strcmp(arg[0], "list_push_front")) {
            // list_push_front list0 1 했음
            struct list_item* new_item = (struct list_item*)malloc(sizeof(struct list_item));
            if (new_item == NULL) return;
            new_item->data=atoi(arg[2]);
            list_push_front(list_ptr, &new_item->elem);
        }
        // list_pop_back
        else if (!strcmp(arg[0], "list_pop_back")) {
            // list_pop_back list0
            struct list_elem* del_elem = list_pop_back(list_ptr);
            struct list_item *del_item = list_entry(del_elem, struct list_item, elem);
            free(del_item);
        }
        // list_pop front
        else if (!strcmp(arg[0], "list_pop_front")) {
            // list_pop_front list0
            struct list_elem* del_elem = list_pop_front(list_ptr);
            struct list_item *del_item = list_entry(del_elem, struct list_item, elem);
            free(del_item);
        }  
        // list_sort
        else if (!strcmp(arg[0], "list_sort")) {
            // list_sort list0
            list_sort(list_ptr, list_less, NULL);
        }
        // list_splice
        else if (!strcmp(arg[0], "list_splice")) {
            /*
            list_splice list0 2 list1 1 4
            arg[1]이 새로 넣을 list
            arg[2]가 넣을 원소들 다음 위치
            arg[3]은 자를 List
            arg[4]는 First
            arg[5]는 Last(exclusive)
            void list_splice (struct list_elem *before, struct list_elem *first, struct list_elem *last)
            */
            // arg[1]은 넣을 List ==> list_ptr 이미 전에 있음.
            struct list *list_put_ptr = list_ptr;
            // arg[3]은 자를 List
            struct list_table* temp;
            for(temp = list_table_head; temp != NULL; temp = temp->right){
                if(!strcmp(temp->name, arg[3])) break;
            }
            if (temp == NULL) return;
            struct list *list_cut_ptr = temp->list;

            int before_pos = atoi(arg[2]);
            int first_pos = atoi(arg[4]);
            int last_pos = atoi(arg[5]);
            
            struct list_elem *before = list_head(list_put_ptr);
            for (int i = 0; i <= before_pos; i++) {
                before = before->next;
            }
            struct list_elem *first = list_head(list_cut_ptr);
            for (int i = 0; i <= first_pos; i++) {
                first = first->next;
            }
            struct list_elem *last = list_head(list_cut_ptr);
            for (int i = 0; i <= last_pos; i++) {
                last = last->next;
            }
            list_splice (before, first, last);
        }
        // list_shuffle
        else if (!strcmp(arg[0], "list_shuffle")) {
            // list_shuffle list1
            list_shuffle(list_ptr);
        }
        // list_unique
        else if (!strcmp(arg[0], "list_unique")) {
            // list_unique list0 list1
            // list_unique list1
            if (arg[2][0] != '\0') {
                struct list *list_ptr01 = list_ptr;
                struct list_table* temp;
                for(temp = list_table_head; temp != NULL; temp = temp->right){
                    if(!strcmp(temp->name, arg[2])) break;
                }
                if (temp == NULL) return;
                struct list *list_ptr02 = temp->list;  
                list_unique(list_ptr01, list_ptr02, list_less, NULL);
            }
            else {
                list_unique(list_ptr, NULL, list_less, NULL);
            }
        }
        else
            ;
    }
    return;
}

void bitmap_functions(char arg[][30]) {
    // 일단 찾아
    // create
    // ex) create bitmap bm0 16
    if (!strcmp(arg[0], "create")){
        struct bitmap_table* new = (struct bitmap_table*)malloc(sizeof(struct bitmap_table));
        if (new == NULL) return;
        new->bitmap = bitmap_create(atoi(arg[3]));
        if (new->bitmap == NULL) {
            free(new);
            return;
        }
        new->right = NULL;
        strcpy(new->name, arg[2]);

        if(!bitmap_table_head)
            bitmap_table_head = new;
        else {
            struct bitmap_table *ptr = bitmap_table_head;
            while (1) {
               if (ptr->right == NULL) {
                    ptr->right = new;     
                    break;
                }
                ptr = ptr->right;
            }
        }
    }
        
    // dumpdata
    // ex) dumpdata bm0
    else if (!strcmp(arg[0], "dumpdata")) {
        struct bitmap_table *ptr = bitmap_table_head;
        while (1) {
            if (ptr == NULL) break;
            if(!strcmp(arg[1], ptr->name)){
                for(int i = 0; i < bitmap_size(ptr->bitmap); i++){
                    if(bitmap_test(ptr->bitmap, i))
                        printf("1");
                    else printf("0");
                }
                printf("\n");
                return;
            }
            ptr = ptr->right;
        }
    }
    
    // delete
    else if (!strcmp(arg[0], "delete")) {
        // ex) delete bm0
        for(struct bitmap_table* ptr = bitmap_table_head; ptr != NULL; ptr = ptr->right){
            // found it!!
            if(!strcmp(arg[1], ptr->name)){
                // bitmap부터 free
                bitmap_destroy(ptr->bitmap);

                // 만약 첫번째 노드였으면
                if(ptr == bitmap_table_head){
                    bitmap_table_head = ptr->right;
                    free(ptr); 
                }
                // 그외의 경우
                else{
                    struct bitmap_table* prev = bitmap_table_head;
                    while(1) {
                        if (prev->right == ptr)
                            break;
                        prev = prev->right;  
                    }
                    prev-> right = ptr->right;
                    free(ptr);
                }
                break;
            }
        }   
    }
    
    // other functions
    else {
        struct bitmap_table* bitmap_table_ptr;    
        for(bitmap_table_ptr = bitmap_table_head; bitmap_table_ptr != NULL; bitmap_table_ptr = bitmap_table_ptr->right){
            // found it!!
            if(!strcmp(arg[1], bitmap_table_ptr->name)) break;
        }
        if (bitmap_table_ptr == NULL) return;
        struct bitmap *bitmap_ptr = bitmap_table_ptr->bitmap;
        // bitmap_set_all
        if (!strcmp(arg[0], "bitmap_set_all")) {
            // bitmap_set_all bm0 false
            if(!strcmp(arg[2],"true"))
                bitmap_set_all(bitmap_ptr, true);
            else 
                bitmap_set_all(bitmap_ptr, false);
            return;
        }
        // bitmap_set
        else if (!strcmp(arg[0], "bitmap_set")) {
            // bitmap_set bm0 0 true
            if(!strcmp(arg[3],"true"))
                bitmap_set(bitmap_ptr, atoi(arg[2]), true);
            else 
                bitmap_set(bitmap_ptr, atoi(arg[2]), false);
            return;
        }
        // bitmap_set_multiple
        else if (!strcmp(arg[0], "bitmap_set_multiple")) {
            // bitmap_set_multiple bm0 0 4 true
            if(!strcmp(arg[4],"true"))
                bitmap_set_multiple(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), true);
            else 
                bitmap_set_multiple(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), false);
            return;
        }
        // bitmap_size
        else if (!strcmp(arg[0], "bitmap_size")) {
            // bitmap_size bm0
            size_t size = bitmap_size(bitmap_ptr);
            printf("%zu\n", size);
        }
        // bitmap_test
        else if (!strcmp(arg[0], "bitmap_test")) {
            // bitmap_test bm0 4
            bool result = bitmap_test(bitmap_ptr, atoi(arg[2]));
            if(result) printf("true\n");
            else printf("false\n");
            return;
        }
        // bitmap_dump bm0
        else if (!strcmp(arg[0], "bitmap_dump")) {
            // bitmap_dump bm0
            bitmap_dump(bitmap_ptr);
        }

        // bitmap_flip
        else if (!strcmp(arg[0], "bitmap_flip")) {
            // bitmap_flip bm0 4
            bitmap_flip(bitmap_ptr, atoi(arg[2]));
            return;
        }
        // bitmap_mark
        else if (!strcmp(arg[0], "bitmap_mark")) {
            // bitmap_mark bm0 0
            bitmap_mark(bitmap_ptr, atoi(arg[2]));
            return;
        }
        // bitmap_reset
        else if (!strcmp(arg[0], "bitmap_reset")) {
            // bitmap_reset bm0 15
            bitmap_reset(bitmap_ptr, atoi(arg[2]));
            return;
        }
        // bitmap_none
        else if (!strcmp(arg[0], "bitmap_none")) {
            // bitmap_none bm0 1 1
            bool result = bitmap_none(bitmap_ptr, atoi(arg[2]), atoi(arg[3]));
            if(result) printf("true\n");
            else printf("false\n");
            return;
        }
        // bitmap_any
        else if (!strcmp(arg[0], "bitmap_any")) {
            // bitmap_any bm0 1 1
            bool result = bitmap_any(bitmap_ptr, atoi(arg[2]), atoi(arg[3]));
            if(result) printf("true\n");
            else printf("false\n");
            return;
        }
        // bitmap_all
        else if (!strcmp(arg[0], "bitmap_all")) {
            bool result = bitmap_all(bitmap_ptr, atoi(arg[2]), atoi(arg[3]));
            if(result) printf("true\n");
            else printf("false\n");
            return;
        }
        // bitmap_expand   
        else if (!strcmp(arg[0], "bitmap_expand")) {
            // bitmap_expand bm0 2
            bitmap_ptr = bitmap_expand(bitmap_ptr, atoi(arg[2]));
        }
        // bitmap_contains
        else if (!strcmp(arg[0], "bitmap_contains")) {
            // bitmap_contains bm0 0 2 true
            bool result;
            if(!strcmp(arg[4],"true"))
                result = bitmap_contains(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), true);
            else 
                result = bitmap_contains(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), false);
            
            if(result) printf("true\n");
            else printf("false\n");
            return;
        }
        // bitmap_count
        else if (!strcmp(arg[0], "bitmap_count")) {
            // bitmap_count bm0 2 10 false
            size_t count;
            if(!strcmp(arg[4],"true"))
                count = bitmap_count(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), true);
            else 
                count = bitmap_count(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), false);
            printf("%zu\n", count);
            return;
        }
        // bitmap_scan
        else if (!strcmp(arg[0], "bitmap_scan")) {
            // bitmap_scan bm0 0 1 true
            size_t index;
            if(!strcmp(arg[4],"true"))
                index = bitmap_scan(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), true);
            else 
                index = bitmap_scan(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), false);
            printf("%zu\n", index);
            return;
        }
        // bitmap_scan_and_flip
        else if (!strcmp(arg[0], "bitmap_scan_and_flip")) {
            // bitmap_scan_and_flip bm0 0 1 true
            size_t index;
            if(!strcmp(arg[4],"true"))
                index = bitmap_scan_and_flip(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), true);
            else 
                index = bitmap_scan_and_flip(bitmap_ptr, atoi(arg[2]), atoi(arg[3]), false);
            printf("%zu\n", index);
            return;
        }
        else
            ;
    }
    return;
}

void hash_functions(char arg[][30]) {
    // create
    // ex) create hashtable hash0
    if (!strcmp(arg[0], "create")){
        struct hash_table*new = (struct hash_table*)malloc(sizeof(struct hash_table));
        if (new == NULL) return;
        new->hash = (struct hash*)malloc(sizeof(struct hash));
        if (new->hash == NULL) {
            free(new);
            return;
        }
        hash_init(new->hash, hash_hash, hash_less, NULL);
        strcpy(new->name, arg[2]);
        new->right = NULL;

        if(hash_table_head == NULL) hash_table_head = new;
        else{
            struct hash_table *ptr = hash_table_head;
            while (1) {
                if (ptr->right == NULL) {
                    ptr->right = new;     
                    break;
                }
                ptr = ptr->right;
            }
        }
        return;
    }
        
    // dumpdata
    // ex) dumpdata hash0
    else if (!strcmp(arg[0], "dumpdata")) {
        
        struct hash_table* hash_table_ptr;    
        for(hash_table_ptr = hash_table_head; hash_table_ptr != NULL; hash_table_ptr = hash_table_ptr->right){
            // found it!!
            if(!strcmp(arg[1], hash_table_ptr->name)) break;
        }
        if (hash_table_ptr == NULL) return;
        struct hash *hash_ptr = hash_table_ptr->hash;
        
        if(hash_empty(hash_ptr)) return;
        
        struct hash_iterator i;
        hash_first(&i, hash_ptr);
        struct hash_elem *elem = hash_next(&i);  // 첫 번째 요소를 가져옴

        while (elem != NULL) {
            printf("%d ", elem->data);
            elem = hash_next(&i);
        }
        printf("\n");
        
        return;
    }
    
    // delete
    // ex) delete hash0
    else if (!strcmp(arg[0], "delete")) {
        for(struct hash_table* ptr = hash_table_head; ptr != NULL; ptr = ptr->right){
            if(!strcmp(arg[1], ptr->name)){
                struct hash* hash_ptr = ptr->hash;
                hash_destroy(hash_ptr, hash_free);

                // 만약 첫번째 노드였으면
                if(ptr == hash_table_head){
                    hash_table_head = ptr->right;
                    free(ptr); 
                }
                // 그외의 경우
                else{
                    struct hash_table* prev = hash_table_head;
                    while(1) {
                        if (prev->right == ptr)
                            break;
                        prev = prev->right;  
                    }
                    prev->right = ptr->right;
                    free(ptr);
                }
                break;
            }
        }
        
    }
    
    // other functions
    else {
        struct hash_table* hash_table_ptr;    
        for(hash_table_ptr = hash_table_head; hash_table_ptr != NULL; hash_table_ptr = hash_table_ptr->right){
            // found it!!
            if(!strcmp(arg[1], hash_table_ptr->name)) break;
        }
        if (hash_table_ptr == NULL) return;
        struct hash *hash_ptr = hash_table_ptr->hash;
        
        // hash_insert
        if (!strcmp(arg[0], "hash_insert")) {
            // hash_insert hash0 10
            struct hash_elem *new = (struct hash_elem*)malloc(sizeof(struct hash_elem));
            if (!new) return;
            new->data = atoi(arg[2]);
            hash_insert(hash_ptr, new);
            return;
        }
        // hash_replace
        else if (!strcmp(arg[0], "hash_replace")) {
            // hash_replace hash0 10
            struct hash_elem *replace = (struct hash_elem*)malloc(sizeof(struct hash_elem));
            if (!replace) return;
            replace->data = atoi(arg[2]);
            struct hash_elem *old = hash_replace(hash_ptr, replace);
            if (old != NULL) free(old);
            return;
        }
        // hash_delete
        else if (!strcmp(arg[0], "hash_delete")) {
            // hash_delete hash0 10
            struct hash_elem *delete = (struct hash_elem*)malloc(sizeof(struct hash_elem));
            if (!delete) return;
            delete->data = atoi(arg[2]);
            struct hash_elem *to_delete = hash_delete(hash_ptr, delete);
            free(delete);
            if (to_delete != NULL) free(to_delete); 
            return;
        }
        // hash_find
        else if (!strcmp(arg[0], "hash_find")) {
            // hash_find hash0 10
            struct hash_elem *find = (struct hash_elem*)malloc(sizeof(struct hash_elem));
            if (!find) return;
            find->data = atoi(arg[2]);
            struct hash_elem *found = hash_find(hash_ptr, find);
            if(found != NULL) printf("%d\n", found->data);
            // free(find);
            return;
        }
        // hash_apply
        else if (!strcmp(arg[0], "hash_apply")) {
            // hash_apply hash0 square
            if (!strcmp(arg[2], "square")) {
                hash_apply(hash_ptr, hash_square);
            }
            // hash_apply hash0 triple 
            else {
                hash_apply(hash_ptr, hash_triple);
            }

            return;
        }

        // hash_empty
        else if (!strcmp(arg[0], "hash_empty")) {
            // hash_empty hash0
            if(hash_empty(hash_ptr)) printf("true\n");
            else printf("false\n");
            return;
        }
        // hash_size
        else if (!strcmp(arg[0], "hash_size")) {
            // hash_size hash0
            printf("%zu\n", hash_size(hash_ptr));
            return;
        }
        // hash_clear
        else if (!strcmp(arg[0], "hash_clear")) {
            hash_clear(hash_ptr, hash_free);
            // hash_clear hash0
            return;
        }
        else
            ;
    }
    return;   
}