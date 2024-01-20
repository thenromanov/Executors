# Executors framework

## Introduction

To summarize, the code of all parallel computing looks something like this:

```c++

class Output;
class Input;

Output Compute(const Input& inputs) {
// Select concurrency level
int num_threads = std::thread::hardware_concurrency();

// Split input into tasks
std::vector<Tasks> tasks = SplitIntoSubtasks(inputs, num_threads);

// Launch threads
std::vector<std::thread> threads;
for (int i = 0; i < num_threads; ++i) {
threads.emplace_back([i, &inputs, &tasks] {
ComputeSubtask(&tasks[i], input);
});
}

// Join all threads
for (auto& t : threads) t.join();

// Aggregate
return Aggregate(tasks, input);
}
```

If you look at the code carefully, you will notice that 2
independent actions are mixed in it:

1. A large task is divided into small independent subtasks. Then
The solutions to the subtasks are combined together. This code is specific to each algorithm.

2. The code decides how many threads to run, how to run them, and when
to complete them. This code is the same everywhere.

Everything is fine with the first point, but there are problems with the second.

* The user cannot control how many threads will be started.
The code acts selfishly and takes up all the cores on the machine._

* It is inconvenient to use such code inside another parallel algorithm.
For example, if we split the task into 10 parts at the first level and want to solve each one using `Compute()`, then we will start `10 * hardware concurrency` threads._

* You cannot cancel the calculation, you cannot monitor the progress.

All problems arise from the fact that the code itself is engaged in creating
threads. We want to take this decision to the highest level, and in
The code should only be split into independent subtasks. In this
assignment, you need to write a library to help you perform this separation.

### Executors and Tasks

* A `Task` is some kind of piece of computing. The calculation code itself is in
the run() method and is defined by the user.
*An `Executor' is a set of threads that can perform a `Task' and.
* `Executor` *must* run threads in the constructor, no new threads should be created during operation.
* To start executing a `Task`, the user must send it to the 'Executor` using the method
`Submit()`.
* After that, the user can wait until the `Task` is completed by calling the `Task::Wait` method.

```c++
class MyPrimeSplittingTask : public Task {
Params params_;
public:
MyPrimeSplittingTask(Params params) : params_(params) {}
bool is_prime = false;
virtual void Run() {
is_prime = check_is_prime(params_);
}
}

bool DoComputation(std::shared_ptr<Executor> pool, Params params) {
auto my_task = std::make_shared<MyPrimeSplittingTask>(params);
pool->Submit(my_task);
my_task.Wait();
return my_task->is_prime;
}
```

* `Task` may complete successfully (`IsCompleted`), with an error
(`IsFailed') and be cancelled (`isCanceled'). After
one of these events has happened to it, it is considered completed
(`isFinished').

* The user can cancel the `Task` at any time using the method
`Cancel()`. In this case, if the execution of the `Task' has not yet been completed
If it has begun, then it should not begin.

* `Task` may have dependencies. For example, in the reduce task, first
The reduction should have been performed in chunks of the vector, and then one
final reduction in intermediate values. The user can
to say that one `Task` should be executed only after some
other `Task` has been executed by calling the method
`Task::AddDependency`.

* `Task` can have triggers (`Task::AddTrigger'). In this case, it should start
execution after at least one trigger has completed.

* A `Task` can have one time trigger
(`Task::SetTimeTrigger`). In this case, it should start
execution if the `deadline` time has arrived.

* In general, the `Executor::Submit` should not start execution
immediately, but wait for the condition:
* _ Or_ there are dependencies and they are all fulfilled
* _ Or_ one of the triggers has been executed
* _ Or_ the `deadline` is set, and it's time for the `deadline`.
If the task has no dependencies, no triggers, and no deadline is set,
then it can be performed immediately.
Until `Submit` is called for the task, it cannot be executed.

* The `Executor` provides an API to stop execution.
* `Executor::StartShutdown' - starts the shutdown process. Tasks that
were sent after the `StartShutdown` should immediately switch to the Cancelled state.
The function can be called several times.
* `Executor::WaitShutdown' - is blocked until the Executor stops.
The function can be called several times.
* `Executor::~Executor` - implicitly does shutdown and waits for threads to finish.

### Futures

The `Task` and `Executor` interfaces are quite verbose, in the second
part of the task you will need to implement the `Future` class and several combinators to it.

* `Future` is a `Task' that has a result (some value).

* Combinator interfaces are defined in the `Executor' class:
* `Invoke(cb)` - execute `cb` inside `Executor`-and return the result via `Future`.
* `Then(input, cb)` - execute `cb` after the `input` ends. Returns `Future` to the result of `cb` without waiting for the execution of `input`.
* `WhenAll(vector<FuturePtr<T>>) -> FuturePtr<vector<T>>` - collects the result of several `Futures` into one.
* `WhenFirst(vector<FuturePtr<T>>) -> FuturePtr<T>` - returns the result that appears first.
* `WhenAllBeforeDeadline(vector<FuturePtr<T>>, deadline) -> FuturePtr<vector<T>>` - returns all the results that appeared before the deadline.
