/*
	zpr.h
	Copyright 2020 zhiayang

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
	- integral types (signed/unsigned char/short/int/long/long long) (but not 'char')
	- floating point types (float, double, long double)
	- strings (char*, const char*, std::string_view, std::string)
	- booleans (prints as 'true'/'false')
	- chars
	- all iterable containers (with begin(x) and end(x) available)
	- enums (will print as their integer representation)
	- std::pair
*/

#include <cmath>
#include <cstdio>

#include <string>
#include <charconv>
#include <type_traits>
#include <string_view>

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


		// forward declare these
		template <typename CallbackFn>
		void print(CallbackFn&& cb, const char* fmt);

		template <typename CallbackFn, typename... Args>
		void print(CallbackFn&& cb, const char* fmt, Args&&... args);



		template <typename CallbackFn, typename... Args>
		void consume_wp(CallbackFn&& cb, format_args fmt_args, const char* fmt, Args&&... args)
		{
			cb("<missing width and precision>");
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename W, typename P, typename Arg, typename... Args>
		void consume_wp(CallbackFn&& cb, format_args fmt_args, const char* fmt, W&& width, P&& prec, Arg&& arg, Args&&... args)
		{
			static_assert(std::is_integral_v<std::remove_reference_t<std::decay_t<W>>>);
			static_assert(std::is_integral_v<std::remove_reference_t<std::decay_t<P>>>);

			fmt_args.width = width;
			fmt_args.precision = prec;
			print_formatter<std::remove_cv_t<std::decay_t<Arg>>>().print(std::move(arg), cb, std::move(fmt_args));
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename... Args>
		void consume_w(CallbackFn&& cb, format_args fmt_args, const char* fmt, Args&&... args)
		{
			cb("<missing width>");
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename W, typename Arg, typename... Args>
		void consume_w(CallbackFn&& cb, format_args fmt_args, const char* fmt, W&& width, Arg&& arg, Args&&... args)
		{
			static_assert(std::is_integral_v<std::remove_reference_t<std::decay_t<W>>>);

			fmt_args.width = width;
			print_formatter<std::remove_cv_t<std::decay_t<Arg>>>().print(std::move(arg), cb, std::move(fmt_args));
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename... Args>
		void consume_p(CallbackFn&& cb, format_args fmt_args, const char* fmt, Args&&... args)
		{
			cb("<missing precision>");
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename P, typename Arg, typename... Args>
		void consume_p(CallbackFn&& cb, format_args fmt_args, const char* fmt, P&& prec, Arg&& arg, Args&&... args)
		{
			static_assert(std::is_integral_v<std::remove_reference_t<std::decay_t<P>>>);

			fmt_args.precision = prec;
			print_formatter<std::remove_cv_t<std::decay_t<Arg>>>().print(std::move(arg), cb, std::move(fmt_args));
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename... Args>
		void consume_0(CallbackFn&& cb, format_args fmt_args, const char* fmt, Args&&... args)
		{
			cb("<missing value>");
			return print(cb, fmt, args...);
		}

		template <typename CallbackFn, typename Arg, typename... Args>
		void consume_0(CallbackFn&& cb, format_args fmt_args, const char* fmt, Arg&& arg, Args&&... args)
		{
			print_formatter<std::remove_cv_t<std::decay_t<Arg>>>().print(std::move(arg), cb, std::move(fmt_args));
			return print(cb, fmt, args...);
		}




		template <typename CallbackFn, typename... Args>
		void print_next(std::string_view sv, CallbackFn&& cb, const char* fmt, Args&&... args)
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
					while(sv.size() > 0 && (sv[0] >= '0') && (sv[0] <= '9'))
						(fmt_args.width = 10 * fmt_args.width + (sv[0] - '0')), sv.remove_prefix(1);

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
						while(sv.size() > 0 && ('0' <= sv[0]) && (sv[0] <= '9'))
							sv.remove_prefix(1);
					}
					else
					{
						fmt_args.precision = 0;
						while(sv.size() > 0 && (sv[0] >= '0') && (sv[0] <= '9'))
							(fmt_args.precision = 10 * fmt_args.precision + (sv[0] - '0')), sv.remove_prefix(1);
					}
				}

				if(!sv.empty())
					(fmt_args.specifier = sv[0]), sv.remove_prefix(1);

			done:
				;
			}

			if(need_prec && need_width)
			{
				return consume_wp(cb, std::move(fmt_args), fmt, args...);
			}
			else if(need_width)
			{
				return consume_w(cb, std::move(fmt_args), fmt, args...);
			}
			else if(need_prec)
			{
				return consume_p(cb, std::move(fmt_args), fmt, args...);
			}
			else
			{
				return consume_0(cb, std::move(fmt_args), fmt, args...);
			}
		}

		template <typename CallbackFn>
		void print(CallbackFn&& cb, const char* fmt)
		{
			cb(fmt);
		}

		template <typename CallbackFn, typename... Args>
		void print(CallbackFn&& cb, const char* fmt, Args&&... args)
		{
			auto beg = fmt;
			auto end = fmt;

			while(end && *end)
			{
				if(*end == '{')
				{
					auto tmp = end;

					// flush whatever we have first:
					cb(std::string_view(beg, end - beg));
					if(end[1] == '{')
					{
						cb("{");
						end += 2;
						continue;
					}

					std::string_view fmt_str;
					while(end[0] && end[0] != '}')
						end++;

					// owo
					if(!end[0])
						return;

					end++;

					return print_next(std::string_view(tmp, end - tmp), cb, end, args...);
				}
				else if(*end == '}')
				{
					// well... we don't need to escape }, but for consistency, we accept either } or }} to print one }.
					if(end[1] == '}')
						end++;

					end++;
					cb("}");
				}
				else
				{
					end++;
				}
			}
		}
	}

	template <typename... Args>
	std::string sprint(const char* fmt, Args&&... args)
	{
		std::string str;

		detail::print([&str](std::string_view sv) {
			str += sv;
		}, fmt, args...);

		return str;
	}





	template <typename... Args>
	size_t print(const char* fmt, Args&&... args)
	{
		size_t n = 0;
		detail::print([&n](std::string_view sv) {
			n += printf("%.*s", (int) sv.size(), sv.data());
		}, fmt, args...);

		return n;
	}

	template <typename... Args>
	size_t println(const char* fmt, Args&&... args)
	{
		size_t n = 0;
		detail::print([&n](std::string_view sv) {
			n += printf("%.*s", (int) sv.size(), sv.data());
		}, fmt, args...);

		printf("\n");
		return n + 1;
	}


	template <typename... Args>
	size_t fprint(FILE* file, const char* fmt, Args&&... args)
	{
		size_t n = 0;
		detail::print([&n, file](std::string_view sv) {
			n += fprintf(file, "%.*s", (int) sv.size(), sv.data());
		}, fmt, args...);

		return n;
	}

	template <typename... Args>
	size_t fprintln(FILE* file, const char* fmt, Args&&... args)
	{
		size_t n = 0;
		detail::print([&n, file](std::string_view sv) {
			n += fprintf(file, "%.*s", (int) sv.size(), sv.data());
		}, fmt, args...);

		n += fprintf(file, "\n");
		return n;
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
			if(args.specifier == 'x' || args.specifier == 'X')      base = 16;
			else if(args.specifier == 'o')                          base = 8;
			else if(args.specifier == 'b')                          base = 2;

			std::string digits;
			{
				// if we print base 2 we need 64 digits!
				char buf[65] = {0};

				size_t digits_len = 0;
				auto spec = args.specifier;

				std::to_chars_result ret;
				if constexpr (std::is_enum_v<T>)
					ret = std::to_chars(&buf[0], &buf[65], static_cast<std::underlying_type_t<T>>(x), /* base: */ base);

				else
					ret = std::to_chars(&buf[0], &buf[65], x, /* base: */ base);

				if(ret.ec == std::errc())   digits_len = (ret.ptr - &buf[0]), *ret.ptr = 0;
				else                        return cb("<to_chars(int) error>");

				if(isupper(args.specifier))
					for(size_t i = 0; i < digits_len; i++)
						buf[i] = static_cast<char>(toupper(buf[i]));

				digits = std::string(buf, digits_len);
			}

			std::string prefix;

			if(args.prepend_plus_if_positive)       prefix += "+";
			else if(args.prepend_blank_if_positive) prefix += " ";

			// prepend 0x or 0b or 0o for alternate.
			int64_t prefix_digits_length = 0;
			if((base == 2 || base == 8 || base == 16) && args.alternate)
			{
				prefix += "0";
				#if HEX_0X_RESPECTS_UPPERCASE
					prefix += args.specifier;
				#else
					prefix += tolower(args.specifier);
				#endif
				prefix_digits_length += 2;
			}

			int64_t output_length_with_precision = (args.precision == -1
				? digits.size()
				: std::max(args.precision, static_cast<int64_t>(digits.size()))
			);

			int64_t digits_length = prefix_digits_length + digits.size();
			int64_t normal_length = prefix.size() + digits.size();
			int64_t length_with_precision = prefix.size() + output_length_with_precision;

			bool use_precision = (args.precision != -1);
			bool use_zero_pad = args.zero_pad && 0 <= args.width && !use_precision;
			bool use_left_pad = !use_zero_pad && 0 <= args.width;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			int64_t abs_field_width = std::abs(args.width);

			std::string pre_prefix;
			if(use_left_pad)
				pre_prefix = std::string(std::max(int64_t(0), abs_field_width - length_with_precision), ' ');

			std::string post_prefix;
			if(use_zero_pad)
				post_prefix = std::string(std::max(int64_t(0), abs_field_width - normal_length), '0');

			std::string prec_string;
			if(use_precision)
				prec_string = std::string(std::max(int64_t(0), args.precision - digits_length), '0');

			std::string postfix;
			if(use_right_pad)
				postfix = std::string(std::max(int64_t(0), abs_field_width - length_with_precision), ' ');

			cb(pre_prefix);
			cb(prefix);
			cb(post_prefix);
			cb(prec_string);
			cb(digits);
			cb(postfix);
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

			char buf[81] = { 0 };
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

				num_length = snprintf(&buf[0], 80, fmt_str, (int) (args.precision == -1 ? default_prec : args.precision),
					std::fabs(x));
			}

			auto abs_field_width = std::abs(args.width);

			bool use_zero_pad = args.zero_pad && args.width >= 0;
			bool use_left_pad = !use_zero_pad && args.width >= 0;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			// account for the signs, if any.
			if(x < 0 || args.prepend_plus_if_positive || args.prepend_blank_if_positive)
				num_length += 1;

			std::string pre_prefix;
			if(use_left_pad)
				pre_prefix = std::string(std::max(int64_t(0), abs_field_width - num_length), ' ');

			std::string prefix;
			if(x < 0)                               prefix = "-";
			else if(args.prepend_plus_if_positive)  prefix = "+";
			else if(args.prepend_blank_if_positive) prefix = " ";

			std::string post_prefix;
			if(use_zero_pad)
				post_prefix = std::string(std::max(int64_t(0), abs_field_width - num_length), '0');

			std::string postfix;
			if(use_right_pad)
				postfix = std::string(std::max(int64_t(0), abs_field_width - num_length), ' ');

			cb(pre_prefix);
			cb(prefix);
			cb(post_prefix);
			cb(buf);
			cb(postfix);
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

			for(int64_t i = 0; (args.precision != -1 ? (i < args.precision && x && x[i]) : (x && x[i])); i++)
				string_length++;

			if(args.width >= 0 && string_length < abs_field_width)
				for(int64_t i = 0; i < abs_field_width - string_length; i++)
					cb(" ");

			cb(std::string_view(x, string_length));

			if(args.width < 0 && string_length < abs_field_width)
				for(int64_t i = 0; i < abs_field_width - string_length; i++)
					cb(" ");
		}
	};


	template <typename T>
	struct print_formatter<T, typename std::enable_if<(
		std::is_same_v<T, std::string> ||
		std::is_same_v<T, std::string_view>
	)>::type>
	{
		template <typename CallbackFn>
		void print(T x, CallbackFn&& cb, format_args args)
		{
			int64_t string_length = 0;
			int64_t abs_field_width = std::abs(args.width);

			if(args.precision >= 0) string_length = std::min(args.precision, static_cast<int64_t>(x.size()));
			else                    string_length = static_cast<int64_t>(x.size());

			if(args.width >= 0 && string_length < abs_field_width)
				for(int64_t i = 0; i < abs_field_width - string_length; i++)
					cb(" ");

			cb(std::string_view(x.c_str(), string_length));

			if(args.width < 0 && string_length < abs_field_width)
				for(int64_t i = 0; i < abs_field_width - string_length; i++)
					cb(" ");
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
			cb(std::string_view(&x, 1));
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
			print_formatter<uintptr_t>().print(x, cb, std::move(args));
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
				detail::consume_0(cb, args, "", *it);
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
			detail::consume_0(cb, args, "", x.first);
			cb(", ");
			detail::consume_0(cb, args, "", x.second);
			cb(" }");
		}
	};
}
