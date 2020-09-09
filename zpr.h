/*
	zpr.h
	Copyright 2020, zhiayang

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.



	detail::print_floating and detail::print_exponent are adapted from _ftoa and _etoa
	from https://github.com/mpaland/printf, which is licensed under the MIT license,
	reproduced below:

	Copyright Marco Paland (info@paland.com), 2014-2019, PALANDesign Hannover, Germany

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/


/*
	Documentation
	=============
	This printing library functions as a lightweight alternative to std::format in C++20 (or fmtlib), with the following (non)features:

	1. no exceptions
	2. short and simple implementation without tons of templates
	3. no support for positional arguments (for now)

	Otherwise, it should support most of the "typical use-case" features that std::format provides. In essence it is a type-safe
	alternative to printf supporting custom type printers. The usage is as such:

	zpr::println("{<spec>}", argument);

	where `<spec>` is exactly a `printf`-style format specifier (note: there is no leading colon unlike the fmtlib/python style),
	and where the final type specifier (eg. `s`, `d`) is optional. Floating point values will print as if `g` was used. Size
	specifiers (eg. `lld`) are not supported.

	As with `printf`, you can use `*` to indicate variable width, precision, or both, like this: `{*.*}`; they should preceed
	the actual value to be printed (just like `printf`! are you seeing a pattern here?)

	To format custom types, specialise the print_formatter struct (in namespace `zpr`). An example of how it should be done
	can be seen from the builtin formatters. Actually you don't need to open up this namespace to specialise it, if you
	so desire -- just provide a suitable print_formatter type at global scope with a zero-argument constructor taking one
	template type argument, and a print() method with the appropriate signature.


	The currently supported builtin formatters are:
	- integral types            (signed/unsigned char/short/int/long/long long) (but not 'char')
	- floating point types      (float, double, long double)
	- strings                   (char*, const char*, anything container value_type == 'char')
	- booleans                  (prints as 'true'/'false')
	- void*, const void*        (prints with %p)
	- chars                     (char)
	- enums                     (will print as their integer representation)
	- std::pair                 (prints as "{ first, second }")
	- all iterable containers   (with begin(x) and end(x) available -- prints as "[ a, b, ..., c ]")


	optional #define macros to control behaviour:

	- ZPR_USE_STD
		this is *TRUE* by default. controls whether or not STL type interfaces are used; with it,
		you get the std::pair printer, and the sprint() overload that returns std::string. that's
		about it. iterator-based container printing is not affected by this flag.

	- ZPR_HEX_0X_RESPECTS_UPPERCASE
		this is *FALSE* by default. basically, if you use '{X}', this setting determines whether
		you'll get '0xDEADBEEF' or '0XDEADBEEF'. i think the capital 'X' looks ugly as heck, so this
		is OFF by default.

	- ZPR_DECIMAL_LOOKUP_TABLE
		this is *TRUE* by default. controls whether we use a lookup table to increase the speed of
		decimal printing. this uses 201 bytes.

	- ZPR_HEXADECIMAL_LOOKUP_TABLE
		this is *TRUE* by default. controls whether we use a lookup table to increase the speed of
		hex printing. this uses 1025 bytes.




	Version History
	===============

	1.4.0 - 10/09/2020
	------------------
	Completely remove dependency on STL types. sadly this includes lots of re-implemented type_traits, but an okay
	cost to pay, I suppose. Introduces ZPR_USE_STD define to control this.

	Bug fixes: lots of fixes on formatting correctness; we should now be correct wrt. printf.



	1.3.1 - 09/09/2020
	------------------
	Remove dependency on std::to_chars, to slowly wean off STL. Introduce two new #defines:
	ZPR_DECIMAL_LOOKUP_TABLE, and ZPR_HEXADECIMAL_LOOKUP_TABLE. see docs for info.



	1.3.0 - 08/09/2020
	------------------
	Add overloads for the user-facing print functions that accept std::string_view as the format string. This also
	works for std::string since string_view has an implicit conversion constructor for it.

	Removed user-facing overloads of print and friends that take 'const char*'; now they either take std::string_view
	(as above), or (const char&)[].

	Change the behaviour of string printers; now, any iterable type with a value_type typedef equal to 'char'
	(exactly char -- not signed char, not unsigned char) will print as a string. This lets us cover custom
	string types as well. A side effect of this is that std::vector<char> will print as a string, which might
	be unexpected.

	Bug fixes:
	- broken formatting for floating point numbers
	- '{}' now uses '%g' format for floating point numbers
	- '{p}' (ie. '%p') now works, including '{}' for void* and const void*.



	1.2.0 - 20/07/2020
	------------------
	Use floating-point printer from mpaland/printf, letting us actually beat printf in all cases.



	1.1.1 - 20/07/2020
	------------------
	Don't include unistd.h



	1.1.0 - 20/07/2020
	------------------
	Performance improvements: switched to tuple-based arguments instead of parameter-packed recursion. We are now (slightly)
	faster than printf (as least on two of my systems) as long as no floating-point is involved. (for now we are still forced
	to call snprintf to print floats... charconv pls)

	Bug fixes: fixed broken escaping of {{ and }}.



	1.0.0 - 19/07/2020
	------------------
	Initial release.
*/

#include <cstdio>
#include <cfloat>
#include <cstring>

#ifndef ZPR_HEX_0X_RESPECTS_UPPERCASE
	#define ZPR_HEX_0X_RESPECTS_UPPERCASE 0
#endif

#ifndef ZPR_DECIMAL_LOOKUP_TABLE
	#define ZPR_DECIMAL_LOOKUP_TABLE 1
#endif

#ifndef ZPR_HEXADECIMAL_LOOKUP_TABLE
	#define ZPR_HEXADECIMAL_LOOKUP_TABLE 1
#endif

#ifndef ZPR_USE_STD
	#define ZPR_USE_STD 1
#endif

#if ZPR_USE_STD
	#include <string>
	#include <string_view>
#endif

namespace zpr::detail
{
	// not std.
	namespace util
	{
		template <typename T> struct type_identity { using type = T; };

		template <typename T> struct remove_reference      { using type = T; };
		template <typename T> struct remove_reference<T&>  { using type = T; };
		template <typename T> struct remove_reference<T&&> { using type = T; };

		template <typename T, T v>
		struct integral_constant
		{
			static constexpr T value = v;
			typedef T value_type;
			typedef integral_constant type; // using injected-class-name

			constexpr operator value_type() const { return value; }
			constexpr value_type operator()() const { return value; }
		};

		using true_type = integral_constant<bool, true>;
		using false_type = integral_constant<bool, false>;

		template <typename T> struct remove_cv                   { using type = T; };
		template <typename T> struct remove_cv<const T>          { using type = T; };
		template <typename T> struct remove_cv<volatile T>       { using type = T; };
		template <typename T> struct remove_cv<const volatile T> { using type = T; };

		template <typename T> struct remove_const                { using type = T; };
		template <typename T> struct remove_const<const T>       { using type = T; };

		template <typename T> struct remove_volatile             { using type = T; };
		template <typename T> struct remove_volatile<volatile T> { using type = T; };

