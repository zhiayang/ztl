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
*/

/*
	Version History
	---------------
	1.0.0 - 19/07/2020
	Initial release.

	1.1.0 - 20/07/2020
	Performance improvements: switched to tuple-based arguments instead of parameter-packed recursion. We are now (slightly)
	faster than printf (as least on two of my systems) as long as no floating-point is involved. (for now we are still forced
	to call snprintf to print floats... charconv pls)

	Bug fixes: fixed broken escaping of {{ and }}.


	Documentation
	-------------
	This printing library functions as a lightweight alternative to std::format in C++20 (or fmtlib), with the following (non)features:

	1. no exceptions
	2. short and simple implementation without tons of templates
	3. no support for positional arguments

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
	- strings                   (char*, const char*, std::string_view, std::string)
	- booleans                  (prints as 'true'/'false')
	- chars
	- all iterable containers   (with begin(x) and end(x) available)
	- enums                     (will print as their integer representation)
	- std::pair
	- void*, const void*        (prints with %p)
*/

#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include <string>
#include <charconv>
#include <type_traits>
#include <string_view>

#ifndef HEX_0X_RESPECTS_UPPERCASE
	#define HEX_0X_RESPECTS_UPPERCASE 0
#endif

namespace zpr
{
	struct format_args
	{
		bool zero_pad = false;
		bool alternate = false;
		bool prepend_plus_if_positive = false;
		bool prepend_blank_if_positive = false;

		char specifier      = -1;
		int64_t width       = -1;
		int64_t length      = -1;
		int64_t precision   = -1;
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
		using std::begin;
		using std::end;

		template <typename T, typename = void>
		struct is_iterable : std::false_type { };

		template <typename T>
		struct is_iterable<T, std::void_t<
			decltype(begin(std::declval<T&>())),
			decltype(end(std::declval<T&>()))
		>> : std::true_type { };

