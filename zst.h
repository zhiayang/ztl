/*
	zpr.h
	Copyright 2020 - 2021, zhiayang

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	Version 1.0.0
	=============



	Documentation
	=============

	Just a collection of various common utility classes/functions that aren't big enough
	to justify their own header library.

	Macros:

	- ZST_USE_STD
		this is *TRUE* by default. controls whether or not STL type interfaces are used; with it,
		str_view gains implicit constructors accepting std::string and std::string_view, as well
		as methods to convert to them (sv() and str()).

	- ZST_FREESTANDING
		this is *FALSE by default; controls whether or not a standard library implementation is
		available. if not, then memset(), memmove(), strlen(), and strncmp() are forward declared according to the
		C library specifications, but not defined.

	Note that ZST_FREESTANDING implies ZST_USE_STD = 0.


	Version History
	===============

	1.0.0 - 24/05/2021
	------------------
	Initial Release
*/

#pragma once

#define ZST_DO_EXPAND(VAL)  VAL ## 1
#define ZST_EXPAND(VAL)     ZST_DO_EXPAND(VAL)

#if !defined(ZST_USE_STD)
	#define ZST_USE_STD 1
#elif (ZST_EXPAND(ZST_USE_STD) == 1)
	#undef ZST_USE_STD
	#define ZST_USE_STD 1
#endif

#if !defined(ZST_FREESTANDING)
	#define ZST_FREESTANDING 0
#elif (ZST_EXPAND(ZST_FREESTANDING) == 1)
	#undef ZST_FREESTANDING
	#define ZST_FREESTANDING 1
#endif

#if !ZST_FREESTANDING
	#include <cstring>
#else
	#undef ZST_USE_STD
	#define ZST_USE_STD 0

	extern "C" void* memset(void* s, int c, size_t n);
	extern "C" void* memmove(void* dest, const void* src, size_t n);

	extern "C" size_t strlen(const char* s);
	extern "C" int strncmp(const char* s1, const char* s2, size_t n);
#endif

#if !ZST_FREESTANDING && ZST_USE_STD
	#include <string>
	#include <string_view>
#endif

#undef ZST_DO_EXPAND
#undef ZST_EXPAND


// forward declare the zpr namespace...
namespace zpr { }

namespace zst
{
	// we need a forward-declaration of error_and_exit for expect().
	template <typename... Args>
	[[noreturn]] void error_and_exit(const char* fmt, Args&&... args);

	// we really just need a very small subset of <type_traits>, so don't include the whole thing.
	namespace detail
	{
		template <typename T> T min(T a, T b) { return a < b ? a : b;}

		template <typename, typename>   struct is_same { static constexpr bool value = false; };
		template <typename T>           struct is_same<T, T> { static constexpr bool value = true; };


		template <bool B, typename T = void>    struct enable_if { };
		template <typename T>                   struct enable_if<true, T> { using type = T; };

		struct true_type  { static constexpr bool value = true; };
		struct false_type { static constexpr bool value = false; };

		template <typename T, T v>
		struct integral_constant
		{
			static constexpr T value = v;
			using value_type = T;
			using type = integral_constant; // using injected-class-name
			constexpr operator value_type() const noexcept { return value; }
			constexpr value_type operator()() const noexcept { return value; }
		};

		template <typename B> true_type  test_pre_ptr_convertible(const volatile B*);
		template <typename>   false_type test_pre_ptr_convertible(const volatile void*);

		template <typename, typename>
		auto test_pre_is_base_of(...) -> true_type;

		template <typename B, typename D>
		auto test_pre_is_base_of(int) -> decltype(test_pre_ptr_convertible<B>(static_cast<D*>(nullptr)));

		template <typename Base, typename Derived>
		struct is_base_of : integral_constant<bool, __is_class(Base) && __is_class(Derived)
			&& decltype(test_pre_is_base_of<Base, Derived>(0))::value
		>
		{ };
	}
}


namespace zst
{
	struct str_view
	{
		using value_type = char;

		str_view() : ptr(nullptr), len(0) { }
		str_view(const char* p, size_t l) : ptr(p), len(l) { }

		template <size_t N>
		str_view(const char (&s)[N]) : ptr(s), len(N - 1) { }

		template <typename T, typename = typename detail::enable_if<detail::is_same<const char*, T>::value>::type>
		str_view(T s) : ptr(s), len(strlen(s)) { }

		str_view(str_view&&) = default;
		str_view(const str_view&) = default;
		str_view& operator= (str_view&&) = default;
		str_view& operator= (const str_view&) = default;

