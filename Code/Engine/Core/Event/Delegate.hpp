#pragma once

// ============================================================================
// Delegate.hpp - Single-cast delegate template
// Part of Event System
// Supports: static functions, lambdas, member functions
// ============================================================================

#include <functional>
#include <type_traits>
#include <utility>

namespace enigma::event
{
    /// Single-cast delegate - can bind only one callback
    /// @tparam Signature Function signature (e.g., void(int, float))
    template <typename Signature>
    class Delegate;

    template <typename R, typename... Args>
    class Delegate<R(Args...)>
    {
    public:
        using FunctionType = R(Args...);
        using CallbackType = std::function<R(Args...)>;

        Delegate()  = default;
        ~Delegate() = default;

        // Move semantics
        Delegate(Delegate&&) noexcept            = default;
        Delegate& operator=(Delegate&&) noexcept = default;

        // Copy semantics
        Delegate(const Delegate&)            = default;
        Delegate& operator=(const Delegate&) = default;

        // ========================================================================
        // Binding Methods
        // ========================================================================

        /// Bind a callable (lambda, function pointer, functor)
        template <typename F>
        void Bind(F&& func)
        {
            m_callback = std::forward<F>(func);
        }

        /// Bind a member function
        /// @param instance Pointer to object instance
        /// @param method Pointer to member function
        template <typename T, typename Method>
        void Bind(T* instance, Method method)
        {
            static_assert(std::is_member_function_pointer_v<Method>,
                          "Method must be a member function pointer");

            m_callback = [instance, method](Args... args) -> R
            {
                return (instance->*method)(std::forward<Args>(args)...);
            };
        }

        /// Unbind the current callback
        void Unbind()
        {
            m_callback = nullptr;
        }

        // ========================================================================
        // Query Methods
        // ========================================================================

        /// Check if delegate is bound
        bool IsBound() const
        {
            return static_cast<bool>(m_callback);
        }

        /// Implicit bool conversion
        explicit operator bool() const
        {
            return IsBound();
        }

        // ========================================================================
        // Execution Methods
        // ========================================================================

        /// Execute the delegate (returns default value if not bound)
        R Execute(Args... args) const
        {
            if (!m_callback)
            {
                // For void return type, just return
                if constexpr (std::is_void_v<R>)
                {
                    return;
                }
                else
                {
                    // For non-void, return default value
                    return R{};
                }
            }
            return m_callback(std::forward<Args>(args)...);
        }

        /// Function call operator (same as Execute)
        R operator()(Args... args) const
        {
            return Execute(std::forward<Args>(args)...);
        }

        /// Execute if bound, otherwise do nothing (safe call)
        /// @return true if executed, false if not bound
        bool ExecuteIfBound(Args... args) const
        {
            if (m_callback)
            {
                m_callback(std::forward<Args>(args)...);
                return true;
            }
            return false;
        }

        /// Execute if bound and return result via out parameter
        /// @param outResult Output parameter for result (only for non-void R)
        /// @return true if executed, false if not bound
        template <typename U = R>
        std::enable_if_t<!std::is_void_v<U>, bool>
        ExecuteIfBound(U& outResult, Args... args) const
        {
            if (m_callback)
            {
                outResult = m_callback(std::forward<Args>(args)...);
                return true;
            }
            return false;
        }

    private:
        CallbackType m_callback;
    };
} // namespace enigma::event
