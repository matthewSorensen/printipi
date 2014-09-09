#ifndef COMMON_TUPLEUTIL_H
#define COMMON_TUPLEUTIL_H

/* 
 * Printipi/common/tupleutil.h
 * (c) 2014 Colin Wallace
 *
 * This file provides utilities for manipulating tuples.
 * Namely, it provides a way to apply a polymorphic (templated) function to each item in a tuple.
 */

#include <tuple>

template <typename TupleT, std::size_t IdxPlusOne, typename Func, typename ...Args> struct __callOnAll {
	void operator()(TupleT &t, Func &f, Args... args) {
		__callOnAll<TupleT, IdxPlusOne-1, Func, Args...>()(t, f, args...); //call on all previous indices
		f(IdxPlusOne-1, std::get<IdxPlusOne-1>(t), args...); //call on our index.
	}
};

template <typename TupleT, typename Func, typename ...Args> struct __callOnAll<TupleT, 0, Func, Args...> {
	//handle the base recursion case.
	void operator()(TupleT &, Func &, Args... ) {}
};

template <typename TupleT, typename Func, typename ...Args> void callOnAll(TupleT &t, Func f, Args... args) {
	__callOnAll<TupleT, std::tuple_size<TupleT>::value, Func, Args...>()(t, f, args...);
};
//This second version allows to pass a function object by pointer, so that it can perhaps be modified.
template <typename TupleT, typename Func, typename ...Args> void callOnAll(TupleT &t, Func *f, Args... args) {
	__callOnAll<TupleT, std::tuple_size<TupleT>::value, Func, Args...>()(t, *f, args...);
};


template <typename TupleT, std::size_t IdxPlusOne, typename Func, typename Reduce, typename ReducedDefault, typename ...Args> struct __callOnAllReduce {
    typename ReducedDefault::type operator()(TupleT &t, Func &f, Reduce &r, ReducedDefault &d, Args... args) {
        auto prev = __callOnAllReduce<TupleT, IdxPlusOne-1, Func, Reduce, ReducedDefault, Args...>()(t, f, r, d, args...); //result of all previous items;
        auto cur = f(IdxPlusOne-1, std::get<IdxPlusOne-1>(t), args...); //call on this index.
        return r(prev, cur);
    }
};

template <typename TupleT, typename Func, typename Reduce, typename ReducedDefault, typename ...Args> struct __callOnAllReduce<TupleT, 0, Func, Reduce, ReducedDefault, Args...> {
    //handle the base recursion case
    typename ReducedDefault::type operator()(TupleT &, Func &, Reduce &, ReducedDefault &d, Args... ) {
        return d();
    }
};

template <typename TupleT, typename Func, typename Reduce, typename ReducedDefault, typename ...Args> typename ReducedDefault::type tupleReduce(TupleT &t, Func f, Reduce r, ReducedDefault d, Args... args) {
    return __callOnAllReduce<TupleT, std::tuple_size<TupleT>::value, Func, Reduce, ReducedDefault, Args...>()(t, f, r, d, args...);
}

template <typename T> struct ValueWrapper {
    //Wraps a value in a struct such that calling the object returns the wrapped value, and the wrapped value is set during construction.
    //Eg:
    //ValueWrapper<bool> b(true);
    //b() //returns true
    typedef T type;
    T _data;
    ValueWrapper() : _data() {}
    ValueWrapper(const T &t) : _data(t) {}
    //ValueWrapper(T t) : _data(t) {}
    //T& operator()() { return _data; }
    const T& operator()() const { return _data; }
};

template <typename TupleT, typename Func, typename ...Args> bool tupleReduceLogicalOr(TupleT &t, Func f, Args... args) {
    //default value must be false, otherwise the only value ever returned would be <True>
    return tupleReduce(t, f, [](bool a, bool b) { return a||b; }, ValueWrapper<bool>(false), args...);
}

#endif
