#pragma once 


///
/// demonstrate "async-tuple"
///
namespace {

using namespace std::chrono;
using namespace rx;

struct cstr_or_string {
    void operator()(const char* cstr) const {cout << "cstr   - " << cstr << endl;}
    void operator()(const string& s) const {cout <<  "string - " << s << endl;}
};

const auto text = [](){
    info("new text");
    return make_observable([=](auto scrb){
        info("text bound to subscriber");
        return make_starter([=](auto ctx) {
            info("text bound to context");
            auto r = scrb.create(ctx);
            info("text started");
            r.next("hello");
            r.next(string("world"));
            r.complete();
            return ctx.lifetime;
        });
    });
};

}

#if EMSCRIPTEN

extern"C" void EMSCRIPTEN_KEEPALIVE intervalsv3(){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto threeeven = copy_if(even) | 
        take(3) |
        delay(makeStrand, 1s);

    auto l = intervals(makeStrand, steady_clock::now(), 1s) | 
        threeeven |
        as_interface<long>() |
        finally([](){cout << "caller stopped" << endl;}) |
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE pushv3(int first, int last){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto l = async_ints(makeStrand, first, last) | 
        copy_if(even) | 
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE lastv3(int first, int last, int defaultValue){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto l = async_ints(makeStrand, first, last) | 
        copy_if(even) | 
        last_or_default(defaultValue) |
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE takev3(int first, int last, int count){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto l = async_ints(makeStrand, first, last) | 
        copy_if(even) | 
        take(count) |
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE errorv3(int first, int last, int count){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto l = async_ints(makeStrand, first, last) | 
        copy_if(always_throw) | 
        take(count) |
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE delayv3(int producems, int delayms, int count){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    const auto printproduced = [](auto& output){
        return make_lifter([&output](auto scbr){
            return make_subscriber([=, &output](auto ctx){
                auto r = scbr.create(ctx);
                auto start = ctx.now();
                return make_observer(r, r.lifetime, [=, &output](auto& r, auto v){
                    defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
                        output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << v << " produced" << endl;
                    }));
                    r.next(v);
                });
            });
        });
    };

    auto l = intervals(makeStrand, steady_clock::now() + milliseconds(producems), milliseconds(producems)) | 
        printproduced(cout) |
        delay(makeStrand, milliseconds(delayms)) |
        take(count) |
        printto(cout) |
        start<destruction>(subscription{}, destruction{});

    lifetime.insert(l);
}

extern"C" void EMSCRIPTEN_KEEPALIVE testdelayv3(int producems, int delayms, int count){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

    auto start = steady_clock::now();

    rx::test_loop<> loop;
    rx::recorded<int> on;

    const auto out = [=](auto& output, auto ctx, std::string w){
        auto start = on.origin();
        defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
            output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << w << endl;
        }));
    };

    const auto printout = [=](auto& output, std::string w){
        return make_lifter([=, &output](auto scbr){
            return make_subscriber([=, &output](auto ctx){
                auto r = scbr.create(ctx);
                return make_observer(r, r.lifetime, [=, &output](auto& r, auto v){
                    out(output, ctx, to_string(v) + " " + w);
                    r.next(v);
                });
            });
        });
    };

    auto gap = milliseconds(producems);
    auto delayby = milliseconds(delayms);

    auto tr = on.hot({
            on.next(on.origin() + gap * 1, 0),
            on.next(on.origin() + gap * 2, 1),
            on.next(on.origin() + gap * 3, 2),
            on.next(on.origin() + gap * 4, 3),
            on.next(on.origin() + gap * 5, 4)
        }) | 
        printout(cout, "produced") |
        on.record("produced") |
        delay(loop.make(), delayby) |
        take(count) |
        printout(cout, "") |
        on.record("emitted") |
        on.test(loop, on.lifespan(on.origin() + 200ms, on.origin() + 6000ms));

    loop.run();

    auto finish = steady_clock::now();

    cout << "Elapsed: " << fixed << setprecision(1) << setw(4) << float_seconds{finish - start}.count() << endl; 

    {
        auto expected = on.expected({
            on.next(on.origin() + gap * 1 + delayby, 0),
            on.next(on.origin() + gap * 2 + delayby, 1),
            on.next(on.origin() + gap * 3 + delayby, 2),
            on.complete(on.origin() + gap * 3 + delayby)
        });
        if (tr.get().marbles["emitted"] == expected) {
            cout << "SUCCEEDED" << endl; 
        } else {
            cout << "FAILED" << endl; 
            cout << "Actual:" << endl; 
            cout << tr.get().marbles["emitted"] << endl;
            cout << "Expected:" << endl; 
            cout << expected << endl;
        }
    }

    {
        auto expected = on.lifespan(on.origin() + 200ms, on.origin() + 4500ms);
        if (tr.get().lifespan == expected) {
            cout << "SUCCEEDED" << endl; 
        } else {
            cout << "FAILED" << endl; 
            cout << "Actual:" << endl; 
            cout << tr.get().lifespan << endl;
            cout << "Expected:" << endl; 
            cout << expected << endl;
        }
    }
}

