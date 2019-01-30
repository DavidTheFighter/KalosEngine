#ifndef COMMON_H_
#define COMMON_H_

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>

#include <stdlib.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define APP_NAME "Not sure yet"
#define ENGINE_NAME "KalosEngine"

#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_REVISION 0

#define ENGINE_VERSION_MAJOR 1
#define ENGINE_VERSION_MINOR 0
#define ENGINE_VERSION_REVISION 0

#define GLFW_DOUBLE_PRESS 3

#define RENDER_DEBUG_MARKERS 1
#define D3D12_DEBUG_COMPATIBILITY_CHECKS 1
#define VULKAN_DEBUG_COMPATIBILITY_CHECKS 1

#ifndef M_PI
#define M_PI 3.1415926535897932384
#endif

#define M_SQRT_2 1.4142135623730950488
#define M_2PI 6.2831853071795864769
#define M_TAU M_2PI

#include <Util/Log.h>

#define DEBUG_ASSERT(x) if (!(x)) { Log::get()->error("Assertion \"{}\" failed @ file \"{}\", line {}", #x, __FILE__, __LINE__); system("pause"); }

/*
 * Turn A into a string literal without expanding macro definitions
 * (however, if invoked from a macro, macro arguments are expanded).
 */
#define STRINGIZE_NX(A) #A

 /*
  * Turn A into a string literal after macro-expanding it.
  */
#define STRINGIZE(A) STRINGIZE_NX(A)

/*
Converts a UTF8 string into a UTF16 string (std::string to std::wstring)
*/
inline std::wstring utf8_to_utf16(const std::string& utf8)
{
	std::vector<unsigned long> unicode;
	size_t i = 0;
	while (i < utf8.size())
	{
		unsigned long uni;
		size_t todo;
		bool error = false;
		unsigned char ch = utf8[i++];
		if (ch <= 0x7F)
		{
			uni = ch;
			todo = 0;
		}
		else if (ch <= 0xBF)
		{
			throw std::logic_error("not a UTF-8 string");
		}
		else if (ch <= 0xDF)
		{
			uni = ch & 0x1F;
			todo = 1;
		}
		else if (ch <= 0xEF)
		{
			uni = ch & 0x0F;
			todo = 2;
		}
		else if (ch <= 0xF7)
		{
			uni = ch & 0x07;
			todo = 3;
		}
		else
		{
			throw std::logic_error("not a UTF-8 string");
		}
		for (size_t j = 0; j < todo; ++j)
		{
			if (i == utf8.size())
				throw std::logic_error("not a UTF-8 string");
			unsigned char ch = utf8[i++];
			if (ch < 0x80 || ch > 0xBF)
				throw std::logic_error("not a UTF-8 string");
			uni <<= 6;
			uni += ch & 0x3F;
		}
		if (uni >= 0xD800 && uni <= 0xDFFF)
			throw std::logic_error("not a UTF-8 string");
		if (uni > 0x10FFFF)
			throw std::logic_error("not a UTF-8 string");
		unicode.push_back(uni);
	}
	std::wstring utf16;
	for (size_t i = 0; i < unicode.size(); ++i)
	{
		unsigned long uni = unicode[i];
		if (uni <= 0xFFFF)
		{
			utf16 += (wchar_t) uni;
		}
		else
		{
			uni -= 0x10000;
			utf16 += (wchar_t) ((uni >> 10) + 0xD800);
			utf16 += (wchar_t) ((uni & 0x3FF) + 0xDC00);
		}
	}
	return utf16;
}

inline std::string utf16_to_utf8(const std::wstring &utf16)
{
	std::string retStr;
	if (!utf16.empty())
	{
		int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, NULL, 0, NULL, NULL);

		if (sizeRequired > 0)
		{
			std::vector<char> utf8String(sizeRequired);
			int bytesConverted = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(),
				-1, &utf8String[0], utf8String.size(), NULL,
				NULL);
			if (bytesConverted != 0)
			{
				retStr = &utf8String[0];
			}
			else
			{
				std::stringstream err;
				err << __FUNCTION__
					<< " std::string WstrToUtf8Str failed to convert wstring '"
					<< utf16.c_str() << L"'";
				throw std::runtime_error(err.str());
			}
		}
	}
	return retStr;
}

inline size_t stringHash(const std::string &str)
{
	return std::hash<std::string> {} (str);
}

/*
Converts a value to a string, uses std::stringstream
*/
template<typename T0>
inline std::string toString(T0 arg)
{
	std::stringstream ss;

	ss << arg;

	std::string str;

	ss >> str;

	return str;
}

/*
Appends one vector onto the end of another
*/
template<typename T0>
inline void appendVector(std::vector<T0> &to, const std::vector<T0> &vectorToBeAppended)
{
	to.insert(to.end(), vectorToBeAppended.begin(), vectorToBeAppended.end());
}

inline void writeFileCharData(const std::string &filename, const std::vector<char> &data)
{
	std::ofstream file(filename, std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		Log::get()->error("Failed to open file: {} for writing", filename);

		//throw std::runtime_error("failed to open file for writing!");
	}

	file.write(reinterpret_cast<const char*> (data.data()), data.size());
	file.close();
}

inline void writeFileStr(const std::string &filename, const std::string &data)
{
	std::ofstream file(filename, std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		Log::get()->error("Failed to open file: {} for writing", filename);

		//throw std::runtime_error("failed to open file for writing!");
	}

	file.write(data.c_str(), data.length());
	file.close();
}

/*
Splits a string into chunks based on a delimiter
*/
inline std::vector<std::string> strsplit(const std::string &s, char delim)
{
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> elems;

	while (getline(ss, item, delim))
	{
		elems.push_back(item);
	}

	return elems;
}

inline void seqmemcpy(char *to, const void *from, size_t size, size_t &offset)
{
	memcpy(to + offset, from, size);
	offset += size;
}

typedef struct simple_float_vector_2
{
	float x, y;

	inline bool operator== (const simple_float_vector_2 &vec0)
	{
		return vec0.x == this->x && vec0.y == this->y;
	}
} svec2;

typedef struct simple_float_vector_3
{
	float x, y, z;

	inline bool operator== (const simple_float_vector_3 &vec0)
	{
		return vec0.x == this->x && vec0.y == this->y && vec0.z == this->z;
	}
} svec3;

typedef struct simple_float_vector_4
{
	float x, y, z, w;

	inline bool operator== (const simple_float_vector_4 &vec0)
	{
		return vec0.x == this->x && vec0.y == this->y && vec0.z == this->z && vec0.w == this->w;
	}
} svec4;

typedef struct simple_integer_vector_2
{
	int32_t x, y;
} sivec2;

typedef struct simple_integer_vector_3
{
	int32_t x, y, z;

	bool operator< (const simple_integer_vector_3 &other) const
	{
		if (x < other.x)
			return true;

		if (x > other.x)
			return false;

		if (y < other.y)
			return true;

		if (y > other.y)
			return false;

		if (z < other.z)
			return true;

		if (z > other.z)
			return false;

		return false;
	}

	bool operator== (const simple_integer_vector_3 &other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
} sivec3;

typedef struct simple_integer_vector_4
{
	int32_t x, y, z, w;
} sivec4;

typedef struct simple_unsigned_integer_vector_2
{
	uint32_t x, y;
} suvec2;

typedef struct simple_unsigned_integer_vector_3
{
	uint32_t x, y, z;
} suvec3;

typedef struct simple_unsigned_integer_vector_4
{
	uint32_t x, y, z, w;
} suvec4;

#endif /* COMMON_H_ */