		inline bool operator== (const str_view& other) const
		{
			return (this->len == other.len) &&
				(this->ptr == other.ptr || (strncmp(this->ptr, other.ptr, detail::min(this->len, other.len)) == 0)
			);
		}

		inline bool operator!= (const str_view& other) const
		{
			return !(*this == other);
		}

		inline const char* begin() const { return this->ptr; }
		inline const char* end() const { return this->ptr + len; }

		inline size_t length() const { return this->len; }
		inline size_t size() const { return this->len; }
		inline bool empty() const { return this->len == 0; }
		inline const char* data() const { return this->ptr; }

		char front() const { return this->ptr[0]; }
		char back() const { return this->ptr[this->len - 1]; }

		inline void clear() { this->ptr = 0; this->len = 0; }

		inline char operator[] (size_t n) { return this->ptr[n]; }

		inline size_t find(char c) const { return this->find(str_view(&c, 1)); }
		inline size_t find(str_view sv) const
		{
			if(sv.size() > this->size())
				return -1;

			else if(sv.empty())
				return 0;

			for(size_t i = 0; i < 1 + this->size() - sv.size(); i++)
			{
				if(this->drop(i).take(sv.size()) == sv)
					return i;
			}

			return -1;
		}

		inline size_t rfind(char c) const { return this->rfind(str_view(&c, 1)); }
		inline size_t rfind(str_view sv) const
		{
			if(sv.size() > this->size())
				return -1;

			else if(sv.empty())
				return this->size() - 1;

			for(size_t i = 1 + this->size() - sv.size(); i-- > 0;)
			{
				if(this->take(1 + i).take_last(sv.size()) == sv)
					return i;
			}

			return -1;
		}

		inline size_t find_first_of(str_view sv)
		{
			for(size_t i = 0; i < this->len; i++)
			{
				for(char k : sv)
				{
					if(this->ptr[i] == k)
						return i;
				}
			}

			return -1;
		}

		inline str_view drop(size_t n) const { return (this->size() >= n ? this->substr(n, this->size() - n) : ""); }
		inline str_view take(size_t n) const { return (this->size() >= n ? this->substr(0, n) : *this); }
		inline str_view take_last(size_t n) const { return (this->size() >= n ? this->substr(this->size() - n, n) : *this); }
		inline str_view drop_last(size_t n) const { return (this->size() >= n ? this->substr(0, this->size() - n) : *this); }

		inline str_view substr(size_t pos = 0, size_t cnt = -1) const
		{
			return str_view(this->ptr + pos, cnt == static_cast<size_t>(-1)
				? (this->len > pos ? this->len - pos : 0)
				: cnt);
		}

		inline str_view& remove_prefix(size_t n) { return (*this = this->drop(n)); }
		inline str_view& remove_suffix(size_t n) { return (*this = this->drop_last(n)); }

	#if ZST_USE_STD
		str_view(const std::string& str) : ptr(str.data()), len(str.size()) { }
		str_view(const std::string_view& sv) : ptr(sv.data()), len(sv.size()) { }

		inline std::string_view sv() const  { return std::string_view(this->data(), this->size()); }
		inline std::string str() const      { return std::string(this->data(), this->size()); }
	#endif // ZST_USE_STD

	private:
		const char* ptr;
		size_t len;
	};

	#if ZST_USE_STD
		inline bool operator== (const std::string& other, str_view sv) { return sv == str_view(other); }
		inline bool operator!= (std::string_view other, str_view sv) { return sv != str_view(other); }
	#endif // ZST_USE_STD


	inline const char* begin(const str_view& sv) { return sv.begin(); }
	inline const char* end(const str_view& sv) { return sv.end(); }
}


namespace zst
{
	template <typename T>
	struct Ok
	{
		Ok() = default;
		~Ok() = default;

		Ok(Ok&&) = delete;
		Ok(const Ok&) = delete;

		Ok(const T& value) : m_value(value) { }
		Ok(T&& value) : m_value(static_cast<T&&>(value)) { }

		template <typename... Args>
		Ok(Args&&... args) : m_value(static_cast<Args&&>(args)...) { }

		template <typename, typename> friend struct Result;

	private:
		T m_value;
	};

	template <>
	struct Ok<void>
	{
		Ok() = default;
		~Ok() = default;

		Ok(Ok&&) = delete;
		Ok(const Ok&) = delete;

		template <typename, typename> friend struct Result;
		template <typename, typename> friend struct zpr::print_formatter;
	};

	// deduction guide
	Ok() -> Ok<void>;