		template <typename T> using remove_cv_t = typename remove_cv<T>::type;
		template <typename T> using remove_const_t = typename remove_const<T>::type;
		template <typename T> using remove_volatile_t = typename remove_volatile<T>::type;

		template <typename> struct is_integral_base : false_type { };

		template <> struct is_integral_base<bool>               : true_type { };
		template <> struct is_integral_base<char>               : true_type { };
		template <> struct is_integral_base<signed char>        : true_type { };
		template <> struct is_integral_base<signed short>       : true_type { };
		template <> struct is_integral_base<signed int>         : true_type { };
		template <> struct is_integral_base<signed long>        : true_type { };
		template <> struct is_integral_base<signed long long>   : true_type { };
		template <> struct is_integral_base<unsigned char>      : true_type { };
		template <> struct is_integral_base<unsigned short>     : true_type { };
		template <> struct is_integral_base<unsigned int>       : true_type { };
		template <> struct is_integral_base<unsigned long>      : true_type { };
		template <> struct is_integral_base<unsigned long long> : true_type { };

		template <typename T> struct is_integral : is_integral_base<remove_cv_t<T>> { };
		template <typename T> constexpr auto is_integral_v = is_integral<T>::value;


		template <typename> struct is_signed_base : false_type { };

		template <> struct is_signed_base<signed char>        : true_type { };
		template <> struct is_signed_base<signed short>       : true_type { };
		template <> struct is_signed_base<signed int>         : true_type { };
		template <> struct is_signed_base<signed long>        : true_type { };
		template <> struct is_signed_base<signed long long>   : true_type { };

		template <typename T> struct is_signed : is_signed_base<remove_cv_t<T>> { };
		template <typename T> constexpr auto is_signed_v = is_signed<T>::value;

		template <typename T, typename U>   struct is_same : false_type { };
		template <typename T>               struct is_same<T, T> : true_type { };

		template <typename A, typename B>
		constexpr auto is_same_v = is_same<A, B>::value;

		// the 3 major compilers -- clang, gcc, and msvc -- support __is_enum. it's not
		// tenable implement is_enum without compiler magic.
		template <typename T> struct is_enum { static constexpr bool value = __is_enum(T); };
		template <typename T> constexpr auto is_enum_v = is_enum<T>::value;

		template <typename T> struct is_reference      : false_type { };
		template <typename T> struct is_reference<T&>  : true_type { };
		template <typename T> struct is_reference<T&&> : true_type { };

		template <typename T> struct is_const          : false_type { };
		template <typename T> struct is_const<const T> : true_type { };

		template <typename T> constexpr auto is_const_v = is_const<T>::value;
		template <typename T> constexpr auto is_reference_v = is_reference<T>::value;

		// a similar story exists for __underlying_type.
		template <typename T> struct underlying_type { using type = __underlying_type(T); };
		template <typename T> using underlying_type_t = typename underlying_type<T>::type;

		template <bool B, typename T = void> struct enable_if { };
		template <typename T> struct enable_if<true, T> { using type = T; };
		template <bool B, typename T> using enable_if_t = typename enable_if<B, T>::type;

		template <typename T> struct is_array : false_type { };
		template <typename T> struct is_array<T[]> : true_type { };
		template <typename T, size_t N> struct is_array<T[N]> : true_type { };

		template <typename T> struct remove_extent { using type = T; };
		template <typename T> struct remove_extent<T[]> { using type = T; };
		template <typename T, size_t N> struct remove_extent<T[N]> { using type = T; };

		template <typename T> struct is_function : integral_constant<bool, !is_const_v<const T> && !is_reference_v<T>> { };
		template <typename T> constexpr auto is_function_v = is_function<T>::value;

		template <typename T> auto try_add_pointer(int) -> type_identity<typename remove_reference<T>::type*>;
		template <typename T> auto try_add_pointer(...) -> type_identity<T>;

		template <typename T>
		struct add_pointer : decltype(try_add_pointer<T>(0)) { };

		template <bool B, typename T, typename F> struct conditional { using type = T; };
		template <typename T, typename F> struct conditional<false, T, F> { using type = F; };
		template <bool B, typename T, typename F> using conditional_t = typename conditional<B,T,F>::type;

		template <typename...> struct conjunction : true_type { };
		template <typename B1> struct conjunction<B1> : B1 { };
		template <typename B1, typename... Bn>
		struct conjunction<B1, Bn...> : conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

		template <typename B>
		struct negation : integral_constant<bool, !bool(B::value)> { };

		template <typename T>
		struct decay
		{
		private:
			using U = typename remove_reference<T>::type;
		public:
			using type = typename conditional<
				is_array<U>::value,
				typename remove_extent<U>::type*,
				typename conditional<
					is_function<U>::value,
					typename add_pointer<U>::type,
					typename remove_cv<U>::type
				>::type
			>::type;
		};

		template <typename T> using decay_t = typename decay<T>::type;


		template <typename T, bool = is_integral<T>::value>
		struct _is_unsigned : integral_constant<bool, T(0) < T(-1)> { };

		template <typename T>
		struct _is_unsigned<T, false> : false_type { };

		template <typename T>
		struct is_unsigned : _is_unsigned<T>::type { };

		template <typename T>
		struct make_unsigned { };

		template <> struct make_unsigned<signed short> { using type = unsigned short; };
		template <> struct make_unsigned<unsigned short> { using type = unsigned short; };
		template <> struct make_unsigned<signed int> { using type = unsigned int; };
		template <> struct make_unsigned<unsigned int> { using type = unsigned int; };
		template <> struct make_unsigned<signed long> { using type = unsigned long; };
		template <> struct make_unsigned<unsigned long> { using type = unsigned long; };
		template <> struct make_unsigned<signed long long> { using type = unsigned long long; };
		template <> struct make_unsigned<unsigned long long> { using type = unsigned long long; };

		template <typename T>
		using make_unsigned_t = typename make_unsigned<T>::type;


		template <typename... Xs>
		using void_t = void;

		template <typename T>
		struct __stop_declval_eval { static constexpr bool __stop = false; };

		template <typename T, typename U = T&&>
		U __declval(int);

		template <typename T>
		T __declval(long);

		template <typename T>
		auto declval() -> decltype(__declval<T>(0))
		{
			static_assert(__stop_declval_eval<T>::__stop, "declval() must not be used!");
			return __stop_declval_eval<T>::__unknown();
		}

		template <typename T>
		typename remove_reference<T>::type&& move(T&& arg) { return static_cast<typename remove_reference<T>::type&&>(arg); }


		template <typename T> T&& forward(typename remove_reference<T>::type& t)  { return static_cast<typename type_identity<T>::type&&>(t); }
		template <typename T> T&& forward(typename remove_reference<T>::type&& t) { return static_cast<typename type_identity<T>::type&&>(t); }

		template <typename T> T min(const T& a, const T& b) { return a < b ? a : b; }
		template <typename T> T max(const T& a, const T& b) { return a > b ? a : b; }
		template <typename T> T abs(const T& x) { return x < 0 ? -x : x; }

		template <typename T>
		void swap(T& t1, T& t2)
		{
			T temp = util::move(t1);
			t1 = util::move(t2);
			t2 = util::move(temp);
		}

		template <size_t Idx, typename... Xs>
		struct one_tuple_thing
		{
		};

