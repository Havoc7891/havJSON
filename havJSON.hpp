/*
havJSON.hpp

ABOUT

Havoc's single-file JSON library for C++.

TODO

- Code refactoring.
- Add more error handling code.
- Improve unicode support.
- BSON support needs more testing.
- Remove "long" data type support?

REVISION HISTORY

v0.2 (2025-02-05) - Resolved issues in BSON data conversion logic.
v0.1 (2024-01-12) - First release.

LICENSE

MIT License

Copyright (c) 2024-2025 René Nicolaus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef HAVJSON_HPP
#define HAVJSON_HPP

// Add -D_FILE_OFFSET_BITS=64 to CFLAGS for large file support

#ifdef _WIN32
#ifdef _MBCS
#error "_MBCS is defined, but only Unicode is supported"
#endif
#undef _UNICODE
#define _UNICODE
#undef UNICODE
#define UNICODE

#undef NOMINMAX
#define NOMINMAX

#undef STRICT
#define STRICT

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif
#ifdef _MSC_VER
#include <SDKDDKVer.h>
#endif

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cuchar>
#include <cstring>
#include <codecvt>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <stdexcept>
#include <string>
#include <sstream>
#include <type_traits>
#include <map>
#include <memory>
#include <variant>
#include <vector>

static_assert(sizeof(signed char) == 1, "expected char to be 1 byte");
static_assert(sizeof(unsigned char) == 1, "expected unsigned char to be 1 byte");
static_assert(sizeof(signed char) == 1, "expected int8 to be 1 byte");
static_assert(sizeof(unsigned char) == 1, "expected uint8 to be 1 byte");
static_assert(sizeof(signed short int) == 2, "expected int16 to be 2 bytes");
static_assert(sizeof(unsigned short int) == 2, "expected uint16 to be 2 bytes");
static_assert(sizeof(signed int) == 4, "expected int32 to be 4 bytes");
static_assert(sizeof(unsigned int) == 4, "expected uint32 to be 4 bytes");
static_assert(sizeof(signed long long) == 8, "expected int64 to be 8 bytes");
static_assert(sizeof(unsigned long long) == 8, "expected uint64 to be 8 bytes");

namespace havJSON
{
    enum class havJSONBOMType : std::uint8_t
    {
        None,
        UTF8,
        UTF16LE,
        UTF16BE,
        UTF32LE,
        UTF32BE
    };

    enum class havJSONType : std::uint8_t
    {
        JSON,
        BSON
    };

    enum class havJSONDataType : std::uint8_t
    {
        Null,
        Boolean,
        Int,
        UInt,
        Long,
        ULong,
        Int64,
        UInt64,
        Double,
        String,
        Array,
        Object
    };

    enum class havJSONBSONType : std::uint8_t
    {
        Double = 0x01,      // double
        String = 0x02,      // string
        Array = 0x04,       // array: integer values as keys
        BinaryData = 0x05,  // binary data in bytes
        Boolean = 0x08,     // 0x00 = false, 0x01 = true
        UTCDateTime = 0x09, // int64
        NullValue = 0x0A,   // null
        JSCode = 0x0D,      // string
        Int = 0x10,         // int32
        Timestamp = 0x11,   // uint64
        Int64 = 0x12        // int64
    };

    enum class havJSONToken : std::uint8_t
    {
        None,
        // Structural tokens
        LeftSquareBracket,
        LeftCurlyBracket,
        RightSquareBracket,
        RightCurlyBracket,
        Colon,
        Comma,
        // Generic type tokens
        Null,
        Boolean,
        Int,
        UInt,
        Long,
        ULong,
        Int64,
        UInt64,
        Double,
        String, // = Name in Name/Value pair
        Value // = String Value
    };

    struct havJSONTokenValue
    {
        havJSONToken mToken;
        std::optional<std::string> mValue;
    };

    struct havJSONBSONTypeValue
    {
        havJSONBSONType mType;
        int mCurrentIndex;
        std::int32_t mTotalLength;
    };

    struct havJSONBSONDataType
    {
        havJSONDataType mDataType;
        int mIndexToWriteTotalSizeTo;
        int mArrayTotalSize;
        std::uint16_t mCurrentArrayItemIndex;
    };

    class havJSONData
    {
    public:
        typedef std::variant<bool, int, unsigned int, long, unsigned long, std::int64_t, std::uint64_t, double, const char*, std::string, std::vector<std::shared_ptr<havJSONData>>, std::map<std::string, std::shared_ptr<havJSONData>>> havJSONVariant;

        explicit havJSONData(const havJSONVariant& value, havJSONDataType valueType) : mType(valueType), mValue(std::move(value))
        {
        }

        explicit havJSONData(havJSONDataType valueType = havJSONDataType::Null)
        {
            switch (valueType)
            {
                case havJSONDataType::Null:
                    setValue<std::string>(std::string("null"), havJSONDataType::Null);
                    break;

                case havJSONDataType::Boolean:
                    setValue<bool>(false, havJSONDataType::Boolean);
                    break;

                case havJSONDataType::Int:
                    setValue<int>(0, havJSONDataType::Int);
                    break;

                case havJSONDataType::UInt:
                    setValue<unsigned int>(0, havJSONDataType::UInt);
                    break;

                case havJSONDataType::Long:
                    setValue<long>(0, havJSONDataType::Long);
                    break;

                case havJSONDataType::ULong:
                    setValue<unsigned long>(0, havJSONDataType::ULong);
                    break;

                case havJSONDataType::Int64:
                    setValue<std::int64_t>(0, havJSONDataType::Int64);
                    break;

                case havJSONDataType::UInt64:
                    setValue<std::uint64_t>(0, havJSONDataType::UInt64);
                    break;

                case havJSONDataType::Double:
                    setValue<double>(0.0, havJSONDataType::Double);
                    break;

                case havJSONDataType::String:
                    setValue<std::string>(std::string(""), havJSONDataType::String);
                    break;

                case havJSONDataType::Array:
                    {
                        std::vector<std::shared_ptr<havJSONData>> tmpArray;
                        setValue<std::vector<std::shared_ptr<havJSONData>>>(std::move(tmpArray), havJSONDataType::Array);
                    }
                    break;

                case havJSONDataType::Object:
                    {
                        std::map<std::string, std::shared_ptr<havJSONData>> tmpObject;
                        setValue<std::map<std::string, std::shared_ptr<havJSONData>>>(std::move(tmpObject), havJSONDataType::Object);
                    }
                    break;

                default:
                    throw std::runtime_error("Unsupported type!");
            }
        }

        explicit havJSONData(havJSONData&& value) : mType(value.mType), mValue(std::move(value.mValue))
        {
        }

        explicit havJSONData(const havJSONData& value) : mType(value.mType), mValue(value.mValue)
        {
        }

        explicit havJSONData(bool value) { setValue<bool>(value, havJSONDataType::Boolean); }
        explicit havJSONData(int value) { setValue<int>(value, havJSONDataType::Int); }
        explicit havJSONData(unsigned int value) { setValue<unsigned int>(value, havJSONDataType::UInt); }
        explicit havJSONData(long value) { setValue<long>(value, havJSONDataType::Long); }
        explicit havJSONData(unsigned long value) { setValue<unsigned long>(value, havJSONDataType::ULong); }
        explicit havJSONData(std::int64_t value) { setValue<std::int64_t>(value, havJSONDataType::Int64); }
        explicit havJSONData(std::uint64_t value) { setValue<std::uint64_t>(value, havJSONDataType::UInt64); }
        explicit havJSONData(double value) { setValue<double>(value, havJSONDataType::Double); }
        explicit havJSONData(const char* value) { setValue<std::string>(std::string(value), havJSONDataType::String); }
        explicit havJSONData(const std::string& value) { setValue<std::string>(value, havJSONDataType::String); }

        havJSONData& operator=(havJSONData&& value)
        {
            mValue = value.mValue;
            mType = value.mType;

            return *this;
        }

        havJSONData& operator=(const havJSONData& value)
        {
            if (this != &value)
            {
                mValue = value.mValue;
                mType = value.mType;
            }

            return *this;
        }

        void operator=(const havJSONVariant& value)
        {
            if (std::holds_alternative<bool>(value))
            {
                mType = havJSONDataType::Boolean;
            }
            else if (std::holds_alternative<int>(value))
            {
                mType = havJSONDataType::Int;
            }
            else if (std::holds_alternative<unsigned int>(value))
            {
                mType = havJSONDataType::UInt;
            }
            else if (std::holds_alternative<long>(value))
            {
                mType = havJSONDataType::Long;
            }
            else if (std::holds_alternative<unsigned long>(value))
            {
                mType = havJSONDataType::ULong;
            }
            else if (std::holds_alternative<std::int64_t>(value))
            {
                mType = havJSONDataType::Int64;
            }
            else if (std::holds_alternative<std::uint64_t>(value))
            {
                mType = havJSONDataType::UInt64;
            }
            else if (std::holds_alternative<double>(value))
            {
                mType = havJSONDataType::Double;
            }
            else if (std::holds_alternative<const char*>(value))
            {
                mType = havJSONDataType::String;
                mValue = std::string(std::get<const char*>(value));
                return;
            }
            else if (std::holds_alternative<std::string>(value))
            {
                mType = havJSONDataType::String;
            }
            else
            {
                throw std::runtime_error("Unsupported type!");
            }

            mValue = value;
        }

        havJSONVariant* getAddress() { return &mValue; }

        havJSONVariant getValue() const { return mValue; }

        havJSONDataType getType() const { return mType; }

        bool isArray() { return mType == havJSONDataType::Array; }
        bool isObject() { return mType == havJSONDataType::Object; }
        bool isNull() { return mType == havJSONDataType::Null; }
        bool isBoolean() { return mType == havJSONDataType::Boolean; }
        bool isInt() { return mType == havJSONDataType::Int; }
        bool isUInt() { return mType == havJSONDataType::UInt; }
        bool isLong() { return mType == havJSONDataType::Long; }
        bool isULong() { return mType == havJSONDataType::ULong; }
        bool isInt64() { return mType == havJSONDataType::Int64; }
        bool isUInt64() { return mType == havJSONDataType::UInt64; }
        bool isDouble() { return mType == havJSONDataType::Double; }
        bool isString() { return mType == havJSONDataType::String; }

        template<typename T>
        T convertTo(bool explicitCast, T defaultValue)
        {
            if (explicitCast == false)
            {
                if constexpr (std::is_same_v<T, bool> == true)
                {
                    bool value;

                    std::stringstream(toString()) >> std::boolalpha >> value;

                    return value;
                }

                std::string::size_type idx;

                std::string value = toString();

                if constexpr (std::is_same_v<T, int> == true)
                {
                    return std::stoi(value, &idx);
                }
                else if constexpr (std::is_same_v<T, unsigned int> == true)
                {
                    return std::stoul(value, &idx);
                }
                else if constexpr (std::is_same_v<T, long> == true)
                {
                    return std::stol(value, &idx);
                }
                else if constexpr (std::is_same_v<T, unsigned long> == true)
                {
                    return std::stoul(value, &idx);
                }
                else if constexpr (std::is_same_v<T, std::int64_t> == true)
                {
                    return std::stoll(value, &idx);
                }
                else if constexpr (std::is_same_v<T, std::uint64_t> == true)
                {
                    return std::stoull(value, &idx);
                }
                else if constexpr (std::is_same_v<T, double> == true)
                {
                    return std::stod(value, &idx);
                }
                else
                {
                    throw std::runtime_error("Unsupported type!");
                }
            }

            try
            {
                return std::get<T>(mValue);
            }
            catch (const std::bad_variant_access& e)
            {
                return defaultValue;
            }
        }

        bool toBoolean(bool explicitCast = false, bool defaultValue = false)
        {
            return convertTo<bool>(explicitCast, defaultValue);
        }

        int toInt(bool explicitCast = false, int defaultValue = 0)
        {
            return convertTo<int>(explicitCast, defaultValue);
        }

        unsigned int toUInt(bool explicitCast = false, unsigned int defaultValue = 0)
        {
            return convertTo<unsigned int>(explicitCast, defaultValue);
        }

        long toLong(bool explicitCast = false, long defaultValue = 0)
        {
            return convertTo<long>(explicitCast, defaultValue);
        }

        unsigned long toULong(bool explicitCast = false, unsigned long defaultValue = 0)
        {
            return convertTo<unsigned long>(explicitCast, defaultValue);
        }

        std::int64_t toInt64(bool explicitCast = false, std::int64_t defaultValue = 0)
        {
            return convertTo<std::int64_t>(explicitCast, defaultValue);
        }

        std::uint64_t toUInt64(bool explicitCast = false, std::uint64_t defaultValue = 0)
        {
            return convertTo<std::uint64_t>(explicitCast, defaultValue);
        }

        double toDouble(bool explicitCast = false, double defaultValue = 0.0)
        {
            return convertTo<double>(explicitCast, defaultValue);
        }

        std::string toString()
        {
            switch (mType)
            {
                case havJSONDataType::Null: return dataToString<std::string>();
                case havJSONDataType::Boolean: return dataToString<bool>();
                case havJSONDataType::Int: return dataToString<int>();
                case havJSONDataType::UInt: return dataToString<unsigned int>();
                case havJSONDataType::Long: return dataToString<long>();
                case havJSONDataType::ULong: return dataToString<unsigned long>();
                case havJSONDataType::Int64: return dataToString<std::int64_t>();
                case havJSONDataType::UInt64: return dataToString<std::uint64_t>();
                case havJSONDataType::Double: return dataToString<double>();
                case havJSONDataType::String: return dataToString<std::string>();
                default: throw std::runtime_error("Unsupported type!");
            }

            return std::string();
        }

        havJSONData& operator[](int index)
        {
            if (mType != havJSONDataType::Array)
            {
                throw std::runtime_error("Value is not an array!");
            }

            auto value = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue);

            return *value[index].get();
        }

        havJSONData& operator[](const char* key)
        {
            if (mType != havJSONDataType::Object)
            {
                throw std::runtime_error("Value is not an object!");
            }

            auto value = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue);

            if (value.find(key) == value.end())
            {
                throw std::runtime_error("Key was not found in object!");
            }

            return *value[key].get();
        }

        havJSONData& operator[](const std::string& key)
        {
            if (mType != havJSONDataType::Object)
            {
                throw std::runtime_error("Value is not an object!");
            }

            auto value = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue);

            if (value.find(key) == value.end())
            {
                throw std::runtime_error("Key was not found in object!");
            }

            return *value[key].get();
        }

        // Array & Object
        void clear()
        {
            if (mType == havJSONDataType::Array)
            {
                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).clear();
            }

            if (mType == havJSONDataType::Object)
            {
                std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).clear();
            }

            throw std::runtime_error("Value is not an array or object!");
        }

        bool empty()
        {
            if (mType == havJSONDataType::Array)
            {
                return std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).empty();
            }

            if (mType == havJSONDataType::Object)
            {
                return std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).empty();
            }

            throw std::runtime_error("Value is not an array or object!");
        }

        // Array
        void erase(std::vector<std::shared_ptr<havJSONData>>::iterator itr)
        {
            if (mType == havJSONDataType::Array)
            {
                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).erase(itr);

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        std::vector<std::shared_ptr<havJSONData>>::iterator arrayBegin()
        {
            if (mType == havJSONDataType::Array)
            {
                return std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).begin();
            }

            throw std::runtime_error("Value is not an array!");
        }

        std::vector<std::shared_ptr<havJSONData>>::iterator arrayEnd()
        {
            if (mType == havJSONDataType::Array)
            {
                return std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).end();
            }

            throw std::runtime_error("Value is not an array!");
        }

        havJSONData& front()
        {
            try
            {
                if (mType != havJSONDataType::Array)
                {
                    throw std::runtime_error("Value is not an array!");
                }

                return *std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).front().get();
            }
            catch (const std::out_of_range& ex)
            {
                std::cout << "Exception thrown after calling front method of array!\n";

                throw;
            }
        }

        havJSONData& back()
        {
            try
            {
                if (mType != havJSONDataType::Array)
                {
                    throw std::runtime_error("Value is not an array!");
                }

                return *std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).back().get();
            }
            catch (const std::out_of_range& ex)
            {
                std::cout << "Exception thrown after calling back method of array!\n";

                throw;
            }
        }

        void insert(int index, std::shared_ptr<havJSONData> newValue)
        {
            if (mType == havJSONDataType::Array)
            {
                auto itr = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).begin();

                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).insert(itr + index, std::move(newValue));

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        void push_back(std::shared_ptr<havJSONData> newValue)
        {
            if (mType == havJSONDataType::Array)
            {
                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).push_back(std::move(newValue));

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        void push_front(std::shared_ptr<havJSONData> newValue)
        {
            if (mType == havJSONDataType::Array)
            {
                auto itr = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).begin();

                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).insert(itr, std::move(newValue));

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        void pop_back()
        {
            if (mType == havJSONDataType::Array)
            {
                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).pop_back();

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        void pop_front()
        {
            if (mType == havJSONDataType::Array)
            {
                auto itr = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).begin();

                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).erase(itr);

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        void remove(int index)
        {
            if (mType == havJSONDataType::Array)
            {
                auto itr = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).begin();

                std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).erase(itr + index);

                return;
            }

            throw std::runtime_error("Value is not an array!");
        }

        bool contains(std::shared_ptr<havJSONData> newValue)
        {
            if (mType == havJSONDataType::Array)
            {
                auto value = std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue);

                return std::find(value.begin(), value.end(), newValue) != value.end();
            }

            throw std::runtime_error("Value is not an array!");
        }

        havJSONData& at(int index)
        {
            try
            {
                if (mType != havJSONDataType::Array)
                {
                    throw std::runtime_error("Value is not an array!");
                }

                return *std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).at(index).get();
            }
            catch (const std::out_of_range& ex)
            {
                std::cout << "Exception thrown after calling at method of array!\n";

                throw;
            }
        }

        std::vector<std::shared_ptr<havJSONData>>::size_type arraySize()
        {
            if (mType == havJSONDataType::Array)
            {
                return std::get<std::vector<std::shared_ptr<havJSONData>>>(mValue).size();
            }

            throw std::runtime_error("Value is not an array!");
        }

        // Object
        std::map<std::string, std::shared_ptr<havJSONData>>::iterator objectBegin()
        {
            if (mType == havJSONDataType::Object)
            {
                return std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).begin();
            }

            throw std::runtime_error("Value is not an object!");
        }

        std::map<std::string, std::shared_ptr<havJSONData>>::iterator objectEnd()
        {
            if (mType == havJSONDataType::Object)
            {
                return std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).end();
            }

            throw std::runtime_error("Value is not an object!");
        }

        void insert(std::string key, std::shared_ptr<havJSONData> newValue)
        {
            if (mType == havJSONDataType::Object)
            {
                std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).insert({key, std::move(newValue)});

                return;
            }

            throw std::runtime_error("Value is not an object!");
        }

        void erase(std::map<std::string, std::shared_ptr<havJSONData>>::iterator itr)
        {
            if (mType == havJSONDataType::Object)
            {
                std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).erase(itr);

                return;
            }

            throw std::runtime_error("Value is not an object!");
        }

        havJSONData& find(const std::string& key)
        {
            if (mType != havJSONDataType::Object)
            {
                throw std::runtime_error("Value is not an object!");
            }

            return *std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).find(key)->second.get();
        }

        void remove(const std::string& key)
        {
            if (mType == havJSONDataType::Object)
            {
                auto value = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue);

                auto itr = value.find(key);

                if (itr != value.end())
                {
                    value.erase(itr);
                }

                return;
            }

            throw std::runtime_error("Value is not an object!");
        }

        bool contains(const std::string& key)
        {
            if (mType == havJSONDataType::Object)
            {
                auto value = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue);

                return value.find(key) != value.end();
            }

            throw std::runtime_error("Value is not an object!");
        }

        std::map<std::string, std::shared_ptr<havJSONData>>::size_type objectSize()
        {
            if (mType == havJSONDataType::Object)
            {
                return std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(mValue).size();
            }

            throw std::runtime_error("Value is not an object!");
        }

    private:
        template<typename T>
        void setValue(const T& value, havJSONDataType valueType)
        {
            mType = valueType;
            mValue = value;
        }

        template<typename T>
        std::string dataToString()
        {
            auto dataRef = std::get<T>(mValue);

            std::ostringstream out;

            out << dataRef;

            std::string result = out.str();

            if constexpr (std::is_same_v<T, bool> == true)
            {
                if (result == "1")
                {
                    return "true";
                }

                return "false";
            }

            return result;
        }

        havJSONDataType mType;
        havJSONVariant mValue;
    };

    class havJSONStream
    {
    public:
        havJSONStream() {}
        ~havJSONStream() {}

        bool SkipWhitespaces(char currentChar, bool ignoreSpace)
        {
            if (ignoreSpace == false)
            {
                if (currentChar == ' ')
                {
                    return true;
                }
            }

            if (currentChar == '\\' || currentChar == '/' || currentChar == '\b' || currentChar == '\f' ||
                currentChar == '\n' || currentChar == '\r' || currentChar == '\t' || currentChar == '\v')
            {
                return true;
            }

            return false;
        }

        havJSONToken CheckToken(char currentChar)
        {
            switch (currentChar)
            {
                case '[': return havJSONToken::LeftSquareBracket;
                case '{': return havJSONToken::LeftCurlyBracket;
                case ']': return havJSONToken::RightSquareBracket;
                case '}': return havJSONToken::RightCurlyBracket;
                case ':': return havJSONToken::Colon;
                case ',': return havJSONToken::Comma;
                default: return havJSONToken::None;
            }
        }

        char GetTokenAsCharacter(havJSONToken currentToken)
        {
            switch (currentToken)
            {
                case havJSONToken::LeftSquareBracket: return '[';
                case havJSONToken::LeftCurlyBracket: return '{';
                case havJSONToken::RightSquareBracket: return ']';
                case havJSONToken::RightCurlyBracket: return '}';
                case havJSONToken::Colon: return ':';
                case havJSONToken::Comma: return ',';
                default: return '\0';
            }
        }

        std::string CodePointToString(const unsigned int codePoint)
        {
            // Note: null-terminated string
            char value[5] = { '\0' };

            if (codePoint < 0x80)
            {
                value[0] = codePoint;
            }
            else if (codePoint < 0x800)
            {
                value[0] = (codePoint >> 6) | 0xc0;
                value[1] = (codePoint & 0x3f) | 0x80;
            }
            else if (codePoint < 0x10000)
            {
                value[0] = (codePoint >> 12) | 0xe0;
                value[1] = ((codePoint >> 6) & 0x3f) | 0x80;
                value[2] = (codePoint & 0x3f) | 0x80;
            }
            else if (codePoint < 0x110000)
            {
                value[0] = (codePoint >> 18) | 0xf0;
                value[1] = ((codePoint >> 12) & 0x3f) | 0x80;
                value[2] = ((codePoint >> 6) & 0x3f) | 0x80;
                value[3] = (codePoint & 0x3f) | 0x80;
            }

            return std::string(value);
        }

        std::string ConvertToEscapedString(const std::string& value)
        {
            std::string resultValue;

            unsigned int codePoint = 0;
            int numOfBytes = 0;
            bool writeAsHex = false;

            for (std::string::size_type index = 0; index < value.size(); ++index)
            {
                switch (value[index])
                {
                case '"':
                    resultValue += "\\\"";
                    break;

                case '\\':
                    resultValue += "\\\\";
                    break;

                case '\b':
                    resultValue += "\\b";
                    break;

                case '\f':
                    resultValue += "\\f";
                    break;

                case '\n':
                    resultValue += "\\n";
                    break;

                case '\r':
                    resultValue += "\\r";
                    break;

                case '\t':
                    resultValue += "\\t";
                    break;

                case '\v':
                    resultValue += "\\v";
                    break;

                default:
                    {
                        writeAsHex = false;
                        codePoint = 0;

                        if ((value[index] & 0x80) == 0x00)
                        {
                            numOfBytes = 1;

                            if (value[index] < 0x1f)
                            {
                                writeAsHex = true;

                                codePoint = (value[index] & 0x7f);
                            }
                        }
                        else if ((value[index] & 0xe0) == 0xc0)
                        {
                            numOfBytes = 2;

                            writeAsHex = true;

                            codePoint = (value[index] & 0x1f);
                        }
                        else if ((value[index] & 0xf0) == 0xe0)
                        {
                            numOfBytes = 3;

                            writeAsHex = true;

                            codePoint = (value[index] & 0x0f);
                        }
                        else if ((value[index] & 0xf8) == 0xf0)
                        {
                            numOfBytes = 4;

                            writeAsHex = true;

                            codePoint = (value[index] & 0x07);
                        }
                        else
                        {
                            throw std::runtime_error("Invalid UTF-8 sequence!");
                        }

                        for (int byteCount = 1; byteCount < numOfBytes; ++byteCount)
                        {
                            if ((value[++index] & 0xc0) != 0x80)
                            {
                                throw std::runtime_error("Invalid UTF-8 sequence!");
                            }

                            codePoint = (codePoint << 6) | (value[index] & 0x3f);
                        }

                        if (writeAsHex == true)
                        {
                            std::ostringstream outputStringStream;

                            // Code point is UTF-16 surrogate pair
                            if (codePoint >= 0x10000 && codePoint <= 0x10ffff)
                            {
                                codePoint -= 0x10000;

                                unsigned int highSurrogate = (codePoint / 0x400) + 0xd800;
                                unsigned int lowSurrogate = (codePoint % 0x400) + 0xdc00;

                                outputStringStream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << highSurrogate;
                                outputStringStream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << lowSurrogate;

                                resultValue += outputStringStream.str();
                            }
                            else
                            {
                                outputStringStream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << codePoint;

                                resultValue += outputStringStream.str();
                            }
                        }
                        else
                        {
                            resultValue += static_cast<unsigned int>(value[index]);
                        }
                    }
                }
            }

            return resultValue;
        }

        havJSONTokenValue CheckForString(std::string::size_type& index, const std::string& jsonStringStream)
        {
            std::optional<std::string> value;

            std::string tempValue;

            ++index;

            for (; index < jsonStringStream.size(); ++index)
            {
                char currentChar = jsonStringStream[index];

                switch (currentChar)
                {
                case '"':
                    {
                        value = tempValue;

                        return havJSONTokenValue { havJSONToken::Value, value };
                    }
                    break;

                case '\\':
                    {
                        if (++index >= jsonStringStream.size())
                        {
                            throw std::runtime_error("Read beyond end of file!");
                        }

                        currentChar = jsonStringStream[index];

                        switch (currentChar)
                        {
                        case '"':
                        case '\\':
                        case '/':
                            tempValue += currentChar;
                            break;

                        case 'b':
                            tempValue += '\b';
                            break;

                        case 'f':
                            tempValue += '\f';
                            break;

                        case 'n':
                            tempValue += '\n';
                            break;

                        case 'r':
                            tempValue += '\r';
                            break;

                        case 't':
                            tempValue += '\t';
                            break;

                        case 'u':
                            {
                                std::locale loc;

                                std::string hexString;

                                bool isUnicodeEscapeSequence = false;

                                for (int valueIndex = 0; valueIndex < 4; ++valueIndex)
                                {
                                    if (++index >= jsonStringStream.size())
                                    {
                                        throw std::runtime_error("Read beyond end of file!");
                                    }

                                    char currentCharInLoop = jsonStringStream[index];

                                    if (std::isxdigit(currentCharInLoop, loc) == false)
                                    {
                                        isUnicodeEscapeSequence = false;

                                        break;
                                    }
                                    else
                                    {
                                        isUnicodeEscapeSequence = true;
                                    }

                                    hexString += currentCharInLoop;
                                }

                                if (isUnicodeEscapeSequence == true)
                                {
                                    unsigned int codePoint = std::strtol(hexString.c_str(), nullptr, 16);

                                    if (++index >= jsonStringStream.size())
                                    {
                                        throw std::runtime_error("Read beyond end of file!");
                                    }

                                    if (jsonStringStream[index] == '\\')
                                    {
                                        if (++index >= jsonStringStream.size())
                                        {
                                            throw std::runtime_error("Read beyond end of file!");
                                        }

                                        if (jsonStringStream[index] == 'u')
                                        {
                                            bool surrogatePair = false;

                                            hexString = "";

                                            for (int valueIndex = 0; valueIndex < 4; ++valueIndex)
                                            {
                                                if (++index >= jsonStringStream.size())
                                                {
                                                    throw std::runtime_error("Read beyond end of file!");
                                                }

                                                char currentCharInLoop = jsonStringStream[index];

                                                if (std::isxdigit(currentCharInLoop, loc) == false)
                                                {
                                                    surrogatePair = false;

                                                    break;
                                                }
                                                else
                                                {
                                                    surrogatePair = true;
                                                }

                                                hexString += currentCharInLoop;
                                            }

                                            if (surrogatePair == true)
                                            {
                                                if (codePoint >= 0xd800 && codePoint <= 0xdbff)
                                                {
                                                    unsigned int secondCodePoint = std::strtol(hexString.c_str(), nullptr, 16);

                                                    if (secondCodePoint >= 0xdc00 && secondCodePoint <= 0xdfff)
                                                    {
                                                        // codePoint = high surrogate, secondCodePoint = low surrogate
                                                        unsigned int surrogatePairValue = 0x10000 + ((codePoint - 0xd800) * 0x400) + (secondCodePoint - 0xdc00);

                                                        tempValue += CodePointToString(surrogatePairValue);
                                                    }
                                                    else
                                                    {
                                                        throw std::runtime_error("Invalid code point range for UTF-16 low surrogate pair!");
                                                    }
                                                }
                                                else
                                                {
                                                    // Set the index back by six characters, otherwise the possible next UTF-8 sequence isn't processed!
                                                    index -= 6;

                                                    tempValue += CodePointToString(codePoint);
                                                }
                                            }
                                            else
                                            {
                                                throw std::runtime_error("Invalid code point for UTF-16 low surrogate pair!");
                                            }
                                        }
                                        else
                                        {
                                            // Set the index back by two characters, otherwise the next characters aren't processed!
                                            index -= 2;

                                            tempValue += CodePointToString(codePoint);
                                        }
                                    }
                                    else
                                    {
                                        // Set the index back by one character, otherwise the next character isn't processed!
                                        --index;

                                        tempValue += CodePointToString(codePoint);
                                    }
                                }
                                else
                                {
                                    // Set the index back by one character, otherwise the next character isn't processed!
                                    --index;

                                    // Don't eat the u character
                                    tempValue += currentChar;

                                    if (hexString.size() > 0)
                                    {
                                        tempValue += hexString;
                                    }
                                }
                            }
                            break;

                        case 'v':
                            tempValue += '\v';
                            break;

                        default:
                            throw std::runtime_error("Invalid escape character!");
                        }
                    }
                    break;

                default:
                    tempValue += currentChar;
                }
            }

            throw std::runtime_error("Invalid string value!");
        }

        havJSONTokenValue CheckForNumber(std::string::size_type& index, const std::string& jsonStringStream, const char value)
        {
            std::string tempValue(1, value);

            for (; index < jsonStringStream.size(); ++index)
            {
                char currentChar = jsonStringStream[index];

                if (currentChar == ',' || SkipWhitespaces(currentChar, false) == true)
                {
                    break;
                }

                tempValue += currentChar;
            }

            --index;

            // Check if we're dealing with a floating point value
            auto foundDecimalPoint = tempValue.find(".");

            // Check if the value contains an exponent
            auto foundExponent = tempValue.find("e");

            if (foundExponent == std::string::npos)
            {
                foundExponent = tempValue.find("E");
            }

            std::string::size_type idx;

            // We're dealing with a signed value
            if (tempValue[0] == '-')
            {
                // We're dealing with a floating point value
                if (foundDecimalPoint != std::string::npos ||
                    foundExponent != std::string::npos)
                {
                    try
                    {
                        // We try to convert the value to double
                        double result = std::stod(tempValue, &idx);

                        std::stringstream tempStringStream;

                        tempStringStream << std::fixed << std::setprecision(15) << result;

                        return havJSONTokenValue { havJSONToken::Double, tempStringStream.str() };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        throw;
                    }
                    catch (const std::out_of_range& e)
                    {
                        throw;
                    }
                }
                // We're dealing with an integer value
                else
                {
                    try
                    {
                        // We try to convert the value to int
                        int result = std::stoi(tempValue, &idx);

                        return havJSONTokenValue { havJSONToken::Int, std::to_string(result) };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        throw;
                    }
                    catch (const std::out_of_range& ex2)
                    {
                        try
                        {
                            // We try to convert the value to long
                            long result = std::stol(tempValue, &idx);

                            return havJSONTokenValue { havJSONToken::Long, std::to_string(result) };
                        }
                        catch (const std::invalid_argument& e)
                        {
                            throw;
                        }
                        catch (const std::out_of_range& ex3)
                        {
                            try
                            {
                                // We try to convert the value to long long
                                long long result = std::stoll(tempValue, &idx);

                                return havJSONTokenValue { havJSONToken::Int64, std::to_string(result) };
                            }
                            catch (const std::invalid_argument& e)
                            {
                                throw;
                            }
                            catch (const std::out_of_range& e)
                            {
                                throw;
                            }
                        }
                    }
                }
            }
            // We're dealing with an unsigned value
            else
            {
                // We're dealing with a floating point value
                if (foundDecimalPoint != std::string::npos ||
                    foundExponent != std::string::npos)
                {
                    try
                    {
                        // We try to convert the value to double
                        double result = std::stod(tempValue, &idx);

                        std::stringstream tempStringStream;

                        tempStringStream << std::fixed << std::setprecision(15) << result;

                        return havJSONTokenValue { havJSONToken::Double, tempStringStream.str() };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        throw;
                    }
                    catch (const std::out_of_range& e)
                    {
                        throw;
                    }
                }
                // We're dealing with an integer value
                else
                {
                    try
                    {
                        // We try to convert the value to unsigned int
                        unsigned long testResult = std::stoul(tempValue, &idx);

                        // Check if value is in unsigned int range
                        if (testResult > std::numeric_limits<unsigned int>::max())
                        {
                            throw std::out_of_range(tempValue);
                        }

                        unsigned int result = testResult;

                        return havJSONTokenValue { havJSONToken::UInt, std::to_string(result) };
                    }
                    catch (const std::invalid_argument& e)
                    {
                        throw;
                    }
                    catch (const std::out_of_range& ex2)
                    {
                        try
                        {
                            // We try to convert the value to unsigned long
                            unsigned long result = std::stoul(tempValue, &idx);

                            return havJSONTokenValue { havJSONToken::ULong, std::to_string(result) };
                        }
                        catch (const std::invalid_argument& e)
                        {
                            throw;
                        }
                        catch (const std::out_of_range& ex3)
                        {
                            try
                            {
                                // We try to convert the value to long long
                                unsigned long long result = std::stoull(tempValue, &idx);

                                return havJSONTokenValue { havJSONToken::UInt64, std::to_string(result) };
                            }
                            catch (const std::invalid_argument& e)
                            {
                                throw;
                            }
                            catch (const std::out_of_range& e)
                            {
                                throw;
                            }
                        }
                    }
                }
            }

            throw std::runtime_error("Unable to read number value!");
        }

        std::string CheckForLiteral(std::string::size_type& index, const std::string& jsonStringStream, const std::string& literalValue)
        {
            std::string tempValue(1, literalValue[0]);

            std::string::size_type stringIndex = 1;

            for (; index < jsonStringStream.size(); ++index)
            {
                char currentChar = jsonStringStream[index];

                if (currentChar != literalValue[stringIndex++])
                {
                    throw std::runtime_error("Literal value invalid: " + literalValue);
                }

                tempValue += currentChar;

                if (tempValue == literalValue)
                {
                    break;
                }
            }

            if (tempValue.size() != literalValue.size())
            {
                throw std::runtime_error("Invalid literal value!");
            }

            return tempValue;
        }

        havJSONTokenValue CheckTypeToken(havJSONToken structuralToken, std::string::size_type& index, const std::string& jsonStringStream)
        {
            // Object
            if (structuralToken == havJSONToken::LeftCurlyBracket)
            {
                for (; index < jsonStringStream.size(); ++index)
                {
                    char currentChar = jsonStringStream[index];

                    if (SkipWhitespaces(currentChar, false) == true)
                    {
                        continue;
                    }

                    havJSONToken token = CheckToken(currentChar);

                    if (token != havJSONToken::None)
                    {
                        mTokens.push_back(havJSONTokenValue { token, std::nullopt });

                        continue;
                    }

                    switch (currentChar)
                    {
                    case '"':
                        {
                            havJSONTokenValue result = CheckForString(index, jsonStringStream);

                            result.mToken = havJSONToken::String;

                            return result;
                        }
                        break;

                    default:
                        return havJSONTokenValue { havJSONToken::None, std::nullopt };
                    }
                }
            }
            // Array
            else if (structuralToken == havJSONToken::LeftSquareBracket)
            {
                for (; index < jsonStringStream.size(); ++index)
                {
                    char currentChar = jsonStringStream[index];

                    if (SkipWhitespaces(currentChar, false) == true)
                    {
                        continue;
                    }

                    switch (currentChar)
                    {
                    case '"':
                    case 't':
                    case 'f':
                    case 'n':
                    case 'u':
                    case '-':
                    case '+':
                    case '.':
                    case ':':
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                    case 'e':
                    case 'E':
                        {
                            if (currentChar == '"')
                            {
                                return CheckForString(index, jsonStringStream);
                            }
                            else
                            {
                                havJSONToken tempToken = havJSONToken::Value;

                                std::optional<std::string> value;

                                std::string tempValue;

                                ++index;

                                if (currentChar == 't')
                                {
                                    tempValue = CheckForLiteral(index, jsonStringStream, "true");

                                    tempToken = havJSONToken::Boolean;
                                }

                                if (currentChar == 'f')
                                {
                                    tempValue = CheckForLiteral(index, jsonStringStream, "false");

                                    tempToken = havJSONToken::Boolean;
                                }

                                if (currentChar == 'n')
                                {
                                    tempValue = CheckForLiteral(index, jsonStringStream, "null");

                                    tempToken = havJSONToken::Null;
                                }

                                if (currentChar == '-' ||
                                    currentChar == '+' ||
                                    currentChar == '.' ||
                                    currentChar == ':' ||
                                    currentChar == '0' ||
                                    currentChar == '1' ||
                                    currentChar == '2' ||
                                    currentChar == '3' ||
                                    currentChar == '4' ||
                                    currentChar == '5' ||
                                    currentChar == '6' ||
                                    currentChar == '7' ||
                                    currentChar == '8' ||
                                    currentChar == '9' ||
                                    currentChar == 'e' ||
                                    currentChar == 'E')
                                {
                                    return CheckForNumber(index, jsonStringStream, currentChar);
                                }

                                if (tempValue.empty() == false)
                                {
                                    value = tempValue;
                                }

                                return havJSONTokenValue { tempToken, ((value.has_value() == false) ? std::nullopt : value) };
                            }
                        }
                        break;

                    case '{':
                        mTokens.push_back(havJSONTokenValue { havJSONToken::LeftCurlyBracket, std::nullopt });
                        return havJSONTokenValue { havJSONToken::LeftCurlyBracket, std::nullopt };

                    case '[':
                        mTokens.push_back(havJSONTokenValue { havJSONToken::LeftSquareBracket, std::nullopt });
                        return havJSONTokenValue { havJSONToken::LeftSquareBracket, std::nullopt };

                    default:
                        return havJSONTokenValue { havJSONToken::None, std::nullopt };
                    }
                }
            }
            // Other types (e.g. Value)
            else
            {
                for (; index < jsonStringStream.size(); ++index)
                {
                    char currentChar = jsonStringStream[index];

                    if (SkipWhitespaces(currentChar, false) == true)
                    {
                        continue;
                    }

                    havJSONToken token = CheckToken(currentChar);

                    if (token != havJSONToken::None)
                    {
                        mTokens.push_back(havJSONTokenValue { token, std::nullopt });
                    }

                    if (token == havJSONToken::None)
                    {
                        switch (currentChar)
                        {
                        case '"':
                        case 't':
                        case 'f':
                        case 'n':
                        case 'u':
                        case '-':
                        case '+':
                        case '.':
                        case ':':
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                        case 'e':
                        case 'E':
                            {
                                if (currentChar == '"')
                                {
                                    return CheckForString(index, jsonStringStream);
                                }
                                else
                                {
                                    havJSONToken tempToken = havJSONToken::Value;

                                    std::optional<std::string> value;

                                    std::string tempValue;

                                    ++index;

                                    if (currentChar == 't')
                                    {
                                        tempValue = CheckForLiteral(index, jsonStringStream, "true");

                                        tempToken = havJSONToken::Boolean;
                                    }

                                    if (currentChar == 'f')
                                    {
                                        tempValue = CheckForLiteral(index, jsonStringStream, "false");

                                        tempToken = havJSONToken::Boolean;
                                    }

                                    if (currentChar == 'n')
                                    {
                                        tempValue = CheckForLiteral(index, jsonStringStream, "null");

                                        tempToken = havJSONToken::Null;
                                    }

                                    if (currentChar == '-' ||
                                        currentChar == '+' ||
                                        currentChar == '.' ||
                                        currentChar == ':' ||
                                        currentChar == '0' ||
                                        currentChar == '1' ||
                                        currentChar == '2' ||
                                        currentChar == '3' ||
                                        currentChar == '4' ||
                                        currentChar == '5' ||
                                        currentChar == '6' ||
                                        currentChar == '7' ||
                                        currentChar == '8' ||
                                        currentChar == '9' ||
                                        currentChar == 'e' ||
                                        currentChar == 'E')
                                    {
                                        return CheckForNumber(index, jsonStringStream, currentChar);
                                    }

                                    if (tempValue.empty() == false)
                                    {
                                        value = tempValue;
                                    }

                                    return havJSONTokenValue { tempToken, ((value.has_value() == false) ? std::nullopt : value) };
                                }
                            }
                            break;

                        default:
                            return havJSONTokenValue { havJSONToken::None, std::nullopt };
                        }
                    }
                    else
                    {
                        return havJSONTokenValue { token, std::nullopt };
                    }
                }
            }

            return havJSONTokenValue { structuralToken, std::nullopt };
        }

        std::string GetBSONKey(const std::string& jsonStringStream, int& index)
        {
            char newChar = jsonStringStream[++index];

            std::string stringValue;

            while (newChar != 0x00)
            {
                stringValue += newChar;

                newChar = jsonStringStream[++index];
            }

            // Skip null terminator
            ++index;

            // Convert to escaped string
            stringValue = ConvertToEscapedString(stringValue);

            std::string keyValue("\"");
            keyValue += stringValue;
            keyValue += "\": ";

            return keyValue;
        }

        std::int32_t GetBSONValueSize(const std::string& jsonStringStream, int& index)
        {
            std::int32_t valueSize = (jsonStringStream[index + 3] << 24) | ((jsonStringStream[index + 2] & 0xff) << 16) | ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);

            if (valueSize < 0)
            {
                throw std::runtime_error("Value size can't be negative!");
            }

            index += 4;

            return valueSize;
        }

        std::string ConvertBSONToJSON(const std::string& jsonStringStream)
        {
            int index = 0;

            std::int32_t totalDocumentSize = GetBSONValueSize(jsonStringStream, index);

            // Keeps track of the depth level
            std::deque<havJSONBSONTypeValue> typeMemory;

            // Note: We assume that BSON documents are always objects, and never arrays. Is that correct, though?
            std::string jsonContent("{");

            for (; index < totalDocumentSize; ++index)
            {
                // Check if we reached the end of the BSON document
                if (index + 1 == totalDocumentSize)
                {
                    // Advance to next byte ("end of object")
                    ++index;
                    break;
                }

                if (typeMemory.empty() == false)
                {
                    const havJSONBSONTypeValue& typeValue = typeMemory.back();

                    if (typeValue.mCurrentIndex > typeValue.mTotalLength)
                    {
                        throw std::runtime_error("BSON type value index is greater than total length!");
                    }
                }

                char currentChar = jsonStringStream[index];

                // Type
                switch (static_cast<havJSON::havJSONBSONType>(currentChar))
                {
                    case havJSON::havJSONBSONType::NullValue:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            jsonContent += "null";

                            if (typeMemory.empty() == false)
                            {
                                const havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        // The current index doesn't need to be increased here

                                        // The off-by-one error preventation is unnecessary here

                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::Boolean:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            bool value = jsonStringStream[index];

                            if (value == true)
                            {
                                jsonContent += "true";
                            }
                            else
                            {
                                jsonContent += "false";
                            }

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    typeValue.mCurrentIndex += 1;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::Int:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            std::int32_t value = (jsonStringStream[index + 3] << 24) | ((jsonStringStream[index + 2] & 0xff) << 16) | ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);

                            index += 3;

                            jsonContent += std::to_string(value);

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    typeValue.mCurrentIndex += 4;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::Timestamp:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            char uint64String[8] = { jsonStringStream[index + 0], jsonStringStream[index + 1], jsonStringStream[index + 2], jsonStringStream[index + 3], jsonStringStream[index + 4], jsonStringStream[index + 5], jsonStringStream[index + 6], jsonStringStream[index + 7] };
                            std::uint64_t value;
                            std::memcpy(&value, uint64String, sizeof(std::uint64_t));

                            index += 7;

                            jsonContent += std::to_string(value);

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    typeValue.mCurrentIndex += 8;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::UTCDateTime:
                    case havJSON::havJSONBSONType::Int64:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            char int64String[8] = { jsonStringStream[index + 0], jsonStringStream[index + 1], jsonStringStream[index + 2], jsonStringStream[index + 3], jsonStringStream[index + 4], jsonStringStream[index + 5], jsonStringStream[index + 6], jsonStringStream[index + 7] };
                            std::int64_t value;
                            std::memcpy(&value, int64String, sizeof(std::int64_t));

                            index += 7;

                            jsonContent += std::to_string(value);

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    typeValue.mCurrentIndex += 8;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::Double:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value
                            char doubleByteArray[8] = { jsonStringStream[index + 0], jsonStringStream[index + 1], jsonStringStream[index + 2], jsonStringStream[index + 3], jsonStringStream[index + 4], jsonStringStream[index + 5], jsonStringStream[index + 6], jsonStringStream[index + 7] };
                            double value;
                            std::memcpy(&value, doubleByteArray, sizeof(double));

                            index += 7;

                            std::stringstream tempStringStream;

                            tempStringStream << std::fixed << std::setprecision(15) << value;

                            jsonContent += tempStringStream.str();

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    typeValue.mCurrentIndex += 8;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::JSCode:
                    case havJSON::havJSONBSONType::String:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value size
                            std::string::size_type valueSize = GetBSONValueSize(jsonStringStream, index);

                            // Value
                            jsonContent += "\"";

                            std::string stringValue;

                            char newChar = jsonStringStream[index];

                            while (newChar != 0x00 && stringValue.size() < valueSize)
                            {
                                stringValue += newChar;

                                newChar = jsonStringStream[++index];
                            }

                            // Convert to escaped string
                            stringValue = ConvertToEscapedString(stringValue);

                            jsonContent += stringValue;

                            jsonContent += "\"";

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Value size
                                    typeValue.mCurrentIndex += 4;

                                    // String size (+ 1 = null terminator)
                                    typeValue.mCurrentIndex += (stringValue.size() + 1);

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    case havJSON::havJSONBSONType::Array:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value size
                            std::int32_t valueSize = GetBSONValueSize(jsonStringStream, index);

                            // Prevent off-by-one error
                            --index;

                            // Value (4 = size of valueSize)
                            typeMemory.push_back({ havJSON::havJSONBSONType::Array, 4, valueSize });

                            jsonContent += "[";
                        }
                        break;

                    case havJSON::havJSONBSONType::BinaryData:
                        {
                            // Key
                            if (typeMemory.empty() == false)
                            {
                                // Integer key
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Advance to next byte
                                    ++index;

#if 0
                                    // Parse integer key
                                    std::uint16_t tempCodepoint = ((jsonStringStream[index + 1] & 0xff) << 8) | (jsonStringStream[index + 0] & 0xff);
                                    std::string tempKey = CodePointToString(tempCodepoint);
                                    std::cout << "Key: " << tempKey << std::endl;
#endif

                                    // Advance index by 2
                                    index += 2;

                                    // data type (1 byte) + integer key (2 byte)
                                    typeValue.mCurrentIndex += 3;
                                }
                                else
                                {
                                    throw std::runtime_error("Other types are not supported!");
                                }
                            }
                            else
                            {
                                // String key
                                jsonContent += GetBSONKey(jsonStringStream, index);
                            }

                            // Value size
                            std::int32_t valueSize = GetBSONValueSize(jsonStringStream, index);

                            // Value
                            jsonContent += "[";

                            std::uint8_t subtypeValue = jsonStringStream[index];

                            // Advance to next byte
                            ++index;

                            // Calculate end of binary data
                            int binaryDataEnd = index + valueSize;

                            // Note: We only support the generic binary subtype
                            switch (subtypeValue)
                            {
                                case 0x00:
                                case 0x02:
                                    for (int binaryDataIndex = index; binaryDataIndex < binaryDataEnd; ++binaryDataIndex)
                                    {
                                        jsonContent += CodePointToString(jsonStringStream[binaryDataIndex]);

                                        if (binaryDataIndex + 1 < binaryDataEnd)
                                        {
                                            jsonContent += ", ";
                                        }
                                    }
                                    break;

                                default:
                                    if (subtypeValue >= 0x80)
                                    {
                                        // User defined subtype
                                        throw std::runtime_error("User defined subtypes are unsupported!");
                                    }
                                    else
                                    {
                                        throw std::runtime_error("Unsupported subtype!");
                                    }
                            }

                            jsonContent += "]";

                            if (typeMemory.empty() == false)
                            {
                                havJSONBSONTypeValue& typeValue = typeMemory.back();

                                if (typeValue.mType == havJSON::havJSONBSONType::Array)
                                {
                                    // Value size
                                    typeValue.mCurrentIndex += 4;

                                    // Binary data size
                                    typeValue.mCurrentIndex += valueSize;

                                    if (typeValue.mCurrentIndex + 1 < typeValue.mTotalLength)
                                    {
                                        jsonContent += ", ";
                                    }
                                }
                            }
                        }
                        break;

                    default:
                        throw std::runtime_error("Unsupported type!");
                }

                if (typeMemory.empty() == false)
                {
                    const havJSONBSONTypeValue& typeValue = typeMemory.back();

                    if (typeValue.mCurrentIndex + 1 == typeValue.mTotalLength)
                    {
                        switch (typeValue.mType)
                        {
                            case havJSON::havJSONBSONType::Array:
                                jsonContent += "]";
                                break;

                            default:
                                throw std::runtime_error("Unsupported type!");
                        }

                        // Skip "array end" byte, and proceed with next byte
                        index += 2;

                        typeMemory.pop_back();

                        if (typeMemory.empty() == false)
                        {
                            havJSONBSONTypeValue& nextTypeValue = typeMemory.back();

                            if (nextTypeValue.mType == havJSON::havJSONBSONType::Array)
                            {
                                // Value size
                                nextTypeValue.mCurrentIndex += 1;

                                // The off-by-one error preventation is unnecessary here

                                if (nextTypeValue.mCurrentIndex + 1 < nextTypeValue.mTotalLength)
                                {
                                    jsonContent += ", ";
                                }
                            }
                        }
                    }
                }
                else
                {
                    jsonContent += ", ";
                }
            }

            if (jsonStringStream[index] != 0x00)
            {
                throw std::runtime_error("EOO not found!");
            }

            if (jsonContent.size() >= 2 && jsonContent.substr(jsonContent.size() - 2) == ", ")
            {
                jsonContent.erase(jsonContent.size() - 2);
            }

            jsonContent += "}";

            return jsonContent;
        }

        bool Tokenization(const std::string& jsonStringStream)
        {
            mTokens.clear();

            havJSONToken currentTypeToken = havJSONToken::None;

            std::deque<havJSONToken> typeTokens;
            std::deque<int> typeTokenDepthLevels;

            int depthLevel = 0;

            for (std::string::size_type index = 0; index < jsonStringStream.size(); ++index)
            {
                char currentChar = jsonStringStream[index];

                if (SkipWhitespaces(currentChar, false) == true)
                {
                    continue;
                }

                havJSONToken token = CheckToken(currentChar);

                if (token != havJSONToken::None)
                {
                    if (token == havJSONToken::LeftCurlyBracket ||
                        token == havJSONToken::LeftSquareBracket)
                    {
                        currentTypeToken = token;

                        typeTokens.push_back(token);

                        if (depthLevel > 0)
                        {
                            typeTokenDepthLevels.push_back(depthLevel++);
                        }
                    }

                    mTokens.push_back(havJSONTokenValue { token, std::nullopt });
                }

                if (token == havJSONToken::RightCurlyBracket ||
                    token == havJSONToken::RightSquareBracket)
                {
                    if (typeTokens.empty() == false)
                    {
                        if (typeTokenDepthLevels.empty() == false)
                        {
                            depthLevel = typeTokenDepthLevels.back();
                            typeTokenDepthLevels.pop_back();
                        }
                        else
                        {
                            depthLevel = 0;
                        }

                        currentTypeToken = typeTokens[depthLevel];
                        typeTokens.pop_back();
                    }

                    continue;
                }

                if (token == havJSONToken::Comma)
                {
                    token = currentTypeToken;
                }

                if (token == havJSONToken::None)
                {
                    token = currentTypeToken;
                    --index;
                }

                havJSONTokenValue typeToken = CheckTypeToken(token, ++index, jsonStringStream);

                if (typeToken.mToken != havJSONToken::None)
                {
                    if (typeToken.mToken == havJSONToken::LeftCurlyBracket ||
                        typeToken.mToken == havJSONToken::LeftSquareBracket)
                    {
                        currentTypeToken = typeToken.mToken;

                        typeTokens.push_back(currentTypeToken);

                        typeTokenDepthLevels.push_back(depthLevel++);
                    }
                    else
                    {
                        mTokens.push_back(typeToken);
                    }
                }
            }

#if 0
            int leftSquareCount = 0;
            int rightSquareCount = 0;
            int leftCurlyCount = 0;
            int rightCurlyCount = 0;

            for (std::deque<havJSONTokenValue>::size_type index = 0; index < mTokens.size(); ++index)
            {
                std::string type;

                switch (mTokens[index].mToken)
                {
                case havJSONToken::None:
                    type = "None";
                    break;

                // Structural tokens
                case havJSONToken::LeftSquareBracket:
                    ++leftSquareCount;
                    type = "LeftSquareBracket";
                    break;

                case havJSONToken::LeftCurlyBracket:
                    ++leftCurlyCount;
                    type = "LeftCurlyBracket";
                    break;

                case havJSONToken::RightSquareBracket:
                    ++rightSquareCount;
                    type = "RightSquareBracket";
                    break;

                case havJSONToken::RightCurlyBracket:
                    ++rightCurlyCount;
                    type = "RightCurlyBracket";
                    break;

                case havJSONToken::Colon:
                    type = "Colon";
                    break;

                case havJSONToken::Comma:
                    type = "Comma";
                    break;

                // Generic type tokens
                case havJSONToken::Null:
                    type = "Null";
                    break;

                case havJSONToken::Boolean:
                    type = "Boolean";
                    break;

                case havJSONToken::Int:
                    type = "Int";
                    break;

                case havJSONToken::UInt:
                    type = "UInt";
                    break;

                case havJSONToken::Long:
                    type = "Long";
                    break;

                case havJSONToken::ULong:
                    type = "ULong";
                    break;

                case havJSONToken::Int64:
                    type = "Int64";
                    break;

                case havJSONToken::UInt64:
                    type = "UInt64";
                    break;

                case havJSONToken::Double:
                    type = "Double";
                    break;

                case havJSONToken::String:
                    type = "String";
                    break;

                case havJSONToken::Value:
                    type = "Value";
                    break;

                default:
                    type = "Unsupported token type!";
                }

                if (mTokens[index].mValue.has_value() == true)
                {
                    std::cout << std::setfill('0') << std::setw(5) << index << ": " << type << " -> " << mTokens[index].mValue.value() << "\n";
                }
                else
                {
                    std::cout << std::setfill('0') << std::setw(5) << index << ": " << type << "\n";
                }
            }

            std::cout << "left square count: " << leftSquareCount << "\n";
            std::cout << "right square count: " << rightSquareCount << "\n";
            std::cout << "left curly count: " << leftCurlyCount << "\n";
            std::cout << "right curly count: " << rightCurlyCount << "\n";
#endif

            return mTokens.size() > 0;
        }

        bool ParseJSONContents(havJSONData& valueNode)
        {
            if (mTokens.empty() == true)
            {
                return false;
            }

            // Check if the root node is an object or array
            if (mTokens[0].mToken != havJSONToken::LeftCurlyBracket &&
                mTokens[0].mToken != havJSONToken::LeftSquareBracket)
            {
                return false;
            }

            std::deque<havJSONData*> tokenTypeReferences;
            std::unique_ptr<havJSONData> rootNode = nullptr;

            while (mTokens.empty() == false)
            {
                havJSONTokenValue token = mTokens.front();

                if (token.mToken == havJSONToken::Comma)
                {
                    mTokens.pop_front();

                    continue;
                }

                // Check if we're dealing with an object
                if (token.mToken == havJSONToken::LeftCurlyBracket)
                {
                    if (rootNode != nullptr)
                    {
                        if (tokenTypeReferences.back()->getType() == havJSONDataType::Array)
                        {
                            std::vector<std::shared_ptr<havJSONData>>* item = std::get_if<std::vector<std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress());

                            std::map<std::string, std::shared_ptr<havJSONData>> tmpObject;
                            item->push_back(std::make_shared<havJSONData>(std::move(tmpObject), havJSONDataType::Object));

                            tokenTypeReferences.push_back(item->back().get());
                        }
                    }

                    if (rootNode == nullptr)
                    {
                        std::map<std::string, std::shared_ptr<havJSONData>> tmpObject;
                        rootNode = std::make_unique<havJSONData>(std::move(tmpObject), havJSONDataType::Object);

                        tokenTypeReferences.push_back(rootNode.get());
                    }

                    mTokens.pop_front();
                    token = mTokens.front();

                    // Check if the next token is valid
                    if (token.mToken != havJSONToken::String &&
                        token.mToken != havJSONToken::Value &&
                        token.mToken != havJSONToken::Null &&
                        token.mToken != havJSONToken::Boolean &&
                        token.mToken != havJSONToken::Int &&
                        token.mToken != havJSONToken::UInt &&
                        token.mToken != havJSONToken::Long &&
                        token.mToken != havJSONToken::ULong &&
                        token.mToken != havJSONToken::Int64 &&
                        token.mToken != havJSONToken::UInt64 &&
                        token.mToken != havJSONToken::Double &&
                        token.mToken != havJSONToken::Colon &&
                        token.mToken != havJSONToken::Comma &&
                        token.mToken != havJSONToken::RightCurlyBracket &&
                        token.mToken != havJSONToken::LeftCurlyBracket &&
                        token.mToken != havJSONToken::LeftSquareBracket)
                    {
                        return false;
                    }

                    if (token.mToken == havJSONToken::LeftCurlyBracket ||
                        token.mToken == havJSONToken::LeftSquareBracket)
                    {
                        continue;
                    }
                }

                // Check if we're dealing with an array
                if (token.mToken == havJSONToken::LeftSquareBracket)
                {
                    if (rootNode != nullptr)
                    {
                        if (tokenTypeReferences.back()->getType() == havJSONDataType::Array)
                        {
                            std::vector<std::shared_ptr<havJSONData>>* item = std::get_if<std::vector<std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress());

                            std::vector<std::shared_ptr<havJSONData>> tmpArray;
                            item->push_back(std::make_shared<havJSONData>(std::move(tmpArray), havJSONDataType::Array));

                            tokenTypeReferences.push_back(item->back().get());
                        }
                    }

                    if (rootNode == nullptr)
                    {
                        std::vector<std::shared_ptr<havJSONData>> tmpArray;
                        rootNode = std::make_unique<havJSONData>(std::move(tmpArray), havJSONDataType::Array);

                        tokenTypeReferences.push_back(rootNode.get());
                    }

                    mTokens.pop_front();
                    token = mTokens.front();

                    // Check if the next token is valid
                    if (token.mToken != havJSONToken::Value &&
                        token.mToken != havJSONToken::Null &&
                        token.mToken != havJSONToken::Boolean &&
                        token.mToken != havJSONToken::Int &&
                        token.mToken != havJSONToken::UInt &&
                        token.mToken != havJSONToken::Long &&
                        token.mToken != havJSONToken::ULong &&
                        token.mToken != havJSONToken::Int64 &&
                        token.mToken != havJSONToken::UInt64 &&
                        token.mToken != havJSONToken::Double &&
                        token.mToken != havJSONToken::Comma &&
                        token.mToken != havJSONToken::RightSquareBracket &&
                        token.mToken != havJSONToken::LeftSquareBracket &&
                        token.mToken != havJSONToken::LeftCurlyBracket)
                    {
                        return false;
                    }

                    if (token.mToken == havJSONToken::LeftSquareBracket ||
                        token.mToken == havJSONToken::LeftCurlyBracket)
                    {
                        continue;
                    }
                }

                if (token.mToken == havJSONToken::RightCurlyBracket ||
                    token.mToken == havJSONToken::RightSquareBracket)
                {
                    tokenTypeReferences.pop_back();
                }

                bool processed = false;

                // Read contents of string token
                if (token.mToken == havJSONToken::String)
                {
                    std::string key = token.mValue.value();

                    mTokens.pop_front();
                    token = mTokens.front();

                    if (token.mToken == havJSONToken::Colon)
                    {
                        mTokens.pop_front();
                        token = mTokens.front();

                        if (token.mToken == havJSONToken::Value ||
                            token.mToken == havJSONToken::Null ||
                            token.mToken == havJSONToken::Boolean ||
                            token.mToken == havJSONToken::Int ||
                            token.mToken == havJSONToken::UInt ||
                            token.mToken == havJSONToken::Long ||
                            token.mToken == havJSONToken::ULong ||
                            token.mToken == havJSONToken::Int64 ||
                            token.mToken == havJSONToken::UInt64 ||
                            token.mToken == havJSONToken::Double)
                        {
                            havJSONData value;

                            switch (token.mToken)
                            {
                            case havJSONToken::Null:
                                value = havJSONData(token.mValue.value(), havJSONDataType::Null);
                                break;

                            case havJSONToken::Boolean:
                                value = havJSONData(((token.mValue.value() == "true") ? true : false), havJSONDataType::Boolean);
                                break;

                            case havJSONToken::Int:
                                value = havJSONData(static_cast<int>(std::stol(token.mValue.value())), havJSONDataType::Int);
                                break;

                            case havJSONToken::UInt:
                                value = havJSONData(static_cast<unsigned int>(std::stoul(token.mValue.value())), havJSONDataType::UInt);
                                break;

                            case havJSONToken::Long:
                                value = havJSONData(static_cast<long>(std::stol(token.mValue.value())), havJSONDataType::Long);
                                break;

                            case havJSONToken::ULong:
                                value = havJSONData(static_cast<unsigned long>(std::stoul(token.mValue.value())), havJSONDataType::ULong);
                                break;

                            case havJSONToken::Int64:
                                value = havJSONData(static_cast<std::int64_t>(std::stoll(token.mValue.value())), havJSONDataType::Int64);
                                break;

                            case havJSONToken::UInt64:
                                value = havJSONData(static_cast<std::uint64_t>(std::stoull(token.mValue.value())), havJSONDataType::UInt64);
                                break;

                            case havJSONToken::Double:
                                value = havJSONData(static_cast<double>(std::stod(token.mValue.value())), havJSONDataType::Double);
                                break;

                            case havJSONToken::Value:
                            default:
                                value = havJSONData(token.mValue.value(), havJSONDataType::String);
                            }

                            if (tokenTypeReferences.back()->getType() == havJSONDataType::Object)
                            {
                                std::get_if<std::map<std::string, std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress())->insert({key, std::make_shared<havJSONData>(value.getValue(), value.getType())});

                                processed = true;
                            }
                            else
                            {
                                throw std::runtime_error("Invalid token! Expected object!");
                            }
                        }
                        else if (token.mToken == havJSONToken::LeftSquareBracket)
                        {
                            if (tokenTypeReferences.back()->getType() == havJSONDataType::Object)
                            {
                                std::map<std::string, std::shared_ptr<havJSONData>>* item = std::get_if<std::map<std::string, std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress());

                                std::vector<std::shared_ptr<havJSONData>> tmpArray;
                                auto itr = item->insert({key, std::make_shared<havJSONData>(std::move(tmpArray), havJSONDataType::Array)});

                                tokenTypeReferences.push_back(itr.first->second.get());

                                processed = true;
                            }
                            else
                            {
                                throw std::runtime_error("Invalid token! Expected object!");
                            }
                        }
                        else if (token.mToken == havJSONToken::LeftCurlyBracket)
                        {
                            if (tokenTypeReferences.back()->getType() == havJSONDataType::Object)
                            {
                                std::map<std::string, std::shared_ptr<havJSONData>>* item = std::get_if<std::map<std::string, std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress());

                                std::map<std::string, std::shared_ptr<havJSONData>> tmpObject;
                                auto itr = item->insert({key, std::make_shared<havJSONData>(std::move(tmpObject), havJSONDataType::Object)});

                                tokenTypeReferences.push_back(itr.first->second.get());

                                processed = true;
                            }
                            else
                            {
                                throw std::runtime_error("Invalid token! Expected object!");
                            }
                        }
                    }
                }

                if (processed == false)
                {
                    if (token.mToken == havJSONToken::Value ||
                        token.mToken == havJSONToken::Null ||
                        token.mToken == havJSONToken::Boolean ||
                        token.mToken == havJSONToken::Int ||
                        token.mToken == havJSONToken::UInt ||
                        token.mToken == havJSONToken::Long ||
                        token.mToken == havJSONToken::ULong ||
                        token.mToken == havJSONToken::Int64 ||
                        token.mToken == havJSONToken::UInt64 ||
                        token.mToken == havJSONToken::Double)
                    {
                        havJSONData value;

                        switch (token.mToken)
                        {
                        case havJSONToken::Null:
                            value = havJSONData(token.mValue.value(), havJSONDataType::Null);
                            break;

                        case havJSONToken::Boolean:
                            value = havJSONData(((token.mValue.value() == "true") ? true : false), havJSONDataType::Boolean);
                            break;

                        case havJSONToken::Int:
                            value = havJSONData(static_cast<int>(std::stol(token.mValue.value())), havJSONDataType::Int);
                            break;

                        case havJSONToken::UInt:
                            value = havJSONData(static_cast<unsigned int>(std::stoul(token.mValue.value())), havJSONDataType::UInt);
                            break;

                        case havJSONToken::Long:
                            value = havJSONData(static_cast<long>(std::stol(token.mValue.value())), havJSONDataType::Long);
                            break;

                        case havJSONToken::ULong:
                            value = havJSONData(static_cast<unsigned long>(std::stoul(token.mValue.value())), havJSONDataType::ULong);
                            break;

                        case havJSONToken::Int64:
                            value = havJSONData(static_cast<std::int64_t>(std::stoll(token.mValue.value())), havJSONDataType::Int64);
                            break;

                        case havJSONToken::UInt64:
                            value = havJSONData(static_cast<std::uint64_t>(std::stoull(token.mValue.value())), havJSONDataType::UInt64);
                            break;

                        case havJSONToken::Double:
                            value = havJSONData(static_cast<double>(std::stod(token.mValue.value())), havJSONDataType::Double);
                            break;

                        case havJSONToken::Value:
                        default:
                            value = havJSONData(token.mValue.value(), havJSONDataType::String);
                        }

                        if (tokenTypeReferences.back()->getType() == havJSONDataType::Array)
                        {
                            std::get_if<std::vector<std::shared_ptr<havJSONData>>>(tokenTypeReferences.back()->getAddress())->push_back(std::make_shared<havJSONData>(value.getValue(), value.getType()));
                        }
                        else
                        {
                            throw std::runtime_error("Invalid token! Expected array!");
                        }
                    }
                }

                mTokens.pop_front();
            }

            if (rootNode == nullptr)
            {
                throw std::runtime_error("No root node found!");
            }

            havJSONData newValueNode(rootNode->getValue(), rootNode->getType());
            valueNode = std::move(newValueNode);

            return true;
        }

        bool ParseFile(const std::string& fileName, havJSONData& valueNode, havJSONType jsonType = havJSONType::JSON)
        {
            // 1. Open file stream
#ifdef _WIN32
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(_wfopen(&ConvertStringToWString(fileName)[0], L"rb"), std::fclose);
#else
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(std::fopen(fileName.c_str(), "rb"), std::fclose);
#endif

            if (fileStream == nullptr)
            {
                if (jsonType == havJSONType::BSON)
                {
                    std::cout << "Unable to parse BSON file: " << fileName << "\n";
                }
                else
                {
                    std::cout << "Unable to parse JSON file: " << fileName << "\n";
                }

                havJSONData newValueNode;

                valueNode = std::move(newValueNode);

                return false;
            }

            // 1.5 Get file size
            std::fseek(fileStream.get(), 0, SEEK_END);
            long fileSize = std::ftell(fileStream.get());
            std::fseek(fileStream.get(), 0, SEEK_SET);

            if (fileSize == 0)
            {
                if (jsonType == havJSONType::BSON)
                {
                    std::cout << "BSON file is empty!\n";
                }
                else
                {
                    std::cout << "JSON file is empty!\n";
                }

                havJSONData newValueNode;

                valueNode = std::move(newValueNode);

                return false;
            }

            // 2. Read file contents
            std::u16string fileContentsU16;
            std::u32string fileContentsU32;
            std::string fileContents;

            // Check for file encoding and BOM (Byte order mark) - BOM is illegal in JSON, but the source JSON file could contain it regardless
            int bytesToSkip = 0;
            int skippedBytes = 0;
            havJSONBOMType bomType = havJSONBOMType::None;

            if (fileSize >= 4 && jsonType == havJSONType::JSON)
            {
                unsigned char unsignedCurrentChar;

                unsigned char bomArray[4] = { '\0' };

                // Read the first four bytes of the file
                for (int index = 0; index < 4; ++index)
                {
                    std::fread(&unsignedCurrentChar, sizeof(unsigned char), 1, fileStream.get());

                    bomArray[index] = unsignedCurrentChar;
                }

                if (bomArray[0] == 0xff && bomArray[1] == 0xfe &&
                    bomArray[2] == 0x00 && bomArray[3] == 0x00)
                {
                    bomType = havJSONBOMType::UTF32LE;

                    bytesToSkip = 4;
                }
                else if (bomArray[0] == 0x00 && bomArray[1] == 0x00 &&
                         bomArray[2] == 0xfe && bomArray[3] == 0xff)
                {
                    bomType = havJSONBOMType::UTF32BE;

                    bytesToSkip = 4;
                }
                else if (bomArray[0] == 0xff && bomArray[1] == 0xfe)
                {
                    bomType = havJSONBOMType::UTF16LE;

                    bytesToSkip = 2;
                }
                else if (bomArray[0] == 0xfe && bomArray[1] == 0xff)
                {
                    bomType = havJSONBOMType::UTF16BE;

                    bytesToSkip = 2;
                }
                else if (bomArray[0] == 0xef && bomArray[1] == 0xbb && bomArray[2] == 0xbf)
                {
                    bomType = havJSONBOMType::UTF8;

                    bytesToSkip = 3;
                }

                // If no BOM has been found, we still need to check for the file encoding
                if (bomType == havJSONBOMType::None)
                {
                    if (bomArray[0] != 0x00 && bomArray[1] == 0x00 &&
                        bomArray[2] == 0x00 && bomArray[3] == 0x00)
                    {
                        bomType = havJSONBOMType::UTF32LE;
                    }
                    else if (bomArray[0] == 0x00 && bomArray[1] == 0x00 &&
                             bomArray[2] == 0x00 && bomArray[3] != 0x00)
                    {
                        bomType = havJSONBOMType::UTF32BE;
                    }
                    else if (bomArray[0] != 0x00 && bomArray[1] == 0x00 &&
                             bomArray[2] != 0x00 && bomArray[3] == 0x00)
                    {
                        bomType = havJSONBOMType::UTF16LE;
                    }
                    else if (bomArray[0] == 0x00 && bomArray[1] != 0x00 &&
                             bomArray[2] == 0x00 && bomArray[3] != 0x00)
                    {
                        bomType = havJSONBOMType::UTF16BE;
                    }
                }

                if (bomType != havJSONBOMType::None)
                {
                    std::string bomTypeString;

                    switch (bomType)
                    {
                    case havJSONBOMType::UTF16LE:
                        bomTypeString = "UTF-16 Little Endian";
                        break;

                    case havJSONBOMType::UTF16BE:
                        bomTypeString = "UTF-16 Big Endian";
                        break;

                    case havJSONBOMType::UTF32LE:
                        bomTypeString = "UTF-32 Little Endian";
                        break;

                    case havJSONBOMType::UTF32BE:
                        bomTypeString = "UTF-32 Big Endian";
                        break;

                    default:
                        bomTypeString = "UTF-8";
                    }

                    std::cout << "File starts with " << bomTypeString << " BOM! Please note that the BOM will be skipped, and removed in case the file gets saved!\n";
                }

                std::fseek(fileStream.get(), 0, SEEK_SET);
            }

            if (bomType == havJSONBOMType::None ||
                bomType == havJSONBOMType::UTF8)
            {
                char currentChar;
                while (std::fread(&currentChar, sizeof(char), 1, fileStream.get()) > 0)
                {
                    if (bytesToSkip > 0)
                    {
                        if (skippedBytes < bytesToSkip)
                        {
                            ++skippedBytes;
                            continue;
                        }
                    }
                    fileContents += currentChar;
                }
            }
            else if (bomType == havJSONBOMType::UTF16LE ||
                     bomType == havJSONBOMType::UTF16BE)
            {
                char16_t currentCharU16;
                while (std::fread(&currentCharU16, sizeof(char16_t), 1, fileStream.get()) > 0)
                {
                    if (bytesToSkip > 0)
                    {
                        if (skippedBytes < bytesToSkip)
                        {
                            skippedBytes += sizeof(char16_t);
                            continue;
                        }
                    }
                    fileContentsU16 += currentCharU16;
                }
            }
            else if (bomType == havJSONBOMType::UTF32LE ||
                     bomType == havJSONBOMType::UTF32BE)
            {
                char32_t currentCharU32;
                while (std::fread(&currentCharU32, sizeof(char32_t), 1, fileStream.get()) > 0)
                {
                    if (bytesToSkip > 0)
                    {
                        if (skippedBytes < bytesToSkip)
                        {
                            skippedBytes += sizeof(char32_t);
                            continue;
                        }
                    }
                    fileContentsU32 += currentCharU32;
                }
            }

            // Convert the file contents to UTF-8, if necessary
            if (bomType == havJSONBOMType::UTF16LE)
            {
                fileContents = std::wstring_convert<havJSONCodeCvt<char16_t, char, std::mbstate_t>, char16_t>{}.to_bytes(fileContentsU16);
            }
            else if (bomType == havJSONBOMType::UTF16BE)
            {
                std::u16string u16ConvBE;
                for (char16_t currentChar : fileContentsU16)
                {
                    u16ConvBE += (((currentChar & 0x00ff) << 8) | ((currentChar & 0xff00) >> 8));
                }
                fileContents = std::wstring_convert<havJSONCodeCvt<char16_t, char, std::mbstate_t>, char16_t>{}.to_bytes(u16ConvBE);
            }
            else if (bomType == havJSONBOMType::UTF32LE)
            {
                fileContents = std::wstring_convert<havJSONCodeCvt<char32_t, char, std::mbstate_t>, char32_t>{}.to_bytes(fileContentsU32);
            }
            else if (bomType == havJSONBOMType::UTF32BE)
            {
                std::u32string u32ConvBE;
                for (char32_t currentChar : fileContentsU32)
                {
                    u32ConvBE += (((currentChar & 0x000000ff) << 24) |
                                  ((currentChar & 0x0000ff00) << 8) |
                                  ((currentChar & 0x00ff0000) >> 8) |
                                  ((currentChar & 0xff000000) >> 24));
                }
                fileContents = std::wstring_convert<havJSONCodeCvt<char32_t, char, std::mbstate_t>, char32_t>{}.to_bytes(u32ConvBE);
            }

            if (jsonType == havJSONType::BSON)
            {
                // 2.5 Convert BSON to JSON
                fileContents = ConvertBSONToJSON(fileContents);

                if (fileContents.empty() == true)
                {
                    std::cout << "Unable to parse BSON file: " << fileName << "\n";

                    havJSONData newValueNode;

                    valueNode = std::move(newValueNode);

                    return false;
                }
            }

            // 3. Tokenization phase
            if (Tokenization(fileContents) == true)
            {
                // 4. Parse JSON contents
                if (ParseJSONContents(valueNode) == true)
                {
                    return true;
                }
            }

            // 5. Return JSON contents as havJSONData object
            havJSONData newValueNode;

            valueNode = std::move(newValueNode);

            return false;
        }

        bool ParseContent(const std::string& fileContents, havJSONData& valueNode)
        {
            // 1. Tokenization phase
            if (Tokenization(fileContents) == true)
            {
                // 2. Parse JSON contents
                if (ParseJSONContents(valueNode) == true)
                {
                    return true;
                }
            }

            // 3. Return JSON contents as havJSONData object
            havJSONData newValueNode;

            valueNode = std::move(newValueNode);

            return false;
        }

        void TokenizeArray(std::vector<std::shared_ptr<havJSONData>>& rootArray, std::deque<havJSONTokenValue>& tokens)
        {
            for (std::vector<std::shared_ptr<havJSONData>>::size_type index = 0; index < rootArray.size(); ++index)
            {
                if (index > 0)
                {
                    tokens.push_back(havJSONTokenValue { havJSONToken::Comma, std::nullopt });
                }

                const havJSONData* currentValue = rootArray[index].get();

                switch (currentValue->getType())
                {
                    case havJSONDataType::Null:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Null, std::get<std::string>(currentValue->getValue()) });
                        break;

                    case havJSONDataType::Boolean:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Boolean, std::get<bool>(currentValue->getValue()) ? "true" : "false" });
                        break;

                    case havJSONDataType::Int:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Int, std::to_string(std::get<int>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::UInt:
                        tokens.push_back(havJSONTokenValue { havJSONToken::UInt, std::to_string(std::get<unsigned int>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::Long:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Long, std::to_string(std::get<long>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::ULong:
                        tokens.push_back(havJSONTokenValue { havJSONToken::ULong, std::to_string(std::get<unsigned long>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::Int64:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Int64, std::to_string(std::get<std::int64_t>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::UInt64:
                        tokens.push_back(havJSONTokenValue { havJSONToken::UInt64, std::to_string(std::get<std::uint64_t>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::Double:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Double, std::to_string(std::get<double>(currentValue->getValue())) });
                        break;

                    case havJSONDataType::String:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Value, std::get<std::string>(currentValue->getValue()) });
                        break;

                    case havJSONDataType::Array:
                        {
                            tokens.push_back(havJSONTokenValue { havJSONToken::LeftSquareBracket, std::nullopt });

                            std::vector<std::shared_ptr<havJSONData>> newArray = std::get<std::vector<std::shared_ptr<havJSONData>>>(currentValue->getValue());

                            TokenizeArray(newArray, tokens);
                        }
                        break;

                    case havJSONDataType::Object:
                        {
                            tokens.push_back(havJSONTokenValue { havJSONToken::LeftCurlyBracket, std::nullopt });

                            std::map<std::string, std::shared_ptr<havJSONData>> rootObject = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(currentValue->getValue());

                            TokenizeObject(rootObject, tokens);
                        }
                        break;
                }
            }

            tokens.push_back(havJSONTokenValue { havJSONToken::RightSquareBracket, std::nullopt });
        }

        void TokenizeObject(std::map<std::string, std::shared_ptr<havJSONData>>& rootObject, std::deque<havJSONTokenValue>& tokens)
        {
            for (std::map<std::string, std::shared_ptr<havJSONData>>::iterator itr = rootObject.begin(); itr != rootObject.end(); ++itr)
            {
                if (itr != rootObject.begin())
                {
                    tokens.push_back(havJSONTokenValue { havJSONToken::Comma, std::nullopt });
                }

                const havJSONData* item = rootObject[(*itr).first].get();

                tokens.push_back(havJSONTokenValue { havJSONToken::String, (*itr).first });
                tokens.push_back(havJSONTokenValue { havJSONToken::Colon, std::nullopt });

                switch (item->getType())
                {
                    case havJSONDataType::Null:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Null, std::get<std::string>(item->getValue()) });
                        break;

                    case havJSONDataType::Boolean:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Boolean, std::get<bool>(item->getValue()) ? "true" : "false" });
                        break;

                    case havJSONDataType::Int:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Int, std::to_string(std::get<int>(item->getValue())) });
                        break;

                    case havJSONDataType::UInt:
                        tokens.push_back(havJSONTokenValue { havJSONToken::UInt, std::to_string(std::get<unsigned int>(item->getValue())) });
                        break;

                    case havJSONDataType::Long:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Long, std::to_string(std::get<long>(item->getValue())) });
                        break;

                    case havJSONDataType::ULong:
                        tokens.push_back(havJSONTokenValue { havJSONToken::ULong, std::to_string(std::get<unsigned long>(item->getValue())) });
                        break;

                    case havJSONDataType::Int64:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Int64, std::to_string(std::get<std::int64_t>(item->getValue())) });
                        break;

                    case havJSONDataType::UInt64:
                        tokens.push_back(havJSONTokenValue { havJSONToken::UInt64, std::to_string(std::get<std::uint64_t>(item->getValue())) });
                        break;

                    case havJSONDataType::Double:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Double, std::to_string(std::get<double>(item->getValue())) });
                        break;

                    case havJSONDataType::String:
                        tokens.push_back(havJSONTokenValue { havJSONToken::Value, std::get<std::string>(item->getValue()) });
                        break;

                    case havJSONDataType::Array:
                        {
                            tokens.push_back(havJSONTokenValue { havJSONToken::LeftSquareBracket, std::nullopt });

                            std::vector<std::shared_ptr<havJSONData>> rootArray = std::get<std::vector<std::shared_ptr<havJSONData>>>(item->getValue());

                            TokenizeArray(rootArray, tokens);
                        }
                        break;

                    case havJSONDataType::Object:
                        {
                            tokens.push_back(havJSONTokenValue { havJSONToken::LeftCurlyBracket, std::nullopt });

                            std::map<std::string, std::shared_ptr<havJSONData>> newObject = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(item->getValue());

                            TokenizeObject(newObject, tokens);
                        }
                        break;
                }
            }

            tokens.push_back(havJSONTokenValue { havJSONToken::RightCurlyBracket, std::nullopt });
        }

        bool Tokenization(havJSONData& rootNode, std::deque<havJSONTokenValue>& tokens)
        {
            havJSONToken currentTypeToken = havJSONToken::None;

            if (rootNode.isArray() == true)
            {
                currentTypeToken = havJSONToken::LeftSquareBracket;
            }

            if (rootNode.isObject() == true)
            {
                currentTypeToken = havJSONToken::LeftCurlyBracket;
            }

            if (currentTypeToken == havJSONToken::None)
            {
                throw std::runtime_error("Invalid token: Expected array or object as root node!");
            }

            tokens.push_back(havJSONTokenValue { currentTypeToken, std::nullopt });

            if (rootNode.isArray() == true)
            {
                std::vector<std::shared_ptr<havJSONData>> rootArray = std::get<std::vector<std::shared_ptr<havJSONData>>>(rootNode.getValue());

                TokenizeArray(rootArray, tokens);
            }

            if (rootNode.isObject() == true)
            {
                std::map<std::string, std::shared_ptr<havJSONData>> rootObject = std::get<std::map<std::string, std::shared_ptr<havJSONData>>>(rootNode.getValue());

                TokenizeObject(rootObject, tokens);
            }

            return tokens.size() > 0;
        }

        bool ConvertJSONToString(havJSONData& valueNode, std::string& jsonContentsAsString, bool formatted = false)
        {
            std::deque<havJSONTokenValue> tokens;

            // 1. Tokenization phase
            if (Tokenization(valueNode, tokens) == true)
            {
                // 2. Write contents to string
                if (tokens[0].mToken != havJSONToken::LeftCurlyBracket &&
                    tokens[0].mToken != havJSONToken::LeftSquareBracket)
                {
                    return false;
                }

                int tokenTypesSize = 0;

                char currentChar;

                std::deque<havJSONDataType> tokenTypes;

                auto itr = tokens.begin();

                havJSONToken previousToken = havJSONToken::None;
                havJSONDataType currentType = havJSONDataType::Null;

                if (tokens[0].mToken == havJSONToken::LeftCurlyBracket)
                {
                    currentType = havJSONDataType::Object;
                }

                if (tokens[0].mToken == havJSONToken::LeftSquareBracket)
                {
                    currentType = havJSONDataType::Array;
                }

                while (itr != tokens.end())
                {
                    if ((*itr).mToken == havJSONToken::RightCurlyBracket ||
                        (*itr).mToken == havJSONToken::RightSquareBracket)
                    {
                        currentType = tokenTypes.back();
                        tokenTypes.pop_back();
                        tokenTypesSize = tokenTypes.size();
                    }

                    if (formatted == true)
                    {
                        if (currentType == havJSONDataType::Array)
                        {
                            if (previousToken == havJSONToken::LeftSquareBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::RightSquareBracket &&
                                (*itr).mToken == havJSONToken::RightCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if ((previousToken == havJSONToken::Colon && (*itr).mToken == havJSONToken::LeftCurlyBracket) ||
                                (previousToken == havJSONToken::Comma && (*itr).mToken == havJSONToken::LeftCurlyBracket))
                            {
                                currentChar = ' ';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::LeftCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::RightCurlyBracket &&
                                (*itr).mToken == havJSONToken::RightSquareBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if ((previousToken == havJSONToken::Colon && (*itr).mToken == havJSONToken::LeftSquareBracket) ||
                                (previousToken == havJSONToken::Comma && (*itr).mToken == havJSONToken::LeftSquareBracket))
                            {
                                currentChar = ' ';

                                jsonContentsAsString += currentChar;
                            }

                            if ((*itr).mToken == havJSONToken::String &&
                                previousToken == havJSONToken::Comma)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::Colon && ((*itr).mToken == havJSONToken::Value ||
                                (*itr).mToken == havJSONToken::Null ||
                                (*itr).mToken == havJSONToken::Boolean ||
                                (*itr).mToken == havJSONToken::Int ||
                                (*itr).mToken == havJSONToken::UInt ||
                                (*itr).mToken == havJSONToken::Long ||
                                (*itr).mToken == havJSONToken::ULong ||
                                (*itr).mToken == havJSONToken::Int64 ||
                                (*itr).mToken == havJSONToken::UInt64 ||
                                (*itr).mToken == havJSONToken::Double))
                            {
                                currentChar = ' ';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::Comma && ((*itr).mToken == havJSONToken::Value ||
                                (*itr).mToken == havJSONToken::Null ||
                                (*itr).mToken == havJSONToken::Boolean ||
                                (*itr).mToken == havJSONToken::Int ||
                                (*itr).mToken == havJSONToken::UInt ||
                                (*itr).mToken == havJSONToken::Long ||
                                (*itr).mToken == havJSONToken::ULong ||
                                (*itr).mToken == havJSONToken::Int64 ||
                                (*itr).mToken == havJSONToken::UInt64 ||
                                (*itr).mToken == havJSONToken::Double))
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (((*itr).mToken == havJSONToken::Value ||
                                 (*itr).mToken == havJSONToken::Null ||
                                 (*itr).mToken == havJSONToken::Boolean ||
                                 (*itr).mToken == havJSONToken::Int ||
                                 (*itr).mToken == havJSONToken::UInt ||
                                 (*itr).mToken == havJSONToken::Long ||
                                 (*itr).mToken == havJSONToken::ULong ||
                                 (*itr).mToken == havJSONToken::Int64 ||
                                 (*itr).mToken == havJSONToken::UInt64 ||
                                 (*itr).mToken == havJSONToken::Double) &&
                                (previousToken == havJSONToken::RightSquareBracket ||
                                 previousToken == havJSONToken::RightCurlyBracket))
                            {
                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if ((previousToken == havJSONToken::Value ||
                                 previousToken == havJSONToken::Null ||
                                 previousToken == havJSONToken::Boolean ||
                                 previousToken == havJSONToken::Int ||
                                 previousToken == havJSONToken::UInt ||
                                 previousToken == havJSONToken::Long ||
                                 previousToken == havJSONToken::ULong ||
                                 previousToken == havJSONToken::Int64 ||
                                 previousToken == havJSONToken::UInt64 ||
                                 previousToken == havJSONToken::Double) &&
                                ((*itr).mToken == havJSONToken::RightSquareBracket ||
                                 (*itr).mToken == havJSONToken::RightCurlyBracket))
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }
                        }

                        if (currentType == havJSONDataType::Object)
                        {
                            if (previousToken == havJSONToken::LeftSquareBracket &&
                                (*itr).mToken == havJSONToken::LeftCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::RightSquareBracket &&
                                (*itr).mToken == havJSONToken::RightSquareBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::RightCurlyBracket &&
                                (*itr).mToken == havJSONToken::RightSquareBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::Comma ||
                                previousToken == havJSONToken::LeftCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::RightSquareBracket &&
                                (*itr).mToken == havJSONToken::RightCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::LeftSquareBracket && ((*itr).mToken == havJSONToken::Value ||
                                (*itr).mToken == havJSONToken::Null ||
                                (*itr).mToken == havJSONToken::Boolean ||
                                (*itr).mToken == havJSONToken::Int ||
                                (*itr).mToken == havJSONToken::UInt ||
                                (*itr).mToken == havJSONToken::Long ||
                                (*itr).mToken == havJSONToken::ULong ||
                                (*itr).mToken == havJSONToken::Int64 ||
                                (*itr).mToken == havJSONToken::UInt64 ||
                                (*itr).mToken == havJSONToken::Double ||
                                (*itr).mToken == havJSONToken::String))
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            if (previousToken == havJSONToken::Comma && ((*itr).mToken == havJSONToken::Value ||
                                (*itr).mToken == havJSONToken::Null ||
                                (*itr).mToken == havJSONToken::Boolean ||
                                (*itr).mToken == havJSONToken::Int ||
                                (*itr).mToken == havJSONToken::UInt ||
                                (*itr).mToken == havJSONToken::Long ||
                                (*itr).mToken == havJSONToken::ULong ||
                                (*itr).mToken == havJSONToken::Int64 ||
                                (*itr).mToken == havJSONToken::UInt64 ||
                                (*itr).mToken == havJSONToken::Double))
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::Colon &&
                                (*itr).mToken == havJSONToken::LeftCurlyBracket)
                            {
                                currentChar = ' ';

                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken == havJSONToken::RightCurlyBracket &&
                                (*itr).mToken == havJSONToken::RightCurlyBracket)
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;
                            }

                            if ((previousToken == havJSONToken::Value ||
                                 previousToken == havJSONToken::Null ||
                                 previousToken == havJSONToken::Boolean ||
                                 previousToken == havJSONToken::Int ||
                                 previousToken == havJSONToken::UInt ||
                                 previousToken == havJSONToken::Long ||
                                 previousToken == havJSONToken::ULong ||
                                 previousToken == havJSONToken::Int64 ||
                                 previousToken == havJSONToken::UInt64 ||
                                 previousToken == havJSONToken::Double) &&
                                ((*itr).mToken == havJSONToken::RightSquareBracket ||
                                 (*itr).mToken == havJSONToken::RightCurlyBracket))
                            {
                                currentChar = '\n';

                                jsonContentsAsString += currentChar;

                                currentChar = ' ';

                                for (int index = 0; index < tokenTypesSize * 4; ++index)
                                {
                                    jsonContentsAsString += currentChar;
                                }
                            }

                            currentChar = ' ';

                            if (previousToken == havJSONToken::Colon &&
                                (*itr).mToken == havJSONToken::LeftSquareBracket)
                            {
                                jsonContentsAsString += currentChar;
                            }

                            if (previousToken != havJSONToken::Colon &&
                                (*itr).mToken != havJSONToken::Value &&
                                (*itr).mToken != havJSONToken::Null &&
                                (*itr).mToken != havJSONToken::Boolean &&
                                (*itr).mToken != havJSONToken::Int &&
                                (*itr).mToken != havJSONToken::UInt &&
                                (*itr).mToken != havJSONToken::Long &&
                                (*itr).mToken != havJSONToken::ULong &&
                                (*itr).mToken != havJSONToken::Int64 &&
                                (*itr).mToken != havJSONToken::UInt64 &&
                                (*itr).mToken != havJSONToken::Double)
                            {
                                if ((*itr).mToken != havJSONToken::Colon &&
                                    (*itr).mToken != havJSONToken::Comma)
                                {
                                    if (previousToken != havJSONToken::Value &&
                                        previousToken != havJSONToken::Null &&
                                        previousToken != havJSONToken::Boolean &&
                                        previousToken != havJSONToken::Int &&
                                        previousToken != havJSONToken::UInt &&
                                        previousToken != havJSONToken::Long &&
                                        previousToken != havJSONToken::ULong &&
                                        previousToken != havJSONToken::Int64 &&
                                        previousToken != havJSONToken::UInt64 &&
                                        previousToken != havJSONToken::Double &&
                                        (*itr).mToken != havJSONToken::RightSquareBracket)
                                    {
                                        for (int index = 0; index < tokenTypesSize * 4; ++index)
                                        {
                                            jsonContentsAsString += currentChar;
                                        }
                                    }
                                }
                            }

                            if ((previousToken == havJSONToken::Comma || previousToken == havJSONToken::Colon) &&
                                ((*itr).mToken == havJSONToken::Value ||
                                 (*itr).mToken == havJSONToken::Null ||
                                 (*itr).mToken == havJSONToken::Boolean ||
                                 (*itr).mToken == havJSONToken::Int ||
                                 (*itr).mToken == havJSONToken::UInt ||
                                 (*itr).mToken == havJSONToken::Long ||
                                 (*itr).mToken == havJSONToken::ULong ||
                                 (*itr).mToken == havJSONToken::Int64 ||
                                 (*itr).mToken == havJSONToken::UInt64 ||
                                 (*itr).mToken == havJSONToken::Double))
                            {
                                jsonContentsAsString += currentChar;
                            }
                        }
                    }

                    if ((*itr).mToken != havJSONToken::Value &&
                        (*itr).mToken != havJSONToken::Null &&
                        (*itr).mToken != havJSONToken::Boolean &&
                        (*itr).mToken != havJSONToken::Int &&
                        (*itr).mToken != havJSONToken::UInt &&
                        (*itr).mToken != havJSONToken::Long &&
                        (*itr).mToken != havJSONToken::ULong &&
                        (*itr).mToken != havJSONToken::Int64 &&
                        (*itr).mToken != havJSONToken::UInt64 &&
                        (*itr).mToken != havJSONToken::Double &&
                        (*itr).mToken != havJSONToken::String)
                    {
                        currentChar = GetTokenAsCharacter((*itr).mToken);

                        jsonContentsAsString += currentChar;
                    }
                    else
                    {
                        std::string value;

                        if ((*itr).mToken == havJSONToken::String ||
                            (*itr).mToken == havJSONToken::Value)
                        {
                            value = "\"";
                            value += ConvertToEscapedString((*itr).mValue.value());
                            value += "\"";
                        }
                        else
                        {
                            value = (*itr).mValue.value();
                        }

                        jsonContentsAsString += value;
                    }

                    if ((*itr).mToken == havJSONToken::LeftCurlyBracket ||
                        (*itr).mToken == havJSONToken::LeftSquareBracket)
                    {
                        if ((*itr).mToken == havJSONToken::LeftCurlyBracket)
                        {
                            currentType = havJSONDataType::Object;

                            tokenTypes.push_back(currentType);
                        }

                        if ((*itr).mToken == havJSONToken::LeftSquareBracket)
                        {
                            currentType = havJSONDataType::Array;

                            tokenTypes.push_back(currentType);
                        }

                        tokenTypesSize = tokenTypes.size();
                    }

                    previousToken = (*itr).mToken;

                    ++itr;
                }

                return true;
            }

            return false;
        }

        // Note: BOM is illegal in JSON!
        bool WriteJSONFile(const std::string& fileName, havJSONData& valueNode, std::string& jsonContentsAsString, bool formatted = false)
        {
            // 1. Open file stream
#ifdef _WIN32
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(_wfopen(&ConvertStringToWString(fileName)[0], L"wb"), std::fclose);
#else
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(std::fopen(fileName.c_str(), "wb"), std::fclose);
#endif

            if (fileStream == nullptr)
            {
                std::cout << "Unable to write JSON file: " << fileName << "\n";

                return false;
            }

            // 2. Convert JSON to string
            if (ConvertJSONToString(valueNode, jsonContentsAsString, formatted) == true)
            {
                std::fwrite(jsonContentsAsString.data(), sizeof(char), jsonContentsAsString.size(), fileStream.get());

                return true;
            }

            return false;
        }

        bool ConvertJSONToBSON(havJSONData& valueNode, std::vector<char>& jsonContentsAsBinaryStream)
        {
            std::deque<havJSONTokenValue> tokens;

            // 1. Tokenization phase
            if (Tokenization(valueNode, tokens) == true)
            {
                // 2. Write contents to binary stream
                if (tokens[0].mToken != havJSONToken::LeftCurlyBracket)
                {
                    throw std::runtime_error("BSON document must be an object!");
                }

                // Remove unnecessary tokens and change unsupported data types where possible
                auto itr = tokens.begin();

                while (itr != tokens.end())
                {
                    if ((*itr).mToken == havJSONToken::Colon ||
                        (*itr).mToken == havJSONToken::Comma)
                    {
                        itr = tokens.erase(itr);
                    }
                    else
                    {
                        if ((*itr).mToken == havJSONToken::UInt)
                        {
                            (*itr).mToken = havJSONToken::Int;
                        }

                        if ((*itr).mToken == havJSONToken::Long)
                        {
                            (*itr).mToken = havJSONToken::Int64;
                        }

                        if ((*itr).mToken == havJSONToken::ULong)
                        {
                            (*itr).mToken = havJSONToken::UInt64;
                        }

                        ++itr;
                    }
                }

                std::deque<havJSONBSONDataType> tokenTypes;

                itr = tokens.begin();

                if ((*itr).mToken == havJSONToken::LeftCurlyBracket)
                {
                    tokenTypes.push_back({ havJSONDataType::Object, 0, 0, 0 });
                }

                ++itr;

                int fileIndex = 0;

                while (itr != tokens.end())
                {
                    havJSONBSONDataType& currentType = tokenTypes.back();

                    if ((*itr).mToken == havJSONToken::LeftCurlyBracket ||
                        (*itr).mToken == havJSONToken::LeftSquareBracket)
                    {
                        if ((*itr).mToken == havJSONToken::LeftCurlyBracket)
                        {
                            tokenTypes.push_back({ havJSONDataType::Object, fileIndex, 0, 0 });

                            ++itr;

                            continue;
                        }

                        if ((*itr).mToken == havJSONToken::LeftSquareBracket)
                        {
                            tokenTypes.push_back({ havJSONDataType::Array, fileIndex, 4, 0 });

                            ++itr;

                            continue;
                        }
                    }

                    if ((*itr).mToken == havJSONToken::RightCurlyBracket ||
                        (*itr).mToken == havJSONToken::RightSquareBracket)
                    {
                        if ((*itr).mToken == havJSONToken::RightSquareBracket)
                        {
                            std::stringstream stream;

                            stream << std::hex << std::setw(8) << std::setfill('0') << ++currentType.mArrayTotalSize;

                            std::string stringOutput = stream.str();

                            int currIndex = 0;

                            for (int sindex = stringOutput.size() - 2; sindex >= 0; sindex -= 2)
                            {
                                std::string twoChar = stringOutput.substr(sindex, 2);
                                char byte = std::strtol(twoChar.c_str(), 0, 16);
                                jsonContentsAsBinaryStream.insert(jsonContentsAsBinaryStream.begin() + currentType.mIndexToWriteTotalSizeTo + currIndex, byte);
                                ++currIndex;
                            }

                            fileIndex += 4;
                        }

                        jsonContentsAsBinaryStream.push_back(0x00);

                        tokenTypes.pop_back();

                        ++fileIndex;

                        ++itr;

                        continue;
                    }

                    if ((*itr).mToken == havJSONToken::Value ||
                        (*itr).mToken == havJSONToken::Null ||
                        (*itr).mToken == havJSONToken::Boolean ||
                        (*itr).mToken == havJSONToken::Int ||
                        (*itr).mToken == havJSONToken::UInt64 ||
                        (*itr).mToken == havJSONToken::Int64 ||
                        (*itr).mToken == havJSONToken::Double ||
                        (*itr).mToken == havJSONToken::String ||
                        (*itr).mToken == havJSONToken::LeftCurlyBracket ||
                        (*itr).mToken == havJSONToken::LeftSquareBracket ||
                        (*itr).mToken == havJSONToken::RightCurlyBracket ||
                        (*itr).mToken == havJSONToken::RightSquareBracket)
                    {
                        // The data type must be known beforehand, so peek at the next token
                        if (currentType.mDataType != havJSONDataType::Array)
                        {
                            ++itr;
                        }

                        havJSONTokenValue nextToken = (*itr);

                        // Return to current token
                        if (currentType.mDataType != havJSONDataType::Array)
                        {
                            --itr;
                        }

                        switch (nextToken.mToken)
                        {
                            case havJSONToken::Value:
                                // String data type
                                jsonContentsAsBinaryStream.push_back(0x02);
                                break;

                            case havJSONToken::Null:
                                jsonContentsAsBinaryStream.push_back(0x0a);
                                break;

                            case havJSONToken::Boolean:
                                jsonContentsAsBinaryStream.push_back(0x08);
                                break;

                            case havJSONToken::Int:
                                jsonContentsAsBinaryStream.push_back(0x10);
                                break;

                            case havJSONToken::UInt64:
                                jsonContentsAsBinaryStream.push_back(0x11);
                                break;

                            case havJSONToken::Int64:
                                jsonContentsAsBinaryStream.push_back(0x12);
                                break;

                            case havJSONToken::Double:
                                jsonContentsAsBinaryStream.push_back(0x01);
                                break;

                            case havJSONToken::LeftSquareBracket:
                                jsonContentsAsBinaryStream.push_back(0x04);
                                break;

                            case havJSONToken::String:
                            case havJSONToken::LeftCurlyBracket:
                            case havJSONToken::RightCurlyBracket:
                            case havJSONToken::RightSquareBracket:
                                // Ignore
                                break;

                            default:
                                throw std::runtime_error("Unsupported token!");
                        }

                        if (nextToken.mToken != havJSONToken::RightCurlyBracket &&
                            nextToken.mToken != havJSONToken::RightSquareBracket)
                        {
                            ++fileIndex;

                            if (currentType.mDataType == havJSONDataType::Array)
                            {
                                ++currentType.mArrayTotalSize;
                            }
                        }

                        if (currentType.mDataType == havJSONDataType::Array)
                        {
                            // Integer to HEX string conversion
                            std::stringstream stream;

                            stream << std::hex << currentType.mCurrentArrayItemIndex;

                            std::string stringOutput = stream.str();

                            for (std::string::size_type sindex = 0; sindex < 2; ++sindex)
                            {
                                if (sindex < stringOutput.size())
                                {
                                    jsonContentsAsBinaryStream.push_back(stringOutput[sindex]);
                                }
                                else
                                {
                                    jsonContentsAsBinaryStream.push_back(0x00);
                                }
                            }

                            fileIndex += 2;

                            currentType.mArrayTotalSize += 2;
                        }

                        switch ((*itr).mToken)
                        {
                            case havJSONToken::Value:
                                {
                                    // String size
                                    std::string stringOutput = CodePointToString((*itr).mValue.value().size() + 1);

                                    for (std::string::size_type sindex = 0; sindex < 4; ++sindex)
                                    {
                                        if (sindex < stringOutput.size())
                                        {
                                            jsonContentsAsBinaryStream.push_back(stringOutput[sindex]);
                                        }
                                        else
                                        {
                                            jsonContentsAsBinaryStream.push_back(0x00);
                                        }
                                    }

                                    // String
                                    std::string stringValue = ConvertToEscapedString((*itr).mValue.value());
                                    for (std::string::size_type stringIndex = 0; stringIndex < stringValue.size(); ++stringIndex)
                                    {
                                        jsonContentsAsBinaryStream.push_back(stringValue[stringIndex]);
                                    }

                                    // Null terminator
                                    jsonContentsAsBinaryStream.push_back(0x00);

                                    fileIndex += (4 + stringValue.size() + 1);

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += (4 + stringValue.size() + 1);
                                    }
                                }
                                break;

                            case havJSONToken::Boolean:
                                {
                                    if ((*itr).mValue.value() == "true")
                                    {
                                        // "true" value
                                        jsonContentsAsBinaryStream.push_back(0x01);
                                    }
                                    else
                                    {
                                        // "false" value
                                        jsonContentsAsBinaryStream.push_back(0x00);
                                    }

                                    ++fileIndex;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        ++currentType.mArrayTotalSize;
                                    }
                                }
                                break;

                            case havJSONToken::Int:
                                {
                                    std::stringstream stream;

                                    stream << std::hex << std::setw(8) << std::setfill('0') << std::stoi((*itr).mValue.value());

                                    std::string stringOutput = stream.str();

                                    for (int sindex = stringOutput.size() - 2; sindex >= 0; sindex -= 2)
                                    {
                                        std::string twoChar = stringOutput.substr(sindex, 2);
                                        char byte = std::strtol(twoChar.c_str(), 0, 16);
                                        jsonContentsAsBinaryStream.push_back(byte);
                                    }

                                    fileIndex += 4;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += 4;
                                    }
                                }
                                break;

                            case havJSONToken::UInt64:
                                {
                                    std::stringstream stream;

                                    stream << std::hex << std::setw(16) << std::setfill('0') << std::stoull((*itr).mValue.value());

                                    std::string stringOutput = stream.str();

                                    for (int sindex = stringOutput.size() - 2; sindex >= 0; sindex -= 2)
                                    {
                                        std::string twoChar = stringOutput.substr(sindex, 2);
                                        char byte = std::strtol(twoChar.c_str(), 0, 16);
                                        jsonContentsAsBinaryStream.push_back(byte);
                                    }

                                    fileIndex += 8;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += 8;
                                    }
                                }
                                break;

                            case havJSONToken::Int64:
                                {
                                    std::stringstream stream;

                                    stream << std::hex << std::setw(16) << std::setfill('0') << std::stoll((*itr).mValue.value());

                                    std::string stringOutput = stream.str();

                                    for (int sindex = stringOutput.size() - 2; sindex >= 0; sindex -= 2)
                                    {
                                        std::string twoChar = stringOutput.substr(sindex, 2);
                                        char byte = std::strtol(twoChar.c_str(), 0, 16);
                                        jsonContentsAsBinaryStream.push_back(byte);
                                    }

                                    fileIndex += 8;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += 8;
                                    }
                                }
                                break;

                            case havJSONToken::Double:
                                {
                                    // Double to HEX converion
                                    std::stringstream stream;

                                    std::int64_t integerValue;

                                    double doubleValue = std::stod((*itr).mValue.value());

                                    std::memcpy(&integerValue, &doubleValue, sizeof(std::int64_t));

                                    stream << std::hex << integerValue;

                                    std::string stringOutput = stream.str();

                                    for (int sindex = stringOutput.size() - 2; sindex >= 0; sindex -= 2)
                                    {
                                        std::string twoChar = stringOutput.substr(sindex, 2);
                                        char byte = std::strtol(twoChar.c_str(), 0, 16);
                                        jsonContentsAsBinaryStream.push_back(byte);
                                    }

                                    fileIndex += 8;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += 8;
                                    }
                                }
                                break;

                            case havJSONToken::String:
                                {
                                    // String
                                    std::string stringValue = ConvertToEscapedString((*itr).mValue.value());
                                    for (std::string::size_type sindex = 0; sindex < stringValue.size(); ++sindex)
                                    {
                                        jsonContentsAsBinaryStream.push_back(stringValue[sindex]);
                                    }

                                    // Null terminator
                                    jsonContentsAsBinaryStream.push_back(0x00);

                                    fileIndex += (stringValue.size() + 1);

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        currentType.mArrayTotalSize += (stringValue.size() + 1);
                                    }
                                }
                                break;

                            case havJSONToken::RightCurlyBracket:
                            case havJSONToken::RightSquareBracket:
                                {
                                    jsonContentsAsBinaryStream.push_back(0x00);

                                    ++fileIndex;

                                    if (currentType.mDataType == havJSONDataType::Array)
                                    {
                                        ++currentType.mArrayTotalSize;
                                    }
                                }
                                break;

                            case havJSONToken::Null:
                            case havJSONToken::LeftCurlyBracket:
                            case havJSONToken::LeftSquareBracket:
                                // Ignore
                                break;

                            default:
                                // Unlikely, but we want to make the compiler happy
                                throw std::runtime_error("Unsupported token!");
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unsupported token!");
                    }

                    if (currentType.mDataType == havJSONDataType::Array)
                    {
                        ++currentType.mCurrentArrayItemIndex;
                    }

                    ++itr;
                }

                return true;
            }

            return false;
        }

        bool WriteBSONFile(const std::string& fileName, havJSONData& valueNode, std::vector<char>& jsonContentsAsBinaryStream)
        {
            // 1. Open file stream
#ifdef _WIN32
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(_wfopen(&ConvertStringToWString(fileName)[0], L"wb"), std::fclose);
#else
            std::unique_ptr<std::FILE, decltype(&std::fclose)> fileStream(std::fopen(fileName.c_str(), "wb"), std::fclose);
#endif

            if (fileStream == nullptr)
            {
                std::cout << "Unable to write BSON file: " << fileName << "\n";

                return false;
            }

            // 2. Convert JSON to BSON
            if (ConvertJSONToBSON(valueNode, jsonContentsAsBinaryStream) == true)
            {
                // Write file size to binary file (+ 4 = file size itself)
                std::int32_t fileSize = jsonContentsAsBinaryStream.size() + 4; // Could result into an overflow. Oh well...
                std::fwrite(&fileSize, sizeof(std::int32_t), 1, fileStream.get());

                // Write file contents to binary file
                for (std::string::size_type index = 0; index < jsonContentsAsBinaryStream.size(); ++index)
                {
                    std::fwrite(&jsonContentsAsBinaryStream[index], sizeof(jsonContentsAsBinaryStream[index]), 1, fileStream.get());
                }

                return true;
            }

            return false;
        }

#ifdef _WIN32
        std::wstring ConvertStringToWString(const std::string& value)
        {
            int numOfChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);

            std::wstring wstr(numOfChars, 0);

            numOfChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, &wstr[0], numOfChars);

            if (numOfChars > 0)
            {
                return wstr;
            }

            return std::wstring();
        }
#endif

    private:
        template<class internT, class externT, class stateT>
        struct havJSONCodeCvt : std::codecvt<internT, externT, stateT>
        {
            ~havJSONCodeCvt() {}
        };

        std::deque<havJSONTokenValue> mTokens;
    };
}

#endif