	template <typename E>
	struct Err
	{
		Err() = default;
		~Err() = default;

		Err(Err&&) = delete;
		Err(const Err&) = delete;

		Err(const E& error) : m_error(error) { }
		Err(E&& error) : m_error(static_cast<E&&>(error)) { }

		template <typename... Args>
		Err(Args&&... args) : m_error(static_cast<Args&&>(args)...) { }

		template <typename, typename> friend struct Result;
		template <typename, typename> friend struct zpr::print_formatter;

	private:
		E m_error;
	};

	template <typename T, typename E>
	struct Result
	{
	private:
		static constexpr int STATE_NONE = 0;
		static constexpr int STATE_VAL  = 1;
		static constexpr int STATE_ERR  = 2;

		struct tag_ok { };
		struct tag_err { };

		Result(tag_ok _, const T& x)  : state(STATE_VAL), val(x) { (void) _; }
		Result(tag_ok _, T&& x)       : state(STATE_VAL), val(static_cast<T&&>(x)) { (void) _; }

		Result(tag_err _, const E& e) : state(STATE_ERR), err(e) { (void) _; }
		Result(tag_err _, E&& e)      : state(STATE_ERR), err(static_cast<E&&>(e)) { (void) _; }

	public:
		using value_type = T;
		using error_type = E;

		~Result()
		{
			if(state == STATE_VAL) this->val.~T();
			if(state == STATE_ERR) this->err.~E();
		}

		template <typename T1 = T>
		Result(Ok<T1>&& ok) : Result(tag_ok(), static_cast<T1&&>(ok.m_value)) { }

		template <typename E1 = E>
		Result(Err<E1>&& err) : Result(tag_err(), static_cast<E1&&>(err.m_error)) { }

		Result(const Result& other)
		{
			this->state = other.state;
			if(this->state == STATE_VAL) new(&this->val) T(other.val);
			if(this->state == STATE_ERR) new(&this->err) E(other.err);
		}

		Result(Result&& other)
		{
			this->state = other.state;
			other.state = STATE_NONE;

			if(this->state == STATE_VAL) new(&this->val) T(static_cast<T&&>(other.val));
			if(this->state == STATE_ERR) new(&this->err) E(static_cast<E&&>(other.err));
		}

		Result& operator=(const Result& other)
		{
			if(this != &other)
			{
				if(this->state == STATE_VAL) this->val.~T();
				if(this->state == STATE_ERR) this->err.~E();

				this->state = other.state;
				if(this->state == STATE_VAL) new(&this->val) T(other.val);
				if(this->state == STATE_ERR) new(&this->err) E(other.err);
			}
			return *this;
		}

		Result& operator=(Result&& other)
		{
			if(this != &other)
			{
				if(this->state == STATE_VAL) this->val.~T();
				if(this->state == STATE_ERR) this->err.~E();

				this->state = other.state;
				other.state = STATE_NONE;

				if(this->state == STATE_VAL) new(&this->val) T(static_cast<T&&>(other.val));
				if(this->state == STATE_ERR) new(&this->err) E(static_cast<E&&>(other.err));
			}
			return *this;
		}

		T* operator -> () { this->assert_has_value(); return &this->val; }
		const T* operator -> () const { this->assert_has_value(); return &this->val; }

		T& operator* () { this->assert_has_value(); return this->val; }
		const T& operator* () const  { this->assert_has_value(); return this->val; }

		operator bool() const { return this->state == STATE_VAL; }
		bool ok() const { return this->state == STATE_VAL; }

		const T& unwrap() const { this->assert_has_value(); return this->val; }
		const E& error() const { this->assert_is_error(); return this->err; }

		T& unwrap() { this->assert_has_value(); return this->val; }
		E& error() { this->assert_is_error(); return this->err; }

		// enable implicit upcast to a base type
		template <typename U, typename = std::enable_if_t<
			std::is_pointer_v<T> && std::is_pointer_v<U>
			&& std::is_base_of_v<std::remove_pointer_t<U>, std::remove_pointer_t<T>>
		>>
		operator Result<U, E> () const
		{
			if(state == STATE_VAL)  return Result<U, E>(this->val);
			if(state == STATE_ERR)  return Result<U, E>(this->err);

			zst::error_and_exit("invalid state of Result");
		}

		const T& expect(str_view msg) const
		{
			if(this->ok())  return this->unwrap();
			else            zst::error_and_exit("{}: {}", msg, this->error());
		}

		const T& or_else(const T& default_value) const
		{
			if(this->ok())  return this->unwrap();
			else            return default_value;
		}

