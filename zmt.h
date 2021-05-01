/*
	zmt.h
	Copyright 2021, zhiayang

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	Version 0.1.0
	=============


	Documentation
	=============

	This C++17 library contains some useful primitives for working in multithreaded programs, including:
	- condvar that actually has a sane API
	- semaphores
	- wait_queue
	- Synchronised<T> wrapper
	- ThreadPool

	Everything is templated, so there's no need to do the "_IMPLEMENTATION" macro for this library.


	Version History
	===============

	0.1.0 - 01/05/2021
	------------------
	Initial release
*/

#pragma once

#include <cstdlib>

#include <mutex>
#include <deque>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <type_traits>
#include <shared_mutex>


// primitives
namespace zmt
{
	// taken from "Move-only version of std::function"
	// https://stackoverflow.com/a/52358928/
	template <typename T>
	struct unique_function : private std::function<T>
	{
	private:
		template <typename Fn, typename = void>
		struct wrapper;

		// specialise for copyable types
		template <typename Fn>
		struct wrapper<Fn, std::enable_if_t<std::is_copy_constructible_v<Fn>>>
		{
			Fn fn;

			template <typename... Args>
			auto operator() (Args&&... args) { return this->fn(static_cast<Args&&>(args)...); }
		};

		// specialise for move-only types
		template <typename Fn>
		struct wrapper<Fn, std::enable_if_t<!std::is_copy_constructible_v<Fn> && std::is_move_constructible_v<Fn>>>
		{
			Fn fn;

			wrapper(Fn&& fn) : fn(static_cast<Fn&&>(fn)) { }

			wrapper(wrapper&&) = default;
			wrapper& operator= (wrapper&&) = default;

			// in theory, these two functions are never called.
			wrapper(const wrapper& other) : fn(const_cast<Fn&&>(other.fn)) { abort(); }
			wrapper& operator= (const wrapper&) { abort(); }

			template <typename... Args>
			auto operator() (Args&&... args) { return this->fn(static_cast<Args&&>(args)...); }
		};

		using base_type = std::function<T>;

	public:
		unique_function() = default;
		unique_function(std::nullptr_t) : base_type(nullptr) { }

		unique_function(unique_function&&) = default;
		unique_function& operator= (unique_function&&) = default;

		template <typename Fn>
		unique_function(Fn&& fn) : base_type(wrapper<Fn>(static_cast<Fn&&>(fn))) { }

		template <typename Fn>
		unique_function& operator= (Fn&& fn) { base_type::operator=(wrapper<Fn>(static_cast<Fn&&>(fn))); return *this; }

		unique_function& operator= (std::nullptr_t) { base_type::operator=(nullptr); return *this; }

		using base_type::operator();
	};

	template <typename T>
	struct condvar
	{
		condvar() : value() { }
		condvar(const T& x) : value(x) { }
		condvar(T&& x) : value(static_cast<T&&>(x)) { }

		condvar(const condvar&) = delete;
		condvar& operator= (const condvar&) = delete;

		condvar(condvar&&) = default;
		condvar& operator= (condvar&&) = default;

		void set(const T& x)
		{
			this->set_quiet(x);
			this->notify_all();
		}

		void set_quiet(const T& x)
		{
			auto lk = std::lock_guard<std::mutex>(this->mtx);
			this->value = x;
		}

		T get()
		{
			return this->value;
		}

		bool wait(const T& x)
		{
			auto lk = std::unique_lock<std::mutex>(this->mtx);
			this->cv.wait(lk, [&]{ return this->value == x; });
			return true;
		}

		// returns true only if the value was set; if we timed out, it returns false.
		bool wait(const T& x, std::chrono::nanoseconds timeout)
		{
			auto lk = std::unique_lock<std::mutex>(this->mtx);
			return this->cv.wait_for(lk, timeout, [&]{ return this->value == x; });
		}

