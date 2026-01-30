#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <sstream>
#include <iostream>

#include <cdr/types/concepts.h>

namespace cdr {

template<NonVoid Err>
class [[nodiscard]] Failure {
public:

    explicit constexpr Failure(Err&& err)
        : value(std::move(err))
    {}

    explicit constexpr Failure(const Err& err)
        : value(err)
    {}

    template<typename... Args>
    explicit constexpr Failure(Args&&... args)
        : value(std::forward<Args>(args)...)
    {}

    [[nodiscard]] constexpr Err& Value() & {
        return value;
    }

    [[nodiscard]] constexpr const Err& Value() const& {
        return value;
    }

    [[nodiscard]] constexpr Err&& Value() && {
        return std::move(value);
    }

    [[nodiscard]] constexpr const Err&& Value() const&& {
        return std::move(value);
    }

private:
    Err value;
};

template<typename T>
class [[nodiscard]] Success {
public:

    explicit constexpr Success(const T& value)
        : value(value)
    {}

    explicit constexpr Success(T&& v)
        : value(std::move(v))
    {}

    template<typename... Args>
    explicit constexpr Success(Args&&... args)
        : value(std::forward<Args>(args)...)
    {}

    [[nodiscard]] constexpr T& Value() & {
        return value;
    }

    [[nodiscard]] constexpr const T& Value() const& {
        return value;
    }

    [[nodiscard]] constexpr T&& Value() && {
        return std::move(value);
    }

    [[nodiscard]] constexpr const T&& Value() const&& {
        return std::move(value);
    }

private:
    T value;
};

template<>
class [[nodiscard]] Success<void> {
public:

    explicit constexpr Success()
    {}
};

[[maybe_unused]] constexpr struct FromFailureType {} OpFailed;
[[maybe_unused]] constexpr struct FromSuccessType {} OpSuccess;

template<typename T, typename Err>
class [[maybe_unused]] CrashMessageBuilder {
public:

    CrashMessageBuilder(const CrashMessageBuilder&) = delete;
    CrashMessageBuilder& operator=(const CrashMessageBuilder&) = delete;

    explicit CrashMessageBuilder(FromFailureType _, Err&& error)
       : condition_satisfied(false)
    {
        if (!condition_satisfied) {
            new(&termination) TerminationInfo {
                .stream = std::stringstream{},
                .error = std::move(error)
            };
        }
    }

    explicit CrashMessageBuilder(FromSuccessType _, T&& result)
        : condition_satisfied(true)
        , data(std::move(result))
    {}

    template<typename U>
    CrashMessageBuilder& operator<<(U&& any_msg) {
        if (!condition_satisfied) {
            termination.stream << std::forward<U>(any_msg);
        }
        return *this;
    }

    CrashMessageBuilder& operator<<(std::ostream&(*manip)(std::ostream&)) {
        if (!condition_satisfied) {
            termination.stream << manip;
        }
        return *this;
    }

    operator T() {
        if (!condition_satisfied) {
            Terminate();
        }
        return data;
    }

    ~CrashMessageBuilder() {
        if (!condition_satisfied) {
            Terminate();
        }
    }

private:

    [[noreturn]] void Terminate() {
        std::cerr << termination.stream.str();
        std::terminate();
    }

private:
    struct TerminationInfo {
        std::stringstream stream;
        Err error;
    };

    union {
        TerminationInfo termination;
        T data;
    };
    bool condition_satisfied;
};

template<typename Err>
class [[maybe_unused]] CrashMessageBuilder<void, Err> {
public:

    CrashMessageBuilder(const CrashMessageBuilder&) = delete;
    CrashMessageBuilder& operator=(const CrashMessageBuilder&) = delete;

    explicit CrashMessageBuilder(FromFailureType, Err&& error)
       : condition_satisfied(false)
    {
        if (!condition_satisfied) {
            new(&termination) TerminationInfo {
                .stream = std::stringstream{},
                .error = std::move(error)
            };
        }
    }

    explicit CrashMessageBuilder(FromSuccessType)
        : condition_satisfied(true)
    {}

    template<typename U>
    CrashMessageBuilder& operator<<(U&& any_msg) {
        if (!condition_satisfied) {
            termination.stream << std::forward<U>(any_msg);
        }
        return *this;
    }

    CrashMessageBuilder& operator<<(std::ostream&(*manip)(std::ostream&)) {
        if (!condition_satisfied) {
            termination.stream << manip;
        }
        return *this;
    }

    ~CrashMessageBuilder() {
        if (!condition_satisfied) {
            Terminate();
        }
    }

private:

    [[noreturn]] void Terminate() {
        std::cerr << termination.stream.str();
        std::terminate();
    }

private:
    struct TerminationInfo {
        std::stringstream stream;
        Err error;
    };

    union {
        TerminationInfo termination;
        struct {} none;
    };
    bool condition_satisfied;
};

template<typename T, typename Err>
class [[nodiscard]] Expect final {
public:

    constexpr Expect()
        : value()
        , is_error(false)
    {}

    constexpr Expect(Failure<Err>&& err)
        : is_error(true)
    {
        std::construct_at(&error, std::move(err).Value());
    }

