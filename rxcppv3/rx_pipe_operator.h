#pragma once

namespace rx {

/// \brief chain operator overload for
/// Subscriber = Lifter | Subscriber
/// \param lifter
/// \param subscriber
/// \returns subscriber
template<class... LN, class... SN>
auto operator|(lifter<LN...> l, subscriber<SN...> scbr) {
    return l.lift(scbr);
}

/// \brief chain operator overload for
/// Lifter = Lifter | Lifter
/// \param lifter
/// \param lifter
/// \returns Lifter
template<class... LLN, class... LRN>
auto operator|(lifter<LLN...> lhs, lifter<LRN...> rhs) {
    return make_lifter([lhs = move(lhs), rhs = move(rhs)](auto scbr){
        lhs.lift(rhs.lift(scbr));
    });
}

namespace detail {

template<typename O, typename L>
struct o_l
{
    mutable O o;
    mutable L l;
    template<typename SCRB>
    auto operator()(SCRB scrb) const -> decltype(o.bind(l.lift(scrb))) {
        return o.bind(l.lift(scrb));
    }
};

}

/// \brief chain operator overload for
/// Observable = Observable | Lifter
/// \param observable
/// \param lifter
/// \returns observable
template<class... ON, class... LN>
auto operator|(observable<ON...> s, lifter<LN...> l) {
    return make_observable(detail::o_l<observable<ON...>, lifter<LN...>>{s, l});
}


/// \brief chain operator overload for
/// Starter = Observable | Subscriber
/// \param observable
/// \param subscriber
/// \returns starter
template<class... ON, class... SN>
auto operator|(observable<ON...> s, subscriber<SN...> scbr) {
    return s.bind(scbr);
}

/// \brief chain operator overload for
/// subscription = Starter | Context
/// \param starter
/// \param context
/// \returns subscription
template<class... SN, class... CN>
subscription operator|(starter<SN...> s, context<CN...> ctx) {
    return s.start(ctx);
}

/// \brief chain operator overload for
/// Adaptor = Adaptor | Adaptor
/// \param adaptor
/// \param adaptor
/// \returns adaptor
template<class... ALN, class... ARN>
auto operator|(adaptor<ALN...> lhs, adaptor<ARN...> rhs) {
    return make_adaptor([=](auto source){
        return rhs.adapt(lhs.adapt(source));
    });
}

/// \brief chain operator overload for
/// Adaptor = Adaptor | Lifter
/// \param adaptor
/// \param lifter
/// \returns adaptor
template<class... AN, class... LN>
auto operator|(adaptor<AN...> a, lifter<LN...> l) {
    return make_adaptor([=](auto source){
        auto s = a.adapt(source);
        return make_observable([=](auto scrb){
            return s.bind(l.lift(scrb));
        });
    });
}

namespace detail {

template<typename L, typename S>
struct l_s
{
    mutable L l;
    mutable S source;
    template<typename SCRB>
    auto operator()(SCRB scrb) const -> decltype(source.bind(l.lift(scrb))) {
        return source.bind(l.lift(scrb));
    }
};

template<typename L, typename A>
struct l_a
{
    mutable L l;
    mutable A a;
    template<typename S>
    auto operator()(S source) const -> decltype(a.adapt(make_observable(l_s<L, S>{l, source}))) {
        return a.adapt(make_observable(l_s<L, S>{l, source}));
    }
};

}

/// \brief chain operator overload for
/// Adaptor = Lifter | Adaptor
/// \param lifter
/// \param adaptor
/// \returns adaptor
template<class... LN, class... AN>
auto operator|(lifter<LN...> l, adaptor<AN...> a) {
    return make_adaptor(detail::l_a<lifter<LN...>, adaptor<AN...>>{l, a});
}

/// \brief chain operator overload for
/// Observable = Observable | Adaptor
/// \param observable
/// \param adaptor
/// \returns observable
template<class... ON, class... AN>
auto operator|(observable<ON...> s, adaptor<AN...> a) {
    return a.adapt(s);
}

/// \brief chain operator overload for
/// Terminator = Adaptor | Subscriber
/// \param adaptor
/// \param subscriber
/// \returns terminator
template<class... AN, class... SN>
auto operator|(adaptor<AN...> a, subscriber<SN...> scrb) {
    return make_terminator([=](auto source){
        return a.adapt(source).bind(scrb);
    });
}

/// \brief chain operator overload for
/// starter = Observable | Terminator
/// \param observable
/// \param terminator
/// \returns starter
template<class... ON, class... TN>
auto operator|(observable<ON...> s, terminator<TN...> t) {
    return t.terminate(s);
}
 
}