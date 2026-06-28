#include <cmath>

#include "sszdef.h"


double SSZ_STDCALL Sin(double x)
{
	return std::sin(x);
}

double SSZ_STDCALL Cos(double x)
{
	return std::cos(x);
}

double SSZ_STDCALL Tan(double x)
{
	return std::tan(x);
}

double SSZ_STDCALL ASin(double x)
{
	return std::asin(x);
}

double SSZ_STDCALL ACos(double x)
{
	return std::acos(x);
}

double SSZ_STDCALL ATan(double x)
{
	return std::atan(x);
}

double SSZ_STDCALL Log(double y, double x)
{
	return std::log(x) / std::log(y);
}

double SSZ_STDCALL Ln(double x)
{
	return std::log(x);
}

double SSZ_STDCALL Exp(double x)
{
	return std::exp(x);
}

double SSZ_STDCALL Sqrt(double x)
{
	return std::sqrt(x);
}

double SSZ_STDCALL Ceil(double x)
{
	return std::ceil(x);
}

double SSZ_STDCALL Floor(double x)
{
	return std::floor(x);
}

bool SSZ_STDCALL IsFinite(double x)
{
	return _finite(x) != 0;
}

bool SSZ_STDCALL IsInf(double x)
{
	return _isnan(x) == 0 && _finite(x) == 0;
}

bool SSZ_STDCALL IsNaN(double x)
{
	return _isnan(x) != 0;
}
