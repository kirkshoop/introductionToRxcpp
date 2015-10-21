#pragma once

namespace rx {


template<class Select = defaults>
struct lifter;

namespace detail {
    template<class VL, class CL, class EL, class VR, class CR, class ER>
    using lift_t = function<subscriber_interface<VL, CL, EL>(subscriber_interface<VR, CR, ER>)>;
}
template<class VL, class CL, class EL, class VR, class CR, class ER>
struct lifter<interface<VL, CL, EL, VR, CR, ER>> {
    detail::lift_t<VL, CL, EL, VR, CR, ER> l;
    lifter(const lifter&) = default;
    template<class Lift>
    lifter(const lifter<Lift>& l) 
        : l(l.l){
    }
    subscriber_interface<VL, CL, EL> lift(subscriber_interface<VR, CR, ER> s) const {
        return l(s);
    }
    template<class... TN>
    lifter as_interface() const {
        return {*this};
    }
};
template<class VL, class CL, class EL, class VR, class CR, class ER>
using lifter_interface = lifter<interface<VL, CL, EL, VR, CR, ER>>;

template<class Lift>
struct lifter {
    using lift_type = decay_t<Lift>;
    lift_type l;
    /// \brief returns subscriber    
    template<class... SN>
    auto lift(subscriber<SN...> s) const {
        static_assert(detail::is_specialization_of<decltype(l(s)), subscriber>::value, "lift function must return subscriber!");
        return l(s);
    }
    template<class VL, class CL = steady_clock, class EL = exception_ptr, class VR = VL, class CR = CL, class ER = EL>
    lifter_interface<VL, CL, EL, VR, CR, ER> as_interface() const {
        return {*this};
    }
};

template<class Lift>
lifter<Lift> make_lifter(Lift&& l) {
    return {forward<Lift>(l)};
}

namespace detail {

template<class T>
using for_lifter = for_specialization_of_t<T, lifter>;

template<class T>
using not_lifter = not_specialization_of_t<T, lifter>;

}

}