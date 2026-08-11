#pragma once

// vec<T, N> — fixed-width, allocation-free lane container.
//
// SPDX-License-Identifier: MIT

#include "defines.h"

#include <cstddef>
#include <type_traits>

namespace soft_fp64 {

template <typename T, std::size_t N> struct vec {
    static_assert(N >= 1, "vec<T,N> requires at least one lane");

    T data[N];

    using value_type = T;

    constexpr T& operator[](std::size_t i) noexcept { return data[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data[i]; }

    constexpr T* begin() noexcept { return data; }
    constexpr const T* begin() const noexcept { return data; }
    constexpr T* end() noexcept { return data + N; }
    constexpr const T* end() const noexcept { return data + N; }

    static constexpr std::size_t size() noexcept { return N; }
};

template <typename T, std::size_t N>
constexpr vec<T, N> load(const T* source) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    vec<T, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = source[lane];
    return result;
}

template <typename T, std::size_t N>
constexpr void store(T* destination,
                     const vec<T, N>& value) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    for (std::size_t lane = 0; lane < N; ++lane)
        destination[lane] = value[lane];
}

template <typename Function, typename T, std::size_t N>
constexpr auto transform(const vec<T, N>& value,
                         Function&& function) noexcept(noexcept(function(value[0])))
    -> vec<std::decay_t<decltype(function(value[0]))>, N> {
    using Result = std::decay_t<decltype(function(value[0]))>;
    vec<Result, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = function(value[lane]);
    return result;
}

template <typename Function, typename T, typename U, std::size_t N>
constexpr auto transform(const vec<T, N>& lhs, const vec<U, N>& rhs,
                         Function&& function) noexcept(noexcept(function(lhs[0], rhs[0])))
    -> vec<std::decay_t<decltype(function(lhs[0], rhs[0]))>, N> {
    using Result = std::decay_t<decltype(function(lhs[0], rhs[0]))>;
    vec<Result, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = function(lhs[lane], rhs[lane]);
    return result;
}

template <typename Function, typename T, typename U, typename V, std::size_t N>
constexpr auto
transform(const vec<T, N>& first, const vec<U, N>& second, const vec<V, N>& third,
          Function&& function) noexcept(noexcept(function(first[0], second[0], third[0])))
    -> vec<std::decay_t<decltype(function(first[0], second[0], third[0]))>, N> {
    using Result = std::decay_t<decltype(function(first[0], second[0], third[0]))>;
    vec<Result, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = function(first[lane], second[lane], third[lane]);
    return result;
}

template <typename Function, typename T, std::size_t N>
constexpr auto
transform_indexed(const vec<T, N>& value,
                  Function&& function) noexcept(noexcept(function(std::size_t{}, value[0])))
    -> vec<std::decay_t<decltype(function(std::size_t{}, value[0]))>, N> {
    using Result = std::decay_t<decltype(function(std::size_t{}, value[0]))>;
    vec<Result, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = function(lane, value[lane]);
    return result;
}

template <typename Function, typename T, typename U, std::size_t N>
constexpr auto
transform_indexed(const vec<T, N>& lhs, const vec<U, N>& rhs,
                  Function&& function) noexcept(noexcept(function(std::size_t{}, lhs[0], rhs[0])))
    -> vec<std::decay_t<decltype(function(std::size_t{}, lhs[0], rhs[0]))>, N> {
    using Result = std::decay_t<decltype(function(std::size_t{}, lhs[0], rhs[0]))>;
    vec<Result, N> result{};
    for (std::size_t lane = 0; lane < N; ++lane)
        result[lane] = function(lane, lhs[lane], rhs[lane]);
    return result;
}

template <typename T> using vec1 = vec<T, 1>;
template <typename T> using vec2 = vec<T, 2>;
template <typename T> using vec3 = vec<T, 3>;
template <typename T> using vec4 = vec<T, 4>;
template <typename T> using vec8 = vec<T, 8>;
template <typename T> using vec16 = vec<T, 16>;

} // namespace soft_fp64
