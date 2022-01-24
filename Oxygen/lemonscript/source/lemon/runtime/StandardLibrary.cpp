/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2022 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "lemon/pch.h"
#include "lemon/runtime/StandardLibrary.h"
#include "lemon/program/FunctionWrapper.h"
#include "lemon/program/Module.h"


namespace
{
	class FastStringStream
	{
	public:
		void clear()
		{
			mLength = 0;
		}

		void addChar(char ch)
		{
			mBuffer[mLength] = ch;
			++mLength;
		}

		void addString(const char* str, int length)
		{
			memcpy(&mBuffer[mLength], str, length);
			mLength += length;
		}

		void addString(const std::string& str)
		{
			addString(str.c_str(), (int)str.length());
		}

		void addString(const std::string_view& str)
		{
			addString(str.data(), (int)str.length());
		}

		void addDecimal(int64 value, int minDigits)
		{
			if (value < 0)
			{
				mBuffer[mLength] = '-';
				++mLength;
				value = -value;
			}
			else if (value == 0)
			{
				mBuffer[mLength] = '0';
				++mLength;
				return;
			}

			int numDigits = 1;
			int64 digitMax = 10;	// One more than the maximum number representable using numDigits
			while (numDigits < 19 && (digitMax <= value || numDigits < minDigits))
			{
				++numDigits;
				digitMax *= 10;
			}
			for (int64 digitBase = digitMax / 10; digitBase > 0; digitBase /= 10)
			{
				mBuffer[mLength] = '0' + (char)((value / digitBase) % 10);
				++mLength;
			}
		}

		void addBinary(uint64 value, int minDigits)
		{
			int shift = 1;
			while (shift < 64 && ((value >> shift) != 0 || shift < minDigits))
			{
				++shift;
			}
			for (--shift; shift >= 0; --shift)
			{
				const int binValue = (value >> shift) & 0x01;
				mBuffer[mLength] = (binValue == 0) ? '0' : '1';
				++mLength;
			}
		}

		void addHex(uint64 value, int minDigits)
		{
			int shift = 4;
			while (shift < 64 && ((value >> shift) != 0 || shift < minDigits * 4))
			{
				shift += 4;
			}
			for (shift -= 4; shift >= 0; shift -= 4)
			{
				const int hexValue = (value >> shift) & 0x0f;
				mBuffer[mLength] = (hexValue <= 9) ? ('0' + (char)hexValue) : ('a' + (char)(hexValue - 10));
				++mLength;
			}
		}

		std::string getStdString() const
		{
			return std::string(mBuffer, mLength);
		}

		uint64 getHash() const
		{
			return rmx::getMurmur2_64((uint8*)mBuffer, (size_t)mLength);
		}

	public:
		char mBuffer[0x100] = { 0 };
		int mLength = 0;
	};
}


namespace lemon
{
	namespace functions
	{
		template<typename T>
		T minimum(T a, T b)
		{
			return std::min(a, b);
		}

		template<typename T>
		T maximum(T a, T b)
		{
			return std::max(a, b);
		}

		template<typename T>
		T clamp(T a, T b, T c)
		{
			return std::min(std::max(a, b), c);
		}

		template<typename R, typename T>
		R absolute(T a)
		{
			return std::abs(a);
		}

		uint32 sqrt_u32(uint32 a)
		{
			return (uint32)std::sqrt((float)a);
		}

		int16 sin_s16(int16 x)
		{
			return (int16)roundToInt(std::sin((float)x / (float)0x100) * (float)0x100);
		}

		int32 sin_s32(int32 x)
		{
			return (int32)roundToInt(std::sin((float)x / (float)0x10000) * (float)0x10000);
		}

		int16 cos_s16(int16 x)
		{
			return (int16)roundToInt(std::cos((float)x / (float)0x100) * (float)0x100);
		}

		int32 cos_s32(int32 x)
		{
			return (int32)roundToInt(std::cos((float)x / (float)0x10000) * (float)0x10000);
		}

