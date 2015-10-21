#pragma once

namespace rx {

const auto merge = [](auto makeStrand){
    info("new merge");
    return make_adaptor([=](auto source){
        info("merge bound to source");
        auto sharedmakestrand = make_shared_make_strand(makeStrand);
        info("merge-input start");
        return source |
            observe_on(sharedmakestrand) |
            make_lifter([=](auto scrb) {
                info("merge bound to subscriber");
                return make_subscriber([=](auto ctx){
                    info("merge bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                    
                    auto sourcecontext = make_context(subscription{}, sharedmakestrand);

                    auto pending = make_state<set<subscription>>(ctx.lifetime);

                    auto& pends = pending.get();
                    ctx.lifetime.insert([&pends](){
                        info("merge-output stopping all inputs");
                        // stop all the inputs
                        for (auto& l : pends) {
                            l.stop();
                        }
                        pends.clear();
                        info("merge-output stop");
                    });

                    auto destctx = copy_context(ctx.lifetime, sharedmakestrand, ctx);
                    auto r = scrb.create(destctx);

                    auto it = pending.get().insert(sourcecontext.lifetime).first;
                    sourcecontext.lifetime.insert([=, &pends](){
                        pends.erase(it);
                        if (pends.empty()){
                            info("merge-input complete destination");
                            r.complete();
                        }
                        info("merge-input stop");
                    });

                    info("merge-input observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(sourcecontext.lifetime.store.get())));
                    return make_observer(r, sourcecontext.lifetime, 
                        [=, &pends](auto& r, auto& v){
                            info("merge-nested start");
                            auto nestedcontext = make_context(subscription{}, sharedmakestrand);
                            auto it = pends.insert(nestedcontext.lifetime).first;
                            nestedcontext.lifetime.insert([=, &pends](){
                                pends.erase(it);
                                if (pends.empty()){
                                    info("merge-nested complete destination");
                                    r.complete();
                                }
                                info("merge-nested stop");
                            });
                            v |
                                observe_on(sharedmakestrand) |
                                make_subscriber([=](auto ctx){
                                    info("merge-nested bound to context lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                                    info("merge-nested observer lifetime - " + to_string(reinterpret_cast<ptrdiff_t>(ctx.lifetime.store.get())));
                                    return make_observer(r, ctx.lifetime, 
                                        [](auto& r, auto& v){
                                            r.next(v);
                                        }, detail::pass{}, detail::skip{});
                                }) | 
                                start(nestedcontext);
                        }, detail::pass{}, detail::skip{});
                });
            });
    });
};

}