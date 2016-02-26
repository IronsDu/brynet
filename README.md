##跨平台游戏开发组件


* src\net:
    * 跨平台网络库
* src\rpc:
    * 只提供序列化和反序列化的rpc api（支持异步回调）
* src\ssdb:
    * 支持多个链接的redis和ssdb客户端，使用异步回调方式的接口
* src\timer:
    * 简易定时器(使用weak_ptr)
* src\utils:
    * 线程间消息队列
    * 一些没咋使用的C库譬如：线程池，协成，定时器之类的
* src\examples:
    * 网络库测试
    * protobuf消息的反射测试