		StringRef stringformat(StringRef format, int argv, uint64* args)
		{
			Runtime* runtime = Runtime::getActiveRuntime();
			RMX_ASSERT(nullptr != runtime, "No lemon script runtime active");
			RMX_CHECK(format.isValid(), "Unable to resolve format string", return StringRef());

			const std::string& formatString = *format;
			const int length = (int)formatString.length();
			const char* fmtPtr = formatString.c_str();
			const char* fmtEnd = fmtPtr + length;

			static FastStringStream result;
			result.clear();

			for (; fmtPtr < fmtEnd; ++fmtPtr)
			{
				if (argv <= 0)
				{
					result.addString(fmtPtr, (int)(fmtEnd - fmtPtr));
					break;
				}

				// Continue until getting a '%' character
				{
					const char* fmtStart = fmtPtr;
					while (fmtPtr != fmtEnd && *fmtPtr != '%')
					{
						++fmtPtr;
					}
					if (fmtPtr != fmtStart)
					{
						result.addString(fmtStart, (int)(fmtPtr - fmtStart));
					}
					if (fmtPtr == fmtEnd)
						break;
				}

				const int remaining = (int)(fmtEnd - fmtPtr);
				if (remaining >= 2)
				{
					char numberOutputCharacter = 0;
					int minDigits = 0;
					int charsRead = 0;

					if (fmtPtr[1] == '%')
					{
						result.addChar('%');
						charsRead = 1;
					}
					else if (fmtPtr[1] == 's')
					{
						// String argument
						const StoredString* argStoredString = runtime->resolveStringByKey(args[0]);
						if (nullptr == argStoredString)
							result.addString("<?>", 3);
						else
							result.addString(argStoredString->getString());
						++args;
						--argv;
						charsRead = 1;
					}
					else if (fmtPtr[1] == 'd' || fmtPtr[1] == 'b' || fmtPtr[1] == 'x')
					{
						// Integer argument
						numberOutputCharacter = fmtPtr[1];
						charsRead = 1;
					}
					else if (remaining >= 4 && fmtPtr[1] == '0' && (fmtPtr[2] >= '1' && fmtPtr[2] <= '9') && (fmtPtr[3] == 'd' || fmtPtr[3] == 'b' || fmtPtr[3] == 'x'))
					{
						// Integer argument with minimum number of digits (9 or less)
						numberOutputCharacter = fmtPtr[3];
						minDigits = (int)(fmtPtr[2] - '0');
						charsRead = 3;
					}
					else if (remaining >= 5 && fmtPtr[1] == '0' && (fmtPtr[2] >= '1' && fmtPtr[2] <= '9') && (fmtPtr[3] >= '0' && fmtPtr[3] <= '9') && (fmtPtr[4] == 'd' || fmtPtr[4] == 'b' || fmtPtr[4] == 'x'))
					{
						// Integer argument with minimum number of digits (10 or more)
						numberOutputCharacter = fmtPtr[4];
						minDigits = (int)(fmtPtr[2] - '0') * 10 + (int)(fmtPtr[3] - '0');
						charsRead = 4;
					}
					else
					{
						result.addChar('%');
					}

					if (numberOutputCharacter != 0)
					{
						if (numberOutputCharacter == 'd')
						{
							result.addDecimal(args[0], minDigits);
						}
						else if (numberOutputCharacter == 'b')
						{
							result.addBinary(args[0], minDigits);
						}
						else if (numberOutputCharacter == 'x')
						{
							result.addHex(args[0], minDigits);
						}
						++args;
						--argv;
					}

					fmtPtr += charsRead;
				}
				else
				{
					result.addChar('%');
				}
			}

			return StringRef(runtime->addString(result.mBuffer, result.mLength));
		}

		StringRef stringformat1(StringRef format, uint64 arg1)
		{
			return stringformat(format, 1, &arg1);
		}

		StringRef stringformat2(StringRef format, uint64 arg1, uint64 arg2)
		{
			uint64 args[] = { arg1, arg2 };
			return stringformat(format, 2, args);
		}

		StringRef stringformat3(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3)
		{
			uint64 args[] = { arg1, arg2, arg3 };
			return stringformat(format, 3, args);
		}

		StringRef stringformat4(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4)
		{
			uint64 args[] = { arg1, arg2, arg3, arg4 };
			return stringformat(format, 4, args);
		}

		StringRef stringformat5(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, uint64 arg5)
		{
			uint64 args[] = { arg1, arg2, arg3, arg4, arg5 };
			return stringformat(format, 5, args);
		}

		StringRef stringformat6(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, uint64 arg5, uint64 arg6)
		{
			uint64 args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
			return stringformat(format, 6, args);
		}

		StringRef stringformat7(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, uint64 arg5, uint64 arg6, uint64 arg7)
		{
			uint64 args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
			return stringformat(format, 7, args);
		}

		StringRef stringformat8(StringRef format, uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4, uint64 arg5, uint64 arg6, uint64 arg7, uint64 arg8)
		{
			uint64 args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
			return stringformat(format, 8, args);
		}

