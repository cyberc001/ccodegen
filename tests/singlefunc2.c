// Тест singlefunc2:
// Функция с пустым заголовком цикла и обращением к полям структуры
// - struct argument
// - empty for initializier/condition/increment
// - switch with default case

void thread_sched_pqueue_heapify(struct thread_sched_pqueue* q)
{
	for(unsigned long int i = 0; i < q->size; ++i){
		unsigned long int k = i;
		struct thread* thr = q->heap[k];
		for(;;){
			if(q->heap[k/2]->vruntime < thr->vruntime) // heap condition is satisfied
				break;
			q->heap[k] = q->heap[k/2];
			k /= 2;
			if(k == 0)
				break;
		}
		q->heap[k] = thr;
	}
	switch(q->alloc_type){
		case ALLOC_INC:
			if(q->alloc_size - q->size < 16){
				q->alloc_size += 16;
				q->heap = realloc(q->heap, q->alloc_size * sizeof(struct thread));
			}
			break;
		case ALLOC_DOUBLE:
		{
			double coef = q->alloc_size / (double)q->size;
			if(coef < 1.5){
				q->alloc_size *= 2;
				q->heap = realloc(q->heap, q->alloc_size * sizeof(struct thread));
			}
			break;
		}
		default:
			kprintf("Invalid alloc type: %d", q->alloc_type);
			break;
	}
}