		template <size_t Idx, typename T, typename... Xs>
		struct one_tuple_thing<Idx, T, Xs...> : one_tuple_thing<Idx + 1, Xs...>
		{
			using base_type = one_tuple_thing<Idx + 1, Xs...>;

			one_tuple_thing() : value() { }

			template <typename U, typename... Us>
			one_tuple_thing(U&& x, Us&&... xs) : base_type(util::forward<Us>(xs)...), value(util::forward<T>(x)) { }

			template <size_t I>
			auto& get() &
			{
				if constexpr (I == Idx) { return this->value; }
				else                    { return base_type::template get<I>(); }
			}

			template <size_t I>
			auto&& get() &&
			{
				if constexpr (I == Idx) { return this->value; }
				else                    { return base_type::template get<I>(); }
			}

			template <size_t I>
			const auto& get() const&
			{
				if constexpr (I == Idx) { return this->value; }
				else                    { return base_type::template get<I>(); }
			}

		private:
			T value;
		};

		template <size_t Idx, typename T>
		struct one_tuple_thing<Idx, T>
		{
			one_tuple_thing() : value() { }

			template <typename U>
			one_tuple_thing(U&& x) : value(util::forward<U>(x)) { }

			template <size_t I>
			T& get() & { static_assert(I == Idx, "invalid tuple index"); return this->value; }

			template <size_t I>
			T&& get() && { static_assert(I == Idx, "invalid tuple index"); return this->value; }

			template <size_t I>
			const T& get() const& { static_assert(I == Idx, "invalid tuple index"); return this->value; }

		private:
			T value;
		};


		template <typename... Args>
		struct tuple : one_tuple_thing<0, Args...>
		{
			tuple() : one_tuple_thing<0, Args...>() { }

			template <typename... Xs>
			tuple(Xs&&... xs) : one_tuple_thing<0, Args...>(util::forward<Xs>(xs)...) { }

			template <size_t Idx> auto& get() & { return one_tuple_thing<0, Args...>::template get<Idx>(); }
			template <size_t Idx> auto&& get() && { return one_tuple_thing<0, Args...>::template get<Idx>(); }
			template <size_t Idx> const auto& get() const& { return one_tuple_thing<0, Args...>::template get<Idx>(); }

			size_t size() const
			{
				return sizeof...(Args);
			}

			static constexpr size_t tuple_size = sizeof...(Args);
		};

		template <typename... Args>
		tuple(Args...) -> tuple<Args...>;

		template <typename... Args>
		util::tuple<Args&&...> forward_as_tuple(Args&&... xs)
		{
			return tuple<Args&&...>(util::forward<Args>(xs)...);
		}


		struct str_view
		{
			str_view() : ptr(nullptr), len(0) { }
			str_view(const char* p, size_t l) : ptr(p), len(l) { }

			template <size_t N>
			str_view(const char (&s)[N]) : ptr(s), len(N) { }

			str_view(const char* s) : ptr(s), len(strlen(s)) { }

		#if ZPR_USE_STD

			str_view(const std::string& str) : ptr(str.data()), len(str.size()) { }
			str_view(const std::string_view& sv) : ptr(sv.data()), len(sv.size()) { }

		#endif

			str_view(str_view&&) = default;
			str_view(const str_view&) = default;
			str_view& operator= (str_view&&) = default;
			str_view& operator= (const str_view&) = default;

			bool operator== (const str_view& other) const
			{
				return (this->ptr == other.ptr && this->len == other.len)
					|| (strncmp(this->ptr, other.ptr, util::min(this->len, other.len)) == 0);
			}

			bool operator!= (const str_view& other) const
			{
				return !(*this == other);
			}

			const char* begin() const { return this->ptr; }
			const char* end() const { return this->ptr + len; }

			size_t size() const { return this->len; }
			bool empty() const { return this->len == 0; }
			const char* data() const { return this->ptr; }

			char operator[] (size_t n) { return this->ptr[n]; }

			str_view drop(size_t n) const { return (this->size() > n ? this->substr(n) : ""); }
			str_view take(size_t n) const { return (this->size() > n ? this->substr(0, n) : *this); }
			str_view take_last(size_t n) const { return (this->size() > n ? this->substr(this->size() - n) : *this); }
			str_view drop_last(size_t n) const { return (this->size() > n ? this->substr(0, this->size() - n) : *this); }
			str_view substr(size_t pos = 0, size_t cnt = -1) const { return str_view(this->ptr + pos, cnt); }

			str_view& remove_prefix(size_t n) { return (*this = this->drop(n)); }
			str_view& remove_suffix(size_t n) { return (*this = this->drop_last(n)); }

		private:
			const char* ptr;
			size_t len;
		};
	}
}

namespace zpr
{
	constexpr uint8_t FMT_FLAG_ZERO_PAD         = 0x1;
	constexpr uint8_t FMT_FLAG_ALTERNATE        = 0x2;
	constexpr uint8_t FMT_FLAG_PREPEND_PLUS     = 0x4;
	constexpr uint8_t FMT_FLAG_PREPEND_SPACE    = 0x8;
	constexpr uint8_t FMT_FLAG_HAVE_WIDTH       = 0x10;
	constexpr uint8_t FMT_FLAG_HAVE_PRECISION   = 0x20;
	constexpr uint8_t FMT_FLAG_WIDTH_NEGATIVE   = 0x40;

	struct format_args
	{
		char specifier      = -1;
		uint8_t flags       = 0;

		int64_t width       = -1;
		int64_t length      = -1;
		int64_t precision   = -1;

		bool zero_pad() const       { return this->flags & FMT_FLAG_ZERO_PAD; }
		bool alternate() const      { return this->flags & FMT_FLAG_ALTERNATE; }
		bool have_width() const     { return this->flags & FMT_FLAG_HAVE_WIDTH; }
		bool have_precision() const { return this->flags & FMT_FLAG_HAVE_PRECISION; }
		bool prepend_plus() const   { return this->flags & FMT_FLAG_PREPEND_PLUS; }
		bool prepend_space() const  { return this->flags & FMT_FLAG_PREPEND_SPACE; }

		bool negative_width() const { return have_width() && (this->flags & FMT_FLAG_WIDTH_NEGATIVE); }
		bool positive_width() const { return have_width() && !negative_width(); }
	};

	template <typename T, typename = void>
	struct print_formatter
	{
		template <typename K>
		struct has_formatter { static constexpr bool value = false; };

		// when printing, we use print_formatter<T>().print(...). if there is no specialisation
		// for print_formatter<T>, then we will instantiate this base class -- which causes the nice
		// static_assert message. note that we must use some predicate that depends on T, so that the
		// compiler can only know the value when it tries to instantiate. using static_assert(false)
		// will always fail to compile.

		// we make a inner type has_formatter which is more descriptive, and since we only make this
		// error when we try to instantiate the base, any specialisations don't even need to care!
		static_assert(has_formatter<T>::value, "no formatter defined for type!");
	};

	namespace detail
	{
		template <typename T, typename = void>
		struct is_iterable : util::false_type { };

		template <typename T>
		struct is_iterable<T, util::void_t<
			decltype(begin(util::declval<T&>())),
			decltype(end(util::declval<T&>()))
		>> : util::true_type { };

