// Тест singlefunc2:
// Функция с пустым циклом и обращением к полям структуры

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
}
