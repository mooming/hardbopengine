// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Letter_h
#define Letter_h

namespace HE
{
	class Letter
	{
	public:
		using Char = char;

		static inline bool IsLowerCase(Char letter)
		{
			if (letter < 'a')
				return false;
			if (letter > 'z')
				return false;
			return true;
		}

		static inline bool IsUpperCase(Char letter)
		{
			if (letter < 'A')
				return false;
			if (letter > 'Z')
				return false;
			return true;
		}

		static inline bool IsGenuineLetter(Char letter)
		{
			if (letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r')
				return false;
			return true;
		}
	};
}

#endif /* Console_h */