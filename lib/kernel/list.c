#include "list.h"
#include "interrupt.h"


//initial list
void list_init(struct list* list){
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

//insert 'elem' into the front of 'before' 
void list_insert_before(struct list_elem* before , struct list_elem* elem){
	enum intr_status old_status = intr_disable();
	before->prev->next = elem;
	elem->prev = before->prev;
	elem->next = before;
	before->prev = elem;
	intr_set_status(old_status);
}

//push to the head
void list_push(struct list* plist , struct list_elem* elem){
	list_insert_before(plist->head.next,elem);
}

//append to the tail
void list_append(struct list* plist,struct list_elem* elem){
	list_insert_before(&plist->tail,elem);
}

//remove the target element
void list_remove(struct list_elem* pelem){
	enum intr_status old_status = intr_disable();
	pelem->prev->next = pelem->next;
	pelem->next->prev = pelem->prev;
	intr_set_status(old_status);
}

//pop the first elem and return the element
struct list_elem* list_pop(struct list* plist){
	struct list_elem* elem = plist->head.next;
	list_remove(elem);
	return elem;
}


//search the target element in 'plist' 
bool elem_find(struct list* plist, struct list_elem* obj_elem){
	struct list_elem* elem = plist->head.next;
	while(elem != &plist->tail){
		if (elem == obj_elem){
			return true;
		}
		elem = elem->next;
	}
	return false;
}

//traversal all element in this list ,and judge elem is it eligible (meet a condition)
struct list_elem* list_traversal(struct list* plist, function func , int arg){
	struct list_elem* elem = plist->head.next;
	if (list_empty(plist)){
		return NULL;
	}
	while (elem != &plist->tail){
		if (func(elem,arg)){
			return elem;
		}
		elem = elem->next;
	}
	return NULL;
}



//return the length of list
uint32_t list_len(struct list* plist){
	struct list_elem* elem = plist->head.next;
	uint32_t length = 0;
	while (elem != &plist->tail){
		length ++;
		elem = elem->next;
	}
	return length;
}

//judge this list is it empty
bool list_empty(struct list* plist){
	return (plist->head.next == &plist->tail ? true : false);
}
