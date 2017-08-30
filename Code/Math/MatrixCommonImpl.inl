public:
inline static This CreateDiagonal(const Number diagonal)
{
    This result;

    for (int i = 0; i < row; ++i)
    {
        result.m[i][i] = diagonal;
    }

    return result;
}

inline static This CreateDiagonal(const Vec& diagonal)
{
    This result;

    for (int i = 0; i < row; ++i)
    {
        result.m[i][i] = diagonal.a[i];
    }

    return result;
}

inline This operator* (const This& rhs) const
{
	return Multiply(rhs);
}

inline Vec operator* (const Vec& rhs) const
{
	return Multiply(rhs);
}

inline bool operator== (const This& rhs) const
{
    for (int i = 0; i < row; ++i)
    {
        if (rows[i] != rhs.rows[i])
            return false;
    }

    return true;
}

inline bool operator!= (const This& rhs) const
{
    return !(*this == rhs);
}

inline bool IsInvertible() const
{
	constexpr bool isSquare = row == column;
	return isSquare && Determinant() != 0;
}

inline bool IsIdentity() const
{
    for (int i = 0; i < row; ++i)
    {
        for (int j = 0; j < column; ++j)
        {
            if (i != j)
            {
                if (!IsZero(m[i][j]))
                    return false;
            }
            else if (!IsEqual(m[i][j], 1.0f))
            {
                return false;
            }
        }
    }
    return true;
}

inline This Transposed() const
{
	This result;

	for (int i = 0; i < row; ++i)
	{
		for (int j = 0; j < column; ++j)
		{
			result.m[j][i] = m[i][j];
		}
	}

	return result;
}

inline This Multiply(const Number value) const
{
	Assert(value == value);

	This result;

	for (int i = 0; i < numberOfElements; ++i)
	{
		result.element[i] = element[i] * value;
	}

	return result;
}

inline This Multiply(const This& rhs) const
{
	This result;
	This tRhs = rhs.Transposed();

	for (int i = 0; i < row; ++i)
	{
		for (int j = 0; j < column; ++j)
		{
			result.m[i][j] = rows[i].Dot(tRhs.rows[j]);
		}
	}

	return result;
}

inline Vec Multiply(const Vec& rhs) const
{
	Vec result;
	for (int i = 0; i < row; ++i)
	{
		result.a[i] = rows[i].Dot(rhs);
	}

	return result;
}

inline Vec Column(int index) const
{
    AssertMessage(index < column, "Matrix: Index out of bounds, i = ", index);

    Vec colVec = nullptr;

    for (int i = 0; i < column; ++i)
    {
        colVec.a[i] = rows[i].a[index];
    }

    return colVec;
}
