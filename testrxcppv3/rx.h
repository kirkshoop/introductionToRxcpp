#pragma once

#include <cstdint>

#include <set>
#include <map>
#include <list>
#include <string>
#include <iostream>
#include <iomanip>
#include <exception>

#include <regex>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <future>
#include <queue>

namespace rx {

using namespace std;
using namespace std::chrono;
using std::decay_t;

}

#include "rx_util.h"

/// a subscription represents a managed asynchronous scope
///
/// similar to shared_ptr a subscription provides allocations that are scoped to its lifetime
/// nested scopes are supported by insert(subscription)/erase(subscription)
/// the async scope can be cancelled using stop(), the stop can be handled using insert(void())
/// the async scope can be joined by join()
///
#include "rx_subscription.h"

/// an observer is informed of next, error and complete
/// an observer has a subscription to represent its lifetime
///
/// the observer enforces a contract
///   0 or more next are followed by
///   0 or 1 call to error or complete folowed by
///   lifetime.stop()
///   there will never be overlapping calls to next, error or complete
///
#include "rx_observer.h"

/// a strand will defer a call to an observer to a 
/// later time and perhaps a different execution context.
/// a strand provides its own notion of now() to
/// support scenarios where time must be independednt 
/// of real-time. (media scrubbing, tests, etc..)
///
/// a strand guarantees FIFO order for items 
/// scheduled to start at the same time.
///
#include "rx_strand.h"

/// a context is used to start an async operation
/// it provides the lifetime (subscription) a 'payload'
/// instance bounded by the subscription lifetime and a
/// strand maker that is used to shift execution context.
///
#include "rx_context.h"

/// a subscriber is a function that takes a context and returns an observer
#include "rx_subscriber.h"
/// a starter is a function that takes a context and returns a subscription
#include "rx_starter.h"
/// an observable is a function that takes a subscriber and returns a starter
#include "rx_observable.h"
/// a lifter is a function that takes a subscriber and returns a subscriber
#include "rx_lifter.h"
/// an adaptor is a function that takes an observable and returns an observable
#include "rx_adaptor.h"
/// a terminator is a function that takes an observable and returns a starter
#include "rx_terminator.h"

/// constructs a context to pass to a starter
#include "rx_start.h"
/// constructs a type-forgetter inbetween two types
#include "rx_as_interface.h"
/// joins with the subscription returned from a started operation
#include "rx_join.h"

/// the pipe operator `operator|()` is used to connect the pieces together.
///
/// the order imposed is:
///   observable -> (adaptor|lifter)* -> subscriber -> context -> join
///
/// the pipe operator supports connecting any subset into an intermediate 
/// type that forms a reusable segment that can be connected into many operations
///
/// the constructed types are not bound to the value type or the error type for the operation.
/// the late binding of the types allows a single operation to process multiple types over time - like an async tuple.
///
/// transform_merge (the STL nominclature for flat_map) 
///   transform_merge(auto makeStrand, auto f) {return transform(f) | merge(makeStrand);}
/// or
///   auto threeeven = copy_if(even) | take(3);
/// 
#include "rx_pipe_operator.h"

#include "observables/rx_ints.h"
#include "observables/rx_intervals.h"

#include "lifters/rx_copy_if.h"
#include "lifters/rx_transform.h"
#include "lifters/rx_delay.h"
#include "lifters/rx_observe_on.h"
#include "lifters/rx_finally.h"
#include "lifters/rx_last_or_default.h"

#include "adaptors/rx_take.h"
#include "adaptors/rx_merge.h"
#include "adaptors/rx_transform_merge.h"

#include "subscribers/rx_printto.h"

#include "schedulers/rx_observe_at_queue.h"
#include "schedulers/rx_run_loop.h"
#include "schedulers/rx_new_thread.h"

namespace rx {

namespace shapes {

template<class Payload>
struct state;

struct subscription
{
    bool is_stopped();
    void stop();

    void insert(const subscription& s);
    void erase(const subscription& s);

    void insert(function<void()> stopper);

    template<class Payload, class... ArgN>
    state<Payload> make_state(ArgN... argn);
    template<class Payload>
    state<Payload> copy_state(const state<Payload>&);

    void bind_defer(function<void(function<void()>)> d);
    
    void join();
};

template<class Payload>
struct state {
    subscription lifetime;
    Payload& get();
};

struct observer {
    template<class T>
    void next(T);
    template<class E>
    void error(E);
    void complete();
};

template<class Clock>
struct strand {
    subscription lifetime;

    typename Clock::time_point now();
    void defer_at(typename Clock::time_point, observer);
};

template<class Payload, class Clock>
struct context {
    subscription lifetime;
    
    typename Clock::time_point now();
    void defer_at(typename Clock::time_point, observer);

    Payload& get();
};

struct starter {
    template<class Payload, class Clock>
    subscription start(context<Payload, Clock>);
};

struct subscriber {
    template<class Payload, class Clock>
    observer create(context<Payload, Clock>);
};

struct observable {
    starter bind(subscriber);
};

struct lifter {
    subscriber lift(subscriber);
};

struct adaptor {
    observable adapt(observable);
};

struct terminator {
    starter terminate(observable);
};


template<class Clock>
void defer(strand<Clock>, observer);
template<class Clock>
void defer_at(strand<Clock>, typename Clock::time_point, observer);
template<class Clock>
void defer_after(strand<Clock>, typename Clock::duration, observer);
template<class Clock>
void defer_periodic(strand<Clock>, typename Clock::time_point, typename Clock::duration, observer);

template<class Payload, class Clock>
void defer(context<Payload, Clock>, observer);
template<class Payload, class Clock>
void defer_at(context<Payload, Clock>, typename Clock::time_point, observer);
template<class Payload, class Clock>
void defer_after(context<Payload, Clock>, typename Clock::duration, observer);
template<class Payload, class Clock>
void defer_periodic(context<Payload, Clock>, typename Clock::time_point, typename Clock::duration, observer);

}

}

