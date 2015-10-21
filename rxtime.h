#pragma once

extern"C" void EMSCRIPTEN_KEEPALIVE rxdelay(int period, int wait, int count)
{
    using namespace rxcpp::operators;
    using namespace std::chrono;
    
    auto realstart = steady_clock::now();

    auto p = milliseconds(period);
    auto d = milliseconds(wait);

    auto start = jsthread.now();
    auto printtime = [](auto begin, auto now, int precision){
        cout << fixed << setprecision(precision) << setw(4) << duration_cast<milliseconds>(now - begin).count() / 1000.0 << "s - ";
    };
    auto printproduced = [=](int value){
        printtime(start, jsthread.now(), 1);
        cout << value << " produced" << endl;
    };
    auto printemitted = [=](int value){
        printtime(start, jsthread.now(), 1);
        cout << value << " emitted" << endl;
    };
    
    lifetime.add(
        interval(start + p, p, jsthread) |
            tap(printproduced) |
            delay(d, jsthread) |
            take(count) |
            finally(
                [=](){
                    printtime(realstart, steady_clock::now(), 3);
                    cout << "real time elapsed" << endl;
                }) |
            subscribe<long>(
                printemitted,
                [](exception_ptr ep){cout << what(ep) << endl;}));
}

extern"C" void EMSCRIPTEN_KEEPALIVE rxtestdelay(int period, int wait)
{
    using namespace rxcpp::operators;
    using namespace std::chrono;

    auto realstart = steady_clock::now();

    auto p = milliseconds(period);
    auto d = milliseconds(wait);

    auto sc = make_test();
    auto so = identity_one_worker(sc);
    auto w = sc.create_worker();

    auto report = [](const char* message, auto required, auto actual){
        if (required == actual) {
            cout << message << " - SUCCEEDED" << endl;
        } else {
            cout << message << " - FAILED" << endl;
            cout << "REQUIRED: " << required << endl;
            cout << "ACTUAL  : " << actual << endl;
        }
    };

    auto start = sc.now();
    
    auto printtime = [](auto begin, auto now, int precision){
        cout << fixed << setprecision(precision) << setw(4) << duration_cast<milliseconds>(now - begin).count() / 1000.0 << "s - ";
    };
    auto printproduced = [=](int value){
        printtime(start, sc.now(), 1);
        cout << value << " produced" << endl;
    };
    auto printemitted = [=](int value){
        printtime(start, sc.now(), 1);
        cout << value << " emitted" << endl;
    };

    const rxsc::test::messages<int> on;

    auto xs = sc.make_hot_observable({
        on.next(period * 1, 1),
        on.next(period * 2, 2),
        on.next(period * 3, 3),
        on.next(period * 4, 4)
    });

    auto body = [&]() {
        return xs |
            tap(printproduced) |
            delay(d, so) |
            take(3) |
            tap(printemitted) |
            finally(
                [=](){
                    printtime(realstart, steady_clock::now(), 3);
                    cout << "real time elapsed" << endl;
                });
    };

    auto res = w.start(body, 0, 1, (period * 5) - 1);

    {
        auto required = rxu::to_vector({
            on.next((period * 1) + wait, 1),
            on.next((period * 2) + wait, 2),
            on.next((period * 3) + wait, 3),
            on.completed((period * 3) + wait)
        });
        auto actual = res.get_observer().messages();
        report("emitted value test", required, actual);
    }

    {
        auto required = rxu::to_vector({
            on.subscribe(1, (period * 3) + wait + 1)
        });
        auto actual = xs.subscriptions();
        report("lifetime test", required, actual);
    }

}