#endif

#if !EMSCRIPTEN

void designcontext(int first, int last){

using namespace std::chrono;

using namespace rx;
using rx::copy_if;
using rx::transform;
using rx::merge;

// silence compiler
[](int, int) {} (first, last);

//auto makeStrand = rx::detail::make_immediate<>{};

auto strand = makeStrand(subscription{});

strand.now();

#if !RX_SKIP_TESTS
{
    int producems = 1000;
    int delayms = 1500;
    int count = 3;

    auto start = steady_clock::now();

    rx::test_loop<> loop;
    rx::recorded<int> on;

    const auto out = [=](auto& output, auto ctx, std::string w){
        auto start = on.origin();
        defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
            output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << w << endl;
        }));
    };

    const auto printout = [=](auto& output, std::string w){
        return make_lifter([=, &output](auto scbr){
            return make_subscriber([=, &output](auto ctx){
                auto r = scbr.create(ctx);
                return make_observer(r, r.lifetime, [=, &output](auto& r, auto v){
                    out(output, ctx, to_string(v) + " " + w);
                    r.next(v);
                });
            });
        });
    };

    auto gap = milliseconds(producems);
    auto delayby = milliseconds(delayms);

    auto tr = on.hot({
            on.next(on.origin() + gap * 1, 0),
            on.next(on.origin() + gap * 2, 1),
            on.next(on.origin() + gap * 3, 2),
            on.next(on.origin() + gap * 4, 3),
            on.next(on.origin() + gap * 5, 4)
        }) | 
        printout(cout, "produced") |
        on.record("produced") |
        delay(loop.make(), delayby) |
        take(count) |
        printout(cout, "emitted") |
        on.record("emitted") |
        on.test(loop, on.lifespan(on.origin() + 200ms, on.origin() + 6000ms));

    loop.run();

    auto finish = steady_clock::now();

    cout << "Elapsed: " << fixed << setprecision(1) << setw(4) << float_seconds{finish - start}.count() << endl; 

    {
        auto expected = on.expected({
            on.next(on.origin() + gap * 1 + delayby, 0),
            on.next(on.origin() + gap * 2 + delayby, 1),
            on.next(on.origin() + gap * 3 + delayby, 2),
            on.complete(on.origin() + gap * 3 + delayby)
        });
        if (tr.get().marbles["emitted"] == expected) {
            cout << "SUCCEEDED" << endl; 
        } else {
            cout << "FAILED" << endl; 
            cout << "Actual:" << endl; 
            cout << tr.get().marbles["emitted"] << endl;
            cout << "Expected:" << endl; 
            cout << expected << endl;
        }
    }

    {
        auto expected = on.lifespan(on.origin() + 200ms, on.origin() + 4500ms);
        if (tr.get().lifespan == expected) {
            cout << "SUCCEEDED" << endl; 
        } else {
            cout << "FAILED" << endl; 
            cout << "Actual:" << endl; 
            cout << tr.get().lifespan << endl;
            cout << "Expected:" << endl; 
            cout << expected << endl;
        }
    }

}
#endif

#if !RX_SKIP_TESTS
{
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer(strand, make_observer(defer_lifetime, [](auto& ){
        cout << this_thread::get_id() << " - deferred immediate strand" << endl;
    }));
}

{
    auto ctx = copy_context(subscription{}, makeStrand, start<shared_ptr<destruction>>(make_shared<destruction>()));
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer(ctx, make_observer(defer_lifetime, [](auto& ){
        cout << this_thread::get_id() << " - deferred immediate context" << endl;
    }));
}