		template <typename Predicate>
		bool wait_pred(Predicate p)
		{
			auto lk = std::unique_lock<std::mutex>(this->mtx);
			this->cv.wait(lk, p);
			return true;
		}

		// returns true only if the value was set; if we timed out, it returns false.
		template <typename Predicate>
		bool wait_pred(std::chrono::nanoseconds timeout, Predicate p)
		{
			auto lk = std::unique_lock<std::mutex>(this->mtx);
			return this->cv.wait_for(lk, timeout, p);
		}

		void notify_one() { this->cv.notify_one(); }
		void notify_all() { this->cv.notify_all(); }

	private:
		T value;
		std::mutex mtx;
		std::condition_variable cv;

		friend struct semaphore;
	};

	struct semaphore
	{
		semaphore(uint64_t x) : value(x) { }

		void post(uint64_t num = 1)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->value += num;
			}

			if(num > 1) this->cv.notify_all();
			else        this->cv.notify_one();
		}

		void wait()
		{
			auto lk = std::unique_lock<std::mutex>(this->mtx);
			while(this->value == 0)
				this->cv.wait(lk);

			this->value -= 1;
		}

	private:
		uint64_t value = 0;
		std::condition_variable cv;
		std::mutex mtx;
	};




	template <typename T>
	struct wait_queue
	{
		void push(const T& x)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.push_back(x);
			}
			this->sem.post();
		}

		void push(T&& x)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.push_back(static_cast<T&&>(x));
			}
			this->sem.post();
		}

		void push_quiet(const T& x)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.push_back(x);
			}
			this->pending_notifies++;
		}

		void push_quiet(T&& x)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.push_back(static_cast<T&&>(x));
			}
			this->pending_notifies++;
		}


		template <typename... Args>
		void emplace(Args&&... xs)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.emplace_back(static_cast<Args&&>(xs)...);
			}
			this->sem.post();
		}

		template <typename... Args>
		void emplace_quiet(Args&&... xs)
		{
			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				this->queue.emplace_back(static_cast<Args&&>(xs)...);
			}
			this->pending_notifies++;
		}

		void notify_pending()
		{
			auto tmp = this->pending_notifies.exchange(0);
			this->sem.post(tmp);
		}

		T pop()
		{
			this->sem.wait();

			{
				auto lk = std::unique_lock<std::mutex>(this->mtx);
				auto ret = static_cast<T&&>(this->queue.front());
				this->queue.pop_front();

				return ret;
			}
		}

		size_t size() const
		{
			return this->queue.size();
		}

		wait_queue() : sem(0) { }

		wait_queue(const wait_queue&) = delete;
		wait_queue& operator= (const wait_queue&) = delete;

		wait_queue(wait_queue&&) = default;
		wait_queue& operator= (wait_queue&&) = default;

	private:
		std::atomic<int64_t> pending_notifies = 0;
		std::deque<T> queue;
		std::mutex mtx;     // mtx is for protecting the queue during push/pop
		semaphore sem;      // sem is for signalling when the queue has stuff (or not)
	};
}



// Synchronised<T>
namespace zmt
{
	template <typename T>
	struct Synchronised
	{
	private:
		struct ReadLockedInstance;
		struct WriteLockedInstance;

		using Lk = std::shared_mutex;

		T value;
		mutable Lk lk;
		std::function<void ()> write_lock_callback = { };

	public:
		Synchronised() { }
		~Synchronised() { }

		Synchronised(const T& x) : value(x) { }
		Synchronised(T&& x) : value(std::move(x)) { }

		template <typename... Args>
		Synchronised(Args&&... xs) : value(std::forward<Args>(xs)...) { }

		Synchronised(Synchronised&&) = delete;
		Synchronised(const Synchronised&) = delete;
		Synchronised& operator= (Synchronised&&) = delete;
		Synchronised& operator= (const Synchronised&) = delete;

