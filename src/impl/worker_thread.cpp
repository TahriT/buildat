// http://www.apache.org/licenses/LICENSE-2.0
// Copyright 2014 Perttu Ahola <celeron55@gmail.com>
#include "interface/worker_thread.h"
#include "interface/mutex.h"
#include "interface/semaphore.h"
#include "core/log.h"
#include <c55/os.h>
#include <deque>
#ifdef _WIN32
	#include "ports/windows_compat.h"
#else
	#include <pthread.h>
	#include <semaphore.h>
#endif
#define MODULE "worker_thread"

namespace interface {
namespace worker_thread {

struct CThreadPool: public ThreadPool
{
	bool m_running = false;
	std::deque<up_<Task>> m_input_queue; // Push back, pop front
	std::deque<up_<Task>> m_output_queue; // Push back, pop front
	interface::Mutex m_mutex; // Protects each of the former variables
	interface::Semaphore m_tasks_sem; // Counts input tasks

	//std::deque<up_<Task>> m_pre_tasks; // Push back, pop front

	~CThreadPool()
	{
		request_stop();
		join();
	}

	struct Thread {
		bool stop_requested = true;
		bool running = false;
		interface::Mutex mutex; // Protects each of the former variables
		CThreadPool *pool = nullptr;
		pthread_t thread;
	};
	sv_<Thread> m_threads;

	static void *run_thread(void *arg)
	{
		log_d(MODULE, "Worker thread %p start", arg);
		Thread *thread = (Thread*)arg;
		for(;;){
			// Wait for a task
			thread->pool->m_tasks_sem.wait();
			up_<Task> current;
			{ // Grab the task from pool's input queue
				interface::MutexScope ms_pool(thread->pool->m_mutex);
				if(thread->pool->m_input_queue.empty()){
					// Can happen in special cases, eg. when stopping thread
					continue;
				}
				current = std::move(thread->pool->m_input_queue.front());
				thread->pool->m_input_queue.pop_front();
			}
			// Run the task's threaded part
			try {
				while(!current->thread());
			} catch(std::exception &e){
				log_w(MODULE, "Worker task failed: %s", e.what());
			}
			// Push the task to pool's output queue
			{
				interface::MutexScope ms_pool(thread->pool->m_mutex);
				thread->pool->m_output_queue.push_back(std::move(current));
			}
		}
		log_d(MODULE, "Worker thread %p exit", arg);
		interface::MutexScope ms(thread->mutex);
		thread->running = false;
		pthread_exit(NULL);
	}

	// Interface

	void add_task(up_<Task> task)
	{
		// TODO: Limit task->pre() execution time per frame
		while(!task->pre());
		interface::MutexScope ms(m_mutex);
		m_input_queue.push_back(std::move(task));
		m_tasks_sem.post();
	}

	void start(size_t num_threads)
	{
		interface::MutexScope ms(m_mutex);
		if(!m_threads.empty()){
			log_w(MODULE, "CThreadPool::start(): Already running");
			return;
		}
		m_threads.resize(num_threads);
		for(size_t i = 0; i < num_threads; i++){
			Thread &thread = m_threads[i];
			thread.pool = this;
			thread.stop_requested = false;
			if(pthread_create(&thread.thread, NULL, run_thread, (void*)&thread)){
				throw Exception("pthread_create() failed");
			}
			thread.running = true;
		}
	}

	void request_stop()
	{
		for(Thread &thread : m_threads){
			interface::MutexScope ms(thread.mutex);
			thread.stop_requested = true;
		}
	}

	void join()
	{
		for(Thread &thread : m_threads){
			interface::MutexScope ms(thread.mutex);
			if(!thread.stop_requested)
				log_w(MODULE, "Joining a thread that was not requested to stop");
			pthread_join(thread.thread, NULL);
		}
		m_threads.clear();
	}

	void run_post()
	{
		int64_t t1 = get_timeofday_us();
		size_t queue_size = 0;
		size_t post_count = 0;
		for(;;){
			// Pop an output task
			up_<Task> task;
			{
				interface::MutexScope ms(m_mutex);
				if(!m_output_queue.empty()){
					queue_size = m_output_queue.size();
					task = std::move(m_output_queue.front());
					m_output_queue.pop_front();
				}
			}
			if(!task)
				break;
			// run post() until too long has passed
			bool done = false;
			for(;;){
				post_count++;
				done = task->post();
				// If done, take next output task
				if(done)
					break;
				int64_t t2 = get_timeofday_us();
				int64_t max_t = 2000;
				if(queue_size > 4)
					max_t += (queue_size - 4) * 5000;
				if(t2 - t1 >= max_t)
					break;
			}
			// If still not done, push task to back to front of queue and stop
			// processing
			if(!done){
				interface::MutexScope ms(m_mutex);
				m_output_queue.push_front(std::move(task));
				break;
			}
		}
		/*int64_t t2 = get_timeofday_us();
		log_v(MODULE, "output post(): %ius (%zu calls; queue size: %zu)",
				(int)(t2 - t1), post_count, queue_size);*/
	}
};

ThreadPool* createThreadPool()
{
	return new CThreadPool();
}

}
}
// vim: set noet ts=4 sw=4:
