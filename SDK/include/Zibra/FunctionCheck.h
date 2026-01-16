#include <type_traits>

namespace Zibra
{
    // Idea for this is described here - https://youtu.be/Nm9-xKsZoNI?si=0q-QpZItVSIhKkTr&t=1296

    // Acceptable arguments are:
    // * References
    // * Small (<= 16 bytes) and trivially copyable types (including raw pointers)
    template <typename T>
    constexpr bool is_acceptable_func_argument_v = std::is_reference_v<T> || (std::is_trivially_copyable_v<T> && (sizeof(T) <= 16));

    template <typename... Args>
    constexpr bool is_acceptable_func_argument_array_v = (is_acceptable_func_argument_v<Args> && ...);

    template <typename T>
    struct is_all_func_arguments_acceptable;

    template <typename Ret, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(Args...)>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(Args...) noexcept>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(*)(Args...)>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(*)(Args...) noexcept>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };
    
    template <typename Ret, typename C, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(C::*)(Args...)>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename C, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(C::*)(Args...) const>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename C, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(C::*)(Args...) noexcept>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Ret, typename C, typename... Args>
    struct is_all_func_arguments_acceptable<Ret(C::*)(Args...) const noexcept>
    {
        static constexpr bool value = is_acceptable_func_argument_array_v<Args...>;
    };

    template <typename Func>
    constexpr bool is_all_func_arguments_acceptable_v = is_all_func_arguments_acceptable<Func>::value;
} // namespace Zibra