		void on_write_lock(std::function<void ()> fn)
		{
			this->write_lock_callback = std::move(fn);
		}

		template <typename Functor>
		void perform_read(Functor&& fn) const
		{
			std::shared_lock lk(this->lk);
			fn(this->value);
		}

		template <typename Functor>
		void perform_write(Functor&& fn)
		{
			if(this->write_lock_callback)
				this->write_lock_callback();

			std::unique_lock lk(this->lk);
			fn(this->value);
		}

		template <typename Functor>
		auto map_read(Functor&& fn) const -> decltype(fn(this->value))
		{
			std::shared_lock lk(this->lk);
			return fn(this->value);
		}

		template <typename Functor>
		auto map_write(Functor&& fn) -> decltype(fn(this->value))
		{
			if(this->write_lock_callback)
				this->write_lock_callback();

			std::unique_lock lk(this->lk);
			return fn(this->value);
		}

		Lk& getLock()
		{
			return this->lk;
		}

		ReadLockedInstance rlock() const
		{
			return ReadLockedInstance(*this);
		}

		WriteLockedInstance wlock()
		{
			if(this->write_lock_callback)
				this->write_lock_callback();

			return WriteLockedInstance(*this);
		}

	private:

		// static Lk& assert_not_held(Lk& lk) { if(lk.held()) assert(!"cannot move held Synchronised"); return lk; }

		struct ReadLockedInstance
		{
			const T* operator -> () { return &this->sync.value; }
			const T* get() { return &this->sync.value; }
			~ReadLockedInstance() { this->sync.lk.unlock_shared(); }

		private:
			ReadLockedInstance(const Synchronised& sync) : sync(sync) { this->sync.lk.lock_shared(); }

			ReadLockedInstance(ReadLockedInstance&&) = delete;
			ReadLockedInstance(const ReadLockedInstance&) = delete;

			ReadLockedInstance& operator = (ReadLockedInstance&&) = delete;
			ReadLockedInstance& operator = (const ReadLockedInstance&) = delete;

			const Synchronised& sync;

			friend struct Synchronised;
		};

		struct WriteLockedInstance
		{
			T* operator -> () { return &this->sync.value; }
			T* get() { return &this->sync.value; }
			~WriteLockedInstance() { this->sync.lk.unlock(); }

		private:
			WriteLockedInstance(Synchronised& sync) : sync(sync) { this->sync.lk.lock(); }

			WriteLockedInstance(WriteLockedInstance&&) = delete;
			WriteLockedInstance(const WriteLockedInstance&) = delete;

			WriteLockedInstance& operator = (WriteLockedInstance&&) = delete;
			WriteLockedInstance& operator = (const WriteLockedInstance&) = delete;

			Synchronised& sync;

			friend struct Synchronised;
		};
	};
}


// async operations (threadpool, futures)
namespace zmt
{
	template <typename T>
	struct future
	{
		template <typename F = T>
		std::enable_if_t<!std::is_same_v<void, F>, T>& get()
		{
			this->wait();
			return this->state->value;
		}

		template <typename F = T>
		void set(std::enable_if_t<!std::is_same_v<void, F>, T>&& x)
		{
			this->state->value = x;
			this->state->cv.set(true);
		}

		template <typename F = T>
		std::enable_if_t<std::is_same_v<void, F>, void> get()
		{
			this->wait();
		}

		template <typename F = T>
		std::enable_if_t<std::is_same_v<void, F>, void> set()
		{
			this->state->cv.set(true);
		}

		void wait() const
		{
			this->state->cv.wait(true);
		}

		void discard()
		{
			this->state->discard = true;
		}

		~future() { if(this->state && !this->state->discard) { this->state->cv.wait(true); } }

		future() { this->state = std::make_shared<internal_state<T>>(); this->state->cv.set(false); }

