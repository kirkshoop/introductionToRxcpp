#pragma once

namespace rx {

const auto take = [](int n){
    info("new take");
    return make_adaptor([=](auto source){
        info("take bound to source");
        return make_observable([=](auto scrb){
            info("take bound to subscriber");
            return source.bind(
                make_subscriber([=](auto ctx){
                    info("take bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                    auto r = scrb.create(ctx);
                    auto remaining = make_state<int>(r.lifetime, n);
                    info("take observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(r.lifetime.store.get())));
                    auto lifted = make_observer(r, r.lifetime,
                        [remaining](auto& r, auto v){
                            r.next(v);
                            if (--remaining.get() == 0) {
                                r.complete();
                            }
                        });
                    if (n == 0) {
                        lifted.complete();
                    }
                    return lifted;
                }));
        });
    });
};

}