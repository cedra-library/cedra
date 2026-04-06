#pragma once

#include <cdr/types/errors.h>
#include <cdr/types/expect.h>
#include <cdr/base/check.h>

#include <coroutine>
#include <iterator>
#include <optional>
#include <utility>

namespace cdr::internal {

template<typename T, typename Err = cdr::Error>
class [[nodiscard]] Generator  {
public:

    class promise_type;
    using HandleType = std::coroutine_handle<promise_type>;

    class promise_type final {
    public:
        using ExpectType = Expect<std::reference_wrapper<T>, Err>;
    public:
        promise_type() noexcept
            : value_ptr_(nullptr)
            , root_(this)
            , parent_or_leaf_(this)
            , error_(std::nullopt)
        {}

        promise_type(const promise_type&) = delete;
        promise_type& operator=(const promise_type&) = delete;

        // C++ coroutine boilerplate
        Generator get_return_object() noexcept { return Generator(*this); }
        constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
        constexpr std::suspend_always final_suspend() const noexcept { return {}; }
        void unhandled_exception() noexcept { std::terminate(); }
        void return_void() noexcept {}


        std::suspend_always yield_value(T& value) noexcept {
            value_ptr_ = std::addressof(value);
            error_ = std::nullopt;
            return {};
        }

        std::suspend_always yield_value(Failure<Error>&& failure) noexcept {
            value_ptr_ = nullptr;
            error_ = failure.Value();
            return {};
        }

        auto yield_value(Generator& new_gen) noexcept {
            struct Awaiter {
                PromiseType* parent_;
                PromiseType* child_;

                bool await_ready() noexcept {
                    return child_ == nullptr || child_->Completed();
                }

                void await_suspend(std::coroutine_handle<>) noexcept {}

                void await_resume() noexcept {
                    if (child_) {
                        if (child_->error_.has_value()) {
                            parent_->error_ = std::move(child_->error_);
                        }
                        parent_->root_->parent_or_leaf_ = parent_;
                    }
                }
            };

            if (new_gen.promise_ == nullptr) {
                return Awaiter{this, nullptr};
            }

            new_gen.promise_->root_ = root_;
            new_gen.promise_->parent_or_leaf_ = this;
            root_->parent_or_leaf_ = new_gen.promise_;


            new_gen.promise_->Resume();

            if (new_gen.promise_->Completed() || new_gen.promise_->error_.has_value()) {
                root_->parent_or_leaf_ = this;
            }

            return Awaiter{this, new_gen.promise_};
        }

        void Resume() noexcept {
            HandleType::from_promise(*this).resume();
        }

        bool Completed() const noexcept {
            return HandleType::from_promise(const_cast<PromiseType&>(*this)).done();
        }

        Expect<std::reference_wrapper<T>, Err> Result() noexcept {
            if (error_.has_value()) {
                return cdr::Expect<std::reference_wrapper<T>, Err>(cdr::OpFailed, std::move(error_).value());
            }
            CDR_CHECK(value_ptr_ != nullptr) << "data must be provided";
            return cdr::Expect<std::reference_wrapper<T>, Err>(cdr::OpSuccess, std::ref(*value_ptr_));
        }

        template<typename U>
        void await_transform(U&&) = delete;

        void Destroy() noexcept {
            HandleType::from_promise(*this).destroy();
        }

        void Pull() noexcept {
            CDR_CHECK(this == root_);
            CDR_CHECK(!parent_or_leaf_->Completed()) << "Pull of completed generator";

            parent_or_leaf_->Resume();

            while (parent_or_leaf_ != this && parent_or_leaf_->Completed()) {

                if (parent_or_leaf_->error_.has_value()) {
                    this->error_ = std::move(parent_or_leaf_->error_);
                    parent_or_leaf_ = this;
                    return;
                }

                parent_or_leaf_ = parent_or_leaf_->parent_or_leaf_;
                parent_or_leaf_->Resume();
            }
        }

    public:
        T* value_ptr_;
        promise_type* root_;
        promise_type* parent_or_leaf_;
        std::optional<Err> error_;
    };

    using PromiseType = promise_type;

public:

    Generator() noexcept
        : promise_(nullptr)
    {}

    explicit Generator(PromiseType& other) noexcept
        : promise_(&other)
    {}

    Generator(Generator&& other) noexcept
        : promise_(std::exchange(other.promise_, nullptr))
    {}

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    ~Generator() {
        if (promise_) {
            promise_->Destroy();
        }
    }

    Generator& operator=(Generator&& other) noexcept {
        if (&other != this) [[likely]] {
            if (!promise_) {
                promise_->Destroy();
            }
            promise_ = std::exchange(other.promise_, nullptr);
        }
        return *this;
    }

public:

    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type        = typename promise_type::ExpectType;
        using difference_type    = std::ptrdiff_t;
        using pointer           = value_type*;
        using reference         = value_type;

    public:
        Iterator() noexcept
            : promise_(nullptr)
        {}

        explicit Iterator(PromiseType* promise) noexcept
            : promise_(promise)
        {
            if (promise_ && !promise_->Completed()) {
                promise_->Pull();
            }
        }

        reference operator*() const noexcept {
            CDR_CHECK(promise_ != nullptr);
            return promise_->parent_or_leaf_->Result();
        }

        Iterator& operator++() noexcept {
            CDR_CHECK(promise_ != nullptr);

            if (!promise_->Completed()) {
                if (promise_->error_.has_value()) {
                    promise_ = nullptr;
                    return *this;
                }

                promise_->Pull();
            }
            return *this;
        }

        bool operator==(const Iterator& other) const noexcept {
            bool this_done = !promise_ || promise_->Completed();
            bool other_done = !other.promise_ || other.promise_->Completed();

            if (this_done && other_done) {
                return true;
            }

            return promise_ == other.promise_;
        }

        bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        PromiseType* promise_{nullptr};
    };

    Iterator begin() noexcept {
        return Iterator{promise_};
    }

    Iterator end() noexcept {
        return Iterator{nullptr};
    }

    Iterator begin() const noexcept {
        return Iterator{promise_};
    }

    Iterator end() const noexcept {
        return Iterator{nullptr};
    }

public:

    void Swap(Generator& other) noexcept {
        std::swap(promise_, other.promise_);
    }

private:

    friend class promise_type;

    PromiseType* promise_{nullptr};
};

template<typename T>
void swap(Generator<T>& lhs, Generator<T>& rhs) noexcept {
    lhs.Swap(rhs);
}

} // namespace cdr::internal