auto sharedmakestrand = make_shared_make_strand(makeStrand);

{
    auto sharedstrand = sharedmakestrand(subscription{});
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer(sharedstrand, make_observer(defer_lifetime, [](auto& ){
        cout << this_thread::get_id() << " - deferred shared-immediate strand" << endl;
    }));
}

{
    auto ctx = copy_context(subscription{}, sharedmakestrand, start<shared_ptr<destruction>>(make_shared<destruction>()));
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer(ctx, make_observer(defer_lifetime, [](auto& ){
        cout << this_thread::get_id() << " - deferred shared-immediate context" << endl;
    }));
}

{
 cout << "compile-time polymorphism (canary)" << endl;
    auto lifetime = ints(1, 3) | 
        transform_merge(sharedmakestrand,
            [=](int){
                return ints(1, 10);
            }) |
            printto(cout) |
            start();
}
#endif

{
 cout << "compile-time polymorphism (multi-typed values)" << endl;
    text() |
        make_subscriber([](auto ctx) {return make_observer(ctx.lifetime, cstr_or_string{});}) |
        start();
}

#if !RX_SKIP_THREAD

auto makeThread = make_shared_make_strand(make_new_thread<>{});

auto thread = makeThread(subscription{});

#if !RX_SKIP_TESTS
{
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer(thread, make_observer(defer_lifetime, [](auto& ){
        cout << this_thread::get_id() << " - deferred thread strand" << endl;
    })).join();
}
this_thread::sleep_for(1s);
cout << endl;
#endif

#if !RX_SKIP_TESTS
{
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer_periodic(thread, thread.now(), 1s, make_observer(defer_lifetime, [=](long c){
        cout << this_thread::get_id() << " - deferred thread strand periodic - " << c << endl;
        if (c > 2) {
            defer_lifetime.stop();
        }
    })).join();
}
this_thread::sleep_for(2s);
cout << endl;
#endif

#if !RX_SKIP_TESTS
{
    auto c = make_context<steady_clock>(subscription{}, makeThread);
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer_periodic(c, c.now(), 1s, make_observer(defer_lifetime, [=](long c){
        cout << this_thread::get_id() << " - deferred thread context periodic - " << c << endl;
        if (c > 2) {
            defer_lifetime.stop();
        }
    })).join();
}
this_thread::sleep_for(2s);
cout << endl;
#endif

#if !RX_SKIP_TESTS
{
    auto f = make_context<steady_clock>(subscription{}, makeThread);
    auto c = copy_context(subscription{}, makeThread, f);
    auto defer_lifetime = subscription{};
    make_state<shared_ptr<destruction>>(defer_lifetime, make_shared<destruction>());
    defer_periodic(c, c.now(), 1s, make_observer(defer_lifetime, [=](long c){
        cout << this_thread::get_id() << " - deferred thread copied context periodic - " << c << endl;
        if (c > 2) {
            defer_lifetime.stop();
        }
    })).join();
}
this_thread::sleep_for(2s);
cout << endl;
#endif

#if !RX_SKIP_TESTS
{
 cout << "intervals" << endl;
    auto threeeven = copy_if(even) | 
        take(3) |
        delay(makeThread, 1s);

    intervals(makeThread, steady_clock::now() + 1s, 1s) | 
        threeeven |
        as_interface<long>() |
        finally([](){cout << "caller stopped" << endl;}) |
        printto(cout) |
        start<shared_ptr<destruction>>(subscription{}, make_shared<destruction>()) |
        join();
}
this_thread::sleep_for(2s);
cout << endl;
#endif

#if !RX_SKIP_TESTS
{
 output("merged multi-thread intervals");

    intervals(make_new_thread<>{}, steady_clock::now(), 20ms) | 
        take(thread::hardware_concurrency()) |
        transform_merge(make_new_thread<>{}, [](long c){
            output("thread started");
            return intervals(make_new_thread<>{}, steady_clock::now(), 1ms) |
                take(5001) |
                transform([=](long n){
                    auto r = (c * 10000) + n;
                    return r;
                }) |
                as_interface<long>() |
                last_or_default(42) |
                finally([](){output("thread stopped");});
        }) |
        as_interface<long>() |
        finally([](){output("caller stopped");}) |
        as_interface<long>() |
        printto(cout) |
        start<shared_ptr<destruction>>(subscription{}, make_shared<destruction>()) |
        join();
}
this_thread::sleep_for(2s);
cout << endl;
#endif

