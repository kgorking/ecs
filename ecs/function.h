#pragma once
#include <functional>

namespace ecs::detail {
	//
	// A small hack to add noexcept to std:functions move constructor.
	// Without noexcept a lot of copies would take place when component_pool::deferred_adds is resized,
	// which should be moved instead
	template <typename F>
	class function_fix : public std::function<F>
	{
	public:
		function_fix(function_fix const& _Right) noexcept
			: std::function<F>(static_cast<std::function<F> const&>(_Right))
		{ }
		function_fix(function_fix<F>&& _Right) noexcept {
			this->_Reset_move(std::move(_Right));
		}
		template <class _Fx, class = typename _Mybase::template _Enable_if_callable_t<_Fx&, function>>
		function_fix(_Fx _Func) {
			this->_Reset(std::move(_Func));
		}

		using std::function<F>::function;
		using std::function<F>::operator =;
	};
}
