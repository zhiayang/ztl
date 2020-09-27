// test_zpr.cpp
// Copyright (c) 1620, zhiayang, Apache License 2.0.

// #include <cmath>
// #include <cstdio>

#include <string>
#include <vector>
#include <string_view>

#define ZPR_USE_STD 0
#include "zpr.h"

int main(int argc, char** argv)
{
	std::string str = "a std::string";
	std::string_view sv = "a std::string_view";
	std::vector<int> vec = { 1, 2, 3, 4, 5 };

	zpr::println("str = {}\nsv = {}\nvec = {}\n\n", str, sv, vec);

	char c_                 = 'a';
	bool b_                 = false;
	int i_                  = 0x33deadf;
	long l_                 = 0;
	unsigned long ul_       =  9384;
	long long ll_           = -123456;
	unsigned long long ull_ =  981234;
	float f_                =  3.14159265358979323846264338327950;
	double d_               = -2 * 3.14159265358979323846264338327950;
	char* s_                = (char*) "OMEGALUL KEKW";

	const char* b__ = b_ ? "true" : "false";

	auto pf = [](auto&&... xs) { printf(xs...); };
	auto zpf = [](auto&&... xs) { zpr::println(xs...); };

	constexpr auto line = "—————————————————————————————————";

#if 0
#endif
	pf("%c\n", c_);                 zpf("{}", c_);                      zpf(line);
	pf("%s\n", b__);                zpf("{}", b_);                      zpf(line);
	pf("%d\n", i_);                 zpf("{}", i_);                      zpf(line);
	pf("%ld\n", l_);                zpf("{}", l_);                      zpf(line);
	pf("%lu\n", ul_);               zpf("{}", ul_);                     zpf(line);
	pf("%lld\n", ll_);              zpf("{}", ll_);                     zpf(line);
	pf("%llu\n", ull_);             zpf("{}", ull_);                    zpf(line);
	pf("%f\n", f_);                 zpf("{}", f_);                      zpf(line);
	pf("%f\n", d_);                 zpf("{}", d_);                      zpf(line);
	pf("%x\n", i_);                 zpf("{x}", i_);                     zpf(line);
	pf("%e\n", d_);                 zpf("{e}", d_);                     zpf(line);
	pf("%s\n", s_);                 zpf("{}", s_);                      zpf(line);

	zpf("\n");

	pf("%10d\n", i_);               zpf("{10}", i_);                    zpf(line);
	pf("%10ld\n", l_);              zpf("{10}", l_);                    zpf(line);
	pf("%10lu\n", ul_);             zpf("{10}", ul_);                   zpf(line);
	pf("%10lld\n", ll_);            zpf("{10}", ll_);                   zpf(line);
	pf("%10llu\n", ull_);           zpf("{10}", ull_);                  zpf(line);
	pf("%10f\n", f_);               zpf("{10}", f_);                    zpf(line);
	pf("%10f\n", d_);               zpf("{10}", d_);                    zpf(line);
	pf("%10x\n", i_);               zpf("{10x}", i_);                   zpf(line);
	pf("%10e\n", d_);               zpf("{10e}", d_);                   zpf(line);
	pf("%10s\n", s_);               zpf("{10}", s_);                    zpf(line);

	zpf("\n");

	pf("%.10d\n", i_);              zpf("{.10}", i_);                   zpf(line);
	pf("%.10ld\n", l_);             zpf("{.10}", l_);                   zpf(line);
	pf("%.10lu\n", ul_);            zpf("{.10}", ul_);                  zpf(line);
	pf("%.10lld\n", ll_);           zpf("{.10}", ll_);                  zpf(line);
	pf("%.10llu\n", ull_);          zpf("{.10}", ull_);                 zpf(line);
	pf("%.10f\n", f_);              zpf("{.10}", f_);                   zpf(line);
	pf("%.10f\n", d_);              zpf("{.10}", d_);                   zpf(line);
	pf("%.10x\n", i_);              zpf("{.10x}", i_);                  zpf(line);
	pf("%.10e\n", d_);              zpf("{.10e}", d_);                  zpf(line);
	pf("%.10s\n", s_);              zpf("{.10}", s_);                   zpf(line);

	zpf("\n");

	pf("%18.10d\n", i_);            zpf("{18.10}", i_);                 zpf(line);
	pf("%18.10ld\n", l_);           zpf("{18.10}", l_);                 zpf(line);
	pf("%18.10lu\n", ul_);          zpf("{18.10}", ul_);                zpf(line);
	pf("%18.10lld\n", ll_);         zpf("{18.10}", ll_);                zpf(line);
	pf("%18.10llu\n", ull_);        zpf("{18.10}", ull_);               zpf(line);
	pf("%18.10f\n", f_);            zpf("{18.10}", f_);                 zpf(line);
	pf("%18.10f\n", d_);            zpf("{18.10}", d_);                 zpf(line);
	pf("%18.10x\n", i_);            zpf("{18.10x}", i_);                zpf(line);
	pf("%18.10e\n", d_);            zpf("{18.10e}", d_);                zpf(line);
	pf("%18.10s\n", s_);            zpf("{18.10}", s_);   	            zpf(line);

	zpf("\n");

	pf("%*.10d\n", 18, i_);         zpf("{.10}",  zpr::w(18)(i_));      zpf(line);
	pf("%*.10ld\n", 18, l_);        zpf("{.10}",  zpr::w(18)(l_));      zpf(line);
	pf("%*.10lu\n", 18, ul_);       zpf("{.10}",  zpr::w(18)(ul_));     zpf(line);
	pf("%*.10lld\n", 18, ll_);      zpf("{.10}",  zpr::w(18)(ll_));     zpf(line);
	pf("%*.10llu\n", 18, ull_);     zpf("{.10}",  zpr::w(18)(ull_));    zpf(line);
	pf("%*.10f\n", 18, f_);         zpf("{.10}",  zpr::w(18)(f_));      zpf(line);
	pf("%*.10f\n", 18, d_);         zpf("{.10}",  zpr::w(18)(d_));      zpf(line);
	pf("%*.10x\n", 18, i_);         zpf("{.10x}", zpr::w(18)(i_));      zpf(line);
	pf("%*.10e\n", 18, d_);         zpf("{.10e}", zpr::w(18)(d_));      zpf(line);
	pf("%*.10s\n", 18, s_);         zpf("{.10}",  zpr::w(18)(s_));      zpf(line);

	zpf("\n");

	pf("%18.*d\n", 10, i_);         zpf("{18}",  zpr::p(10)(i_));       zpf(line);
	pf("%18.*ld\n", 10, l_);        zpf("{18}",  zpr::p(10)(l_));       zpf(line);
	pf("%18.*lu\n", 10, ul_);       zpf("{18}",  zpr::p(10)(ul_));      zpf(line);
	pf("%18.*lld\n", 10, ll_);      zpf("{18}",  zpr::p(10)(ll_));      zpf(line);
	pf("%18.*llu\n", 10, ull_);     zpf("{18}",  zpr::p(10)(ull_));     zpf(line);
	pf("%18.*f\n", 10, f_);         zpf("{18}",  zpr::p(10)(f_));       zpf(line);
	pf("%18.*f\n", 10, d_);         zpf("{18}",  zpr::p(10)(d_));       zpf(line);
	pf("%18.*x\n", 10, i_);         zpf("{18x}", zpr::p(10)(i_));       zpf(line);
	pf("%18.*e\n", 10, d_);         zpf("{18e}", zpr::p(10)(d_));       zpf(line);
	pf("%18.*s\n", 10, s_);         zpf("{18}",  zpr::p(10)(s_));       zpf(line);

	zpf("\n");

	pf("%*.*d\n", 18, 10, i_);      zpf("{}",  zpr::wp(18, 10)(i_));    zpf(line);
	pf("%*.*ld\n", 18, 10, l_);     zpf("{}",  zpr::wp(18, 10)(l_));    zpf(line);
	pf("%*.*lu\n", 18, 10, ul_);    zpf("{}",  zpr::wp(18, 10)(ul_));   zpf(line);
	pf("%*.*lld\n", 18, 10, ll_);   zpf("{}",  zpr::wp(18, 10)(ll_));   zpf(line);
	pf("%*.*llu\n", 18, 10, ull_);  zpf("{}",  zpr::wp(18, 10)(ull_));  zpf(line);
	pf("%*.*f\n", 18, 10, f_);      zpf("{}",  zpr::wp(18, 10)(f_));    zpf(line);
	pf("%*.*f\n", 18, 10, d_);      zpf("{}",  zpr::wp(18, 10)(d_));    zpf(line);
	pf("%*.*x\n", 18, 10, i_);      zpf("{x}", zpr::wp(18, 10)(i_));    zpf(line);
	pf("%*.*e\n", 18, 10, d_);      zpf("{e}", zpr::wp(18, 10)(d_));    zpf(line);
	pf("%*.*s\n", 18, 10, s_);      zpf("{}",  zpr::wp(18, 10)(s_));    zpf(line);

	zpf("\n");

	pf("%018.10d\n", i_);           zpf("{018.10}", i_);                zpf(line);
	pf("%018.10ld\n", l_);          zpf("{018.10}", l_);                zpf(line);
	pf("%018.10lu\n", ul_);         zpf("{018.10}", ul_);               zpf(line);
	pf("%018.10lld\n", ll_);        zpf("{018.10}", ll_);               zpf(line);
	pf("%018.10llu\n", ull_);       zpf("{018.10}", ull_);              zpf(line);
	pf("%018.10f\n", f_);           zpf("{018.10}", f_);                zpf(line);
	pf("%018.10f\n", d_);           zpf("{018.10}", d_);                zpf(line);
	pf("%018.10x\n", i_);           zpf("{018.10x}", i_);               zpf(line);
	pf("%018.10e\n", d_);           zpf("{018.10e}", d_);               zpf(line);
	pf("%018.10s\n", s_);           zpf("{018.10}", s_);                zpf(line);

	zpf("\n");

	pf("%-18.10d|\n", i_);          zpf("{-18.10}|", i_);               zpf(line);
	pf("%-18.10ld|\n", l_);         zpf("{-18.10}|", l_);               zpf(line);
	pf("%-18.10lu|\n", ul_);        zpf("{-18.10}|", ul_);              zpf(line);
	pf("%-18.10lld|\n", ll_);       zpf("{-18.10}|", ll_);              zpf(line);
	pf("%-18.10llu|\n", ull_);      zpf("{-18.10}|", ull_);             zpf(line);
	pf("%-18.10f|\n", f_);          zpf("{-18.10}|", f_);               zpf(line);
	pf("%-18.10f|\n", d_);          zpf("{-18.10}|", d_);               zpf(line);
	pf("%-18.10x|\n", i_);          zpf("{-18.10x}|", i_);              zpf(line);
	pf("%-18.10e|\n", d_);          zpf("{-18.10e}|", d_);              zpf(line);
	pf("%-18.10s|\n", s_);          zpf("{-18.10}|", s_);               zpf(line);

	zpf("\n");
}
