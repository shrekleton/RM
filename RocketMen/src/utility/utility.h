
#pragma once

#include <common.h>

#include <algorithm>

inline std::string toLower(const std::string& str)
{
	std::string out(str);
	std::transform(out.begin(), out.end(), out.begin(), ::tolower);
	return out;
}

inline std::string toUpper(const std::string& str)
{
	std::string out(str);
	std::transform(out.begin(), out.end(), out.begin(), ::toupper);
	return out;
}

inline uint32_t toABGR(Color c)
{
	uint32_t red = static_cast<uint32_t>(c.x * 255.0f);
	uint32_t green = static_cast<uint32_t>(c.y * 255.0f);
	uint32_t blue = static_cast<uint32_t>(c.z * 255.0f);
	uint32_t alpha = static_cast<uint32_t>(c.w * 255.0f);
	return (alpha << 24) + (blue << 16) + (green << 8) + red;
}

inline uint32_t toABGR(iColor c)
{
	uint32_t red = static_cast<uint32_t>(c.x);
	uint32_t green = static_cast<uint32_t>(c.y);
	uint32_t blue = static_cast<uint32_t>(c.z);
	uint32_t alpha = static_cast<uint32_t>(c.w);
	return (alpha << 24) + (blue << 16) + (green << 8) + red;
}

inline Color toColor(uint32_t p)
{
	const float a = ((p >> 24) & 0xFF) / 255.0f;
	const float b = ((p >> 16) & 0xFF) / 255.0f;
	const float g = ((p >> 8) & 0xFF) / 255.0f;
	const float r = (p & 0xFF) / 255.0f;
	return Color(r, g, b, a);
}

inline iColor toiColor(uint32_t p)
{
	const uint32_t a = ((p >> 24) & 0xFF);
	const uint32_t b = ((p >> 16) & 0xFF);
	const uint32_t g = ((p >> 8) & 0xFF);
	const uint32_t r = (p & 0xFF);
	return iColor(r, g, b, a);
}

template<typename T>
std::string to_binary_string(T val)
{
	std::size_t sz = sizeof(val) * CHAR_BIT;
	std::string ret(sz, ' ');
	while (sz--)
	{
		ret[sz] = '0' + (val & 1);
		val >>= 1;
	}

	return ret;
}

template<typename Iter, typename UnaryPredicate>
typename std::iterator_traits<Iter>::value_type findPtrByPredicate(Iter begin, Iter end, UnaryPredicate predicate)
{
	auto it = std::find_if(begin,  end, predicate);
	if (it != end)
	{
		return *it;
	}

	return nullptr;
}