		static inline util::tuple<format_args, bool, bool> parse_fmt_spec(detail::util::str_view sv)
		{
			// remove the first and last (they are { and })
			sv = sv.drop(1).drop_last(1);

			bool need_prec = false;
			bool need_width = false;
			format_args fmt_args = { };
			{
				while(sv.size() > 0)
				{
					switch(sv[0])
					{
						case '0':   fmt_args.flags |= FMT_FLAG_ZERO_PAD; sv.remove_prefix(1); continue;
						case '#':   fmt_args.flags |= FMT_FLAG_ALTERNATE; sv.remove_prefix(1); continue;
						case '-':   fmt_args.flags |= FMT_FLAG_WIDTH_NEGATIVE; sv.remove_prefix(1); continue;
						case '+':   fmt_args.flags |= FMT_FLAG_PREPEND_PLUS; sv.remove_prefix(1); continue;
						case ' ':   fmt_args.flags |= FMT_FLAG_PREPEND_SPACE; sv.remove_prefix(1); continue;
						default:    break;
					}

					break;
				}

				if(sv.empty())
					goto done;

				if(sv[0] == '*')
				{
					// note: if you use *, then the negative width is ignored!
					need_width = true;
					sv.remove_prefix(1);
					fmt_args.flags |= FMT_FLAG_HAVE_WIDTH;
				}
				else if('0' <= sv[0] && sv[0] <= '9')
				{
					fmt_args.flags |= FMT_FLAG_HAVE_WIDTH;

					size_t k = 0;
					fmt_args.width = 0;

					while(sv.size() > k && ('0' <= sv[k] && sv[k] <= '9'))
						(fmt_args.width = 10 * fmt_args.width + (sv[k] - '0')), k++;

					sv.remove_prefix(k);
				}

				if(sv.empty())
					goto done;

				if(sv[0] == '.')
				{
					sv.remove_prefix(1);

					if(sv.size() > 0 && sv[0] == '*')
					{
						sv.remove_prefix(1);
						need_prec = true;
						fmt_args.flags |= FMT_FLAG_HAVE_PRECISION;
					}
					else if(sv.size() > 0 && sv[0] == '-')
					{
						// just ignore negative precision i guess.
						size_t k = 1;
						while(sv.size() > k && ('0' <= sv[k]) && (sv[k] <= '9'))
							k++;

						sv.remove_prefix(k);
					}
					else if('0' <= sv[0] && sv[0] <= '9')
					{
						fmt_args.flags |= FMT_FLAG_HAVE_PRECISION;
						fmt_args.precision = 0;

						size_t k = 0;
						while(sv.size() > k && (sv[k] >= '0') && (sv[k] <= '9'))
							(fmt_args.precision = 10 * fmt_args.precision + (sv[k] - '0')), k++;

						sv.remove_prefix(k);
					}
				}

				if(!sv.empty())
					(fmt_args.specifier = sv[0]), sv.remove_prefix(1);
			}

		done:
			return { fmt_args, need_width, need_prec };
		}

		template <typename CallbackFn, typename Fn, typename Tuple, size_t N = 0>
		void visit_one(CallbackFn&& cb, Tuple&& args, size_t idx, format_args fmt_args, Fn&& fn)
		{
			if(N == idx)
			{
				fn(cb, util::move(fmt_args), util::move(args.template get<N>()));
				return;
			}

			if constexpr (N + 1 < Tuple::tuple_size)
				return visit_one<CallbackFn, Fn, Tuple, N + 1>(util::forward<CallbackFn>(cb), util::forward<Tuple>(args),
					idx, util::move(fmt_args), util::forward<Fn>(fn));
		}

		template <typename CallbackFn, typename Fn, typename Tuple, size_t N = 0>
		void visit_two(CallbackFn&& cb, Tuple&& args, size_t idx, format_args fmt_args, Fn&& fn)
		{
			if(N == idx)
			{
				fn(cb, util::move(fmt_args),
					util::move(args.template get<N>()),
					util::move(args.template get<N + 1>())
				);
				return;
			}

			if constexpr (N + 2 < Tuple::tuple_size)
				return visit_two<CallbackFn, Fn, Tuple, N + 1>(util::forward<CallbackFn>(cb), util::forward<Tuple>(args),
					idx, util::move(fmt_args), util::forward<Fn>(fn));
		}

		template <typename CallbackFn, typename Fn, typename Tuple, size_t N = 0>
		void visit_three(CallbackFn&& cb, Tuple&& args, size_t idx, format_args fmt_args, Fn&& fn)
		{
			if(N == idx)
			{
				fn(cb, util::move(fmt_args),
					util::move(args.template get<N>()),
					util::move(args.template get<N + 1>()),
					util::move(args.template get<N + 2>())
				);
				return;
			}

			if constexpr (N + 3 < Tuple::tuple_size)
				return visit_three<CallbackFn, Fn, Tuple, N + 1>(util::forward<CallbackFn>(cb), util::forward<Tuple>(args),
					idx, util::move(fmt_args), util::forward<Fn>(fn));
		}


		template <typename CallbackFn>
		size_t print_string(CallbackFn&& cb, const char* str, size_t len, format_args args)
		{
			int64_t string_length = 0;

			if(args.have_precision())   string_length = util::min(args.precision, static_cast<int64_t>(len));
			else                        string_length = static_cast<int64_t>(len);

			size_t ret = string_length;
			auto padding_width = args.width - string_length;

			if(args.positive_width() && padding_width > 0)
				cb(' ', padding_width), ret += padding_width;

			cb(str, string_length);

			if(args.negative_width() && padding_width > 0)
				cb(' ', padding_width), ret += padding_width;

			return ret;
		}

		template <typename CallbackFn>
		size_t print_special_floating(CallbackFn&& cb, double value, format_args args)
		{
			if(value != value)
				return print_string(cb, "nan", 3, util::move(args));

			if(value < -DBL_MAX)
				return print_string(cb, "-inf", 4, util::move(args));

			if(value > DBL_MAX)
			{
				return print_string(cb, args.prepend_plus()
					? "+inf" : args.prepend_space()
					? " inf" : "inf",
					args.prepend_space() || args.prepend_plus() ? 4 : 3,
					util::move(args)
				);
			}

			return 0;
		}



		// note that all the base printers need to return the same type, so just do 80 to account for
		// the base 2 printing.
		struct __buffer_thingy
		{
			static constexpr size_t BUFFER_LEN = 80;

			char* buf = 0;
			size_t len = 0;
			char buffer[BUFFER_LEN] = { 0 };
		};

		// forward declare these
		template <typename CallbackFn>
		size_t print_floating(CallbackFn&& cb, double value, format_args args);

		template <typename T>
		__buffer_thingy print_decimal_integer(T value);