		uint32 strlen(StringRef string)
		{
			return (string.isValid()) ? (uint32)string->length() : 0;
		}

		uint8 getchar(StringRef string, uint32 index)
		{
			if (!string.isValid())
				return 0;
			if (index >= string->length())
				return 0;
			return (*string)[index];
		}

		uint64 substring(StringRef string, uint32 index, uint32 length)
		{
			Runtime* runtime = Runtime::getActiveRuntime();
			RMX_ASSERT(nullptr != runtime, "No lemon script runtime active");
			if (!string.isValid())
				return 0;

			const std::string part = string->substr(index, length);
			return runtime->addString(part);
		}

		StringRef getStringFromHash(uint64 hash)
		{
			Runtime* runtime = Runtime::getActiveRuntime();
			RMX_ASSERT(nullptr != runtime, "No lemon script runtime active");
			const StoredString* str = runtime->resolveStringByKey(hash);
			return (nullptr == str) ? StringRef() : StringRef(hash, str);
		}
	}


	void StandardLibrary::registerBindings(lemon::Module& module)
	{
		const uint8 flags = UserDefinedFunction::FLAG_ALLOW_INLINE_EXECUTION;

		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<int8>), flags);
		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<uint8>), flags);
		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<int16>), flags);
		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<uint16>), flags);
		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<int32>), flags);
		module.addUserDefinedFunction("min", lemon::wrap(&functions::minimum<uint32>), flags);

		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<int8>), flags);
		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<uint8>), flags);
		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<int16>), flags);
		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<uint16>), flags);
		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<int32>), flags);
		module.addUserDefinedFunction("max", lemon::wrap(&functions::maximum<uint32>), flags);

		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<int8>), flags);
		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<uint8>), flags);
		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<int16>), flags);
		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<uint16>), flags);
		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<int32>), flags);
		module.addUserDefinedFunction("clamp", lemon::wrap(&functions::clamp<uint32>), flags);

		module.addUserDefinedFunction("abs", lemon::wrap(&functions::absolute<uint8, int8>), flags);
		module.addUserDefinedFunction("abs", lemon::wrap(&functions::absolute<uint16, int16>), flags);
		module.addUserDefinedFunction("abs", lemon::wrap(&functions::absolute<uint32, int32>), flags);

		module.addUserDefinedFunction("sqrt", lemon::wrap(&functions::sqrt_u32), flags);

		module.addUserDefinedFunction("sin_s16", lemon::wrap(&functions::sin_s16), flags);
		module.addUserDefinedFunction("sin_s32", lemon::wrap(&functions::sin_s32), flags);
		module.addUserDefinedFunction("cos_s16", lemon::wrap(&functions::cos_s16), flags);
		module.addUserDefinedFunction("cos_s32", lemon::wrap(&functions::cos_s32), flags);

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat1), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat2), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat3), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat4), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3")
			.setParameterInfo(4, "arg4");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat5), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3")
			.setParameterInfo(4, "arg4")
			.setParameterInfo(5, "arg5");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat6), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3")
			.setParameterInfo(4, "arg4")
			.setParameterInfo(5, "arg5")
			.setParameterInfo(6, "arg6");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat7), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3")
			.setParameterInfo(4, "arg4")
			.setParameterInfo(5, "arg5")
			.setParameterInfo(6, "arg6")
			.setParameterInfo(7, "arg7");

		module.addUserDefinedFunction("stringformat", lemon::wrap(&functions::stringformat8), flags)
			.setParameterInfo(0, "format")
			.setParameterInfo(1, "arg1")
			.setParameterInfo(2, "arg2")
			.setParameterInfo(3, "arg3")
			.setParameterInfo(4, "arg4")
			.setParameterInfo(5, "arg5")
			.setParameterInfo(6, "arg6")
			.setParameterInfo(7, "arg7")
			.setParameterInfo(8, "arg8");

		module.addUserDefinedFunction("strlen", lemon::wrap(&functions::strlen), flags)
			.setParameterInfo(0, "str");

		module.addUserDefinedFunction("getchar", lemon::wrap(&functions::getchar), flags)
			.setParameterInfo(0, "str")
			.setParameterInfo(1, "index");

		module.addUserDefinedFunction("substring", lemon::wrap(&functions::substring), flags)
			.setParameterInfo(0, "str")
			.setParameterInfo(1, "index")
			.setParameterInfo(2, "length");

		module.addUserDefinedFunction("getStringFromHash", lemon::wrap(&functions::getStringFromHash), flags)
			.setParameterInfo(0, "hash");
	}
}