		template <typename F = T>
		future(std::enable_if_t<!std::is_same_v<void, F>, T>&& val)
		{
			this->state = std::make_shared<internal_state<T>>();
			this->state->value = val;
			this->state->cv.set(true);
		}

		future(future&& f)
		{
			this->state = f.state;
			f.state = nullptr;
		}

		future& operator = (future&& f)
		{
			if(this != &f)
			{
				this->state = f.state;
				f.state = nullptr;
			}

			return *this;
		}

		future(const future&) = delete;
		future& operator = (const future&) = delete;

	private:
		friend struct ThreadPool;

		template <typename E>
		struct internal_state
		{
			internal_state() { cv.set(false); }

			E value;
			condvar<bool> cv;
			bool discard = false;

			internal_state(internal_state&& f) = delete;
			internal_state& operator = (internal_state&& f) = delete;
		};

		template <>
		struct internal_state<void>
		{
			internal_state() { discard = false; cv.set(false); }

			condvar<bool> cv;
			bool discard;

			internal_state(internal_state&& f) = delete;
			internal_state& operator = (internal_state&& f) = delete;
		};


		future(std::shared_ptr<internal_state<T>> st) : state(st) { }
		future clone() { return future(this->state); }

		std::shared_ptr<internal_state<T>> state;
	};

	struct ThreadPool
	{
		template <typename Fn, typename... Args>
		auto run(Fn&& fn, Args&&... args) -> future<decltype(fn(static_cast<Args&&>(args)...))>
		{
			using T = decltype(fn(static_cast<Args&&>(args)...));

			auto fut = future<T>();
			this->jobs.emplace([fn = std::move(fn), args..., f1 = fut.clone()]() mutable {
				if constexpr (!std::is_same_v<T, void>)
				{
					f1.set(fn(static_cast<decltype(args)&&>(args)...));
				}
				else
				{
					fn(static_cast<decltype(args)&&>(args)...);
					f1.set();
				}
			});

			return fut;
		}

		ThreadPool(size_t num = std::thread::hardware_concurrency())
		{
			if(num == 0)
				num = 1;

			this->num_workers = num;
			this->workers = new std::thread[this->num_workers];
			this->start_workers();
		}

		~ThreadPool()
		{
			this->stop_all();
			delete[] this->workers;
		}

		void stop_all()
		{
			this->jobs.push(Job::stop());
			for(size_t i = 0; i < this->num_workers; i++)
				this->workers[i].join();
		}

		void set_max_workers(size_t num)
		{
			this->stop_all();
			delete[] this->workers;

			if(num == 0)
				num = 1;

			this->num_workers = num;
			this->workers = new std::thread[this->num_workers];
			this->start_workers();
		}

		ThreadPool(ThreadPool&&) = delete;
		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator = (ThreadPool&&) = delete;
		ThreadPool& operator = (const ThreadPool&) = delete;

	private:
		void start_workers()
		{
			for(size_t i = 0; i < num_workers; i++)
			{
				this->workers[i] = std::thread([this]() {
					worker(this);
				});
			}
		}


		static void worker(ThreadPool* tp)
		{
			while(true)
			{
				auto job = tp->jobs.pop();
				if(job.should_stop)
				{
					tp->jobs.push(Job::stop());
					break;
				}

				job.func();
			}
		}

		struct Job
		{
			bool should_stop = false;
			unique_function<void (void)> func;

			Job() { }
			explicit Job(unique_function<void (void)>&& f) : func(std::move(f)) { }

			static inline Job stop() { Job j; j.should_stop = true; return j; }
		};

		size_t num_workers = 0;
		std::thread* workers = nullptr;

		wait_queue<Job> jobs;
	};

	namespace futures
	{
		template <typename... Args>
		inline void wait(future<Args>&... futures)
		{
			// i love c++17
			(futures.wait(), ...);
		}

		template <typename L>
		inline void wait(const L& futures)
		{
			for(const auto& f : futures)
				f.wait();
		}
	}
}