#endif 

#if !RX_INFO && !RX_SKIP_TESTS

{
 cout << "compile-time polymorphism" << endl;
    auto lastofeven = copy_if(even) | 
        take(100000000) |
        last_or_default(42);

 auto t0 = high_resolution_clock::now();
    auto lifetime = ints(0, 2) | 
        transform_merge(rx::detail::make_immediate<>{},
            [=](int){
                return ints(first, last * 100) |
                    lastofeven;
            }) |
            printto(cout) |
            start();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = ((last * 100) - first) * 3;
 cout << d / sc << " ms per value\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " values per second\n"; 
}

{
 cout << "interface polymorphism" << endl;
    auto lastofeven = copy_if(even) | 
        as_interface<int>() |
        take(100000000) |
        as_interface<int>() |
        last_or_default(42) |
        as_interface<int>();
        
 auto t0 = high_resolution_clock::now();
    auto lifetime = ints(0, 2) | 
        as_interface<int>() |
        transform_merge(rx::detail::make_immediate<>{},
            [=](int){
                return ints(first, last * 100) |
                    as_interface<int>() |
                    lastofeven |
                    as_interface<int>();
            }) |
            as_interface<int>() |
            printto(cout) |
            as_interface<>() |
            start();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = ((last * 100) - first) * 3;
 cout << d / sc << " ms per value\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " values per second\n"; 
}

#if !RX_SKIP_THREAD

{
 cout << "new thread" << endl;
    auto lastofeven = copy_if(even) | 
        take(100000000) |
        last_or_default(42);

 auto t0 = high_resolution_clock::now();
    ints(0, 2) | 
        observe_on(make_new_thread<>{}) |
        transform_merge(make_new_thread<>{}, [=](int){
            return ints(first, last * 100) |
                observe_on(make_new_thread<>{}) |
                lastofeven;
        }) |
        as_interface<long>() |
        printto(cout) |
        start() |
        join();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = ((last * 100) - first) * 3;
 cout << d / sc << " ms per value\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " values per second\n"; 
}

#endif

{
 cout << "for" << endl;
 auto t0 = high_resolution_clock::now();
    for(auto i = first; i < last; ++i) {
        auto lifetime = ints(0, 0) |
            transform([](int i) {
                return to_string(i);
            }) |
            transform([](const string& s) {
                int i = '0' - s[0];
                return i;
            }) |
        make_subscriber() |
        start();

    }

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}

{
 cout << "transform | merge" << endl;
 auto t0 = high_resolution_clock::now();

    auto lifetime = ints(first, last) | 
        transform([=](int){
            return ints(0, 0) |
                transform([](int i) {
                    return to_string(i);
                }) |
                transform([](const string& s) {
                    int i = '0' - s[0];
                    return i;
                });
        }) |
        merge(rx::detail::make_immediate<>{}) |
        make_subscriber() |
        start();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}

{
 cout << "transform_merge" << endl;
 auto t0 = high_resolution_clock::now();

    auto lifetime = ints(first, last) | 
        transform_merge(rx::detail::make_immediate<>{}, 
            [=](int){
                return ints(0, 0) |
                    transform([](int i) {
                        return to_string(i);
                    }) |
                    transform([](const string& s) {
                        int i = '0' - s[0];
                        return i;
                    });
            }) |
            make_subscriber() |
            start();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}

#if !RX_SKIP_THREAD

{
 cout << "transform_merge new_thread" << endl;
 auto t0 = high_resolution_clock::now();

    ints(first, last) | 
        transform_merge(make_new_thread<>{}, 
            [=](int){
                return ints(0, 0) |
                    transform([](int i) {
                        return to_string(i);
                    }) |
                    transform([](const string& s) {
                        int i = '0' - s[0];
                        return i;
                    });
            }) |
        as_interface<int>() |
        make_subscriber() |
        start() |
        join();

 auto t1 = high_resolution_clock::now();
 auto d = duration_cast<milliseconds>(t1-t0).count() * 1.0;
 auto sc = last - first;
 cout << d / sc << " ms per subscription\n"; 
 auto s = d / 1000.0;
 cout << sc / s << " subscriptions per second\n"; 
}
this_thread::sleep_for(1s);
cout << endl;

#endif

#endif
}
#endif