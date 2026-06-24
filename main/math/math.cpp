
#include <cmath>

#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"


extern "C" double SSZ_STDCALL Sin(PluginUtil* pu, double x)
{
	return std::sin(x);
}

extern "C" double SSZ_STDCALL Cos(PluginUtil* pu, double x)
{
	return std::cos(x);
}

extern "C" double SSZ_STDCALL Tan(PluginUtil* pu, double x)
{
	return std::tan(x);
}

extern "C" double SSZ_STDCALL ASin(PluginUtil* pu, double x)
{
	return std::asin(x);
}

extern "C" double SSZ_STDCALL ACos(PluginUtil* pu, double x)
{
	return std::acos(x);
}

extern "C" double SSZ_STDCALL ATan(PluginUtil* pu, double x)
{
	return std::atan(x);
}

extern "C" double SSZ_STDCALL Log(PluginUtil* pu, double y, double x)
{
	return std::log(x) / std::log(y);
}

extern "C" double SSZ_STDCALL Ln(PluginUtil* pu, double x)
{
	return std::log(x);
}

extern "C" double SSZ_STDCALL Exp(PluginUtil* pu, double x)
{
	return std::exp(x);
}

extern "C" double SSZ_STDCALL Sqrt(PluginUtil* pu, double x)
{
	return std::sqrt(x);
}

extern "C" double SSZ_STDCALL Ceil(PluginUtil* pu, double x)
{
	return std::ceil(x);
}

extern "C" double SSZ_STDCALL Floor(PluginUtil* pu, double x)
{
	return std::floor(x);
}

extern "C" bool SSZ_STDCALL IsFinite(PluginUtil* pu, double x)
{
	return _finite(x) != 0;
}

extern "C" bool SSZ_STDCALL IsInf(PluginUtil* pu, double x)
{
	return _isnan(x) == 0 && _finite(x) == 0;
}

extern "C" bool SSZ_STDCALL IsNaN(PluginUtil* pu, double x)
{
	return _isnan(x) != 0;
}
