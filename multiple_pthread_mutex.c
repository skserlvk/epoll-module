#include <pthread.h>
#include <stdlib.h>

#define NHASH 20
#define HASH(id) (((unsigned long)id)%NHASH)

typedef struct {
	int id;
	int count;
	pthread_mutex_t obj_lock;
	counter_t *obj_next;
}counter_t;

counter_t *g_htable[NHASH];
pthread_mutex_t h_lock = PTHREAD_MUTEX_INITIALIZER;

/* only one mutex_lock */
counter_t single_count_init(int id)
{
	counter_t *obj;

	if ((obj = malloc(sizeof(counter_t))) != NULL) {
		obj->id = id;
		obj->count = 1;
		if (pthread_mutex_init(obj->obj_lock, NULL) != 0) {
			free(obj);
			return NULL;
		}
		
	}

	return obj;
}

/* multiple mutex_lock */
counter_t mult_count_init(int id)
{
	counter_t *obj;
	int idx;
	
	if ((obj = malloc(sizeof(counter_t))) != NULL) {
		obj->id = 1;
		obj->count = 1;
		if (pthread_mutex_init(obj->obj_lock, NULL) != 0) {
			free(obj);
			return NULL;
		}

		idx = HASH(id);
		pthread_mutex_lock(&h_lock);
		obj->obj_next = g_htable[idx];
		g_htable[idx] = obj;
		pthread_mutex_lock(&obj->obj_lock);
		pthread_mutex_unlock(&h_lock);
		pthread_mutex_unlock(&obj->obj_lock);
	}
	
	return obj;
}

void count_add(counter_t *obj)
{
	pthread_mutex_lock(&obj->obj_lock);
	obj->count++;
	pthread_mutex_unlock(&obj->obj_lock);
}

void single_count_release(counter_t *obj)
{
	pthread_mutex_lock(&obj->obj_lock);
	if (--obj->count == 0) {	/* last reference */
		pthread_mutex_unlock(&obj->obj_lock);
		pthread_mutex_destroy(&obj->obj_lock);
		free(obj);
	} else {
		pthread_mutex_unlock(&obj->obj_lock);
	}
	
}

counter_t obj_find(int id)
{
	counter_t *obj;

	pthread_mutex_lock(&h_lock);
	for(obj = g_htable[HASH(id)]; obj != NULL; obj = obj->obj_next) {
		if (obj->id == id) {
			count_add(obj);
			break;
		}
	}
	pthread_mutex_unlock(&h_lock);

	return (obj);
}

void mult_count_release(counter_t *obj)
{
	counter_t *tobj;
	int idx;

	pthread_mutex_lock(&obj->obj_lock);
	if (obj->count != 1) {
		obj->count--;
		pthread_mutex_unlock(&obj->obj_lock);
	} else {	/* last reference */
		/*will del obj, should unlock obj_lock before get h_lock*/
		pthread_mutex_unlock(&obj->obj_lock);

		pthread_mutex_lock(&h_lock);
		pthread_mutex_lock(&obj->obj_lock);

		/* need to recheck the condition */
		if (obj->count != 1) {
			obj->count--;	/* another pthread get obj_lock, just sub 1*/
			pthread_mutex_unlock(&obj->lock);
			pthread_mutex_unlock(&h_lock);
			return ;
		}

		/* remove from table */
		idx = HASH(obj->id);
		tobj = HASH(idx);
		if (tobj == obj) {
			HASH(idx) = obj->next;
		} else {
			while (tobj->obj_next != obj)
				tobj = obj->obj_next;

			tobj->obj_next = obj->obj_next;			
		}

		pthread_mutex_unlock(&h_lock);
		pthread_mutex_unlock(&obj->obj_lock); /* the order ??? */
		pthread_mutex_destroy(&obj->obj_lock);
		free(obj);
	}

}





