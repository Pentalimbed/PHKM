#include "utils.h"

namespace phkm
{
	bool orCheck(nlohmann::json j_array, check_func_t func)
	{
		for (const auto& item : j_array)
			if (item.is_array() ? andCheck(item, func) : func(item))
				return true;
		return j_array.empty();
	}

	bool andCheck(nlohmann::json j_array, check_func_t func)
	{
		for (const auto& item : j_array)
			if (!(item.is_array() ? andCheck(item, func) : func(item)))
				return false;
		return j_array.empty();
	}
}  // namespace phkm