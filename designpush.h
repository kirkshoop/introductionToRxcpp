#pragma once

namespace designpushdef
{
/*
template<class V>
struct receiver
{
    void operator()(V v);
};

template<class SenderV>
struct algorithm
{
    SenderU operator()(SenderV s);
};

template<class Receiver>
struct sender
{
    void operator()(Receiver r);
};
*/

template<class Next>
struct receiver
{
    Next i;
    template<class V>
    void operator()(V&& v) const {
        return i(std::forward<V>(v));
    }
};
template<class Next>
auto make_receiver(Next i) {
    return receiver<std::decay_t<Next>>{i};
}
template<class T>
struct receiver_check : public false_type {};

template<class Next>
struct receiver_check<receiver<Next>> : public true_type {};

template<class T>
using for_receiver = typename enable_if<receiver_check<T>::value>::type;

template<class T>
using not_receiver = typename enable_if<!receiver_check<T>::value>::type;

const auto ints = [](auto first, auto last){
    return [=](auto r){
        for(auto i=first;i <= last; ++i){
            r(i);
        }
    };
};
const auto copy_if = [](auto pred){
    return [=](auto dest){
        return [=](auto v){
            if (pred(v)) dest(v);
        };
    };
};
const auto printto = [](auto& output){
    return make_receiver([&](auto v) {
        output << v << endl;
    });
};

template<class SenderV, class SenderU, class CheckS = not_receiver<SenderU>>
auto operator|(SenderV sv, SenderU su){
    return [=](auto dest){
        return sv(su(dest));
    };
}

template<class SenderV, class ReceiverV, class CheckS = not_receiver<SenderV>, class CheckR = for_receiver<ReceiverV>>
auto operator|(SenderV sv, ReceiverV rv) {
    return sv(rv);
}


}

extern"C" void EMSCRIPTEN_KEEPALIVE designpush(int first, int last){
    using namespace designpushdef;
    ints(first, last)(designpushdef::copy_if(even)(printto(cout)));
}

extern"C" void EMSCRIPTEN_KEEPALIVE designoperator(int first, int last){
    using namespace designpushdef;
    ints(first, last) | designpushdef::copy_if(even) | printto(cout);
}