#pragma once

namespace rx {

// based on Walter Brown's void_t proposal
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3911.pdf
namespace detail {
    template<class... TN> struct void_type {typedef void type;};
}
template<class... TN>
using void_t = typename detail::void_type<TN...>::type;

namespace detail {

/// test type against a model (no match)
template<class T, template<class...> class M>
struct is_specialization_of : public false_type {};

/// test type against a model (matches)
template<template<class...> class M, class... TN>
struct is_specialization_of<M<TN...>, M> : public true_type {};

/// enabled if T is matched 
template<class T, template<class...> class M>
using for_specialization_of_t = enable_if_t<is_specialization_of<decay_t<T>, M>::value>;

/// enabled if T is not a match 
template<class T, template<class...> class M>
using not_specialization_of_t = enable_if_t<!is_specialization_of<decay_t<T>, M>::value>;

/// enabled if T is same 
template<class T, class M>
using for_same_t = enable_if_t<is_same<decay_t<T>, decay_t<M>>::value>;

/// enabled if T is not same 
template<class T, class M>
using not_same_t = enable_if_t<!is_same<decay_t<T>, decay_t<M>>::value>;

}


/// selects default implementation
struct defaults {};
/// selects interface implementation
template<class... TN>
struct interface {};

template<class T>
using time_point_t = typename decay_t<T>::time_point;
template<class T>
using duration_t = typename decay_t<T>::duration;

template<class T>
using clock_t = typename decay_t<T>::clock_type;
template<class T>
using clock_time_point_t = time_point_t<clock_t<T>>;
template<class T>
using clock_duration_t = duration_t<clock_t<T>>;


}
