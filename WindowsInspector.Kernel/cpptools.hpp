#pragma once

namespace std {
	template<class T>
	struct remove_reference
	{   // remove reference
		typedef T type;
	};

	template<class T>
	struct remove_reference<T&>
	{   // remove reference
		typedef T type;
	};

	template<class T>
	struct remove_reference<T&&>
	{   // remove rvalue reference
		typedef T type;
	};

	template< class T >
	using remove_reference_t = typename remove_reference<T>::type;

	template<class T>
	inline remove_reference_t<T>&& move(T&& _Arg)
	{   
		// forward _Arg as movable
		return (remove_reference<T>&&)_Arg;
	}
}