	private:
		inline void assert_has_value() const
		{
			if(this->state != STATE_VAL)
				zst::error_and_exit("unwrapping result of Err: {}", this->error());
		}

		inline void assert_is_error() const
		{
			if(this->state != STATE_ERR)
				zst::error_and_exit("result is not an Err");
		}


		// 0 = schrodinger -- no error, no value.
		// 1 = valid
		// 2 = error
		int state = 0;
		union {
			T val;
			E err;
		};
	};


	template <typename E>
	struct Result<void, E>
	{
	private:
		static constexpr int STATE_NONE = 0;
		static constexpr int STATE_VAL  = 1;
		static constexpr int STATE_ERR  = 2;

	public:
		using value_type = void;
		using error_type = E;

		Result() : state(STATE_VAL) { }

		Result(Ok<void>&& ok) : Result() { (void) ok; }

		template <typename E1 = E>
		Result(Err<E1>&& err) : state(STATE_ERR), err(static_cast<E1&&>(err.m_error)) { }


		Result(const Result& other)
		{
			this->state = other.state;
			if(this->state == STATE_ERR)
				this->err = other.err;
		}

		Result(Result&& other)
		{
			this->state = other.state;
			other.state = STATE_NONE;

			if(this->state == STATE_ERR)
				this->err = static_cast<E&&>(other.err);
		}

		Result& operator=(const Result& other)
		{
			if(this != &other)
			{
				this->state = other.state;
				if(this->state == STATE_ERR)
					this->err = other.err;
			}
			return *this;
		}

		Result& operator=(Result&& other)
		{
			if(this != &other)
			{
				this->state = other.state;
				other.state = STATE_NONE;

				if(this->state == STATE_ERR)
					this->err = static_cast<E&&>(other.err);
			}
			return *this;
		}

		operator bool() const { return this->state == STATE_VAL; }
		bool ok() const { return this->state == STATE_VAL; }

		const E& error() const { this->assert_is_error(); return this->err; }
		E& error() { this->assert_is_error(); return this->err; }

		void expect(zst::str_view msg) const
		{
			if(!this->ok())
				zst::error_and_exit("{}: {}", msg, this->error());
		}

		static Result of_value()
		{
			return Result<void, E>();
		}

		template <typename E1 = E>
		static Result of_error(E1&& err)
		{
			return Result<void, E>(E(static_cast<E1&&>(err)));
		}

		template <typename... Args>
		static Result of_error(Args&&... xs)
		{
			return Result<void, E>(E(static_cast<Args&&>(xs)...));
		}

	private:
		inline void assert_is_error() const
		{
			if(this->state != STATE_ERR)
				zst::error_and_exit("result is not an Err");
		}

		int state = 0;
		E err;
	};
}




// we rely on zpr.h defining some macros that "leak" globally. zpr.h always "force-defines" these macros,
// so we can be sure that if they are defined, then zpr.h is included.

#if defined(ZPR_USE_STD) || defined(ZPR_FREESTANDING)

namespace zpr
{
	// make sure that these are only defined if print_formatter is a complete type.
	template <typename T>
	struct print_formatter<zst::Ok<T>, typename zst::detail::enable_if<detail::has_formatter_v<T>>::type>
	{
		template <typename Cb>
		void print(const zst::Ok<T>& ok, Cb&& cb, format_args args)
		{
			cb("Ok(");
			detail::print_one(cb, static_cast<format_args&&>(args), ok.m_value);
			cb(")");
		}
	};

	template <typename E>
	struct print_formatter<zst::Err<E>, typename zst::detail::enable_if<detail::has_formatter_v<E>>::type>
	{
		template <typename Cb>
		void print(const zst::Err<E>& err, Cb&& cb, format_args args)
		{
			cb("Err(");
			detail::print_one(cb, static_cast<format_args&&>(args), err.m_error);
			cb(")");
		}
	};

	template <typename E, typename T>
	struct print_formatter<zst::Result<T, E>,
		typename zst::detail::enable_if<detail::has_formatter_v<T> && detail::has_formatter_v<E>>::type
	>
	{
		template <typename Cb>
		void print(const zst::Result<T, E>& res, Cb&& cb, format_args args)
		{
			if(res.ok())
			{
				cb("Ok(");
				detail::print_one(cb, static_cast<format_args&&>(args), res.unwrap());
				cb(")");
			}
			else
			{
				cb("Err(");
				detail::print_one(cb, static_cast<format_args&&>(args), res.error());
				cb(")");
			}
		}
	};
}
#endif