		static inline std::tuple<format_args, bool, bool> parse_fmt_spec(std::string_view sv)
		{
			// remove the first and last (they are { and })
			sv = sv.substr(1, sv.size() - 2);

			bool need_prec = false;
			bool need_width = false;
			format_args fmt_args = { };
			{
				bool negative_width = false;
				while(sv.size() > 0)
				{
					switch(sv[0])
					{
						case '0':   fmt_args.zero_pad = true; sv.remove_prefix(1); continue;
						case '#':   fmt_args.alternate = true; sv.remove_prefix(1); continue;
						case '-':   negative_width = true; sv.remove_prefix(1); continue;
						case '+':   fmt_args.prepend_plus_if_positive = true; sv.remove_prefix(1); continue;
						case ' ':   fmt_args.prepend_blank_if_positive = true; sv.remove_prefix(1); continue;
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
				}
				else
				{
					size_t k = 0;
					while(sv.size() > k && (sv[k] >= '0') && (sv[k] <= '9'))
						(fmt_args.width = 10 * fmt_args.width + (sv[k] - '0')), k++;

					sv.remove_prefix(k);
					if(negative_width)
						fmt_args.width *= -1;
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
					}
					else if(sv.size() > 0 && sv[0] == '-')
					{
						// just ignore negative precision i guess.
						size_t k = 1;
						while(sv.size() > k && ('0' <= sv[k]) && (sv[k] <= '9'))
							k++;

						sv.remove_prefix(k);
					}
					else
					{
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
				fn(cb, std::move(fmt_args), std::move(std::get<N>(args)));
				return;
			}

			if constexpr (N + 1 < std::tuple_size_v<Tuple>)
				return visit_one<CallbackFn, Fn, Tuple, N + 1>(std::forward<CallbackFn>(cb), std::forward<Tuple>(args),
					idx, std::move(fmt_args), std::forward<Fn>(fn));
		}

		template <typename CallbackFn, typename Fn, typename Tuple, size_t N = 0>
		void visit_two(CallbackFn&& cb, Tuple&& args, size_t idx, format_args fmt_args, Fn&& fn)
		{
			if(N == idx)
			{
				fn(cb, std::move(fmt_args),
					std::move(std::get<N>(args)),
					std::move(std::get<N + 1>(args))
				);
				return;
			}

			if constexpr (N + 2 < std::tuple_size_v<Tuple>)
				return visit_two<CallbackFn, Fn, Tuple, N + 1>(std::forward<CallbackFn>(cb), std::forward<Tuple>(args),
					idx, std::move(fmt_args), std::forward<Fn>(fn));
		}

		template <typename CallbackFn, typename Fn, typename Tuple, size_t N = 0>
		void visit_three(CallbackFn&& cb, Tuple&& args, size_t idx, format_args fmt_args, Fn&& fn)
		{
			if(N == idx)
			{
				fn(cb, std::move(fmt_args),
					std::move(std::get<N>(args)),
					std::move(std::get<N + 1>(args)),
					std::move(std::get<N + 2>(args))
				);
				return;
			}

			if constexpr (N + 3 < std::tuple_size_v<Tuple>)
				return visit_three<CallbackFn, Fn, Tuple, N + 1>(std::forward<CallbackFn>(cb), std::forward<Tuple>(args),
					idx, std::move(fmt_args), std::forward<Fn>(fn));
		}










		template <typename CallbackFn, typename Tuple>
		void print(CallbackFn&& cb, const char* fmt, Tuple&& args)
		{
			auto beg = fmt;
			auto end = fmt;

			size_t tup_idx = 0;

			while(end && *end)
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
					auto [ fmt_spec, width, prec ] = parse_fmt_spec(std::string_view(tmp, end - tmp));

					if(width && prec)
					{
						visit_three(cb, std::forward<Tuple>(args), tup_idx, std::move(fmt_spec),
							[](auto&& cb, format_args fmt_args, auto&& width, auto&& prec, auto&& x) {
								if constexpr (std::is_integral_v<decltype(width)> && std::is_integral_v<decltype(prec)>)
								{
									fmt_args.width = width;
									fmt_args.precision = prec;
								}

								print_formatter<std::remove_cv_t<std::decay_t<decltype(x)>>>()
									.print(std::move(x), cb, std::move(fmt_args));
							});

						tup_idx += 3;
					}
					else if(width)
					{
						visit_two(cb, std::forward<Tuple>(args), tup_idx, std::move(fmt_spec),
							[](auto&& cb, format_args fmt_args, auto&& width, auto&& x) {
								if constexpr (std::is_integral_v<decltype(width)>)
									fmt_args.width = width;

								print_formatter<std::remove_cv_t<std::decay_t<decltype(x)>>>()
									.print(std::move(x), cb, std::move(fmt_args));
							});

						tup_idx += 2;
					}
					else if(prec)
					{
						visit_two(cb, std::forward<Tuple>(args), tup_idx, std::move(fmt_spec),
							[](auto&& cb, format_args fmt_args, auto&& prec, auto&& x) {
								if constexpr (std::is_integral_v<decltype(prec)>)
									fmt_args.precision = prec;

								print_formatter<std::remove_cv_t<std::decay_t<decltype(x)>>>()
									.print(std::move(x), cb, std::move(fmt_args));
							});

						tup_idx += 2;
					}
					else
					{
						visit_one(cb, std::forward<Tuple>(args), tup_idx, std::move(fmt_spec),
							[](auto&& cb, format_args fmt_args, auto&& x) {
								print_formatter<std::remove_cv_t<std::decay_t<decltype(x)>>>()
									.print(std::move(x), cb, std::move(fmt_args));
							});

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



		struct appender
		{
			appender(std::string& buf) : buf(buf) { }

			void operator() (char c) { this->buf += c; }
			void operator() (std::string_view sv) { this->buf += sv; }
			void operator() (char c, size_t n) { this->buf.resize(this->buf.size() + n, c); }
			void operator() (const char* begin, const char* end) { this->buf.append(begin, end); }
			void operator() (const char* begin, size_t len) { this->buf.append(begin, begin + len); }

			appender(appender&&) = delete;
			appender(const appender&) = delete;
			appender& operator= (appender&&) = delete;
			appender& operator= (const appender&) = delete;

		private:
			std::string& buf;
		};

		template <size_t Limit, bool Newline>
		struct appender2
		{
			appender2(FILE* fd, size_t& written) : fd(fd), written(written) { }
			~appender2() { flush(true); }

			appender2(appender2&&) = delete;
			appender2(const appender2&) = delete;
			appender2& operator= (appender2&&) = delete;
			appender2& operator= (const appender2&) = delete;

			void operator() (char c) { *ptr++ = c; flush(); }

			void operator() (std::string_view sv) { (*this)(sv.data(), sv.size()); }
			void operator() (const char* begin, const char* end) { (*this)(begin, static_cast<size_t>(end - begin)); }

			void operator() (char c, size_t n)
			{
				while(n > 0)
				{
					auto x = std::min(n, remaining());
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
					auto x = std::min(len, remaining());
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


		constexpr size_t STDIO_BUFFER_SIZE = 256;
	}




	template <typename... Args>
	std::string sprint(const char* fmt, Args&&... args)
	{
		std::string buf;
		detail::print(detail::appender(buf), fmt, std::forward<Args>(args)...);

		return buf;
	}

	template <typename... Args>
	size_t print(const char* fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::appender2<detail::STDIO_BUFFER_SIZE, false>(stdout, ret),
			fmt, std::forward<Args>(args)...);

		return ret;
	}

	template <typename... Args>
	size_t println(const char* fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::appender2<detail::STDIO_BUFFER_SIZE, true>(stdout, ret),
			fmt, std::forward<Args>(args)...);

		return ret;
	}


	template <typename... Args>
	size_t fprint(FILE* file, const char* fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::appender2<detail::STDIO_BUFFER_SIZE, false>(file, ret),
			fmt, std::forward_as_tuple(args...));

		return ret;
	}

	template <typename... Args>
	size_t fprintln(FILE* file, const char* fmt, Args&&... args)
	{
		size_t ret = 0;
		detail::print(detail::appender2<detail::STDIO_BUFFER_SIZE, true>(file, ret),
			fmt, std::forward<Args>(args)...);

		return ret;
	}



	// formatters lie here.

	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, signed char> ||
		std::is_same_v<T, unsigned char> ||
		std::is_same_v<T, signed short> ||
		std::is_same_v<T, unsigned short> ||
		std::is_same_v<T, signed int> ||
		std::is_same_v<T, unsigned int> ||
		std::is_same_v<T, signed long> ||
		std::is_same_v<T, unsigned long> ||
		std::is_same_v<T, signed long long> ||
		std::is_same_v<T, unsigned long long> ||
		std::is_enum_v<T>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			int base = 10;
			if(args.specifier == 'x' || args.specifier == 'X')  base = 16;
			else if(args.specifier == 'o')                      base = 8;
			else if(args.specifier == 'b')                      base = 2;

			// if we print base 2 we need 64 digits!
			char digits[65] = {0};
			int64_t digits_len = 0;

			{
				auto spec = args.specifier;

				std::to_chars_result ret;
				if constexpr (std::is_enum_v<T>)
					ret = std::to_chars(&digits[0], &digits[65], static_cast<std::underlying_type_t<T>>(x), /* base: */ base);

				else
					ret = std::to_chars(&digits[0], &digits[65], x, /* base: */ base);

				if(ret.ec == std::errc())   digits_len = (ret.ptr - &digits[0]), *ret.ptr = 0;
				else                        return cb("<to_chars(int) error>");

				if(isupper(args.specifier))
					for(size_t i = 0; i < digits_len; i++)
						digits[i] = static_cast<char>(toupper(digits[i]));
			}

			char prefix[4] = { 0 };
			int64_t prefix_len = 0;
			int64_t prefix_digits_length = 0;
			{
				char* pf = prefix;
				if(args.prepend_plus_if_positive)
					prefix_len++, *pf++ = '+';

				else if(args.prepend_blank_if_positive)
					prefix_len++, *pf++ = ' ';

				if(base != 10 && args.alternate)
				{
					*pf++ = '0';
					*pf++ = (HEX_0X_RESPECTS_UPPERCASE ? args.specifier : (args.specifier | 0x20));

					prefix_digits_length += 2;
					prefix_len += 2;
				}
			}

			int64_t output_length_with_precision = (args.precision == -1
				? digits_len
				: std::max(args.precision, digits_len)
			);

			int64_t total_digits_length = prefix_digits_length + digits_len;
			int64_t normal_length = prefix_len + digits_len;
			int64_t length_with_precision = prefix_len + output_length_with_precision;

			bool use_precision = (args.precision != -1);
			bool use_zero_pad = args.zero_pad && 0 <= args.width && !use_precision;
			bool use_left_pad = !use_zero_pad && 0 <= args.width;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			int64_t abs_field_width = std::abs(args.width);

			int64_t padding_width = abs_field_width - length_with_precision;
			int64_t zeropad_width = abs_field_width - normal_length;
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
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, float> ||
		std::is_same_v<T, double> ||
		std::is_same_v<T, long double>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			constexpr int default_prec = 6;

			char buf[128] = { 0 };
			int64_t num_length = 0;

			// lmao. nobody except msvc stl (and only the most recent version) implements std::to_chars
			// for floating point types, even though it's in the c++17 standard. so we just cheat.
			// let printf handle the precision, but we'll handle the width and the negativity.
			{
				const char* fmt_str = 0;
				constexpr bool ldbl = std::is_same_v<T, long double>;
				constexpr bool dbl  = std::is_same_v<T, double>;

				switch(args.specifier)
				{
					case 'E': fmt_str = (ldbl ? "%.*LE" : (dbl ? "%.*lE" : "%.*E")); break;
					case 'e': fmt_str = (ldbl ? "%.*Le" : (dbl ? "%.*le" : "%.*e")); break;
					case 'F': fmt_str = (ldbl ? "%.*LF" : (dbl ? "%.*lF" : "%.*F")); break;
					case 'f': fmt_str = (ldbl ? "%.*Lf" : (dbl ? "%.*lf" : "%.*f")); break;
					case 'G': fmt_str = (ldbl ? "%.*LG" : (dbl ? "%.*lG" : "%.*G")); break;

					case 'g': [[fallthrough]];
					default:  fmt_str = (ldbl ? "%.*Lg" : (dbl ? "%.*lg" : "%.*g")); break;
				}

				num_length = snprintf(&buf[0], 128, fmt_str, (int) (args.precision == -1 ? default_prec : args.precision),
					std::fabs(x));
			}

			auto abs_field_width = std::abs(args.width);

			bool use_zero_pad = args.zero_pad && args.width >= 0;
			bool use_left_pad = !use_zero_pad && args.width >= 0;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			// account for the signs, if any.
			if(x < 0 || args.prepend_plus_if_positive || args.prepend_blank_if_positive)
				num_length += 1;

			int64_t padding_width = abs_field_width - num_length;
			if(padding_width < 0) { use_left_pad = false; use_right_pad = false; use_zero_pad = false; }

			char prefix = 0;
			if(x < 0)                               prefix = '-';
			else if(args.prepend_plus_if_positive)  prefix = '+';
			else if(args.prepend_blank_if_positive) prefix = ' ';

			if(use_left_pad)  cb(' ', padding_width);
			if(prefix != 0)   cb(prefix);
			if(use_zero_pad)  cb('0', padding_width);

			cb(buf, num_length);

			if(use_right_pad) cb(' ', padding_width);
		}
	};



	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, char*> ||
		std::is_same_v<T, char const*> ||
		std::is_same_v<T, const char*> ||
		std::is_same_v<T, const char* const>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			int64_t string_length = 0;
			int64_t abs_field_width = std::abs(args.width);

			if(args.precision >= 0) string_length = std::min(args.precision, static_cast<int64_t>(strlen(x)));
			else                    string_length = static_cast<int64_t>(strlen(x));

			if(args.width >= 0 && string_length < abs_field_width)
				cb(' ', abs_field_width - string_length);

			cb(x, string_length);

			if(args.width < 0 && string_length < abs_field_width)
				cb(' ', abs_field_width - string_length);
		}
	};


	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, std::string> ||
		std::is_same_v<T, std::string_view>
	)>::type>
	{
		template <typename CallbackFn>
		void print(const T& x, CallbackFn&& cb, format_args args)
		{
			int64_t string_length = 0;
			int64_t abs_field_width = std::abs(args.width);

			if(args.precision >= 0) string_length = std::min(args.precision, static_cast<int64_t>(x.size()));
			else                    string_length = static_cast<int64_t>(x.size());

			if(args.width >= 0 && string_length < abs_field_width)
				cb(' ', abs_field_width - string_length);

			cb(x, string_length);

			if(args.width < 0 && string_length < abs_field_width)
				cb(' ', abs_field_width - string_length);
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, char>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			cb(x);
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, bool>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			cb(x ? "true" : "false");
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, void*> ||
		std::is_same_v<T, const void*>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			args.specifier = 'p';
			print_formatter<uintptr_t>().print(reinterpret_cast<uintptr_t>(x), cb, std::move(args));
		}
	};

	// exclude strings and string_views
	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		detail::is_iterable<T>::value &&
		!std::is_same_v<T, std::string> &&
		!std::is_same_v<T, std::string_view>
	)>::type>
	{
		template <typename CallbackFn>
		void print(const T& x, CallbackFn&& cb, format_args args)
		{
			using std::begin;
			using std::end;

			if(begin(x) == end(x))
			{
				cb("[ ]");
				return;
			}

			cb("[");
			for(auto it = begin(x);;)
			{
				detail::print(cb, "", std::forward_as_tuple(*it));
				++it;

				if(it != end(x)) cb(", ");
				else             break;
			}

			cb("]");
		}
	};

	template <typename A, typename B>
	struct print_formatter<std::pair<A, B>>
	{
		template <typename CallbackFn>
		void print(const std::pair<A, B>& x, CallbackFn&& cb, format_args args)
		{
			cb("{ ");
			detail::print(cb, "", std::forward_as_tuple(x.first));
			cb(", ");
			detail::print(cb, "", std::forward_as_tuple(x.second));
			cb(" }");
		}
	};
}