		template <typename CallbackFn>
		size_t print_exponent(CallbackFn&& cb, double value, format_args args)
		{
			constexpr int DEFAULT_PRECISION = 6;

			// check for NaN and special values
			if((value != value) || (value > DBL_MAX) || (value < -DBL_MAX))
				return print_special_floating(cb, value, util::move(args));

			int prec = (args.have_precision() ? static_cast<int>(args.precision) : DEFAULT_PRECISION);

			bool use_precision  = args.have_precision();
			bool use_zero_pad   = args.zero_pad() && args.positive_width() && !use_precision;
			bool use_left_pad   = !use_zero_pad && args.positive_width();
			bool use_right_pad  = !use_zero_pad && args.negative_width();

			// determine the sign
			const bool negative = (value < 0);
			if(negative)
				value = -value;

			// determine the decimal exponent
			// based on the algorithm by David Gay (https://www.ampl.com/netlib/fp/dtoa.c)
			union {
				uint64_t U;
				double F;
			} conv;

			conv.F = value;
			auto exp2 = static_cast<int64_t>((conv.U >> 52U) & 0x07FFU) - 1023; // effectively log2
			conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);        // drop the exponent so conv.F is now in [1,2)

			// now approximate log10 from the log2 integer part and an expansion of ln around 1.5
			auto expval = static_cast<int64_t>(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);

			// now we want to compute 10^expval but we want to be sure it won't overflow
			exp2 = static_cast<int64_t>(expval * 3.321928094887362 + 0.5);

			const double z = expval * 2.302585092994046 - exp2 * 0.6931471805599453;
			const double z2 = z * z;

			conv.U = static_cast<uint64_t>(exp2 + 1023) << 52U;

