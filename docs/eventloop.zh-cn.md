# 概述
`EventLoop`用于socket的读写检测,以及线程间通信.

源码见：[EventLoop.hpp](https://github.com/IronsDu/brynet/blob/master/include/brynet/net/EventLoop.hpp)，
`EventLoop`不是必须使用智能指针，可以使用值对象，当然它也是禁止拷贝的。

# 接口
- `EventLoop::loop(int64_t milliseconds)`
	
	进行一次轮询，milliseconds为超时时间(毫秒).</br>
	也即当没有任何外部事件产生时,此函数等待milliseconds毫秒后返回.</br>
	当有事件产生时,做完工作后即可返回,所需时间依负荷而定.</br>
	通常，我们会开启一个线程，在其中间断性的调用`loop`接口。

- `EventLoop::bindCurrentThread`

	初始化调度EventLoop的thread id，用于使用EventLoop接口时时判断当前线程是否处于EventLoop::loop所在线程。在使用EventLoop的一些函数，比如runAsyncFunctor时，都需要执行bindCurrentThread（当然，你也可以直接调用loop来初始化）

- `EventLoop::wakeup(void)`
	
	(线程安全)唤醒可能阻塞在`EventLoop::loop`中的等待。</br>
	（当然，当EventLoop没有处于等待时，wakeup不会做任何事情，即没有额外开销）</br>
	另外! 此函数永远成功。其返回值仅仅表示函数内部是否真正触发了唤醒所需的函数。

- `EventLoop::runAsyncFunctor(std::function<void(void)>)`
	
	(线程安全)投递一个异步函数给`EventLoop`，此函数会在`EventLoop::loop`调用中被执行。如果此时EventLoop还没有初始化thread id（通过bindCurrentThread或loop），那么此函数会抛出异常，避免用户忘记调度IO工作线程时就投递了任务而导致逻辑错误。

- `EventLoop::runFunctorAfterLoop(std::function<void(void)>)`

	在`EventLoop::loop`所在线程中投递一个延迟函数，此函数会在`loop`接口中的末尾(也即函数返回之前)时被调用。</br>
	此函数只能在io线程（也就是调用loop函数的所在线程）使用，如果在其他线程中调用此函数会产生异常。

- `EventLoop::isInLoopThread(void)`
	
	(线程安全)检测当前线程是否和 `EventLoop::loop`所在线程(也就是最先调用`loop`接口的线程)一样。

- `brynet::base::Timer::WeakPtr EventLoop::runAfter(std::chrono::nanoseconds timeout, std::function<void(void)>&& callback)
	
	开启一个延迟执行的函数，当到期时会在EventLoop工作线程（即loop函数所在线程）里执行callback。可通过返回的brynet::base::Timer::WeakPtr的cancel函数来取消定时器。

- `RepeatTimer::Ptr EventLoop::brynet::base::RepeatTimer::Ptr runIntervalTimer(std::chrono::nanoseconds timeout, std::function<void(void)>&& callback)`

	开启一个重复执行的延迟执行的函数，当每次到期时会在EventLoop工作线程（即loop函数所在线程）里执行callback。可通过返回的RepeatTimer::Ptr的cancel函数来取消定时器。


# 注意事项
- 当我们第一次在某个线程中调用`loop`之后，就不应该在其他线程中调用`loop`(当然如果你调用了,也没有任何效果/影响)
- 如果没有任何线程调用`loop`，那么使用`pushAsyncProc`投递的异步函数将不会被执行，直到有线程调用了`loop`接口，这些异步函数才会被执行。

# 示例
```C++
EventLoop ev;
ev.runAsyncFunctor([] {
    	std::cout << "hello world" << std::endl;
	});
ev.loop(1);

// output hello world
```
