#pragma once

namespace rx {

template<class Select = defaults>
struct adaptor;

namespace detail {
    template<class VL, class CL, class EL, class VR, class CR, class ER>
    using adapt_t = function<observable_interface<VR, CR, ER>(const observable_interface<VL, CL, EL>&)>;
}
template<class VL, class CL, class EL, class VR, class CR, class ER>
struct adaptor<interface<VL, CL, EL, VR, CR, ER>> {
    detail::adapt_t<VL, CL, EL, VR, CR, ER> a;
    adaptor(const adaptor&) = default;
    template<class Adapt>
    adaptor(const adaptor<Adapt>& a)
        : a(a.a) {
    }
    observable_interface<VR, CR, ER> adapt(const observable_interface<VL, CL, EL>& ovr) const {
        return a(ovr);
    }
    template<class... TN>
    adaptor as_interface() const {
        return {*this};
    }
};
template<class VL, class CL, class EL, class VR, class CR, class ER>
using adaptor_interface = adaptor<interface<VL, CL, EL, VR, CR, ER>>;

template<class Adapt>
struct adaptor {
    Adapt a;
    /// \brief returns observable
    template<class... ON>
    auto adapt(observable<ON...> o) const {
        static_assert(detail::is_specialization_of<decltype(a(o)), observable>::value, "adaptor function must return observable!");
        return a(o);
    }
    template<class VL, class CL = steady_clock, class EL = exception_ptr, class VR = VL, class CR = CL, class ER = EL>
    adaptor_interface<VL, CL, EL, VR, CR, ER> as_interface() const {
        return {*this};
    }
};

template<class Adapt>
adaptor<Adapt> make_adaptor(Adapt&& l) {
    return {forward<Adapt>(l)};
}

namespace detail {

template<class T>
using for_adaptor = for_specialization_of_t<T, adaptor>;

template<class T>
using not_adaptor = not_specialization_of_t<T, adaptor>;

}

}