			// compute exp(z) using continued fractions, see https://en.wikipedia.org/wiki/Exponential_function#Continued_fractions_for_ex
			conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));

			// correct for rounding errors
			if(value < conv.F)
			{
				expval--;
				conv.F /= 10;
			}

			// the exponent format is "%+02d" and largest value is "307", so set aside 4-5 characters (including the e+ part)
			int minwidth = (-100 < expval && expval < 100) ? 4U : 5U;

			// in "%g" mode, "prec" is the number of *significant figures* not decimals
			if(args.specifier == 'g' || args.specifier == 'G')
			{
				// do we want to fall-back to "%f" mode?
				if((value >= 1e-4) && (value < 1e6))
				{
					if(static_cast<int64_t>(prec) > expval)
						prec = static_cast<uint64_t>(static_cast<int64_t>(prec) - expval - 1);

					else
						prec = 0;

					args.precision = prec;

					// no characters in exponent
					minwidth = 0;
					expval = 0;
				}
				else
				{
					// we use one sigfig for the whole part
					if(prec > 0 && use_precision)
						prec -= 1;
				}
			}

			// will everything fit?
			uint64_t fwidth = args.width;
			if(args.width > minwidth)
			{
				// we didn't fall-back so subtract the characters required for the exponent
				fwidth -= minwidth;
			}
			else
			{
				// not enough characters, so go back to default sizing
				fwidth = 0;
			}

			if(use_right_pad && minwidth)
			{
				// if we're padding on the right, DON'T pad the floating part
				fwidth = 0;
			}

			// rescale the float value
			if(expval)
				value /= conv.F;

			// output the floating part
			args.width = fwidth;
			auto len = print_floating(cb, negative ? -value : value, args);

			// output the exponent part
			if(minwidth > 0)
			{
				len++;
				if(args.specifier & 0x20)   cb('e');
				else                        cb('E');

				// output the exponent value
				char tmp[8] = { 0 };
				size_t digits_len = 0;


				auto buf = print_decimal_integer(static_cast<int64_t>(util::abs(expval)));
				memcpy(tmp, buf.buf, buf.len);
				digits_len = buf.len;

				len += digits_len;
				cb(expval < 0 ? '-' : '+');

				// zero-pad to minwidth - 2
				if(auto tmp = (minwidth - 2) - static_cast<int>(digits_len); tmp > 0)
					len += tmp, cb('0', tmp);

				cb(tmp, digits_len);

				// might need to right-pad spaces
				if(use_right_pad && args.width > len)
					cb(' ', args.width - len), len = args.width;
			}

			return len;
		}


		template <typename CallbackFn>
		size_t print_floating(CallbackFn&& cb, double value, format_args args)
		{
			constexpr int DEFAULT_PRECISION = 6;
			constexpr size_t MAX_BUFFER_LEN = 128;
			constexpr long double EXPONENTIAL_CUTOFF = 1e15;

			char buf[MAX_BUFFER_LEN] = { 0 };

			size_t len = 0;

			int prec = (args.have_precision() ? static_cast<int>(args.precision) : DEFAULT_PRECISION);

			bool use_precision  = args.have_precision();
			bool use_zero_pad   = args.zero_pad() && args.positive_width() && !use_precision;
			bool use_left_pad   = !use_zero_pad && args.positive_width();
			bool use_right_pad  = !use_zero_pad && args.negative_width();

			// powers of 10
			constexpr double pow10[] = {
				1,
				10,
				100,
				1000,
				10000,
				100000,
				1000000,
				10000000,
				100000000,
				1000000000,
				10000000000,
				100000000000,
				1000000000000,
				10000000000000,
				100000000000000,
				1000000000000000,
				10000000000000000,
			};

			// test for special values
			if((value != value) || (value > DBL_MAX) || (value < -DBL_MAX))
				return print_special_floating(cb, value, util::move(args));

			// switch to exponential for large values.
			if((value > EXPONENTIAL_CUTOFF) || (value < -EXPONENTIAL_CUTOFF))
				return print_exponent(cb, value, util::move(args));

			// default to g.
			if(args.specifier == -1)
				args.specifier = 'g';

			// test for negative
			const bool negative = (value < 0);
			if(value < 0)
				value = -value;

			// limit precision to 16, cause a prec >= 17 can lead to overflow errors
			while((len < MAX_BUFFER_LEN) && (prec > 16))
			{
				buf[len++] = '0';
				prec--;
			}

			auto whole = static_cast<int64_t>(value);
			auto tmp = (value - whole) * pow10[prec];
			auto frac = static_cast<unsigned long>(tmp);

			double diff = tmp - frac;

			if(diff > 0.5)
			{
				frac += 1;

				// handle rollover, e.g. case 0.99 with prec 1 is 1.0
				if(frac >= pow10[prec])
				{
					frac = 0;
					whole += 1;
				}
			}
			else if(diff < 0.5)
			{
				// ?
			}
			else if((frac == 0U) || (frac & 1U))
			{
				// if halfway, round up if odd OR if last digit is 0
				frac += 1;
			}

			if(prec == 0U)
			{
				diff = value - static_cast<double>(whole);
				if((!(diff < 0.5) || (diff > 0.5)) && (whole & 1))
				{
					// exactly 0.5 and ODD, then round up
					// 1.5 -> 2, but 2.5 -> 2
					whole += 1;
				}
			}
			else
			{
				auto count = prec;

				bool flag = (args.specifier == 'g' || args.specifier == 'G');
				// now do fractional part, as an unsigned number
				while(len < MAX_BUFFER_LEN)
				{
					if(flag && (frac % 10) == 0)
						goto skip;

					flag = false;
					buf[len++] = static_cast<char>('0' + (frac % 10));

				skip:
					count -= 1;
					if(!(frac /= 10))
						break;
				}

				// add extra 0s
				while((len < MAX_BUFFER_LEN) && (count-- > 0))
					buf[len++] = '0';

				// add decimal
				if(len < MAX_BUFFER_LEN)
					buf[len++] = '.';
			}

			// do whole part, number is reversed
			while(len < MAX_BUFFER_LEN)
			{
				buf[len++] = static_cast<char>('0' + (whole % 10));
				if(!(whole /= 10))
					break;
			}

			// pad leading zeros
			if(use_zero_pad)
			{
				auto width = args.width;

				if(args.have_width() != 0 && (negative || args.prepend_plus() || args.prepend_space()))
					width--;

				while((len < width) && (len < MAX_BUFFER_LEN))
					buf[len++] = '0';
			}

			if(len < MAX_BUFFER_LEN)
			{
				if(negative)
					buf[len++] = '-';

				else if(args.prepend_plus())
					buf[len++] = '+'; // ignore the space if the '+' exists

				else if(args.prepend_space())
					buf[len++] = ' ';
			}

			// reverse it.
			for(size_t i = 0; i < len / 2; i++)
				util::swap(buf[i], buf[len - i - 1]);

			auto padding_width = util::max(int64_t(0), args.width - static_cast<int64_t>(len));

			if(use_left_pad)
				cb(' ', padding_width);

			cb(buf, len);

			if(use_right_pad)
				cb(' ', padding_width);

			return len + ((use_left_pad || use_right_pad) ? padding_width : 0);
		}



		template <typename T>
		__buffer_thingy print_decimal_integer(T value)
		{
			constexpr const char lookup_table[] =
				"000102030405060708091011121314151617181920212223242526272829"
				"303132333435363738394041424344454647484950515253545556575859"
				"606162636465666768697071727374757677787980818283848586878889"
				"90919293949596979899";

			bool neg = false;
			if constexpr (util::is_signed_v<T>)
			{
				neg = (value < 0);
				if(neg)
					value = -value;
			}

			auto result = __buffer_thingy();
			auto& BUFFER_LEN = __buffer_thingy::BUFFER_LEN;

			auto& len = result.len;
			char* ptr = &result.buffer[0];

			// if we have the lookup table, do two digits at a time.
		#if ZPR_DECIMAL_LOOKUP_TABLE

			while(value >= 10)
			{
				memcpy(ptr + (BUFFER_LEN - 1) - len - 2, &lookup_table[(value % 100) * 2], 2);
				value /= 100;
				len += 2;
			}

			if(value > 0)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = ('0' + (value % 10));
				len++;
			}

		#else

			while(value > 0)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = ('0' + (value % 10));
				value /= 10;
				len++;
			}

		#endif

			if(neg)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = '-';
				len++;
			}

			result.buf = result.buffer + (BUFFER_LEN - 1) - len;
			return result;
		}




		template <typename T>
		__buffer_thingy print_hex_integer(T value)
		{
			constexpr const char lookup_table[] =
				"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
				"202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
				"404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
				"606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
				"808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
				"a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
				"c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
				"e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

			auto result = __buffer_thingy();
			auto& BUFFER_LEN = __buffer_thingy::BUFFER_LEN;

			auto& len = result.len;
			char* ptr = &result.buffer[0];

			auto hex_digit = [](int x) -> char {
				if(0 <= x && x <= 9)
					return '0' + x;

				return 'a' + x;
			};


			// if we have the lookup table, do two digits at a time.
		#if ZPR_HEXADECIMAL_LOOKUP_TABLE

			while(value >= 0x10)
			{
				memcpy(ptr + (BUFFER_LEN - 1) - len - 2, &lookup_table[(value % 0x100) * 2], 2);
				value /= 0x100;
				len += 2;
			}

			if(value > 0)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = hex_digit(value % 0x10);
				len++;
			}

		#else

			while(value > 0)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = hex_digit(value % 0x10);
				value /= 0x10;
				len++;
			}

		#endif

			result.buf = result.buffer + (BUFFER_LEN - 1) - len;
			return result;
		}

		template <typename T>
		__buffer_thingy print_binary_integer(T value)
		{
			auto result = __buffer_thingy();
			auto& BUFFER_LEN = __buffer_thingy::BUFFER_LEN;

			auto& len = result.len;
			char* ptr = &result.buffer[0];

			while(value > 0)
			{
				*(ptr + (BUFFER_LEN - 1) - len - 1) = ('0' + (value & 1));
				value >>= 1;
				len++;
			}

			result.buf = result.buffer + (BUFFER_LEN - 1) - len;
			return result;
		}

		template <typename T>
		auto print_integer(T value, int base)
		{
			if(base == 2)       return print_binary_integer(value);
			else if(base == 16) return print_hex_integer(value);
			else                return print_decimal_integer(value);
		}











		template <typename CallbackFn, typename Tuple>
		void print(CallbackFn&& cb, const char* fmt, size_t len, Tuple&& args)
		{
			auto beg = fmt;
			auto end = fmt;

			size_t tup_idx = 0;

			while(end - fmt <= len && end && *end)
			{
				if(*end == '{')
				{
					auto tmp = end;

					// flush whatever we have first:
					cb(beg, end);
					if(end[1] == '{')
					{
						cb('{');
						end += 2;
						beg = end;
						continue;
					}

					while(end[0] && end[0] != '}')
						end++;

					// owo
					if(!end[0])
						return;

					end++;

					auto fmts = parse_fmt_spec(detail::util::str_view(tmp, end - tmp));

					auto& fmt_spec = fmts.get<0>();
					auto& width    = fmts.get<1>();
					auto& prec     = fmts.get<2>();

					if(width && prec)
					{
						if constexpr (Tuple::tuple_size >= 3)
						{
							visit_three(cb, util::forward<Tuple>(args), tup_idx, util::move(fmt_spec),
								[](auto&& cb, format_args fmt_args, auto&& width, auto&& prec, auto&& x) {
									if constexpr (util::is_integral_v<util::remove_cv_t<util::decay_t<decltype(width)>>>
										&& util::is_integral_v<util::remove_cv_t<util::decay_t<decltype(prec)>>>)
									{
										fmt_args.width = width;
										fmt_args.precision = prec;

										fmt_args.flags |= (FMT_FLAG_HAVE_WIDTH | FMT_FLAG_HAVE_PRECISION);
									}

									print_formatter<util::remove_cv_t<util::decay_t<decltype(x)>>>()
										.print(util::move(x), cb, util::move(fmt_args));
								});
						}
						else
						{
							cb("<missing width and prec>");
						}

						tup_idx += 3;
					}
					else if(width)
					{
						if constexpr (Tuple::tuple_size >= 2)
						{
							visit_two(cb, util::forward<Tuple>(args), tup_idx, util::move(fmt_spec),
								[](auto&& cb, format_args fmt_args, auto&& width, auto&& x) {
									if constexpr (util::is_integral_v<util::remove_cv_t<util::decay_t<decltype(width)>>>)
									{
										fmt_args.width = width;
										fmt_args.flags |= FMT_FLAG_HAVE_WIDTH;
									}

									print_formatter<util::remove_cv_t<util::decay_t<decltype(x)>>>()
										.print(util::move(x), cb, util::move(fmt_args));
								});
						}
						else
						{
							cb("<missing width>");
						}

						tup_idx += 2;
					}
					else if(prec)
					{
						if constexpr (Tuple::tuple_size >= 2)
						{
							visit_two(cb, util::forward<Tuple>(args), tup_idx, util::move(fmt_spec),
								[](auto&& cb, format_args fmt_args, auto&& prec, auto&& x) {
									if constexpr (util::is_integral_v<util::remove_cv_t<util::decay_t<decltype(prec)>>>)
									{
										fmt_args.precision = prec;
										fmt_args.flags |= FMT_FLAG_HAVE_PRECISION;
									}

									print_formatter<util::remove_cv_t<util::decay_t<decltype(x)>>>()
										.print(util::move(x), cb, util::move(fmt_args));
								});
						}
						else
						{
							cb("<missing prec>");
						}

						tup_idx += 2;
					}
					else
					{
						if constexpr (Tuple::tuple_size >= 1)
						{
							visit_one(cb, util::forward<Tuple>(args), tup_idx, util::move(fmt_spec),
								[](auto&& cb, format_args fmt_args, auto&& x) {
									print_formatter<util::remove_cv_t<util::decay_t<decltype(x)>>>()
										.print(util::move(x), cb, util::move(fmt_args));
								});
						}
						else
						{
							cb("<missing value>");
						}

						tup_idx += 1;
					}

					beg = end;
				}
				else if(*end == '}')
				{
					cb(beg, end);

					// well... we don't need to escape }, but for consistency, we accept either } or }} to print one }.
					if(end[1] == '}')
						end++;

					end++;
					cb('}');
					beg = end;
				}
				else
				{
					end++;
				}
			}

			// flush
			cb(beg, end);
		}

		template <size_t N, typename CallbackFn, typename Tuple>
		void print(CallbackFn&& cb, const char (&fmt)[N], Tuple&& args)
		{
			print(cb, fmt, N, util::forward<Tuple>(args));
		}

		template <size_t Limit, bool Newline>
		struct file_appender
		{
			file_appender(FILE* fd, size_t& written) : fd(fd), written(written) { }
			~file_appender() { flush(true); }

			file_appender(file_appender&&) = delete;
			file_appender(const file_appender&) = delete;
			file_appender& operator= (file_appender&&) = delete;
			file_appender& operator= (const file_appender&) = delete;

			void operator() (char c) { *ptr++ = c; flush(); }

			void operator() (const detail::util::str_view& sv) { (*this)(sv.data(), sv.size()); }
			void operator() (const char* begin, const char* end) { (*this)(begin, static_cast<size_t>(end - begin)); }

			void operator() (char c, size_t n)
			{
				while(n > 0)
				{
					auto x = util::min(n, remaining());
					memset(ptr, c, x);
					ptr += x;
					n -= x;
					flush();
				}
			}

			void operator() (const char* begin, size_t len)
			{
				while(len > 0)
				{
					auto x = util::min(len, remaining());
					memcpy(ptr, begin, x);
					ptr += x;
					begin += x;
					len -= x;

					flush();
				}
			}

		private:
			inline size_t remaining()
			{
				return Limit - (ptr - buf);
			}

			inline void flush(bool last = false)
			{
				if(!last && ptr - buf < Limit)
					return;

				fwrite(buf, sizeof(char), ptr - buf, fd);
				written += ptr - buf;

				if(last && Newline)
					written++, fputc('\n', fd);

				ptr = buf;
			}

			FILE* fd = 0;

			char buf[Limit];
			char* ptr = &buf[0];
			size_t& written;
		};

		struct buffer_appender
		{
			buffer_appender(char* buf, size_t cap) : buf(buf), cap(cap), len(0) { }

			void operator() (char c)
			{
				if(this->len < this->cap)
					this->buf[this->len++] = c;
			}

			void operator() (const detail::util::str_view& sv)
			{
				auto l = this->remaining(sv.size());
				memmove(&this->buf[this->len], sv.data(), l);
				this->len += l;
			}

			void operator() (char c, size_t n)
			{
				for(size_t i = 0; i < this->remaining(n); i++)
					this->buf[this->len++] = c;
			}

			void operator() (const char* begin, const char* end)
			{
				(*this)(detail::util::str_view(begin, end - begin));
			}

			void operator() (const char* begin, size_t len)
			{
				(*this)(detail::util::str_view(begin, len));
			}

			buffer_appender(buffer_appender&&) = delete;
			buffer_appender(const buffer_appender&) = delete;
			buffer_appender& operator= (buffer_appender&&) = delete;
			buffer_appender& operator= (const buffer_appender&) = delete;

			size_t size() { return this->len; }

		private:
			size_t remaining(size_t n) { return detail::util::min(this->cap - this->len, n); }

			char* buf = 0;
			size_t cap = 0;
			size_t len = 0;
		};

	#if ZPR_USE_STD
		struct string_appender
		{
			string_appender(std::string& buf) : buf(buf) { }

			void operator() (char c) { this->buf += c; }
			void operator() (const detail::util::str_view& sv) { this->buf += std::string_view(sv.data(), sv.size()); }
			void operator() (char c, size_t n) { this->buf.resize(this->buf.size() + n, c); }
			void operator() (const char* begin, const char* end) { this->buf.append(begin, end); }
			void operator() (const char* begin, size_t len) { this->buf.append(begin, begin + len); }

			string_appender(string_appender&&) = delete;
			string_appender(const string_appender&) = delete;
			string_appender& operator= (string_appender&&) = delete;
			string_appender& operator= (const string_appender&) = delete;

		private:
			std::string& buf;
		};
	#endif

		constexpr size_t STDIO_BUFFER_SIZE = 256;
	}




	template <size_t fmt_N, typename... Args>
	size_t print(const char (&fmt)[fmt_N], Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, false>(stdout, ret),
			fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return ret;
	}

	template <size_t fmt_N, typename... Args>
	size_t println(const char (&fmt)[fmt_N], Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, true>(stdout, ret),
			fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return ret;
	}


	template <size_t fmt_N, typename... Args>
	size_t fprint(FILE* file, const char (&fmt)[fmt_N], Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, false>(file, ret),
			fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return ret;
	}

	template <size_t fmt_N, typename... Args>
	size_t fprintln(FILE* file, const char (&fmt)[fmt_N], Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, true>(file, ret),
			fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return ret;
	}




	// overloads taking str_view fmt instead of const char* fmt
	template <typename... Args>
	size_t print(const detail::util::str_view& fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, false>(stdout, ret),
			fmt.data(), fmt.size(), detail::util::forward_as_tuple(args...));

		return ret;
	}

	template <typename... Args>
	size_t println(const detail::util::str_view& fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, true>(stdout, ret),
			fmt.data(), fmt.size(), detail::util::forward_as_tuple(args...));

		return ret;
	}


	template <typename... Args>
	size_t fprint(FILE* file, const detail::util::str_view& fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, false>(file, ret),
			fmt.data(), fmt.size(), detail::util::forward_as_tuple(args...));

		return ret;
	}

	template <typename... Args>
	size_t fprintln(FILE* file, const detail::util::str_view& fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::file_appender<detail::STDIO_BUFFER_SIZE, true>(file, ret),
			fmt.data(), fmt.size(), detail::util::forward_as_tuple(args...));

		return ret;
	}




	template <size_t fmt_N, typename... Args>
	size_t sprint(char* buf, size_t len, const char (&fmt)[fmt_N], Args&&... args)
	{
		auto appender = detail::buffer_appender(buf, len);
		detail::print(appender, fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return appender.size();
	}

	template <typename... Args>
	size_t sprint(char* buf, size_t len, const detail::util::str_view& fmt, Args&&... args)
	{
		auto appender = detail::buffer_appender(buf, len);
		detail::print(appender, fmt.data(), fmt.size(), detail::util::forward_as_tuple(args...));

		return appender.size();
	}

















	// formatters lie here.

	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, signed char> ||
		detail::util::is_same_v<T, unsigned char> ||
		detail::util::is_same_v<T, signed short> ||
		detail::util::is_same_v<T, unsigned short> ||
		detail::util::is_same_v<T, signed int> ||
		detail::util::is_same_v<T, unsigned int> ||
		detail::util::is_same_v<T, signed long> ||
		detail::util::is_same_v<T, unsigned long> ||
		detail::util::is_same_v<T, signed long long> ||
		detail::util::is_same_v<T, unsigned long long>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			int base = 10;
			if((args.specifier | 0x20) == 'x')  base = 16;
			else if(args.specifier == 'b')      base = 2;
			else if(args.specifier == 'p')
			{
				base = 16;
				args.specifier = 'x';
				args.flags |= FMT_FLAG_ALTERNATE;
			}

			// if we print base 2 we need 64 digits!
			char digits[65] = { 0 };
			int64_t digits_len = 0;

			{
				detail::__buffer_thingy buf;
				if constexpr (detail::util::is_unsigned<T>::value)
				{
					buf = detail::print_integer(x, base);
				}
				else
				{
					if(base == 16)
					{
						buf = detail::print_integer(static_cast<detail::util::make_unsigned_t<T>>(x), base);
					}
					else
					{
						auto abs_val = detail::util::abs(x);
						buf = detail::print_integer(abs_val, base);
					}
				}

				memcpy(digits, buf.buf, buf.len);
				digits_len = buf.len;

				if(isupper(args.specifier))
					for(size_t i = 0; i < digits_len; i++)
						digits[i] = static_cast<char>(toupper(digits[i]));
			}

			char prefix[4] = { 0 };
			int64_t prefix_len = 0;
			int64_t prefix_digits_length = 0;
			{
				char* pf = prefix;
				if(args.prepend_plus())
					prefix_len++, *pf++ = '+';

				else if(args.prepend_space())
					prefix_len++, *pf++ = ' ';

				else if(x < 0 && base == 10)
					prefix_len++, *pf++ = '-';

				if(base != 10 && args.alternate())
				{
					*pf++ = '0';
					*pf++ = (ZPR_HEX_0X_RESPECTS_UPPERCASE ? args.specifier : (args.specifier | 0x20));

					prefix_digits_length += 2;
					prefix_len += 2;
				}
			}

			int64_t output_length_with_precision = (args.have_precision()
				? detail::util::max(args.precision, digits_len)
				: digits_len
			);

			int64_t total_digits_length = prefix_digits_length + digits_len;
			int64_t normal_length = prefix_len + digits_len;
			int64_t length_with_precision = prefix_len + output_length_with_precision;

			bool use_precision = args.have_precision();
			bool use_zero_pad  = args.zero_pad() && args.positive_width() && !use_precision;
			bool use_left_pad  = !use_zero_pad && args.positive_width();
			bool use_right_pad = !use_zero_pad && args.negative_width();

			int64_t padding_width = args.width - length_with_precision;
			int64_t zeropad_width = args.width - normal_length;
			int64_t precpad_width = args.precision - total_digits_length;

			if(padding_width <= 0) { use_left_pad = false; use_right_pad = false; }
			if(zeropad_width <= 0) { use_zero_pad = false; }
			if(precpad_width <= 0) { use_precision = false; }

			// pre-prefix
			if(use_left_pad) cb(' ', padding_width);

			cb(prefix, prefix_len);

			// post-prefix
			if(use_zero_pad) cb('0', zeropad_width);

			// prec-string
			if(use_precision) cb('0', precpad_width);

			cb(digits, digits_len);

			// postfix
			if(use_right_pad) cb(' ', padding_width);
		}
	};


	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_enum_v<T>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			using underlying = detail::util::underlying_type_t<T>;
			print_formatter<underlying>().print(static_cast<underlying>(x), cb, detail::util::move(args));
		}
	};



	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, float> ||
		detail::util::is_same_v<T, double> ||
		detail::util::is_same_v<T, long double>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			if(args.specifier == 'e' || args.specifier == 'E')
				print_exponent(cb, x, detail::util::move(args));

			else
				print_floating(cb, x, detail::util::move(args));
		}
	};



	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, char*> ||
		detail::util::is_same_v<T, const char*>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			detail::print_string(cb, x, strlen(x), detail::util::move(args));
		}
	};


	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::conjunction<detail::is_iterable<T>, detail::util::is_same<typename T::value_type, char>>::value
	)>::type>
	{
		template <typename CallbackFn>
		void print(const T& x, CallbackFn&& cb, format_args args)
		{
			detail::print_string(cb, x.data(), x.size(), detail::util::move(args));
		}
	};

	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, char>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			cb(x);
		}
	};

	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, bool>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			cb(x ? "true" : "false");
		}
	};

	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::is_same_v<T, void*> ||
		detail::util::is_same_v<T, const void*>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			args.specifier = 'p';
			print_formatter<uintptr_t>().print(reinterpret_cast<uintptr_t>(x), cb, detail::util::move(args));
		}
	};

	// exclude strings and string_views
	template <typename T>
	struct print_formatter<T, typename detail::util::enable_if<(
		detail::util::conjunction<detail::is_iterable<T>, detail::util::negation<detail::util::is_same<typename T::value_type, char>>>::value
	)>::type>
	{
		template <typename CallbackFn>
		void print(const T& x, CallbackFn&& cb, format_args args)
		{
			if(begin(x) == end(x))
			{
				cb("[ ]");
				return;
			}

			cb("[");
			for(auto it = begin(x);;)
			{
				detail::print(cb, "{}", detail::util::forward_as_tuple(*it));
				++it;

				if(it != end(x)) cb(", ");
				else             break;
			}

			cb("]");
		}
	};

#if ZPR_USE_STD

	template <size_t fmt_N, typename... Args>
	std::string sprint(const char (&fmt)[fmt_N], Args&&... args)
	{
		std::string buf;
		detail::print(detail::string_appender(buf),
			fmt, fmt_N, detail::util::forward_as_tuple(args...));

		return buf;
	}

	template <typename... Args>
	std::string sprint(const detail::util::str_view& fmt, Args&&... args)
	{
		std::string buf;
		detail::print(detail::string_appender(buf), fmt.data(), fmt.size(),
			detail::util::forward_as_tuple(args...));

		return buf;
	}

	template <typename A, typename B>
	struct print_formatter<std::pair<A, B>>
	{
		template <typename CallbackFn>
		void print(const std::pair<A, B>& x, CallbackFn&& cb, format_args args)
		{
			cb("{ ");
			detail::print(cb, "{}", detail::util::forward_as_tuple(x.first));
			cb(", ");
			detail::print(cb, "{}", detail::util::forward_as_tuple(x.second));
			cb(" }");
		}
	};
#endif
}
