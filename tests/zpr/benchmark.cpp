// benchmark.cpp
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

#include <climits>
#include <cfloat>
#include <cstddef>

#include <string>
#include <fstream>
#include <stdio.h>

#include "zpr.h"

#include <cassert>
#include <fmt/core.h>
#include <fmt/compile.h>

#define BIG_STRING "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."

void speedTest(FILE* fd, const std::string& which, long count)
{
	// Following is required so that we're not limited by per-character buffering.
	if(which == "printf")
	{
		for(long i = 0; i < count; ++i)
		{
			fprintf(fd, "%0.10f:%04d:%+g:%s:%p:%c:%%\n", 1.234, 42, 3.13, "str", (void*) 1000, 'X');
			// fprintf(fd, "%0.10f:%+g:%e\n", 1.23456, 3.4951, 1234567.890123456);
			// fprintf(fd, "%s:%.99s:%s\n", BIG_STRING, BIG_STRING, BIG_STRING);
		}
	}
	else if(which == "zpr")
	{
		for(long i = 0; i < count; ++i)
		{
			zpr::fprint(fd, "{.10f}:{04}:{+g}:{}:{p}:{}:%\n", 1.234, 42, 3.13, "str", (void*) 1000, 'X');
			// zpr::fprint(fd, "{.10f}:{+g}:{e}\n", 1.23456, 3.4951, 1234567.890123456);
			// zpr::fprint(fd, "{}:{.99}:{}\n", BIG_STRING, BIG_STRING, BIG_STRING);
		}
	}
	else if(which == "fmt")
	{
		for(long i = 0; i < count; ++i)
		{
			fmt::print(fd, "{:.10f}:{:04}:{:+g}:{:}:{:p}:{}:%\n", 1.234, 42, 3.13, "str", (void*) 1000, 'X');
			// fmt::print(fd, "{:.10f}:{:+g}:{:e}\n", 1.23456, 3.4951, 1234567.890123456);
			// fmt::print(fd, "{}:{:.99}:{}\n", BIG_STRING, BIG_STRING, BIG_STRING);
		}
	}
	else if(which == "fmt2")
	{
		for(long i = 0; i < count; ++i)
		{
			fmt::print(fd, fmt::format(FMT_COMPILE("{:.10f}:{:04}:{:+g}:{:}:{:p}:{}:%\n"), 1.234, 42, 3.13, "str", (void*) 1000, 'X'));
			// fmt::print(fd, fmt::format(FMT_COMPILE("{:.10f}:{:+g}:{:e}\n"), 1.23456, 3.4951, 1234567.890123456));
			// fmt::print(fd, fmt::format(FMT_COMPILE("{}:{:.*}:{}\n"), BIG_STRING, 999, BIG_STRING, BIG_STRING));
		}
	}
	else
	{
		assert(0 && "speed test for which version?");
	}
}


int main(int argc, char* argv[])
{
	FILE* fd = fopen(argc > 2 ? argv[2] : "/dev/null", "w");
	if(argc >= 2)
		speedTest(fd, argv[1], argc > 3 ? std::stol(argv[3]) : 1000000);

	fclose(fd);
	return 0;
}
