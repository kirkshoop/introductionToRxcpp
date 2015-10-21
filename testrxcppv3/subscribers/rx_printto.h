#pragma once

namespace rx {

const auto printto = [](auto& output){
    info("new printto");
    return make_subscriber([&](auto ctx) {
        ::info("printto bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
        auto values = make_state<int>(ctx.lifetime, 0);
        ::info("printto observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
        auto start = ctx.now();
        return make_observer(
            ctx.lifetime,
            [=, &output](auto v) {
                ++values.get();
                defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
                    output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << v << endl;
                }));
            },
            [=, &output](exception_ptr ep){
                defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
                    output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << what(ep) << endl;
                }));
            },
            [=, &output](){
                defer(ctx, make_observer(subscription{}, [=, &output](auto& ){
                    output << this_thread::get_id() << " - " << fixed << setprecision(1) << setw(4) << duration_cast<milliseconds>(ctx.now() - start).count()/1000.0 << "s - " << values.get() << " values received - done!" << endl;
                }));
            });
    });
};

}