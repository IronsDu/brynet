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

- `EventLoop::wakeup(void)`
	
	(线程安全)唤醒可能阻塞在`EventLoop::loop`中的等待。</br>
	（当然，当EventLoop没有处于等待时，wakeup不会做任何事情，即没有额外开销）</br>
	另外! 此函数永远成功。其返回值仅仅表示函数内部是否真正触发了唤醒所需的函数。

- `EventLoop::runAsyncFunctor(std::function<void(void)>)`
	
	(线程安全)投递一个异步函数给`EventLoop`，此函数会在`EventLoop::loop`调用中被执行。

- `EventLoop::runFunctorAfterLoop(std::function<void(void)>)`

	在`EventLoop::loop`所在线程中投递一个延迟函数，此函数会在`loop`接口中的末尾(也即函数返回之前)时被调用。</br>
	此函数只能在io线程（也就是调用loop函数的所在线程）使用，如果在其他线程中调用此函数会产生异常。

- `EventLoop::isInLoopThread(void)`
	
	(线程安全)检测当前线程是否和 `EventLoop::loop`所在线程(也就是最先调用`loop`接口的线程)一样。

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
