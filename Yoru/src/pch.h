#pragma once

#ifdef _WIN64
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
#endif

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <fstream>
#include <stdarg.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#include <array>
#include <vector>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <deque>