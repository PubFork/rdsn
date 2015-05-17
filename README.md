
## Quick Links

* [Installation](https://github.com/Microsoft/rDSN/wiki/Installation)
* [Tutorial](https://github.com/Microsoft/rDSN/wiki/A-Tutorial-for-Developers)
* [Built-in Tools, Runtime Policies and Distributed Frameworks](https://github.com/Microsoft/rDSN/wiki/Available-Tools,-Policies-and-Frameworks)
* [Programming Tips and FAQ](https://github.com/Microsoft/rDSN/wiki/Programming-Tips-and-FAQ)
* [Design Rational and Architecture](https://github.com/Microsoft/rDSN/wiki/Design-Rational-and-Architecture)
* [Contribute to rDSN](https://github.com/Microsoft/rDSN/wiki/Contribute)

***

Robust Distributed System Nucleus (rDSN) is an open framework for quickly building and managing high performance and robust distributed systems. It is [now open source](http://research.microsoft.com/en-us/projects/rdsn/default.aspx) with the MIT license. The idea is a [coherent and principled design](https://github.com/Microsoft/rDSN/wiki/Design-Rational-and-Architecture) that distributed systems, tools, and frameworks can be developed independently and later on integrated (almost) transparently. Following are some highlights for different audience of this framework.

##### Developers: a framework for quickly building and managing high performance and robust distributed systems.

* flexible programming and deployment
 * multiple-platform support (Linux, Windows, Mac)
 * quick prototyping via code generation with Thrift and Protocol Buffer, extensible for others
 * enhanced RPC library with multi-port, multi-channel, multi-language-client support
 * flexible to plugin your own low level constructs (network, logging, task queue, lock, etc.)
 * single executable, multiple role, multiple instance deployment
* **built-in support from [a growing set of tools and frameworks](https://github.com/Microsoft/rDSN/wiki/Available-Tools,-Policies-and-Frameworks) with little or no cost**
 * layer 1: single node tools: simulation, fault injection, tracing, profiling, replay, ...
 * layer 2: from single node to a partitioned and replicated service: partitioning and replication with little cost
 * layer 3: from single service to workflow across multiple services to handle end-to-end incoming workloads

##### Researchers and tool-oriented developers: a tool platform which easies tool development and enables transparent integration with upper applications.

* reliably expose dependencies and non-determinisms in upper systems to the underlying tools
* dedicated Tool API for tool and runtime policy development
* transparent integration of the tools with the upper applications to make real impact

##### Students: a distributed system learning platform where you can easily simplify, understand and manipulate a system.

* progressive protocol development to align with the courses, e.g., synchronous and reliable messages first, then asynchronous and unreliable 
* easy test, debug, and monitoring for system understanding, as with the developers
* easy further tool development, as with the researchers
