#ifndef __OPTIONS_H
#define __OPTIONS_H

namespace ecs::opts {
    template<int I>
    struct group {
        static constexpr int group_id = I;
    };

    template<size_t I>
    struct frequency {
        static constexpr size_t hz = I;
    };

    struct manual_update {};

    struct not_parallel {};
    //struct not_concurrent {};

    namespace detail {
        template<size_t N>
        struct fixed_string {
            constexpr fixed_string(const char (&foo)[N]) {
                std::copy_n(foo, N, m_data);
            }

            char m_data[N];
        };

        template<size_t N>
        fixed_string(const char (&str)[N]) -> fixed_string<N>;
    } // namespace detail

    template<detail::fixed_string Name>
    struct name {
        static constexpr char const* _internal_get() {
            return Name.m_data;
        }
    };

} // namespace ecs::opts

#endif // !__OPTIONS_H
