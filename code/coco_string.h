inline b32 IsEndOfLine(char C)
{
    b32 Result = ((C == '\n') ||
                  (C == '\r'));

    return Result;
}

inline b32 IsWhiteSpace(char C)
{
    b32 Result = ((C == ' ') ||
                  (C == '\t') ||
                  (C == '\v') ||
                  (C == '\f') ||
                  IsEndOfLine(C));

    return Result;
}

inline b32 IsDigit(char Val)
{
    b32 Result = (Val == '0' ||
                  Val == '1' ||
                  Val == '2' ||
                  Val == '3' ||
                  Val == '4' ||
                  Val == '5' ||
                  Val == '6' ||
                  Val == '7' ||
                  Val == '8' ||
                  Val == '9');
    
    return Result;
}

inline f64 CharDigitToFloat(char Val)
{
    f64 Result = 0.0;
    switch (Val)
    {
        case '0':
        {
            Result = 0.0;
        } break;

        case '1':
        {
            Result = 1.0;
        } break;

        case '2':
        {
            Result = 2.0;
        } break;

        case '3':
        {
            Result = 3.0;
        } break;

        case '4':
        {
            Result = 4.0;
        } break;

        case '5':
        {
            Result = 5.0;
        } break;

        case '6':
        {
            Result = 6.0;
        } break;

        case '7':
        {
            Result = 7.0;
        } break;

        case '8':
        {
            Result = 8.0;
        } break;

        case '9':
        {
            Result = 9.0;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    return Result;
}

inline void GetUIntFromStr(char** StrPtr, u32* Result)
{
    char* Str = *StrPtr;

    u32 NumChars = 0;
    u32 Exp = 1;
    u32 CurrChar = 0;
    while (IsDigit(Str[CurrChar++]))
    {
        Exp *= 10;
        NumChars += 1;
    }
    Exp /= 10;
    u32 UInt = 0;
    
    // NOTE: Add the digits on the left of the point
    for (u32 NumDigits = 0; NumDigits < NumChars; ++NumDigits)
    {
        char CurrDigitChar = *Str;
        Str += 1;
        UInt += (u32)CharDigitToFloat(CurrDigitChar)*Exp;
        Exp /= 10;
    }

    *Result = UInt;
    *StrPtr = Str;
}

inline void GetFloatFromStr(char** StrPtr, f32* Result)
{
    char* Str = *StrPtr;
    
    f64 Neg = 1.0f;
    if (Str[0] == '-')
    {
        ++Str;
        Neg = -1.0f;
    }

    f64 Exp = 1.0f/10.0f;
    u32 PointId = 0;
    u32 CurrChar = 0;
    b32 NoPoint = false;
    while (Str[CurrChar++] != '.')
    {
        Exp *= 10.0f;
        if (IsWhiteSpace(Str[CurrChar]))
        {
            NoPoint = true;
            break;
        }
    }    
    PointId = CurrChar;
    
    f64 Float = 0.0f;
    
    // NOTE: Add the digits on the left of the point
    u32 LoopEnd = PointId;
    if (!NoPoint)
    {
        LoopEnd -= 1;
    }
    
    for (u32 NumDigits = 0; NumDigits < LoopEnd; ++NumDigits)
    {
        Float += CharDigitToFloat(*Str)*Exp;
        Str += 1;
        Exp /= 10.0;
    }

    if (NoPoint)
    {
        *Result = (f32)(Neg*Float);
        *StrPtr = Str;

        return;
    }

    // NOTE: Get past the point/e
    Str += 1;

    // NOTE: Add the digits after the point
    u32 Denom = 10;
    while (IsDigit(*Str))
    {
        Float += CharDigitToFloat(*Str)*(1.0 / (f64)Denom);
        Denom *= 10;
        Str += 1;
    }

    Float = Neg*Float;

    // NOTE: Check if we still have an exponent
    if (*Str == 'e')
    {
        Str += 1;
        
        // NOTE: Get the exponent
        b32 Negative = false;
        if (Str[0] == '-')
        {
            ++Str;
            Negative = true;
        }

        u32 ExpVal;
        GetUIntFromStr(&Str, &ExpVal);

        f64 Power = 1.0f;
        if (ExpVal != 1)
        {
            if (Negative)
            {
                Power = Pow64(1.0f / 10.0f, ExpVal-1);
            }
            else
            {
                Power = Pow64(10.0f, ExpVal-1);
            }
        }
        
        Float = Float*Power;
    }

    *Result = (f32)Float;
    *StrPtr = Str;
}

inline b32 StringsAreEqual(char* A, char* B)
{
    // NOTE: If both are null, they are equal
    b32 Result = (A == B);

    if (A && B)
    {
        while(*A && *B && *A == *B)
        {
            ++A;
            ++B;
        }
        
        Result = (*A == 0) && (*B == 0);
    }
    
    return Result;
}

inline b32 StringsAreEqual(umm ALength, char* A, char* B)
{
    b32 Result = false;
    
    if (B)
    {
        char* At = B;
        for (umm Index = 0; Index < ALength; ++Index, ++At)
        {
            if ((*At == 0) || (A[Index] != *At))
            {
                return false;
            }
        }

        // NOTE: Return true if both strings ended
        Result = (*At == 0);
    }
    else
    {
        Result = ALength == 0;
    }

    return Result;
}

inline b32 StringsAreEqual(umm ALength, char* A, umm BLength, char* B)
{
    b32 Result = true;

    Assert(A && B && ALength == BLength);

    for (u32 CharId = 0; CharId < ALength; ++CharId)
    {
        if (A[CharId] != B[CharId])
        {
            Result = false;
            break;
        }
    }
    
    return Result;
}