    constexpr Expect(Success<T>&& v)
        : is_error(false)
    {
        std::construct_at(&value, std::move(v).Value());
    }


    constexpr Expect(Expect&& other) noexcept
        : is_error(other.is_error)
    {
        if (is_error) {
            std::construct_at(&error, std::move(other).GetFailure());
        } else {
            std::construct_at(&value, std::move(other).Value());
        }
    }

    constexpr Expect& operator=(Expect&& other) noexcept {
        if (this != &other) {
            this->~Expect();
            is_error = other.is_error;
            if (is_error) {
                std::construct_at(&error, std::move(other).GetFailure());
            } else {
                std::construct_at(&value, std::move(other).Value());
            }
        }
        return *this;
    }

    template<typename... Args>
    constexpr Expect(FromFailureType _, Args&&... args)
        : is_error(true)
    {
        std::construct_at(&error, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr Expect(FromSuccessType _, Args&&... args)
        : is_error(false)
    {
        std::construct_at(&value, std::forward<Args>(args)...);
    }

    [[maybe_unused]] CrashMessageBuilder<T, Err> OrCrashProgram() && {
        if (is_error) {
            return CrashMessageBuilder<T, Err>(OpFailed, std::move(error));
        } else {
            return CrashMessageBuilder<T, Err>(OpSuccess, std::move(value));
        }
    }

    constexpr bool Succeed() const {
        return !is_error;
    }

    constexpr bool Failed() const {
        return is_error;
    }

    constexpr T& Value() & {
        return value;
    }

    constexpr const T& Value() const& {
        return value;
    }

    constexpr T&& Value() && {
        return std::move(value);
    }

    constexpr const T&& Value() const&& {
        return std::move(value);
    }

    constexpr Err& GetFailure() & {
        return error;
    }

    constexpr const Err& GetFailure() const& {
        return error;
    }

    constexpr Err&& GetFailure() && {
        return std::move(error);
    }

    constexpr const Err&& GetFailure() const&& {
        return std::move(error);
    }

    constexpr operator bool() const {
        return Succeed();
    }

    ~Expect() {
        if (is_error) {
            error.~Err();
        } else {
            value.~T();
        }
    }

    constexpr bool operator==(const Failure<Err>& fail) const {
        return is_error && fail.Value() == error;
    }

    constexpr bool operator==(const Success<T>& ok) const {
        return !is_error && ok.Value() == value;
    }

private:

    union {
        T value;
        Err error;
    };

    bool is_error;
};

template<typename Err>
class [[nodiscard]] Expect<void, Err> final {
public:

    constexpr Expect()
        : is_error(false)
    {}

    constexpr Expect(Failure<Err>&& err)
        : is_error(true)
    {
        std::construct_at(&error, std::move(err).Value());
    }

    constexpr Expect(Success<void>&& v)
        : is_error(false)
    {}

    template<typename... Args>
    constexpr Expect(FromFailureType _, Args&&... args)
        : is_error(true)
    {
        std::construct_at(&error, std::forward<Args>(args)...);
    }

    constexpr Expect(Expect&& other) noexcept
        : is_error(other.is_error)
    {
        if (is_error) {
            std::construct_at(&error, std::move(other.error));
        }
    }

    constexpr Expect& operator=(Expect&& other) noexcept {
        if (this != &other) {
            if (is_error) {
                error.~Err();
            }
            is_error = other.is_error;
            if (is_error) {
                std::construct_at(&error, std::move(other.error));
            }
        }
        return *this;
    }

    [[maybe_unused]] CrashMessageBuilder<void, Err> OrCrashProgram() && {
        if (is_error) {
            return CrashMessageBuilder<void, Err>(OpFailed, std::move(error));
        }
        return CrashMessageBuilder<void, Err>(OpSuccess);
    }

    constexpr bool Succeed() const {
        return !is_error;
    }

    constexpr bool Failed() const {
        return is_error;
    }

    constexpr Err& GetFailure() & {
        return error;
    }

    constexpr const Err& GetFailure() const& {
        return error;
    }

    constexpr Err&& GetFailure() && {
        return std::move(error);
    }

    constexpr const Err&& GetFailure() const&& {
        return std::move(error);
    }

    constexpr operator bool() const {
        return Succeed();
    }

    constexpr bool operator==(const Failure<Err>& fail) const {
        return is_error && fail.Value() == error;
    }

    template<NonVoid T>
    consteval bool operator==(const Success<T>& other) const noexcept {
        return false;
    }

private:
    union {
        Err error;
    };
    bool is_error;
};

template<NonVoid T>
inline Success<std::remove_reference_t<T>> Ok(T&& val) {
    return Success<std::remove_reference_t<T>>(std::forward<T>(val));
}

inline Success<void> Ok() {
    return Success<void>();
}

template<NonVoid T, typename... Args>
inline Success<std::remove_reference_t<T>> Ok(std::in_place_t, Args&&... args) {
    return Success<std::remove_reference_t<T>>(std::forward<Args>(args)...);
}

} // namespace cdr
