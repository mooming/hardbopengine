public:
	[[nodiscard]] static This CreateDiagonal(const TNumber diagonal) noexcept
	{
		This result;

		for (int i = 0; i < row; ++i)
		{
			result.m[i][i] = diagonal;
		}

		return result;
	}

	[[nodiscard]] static This CreateDiagonal(const TVec& diagonal) noexcept
	{
		This result;

		for (int i = 0; i < row; ++i)
		{
			result.m[i][i] = diagonal.a[i];
		}

		return result;
	}

	[[nodiscard]] This operator* (const This& rhs) const noexcept
	{
		return Multiply(rhs);
	}

	[[nodiscard]] TVec operator* (const TVec& rhs) const noexcept
	{
		return Multiply(rhs);
	}

	[[nodiscard]] bool operator== (const This& rhs) const noexcept
	{
		for (int i = 0; i < row; ++i)
		{
			if (rows[i] != rhs.rows[i])
				return false;
		}

		return true;
	}

	[[nodiscard]] bool operator!= (const This& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	[[nodiscard]] bool IsInvertible() const noexcept
	{
		constexpr bool isSquare = row == column;

		return isSquare && Determinant() != 0;
	}

	[[nodiscard]] bool IsIdentity() const noexcept
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

	[[nodiscard]] This Transposed() const noexcept
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

	[[nodiscard]] This Multiply(const TNumber value) const noexcept
	{
		Assert(value == value);

		This result;

		for (int i = 0; i < numberOfElements; ++i)
		{
			result.element[i] = element[i] * value;
		}

		return result;
	}

	[[nodiscard]] This Multiply(const This& rhs) const noexcept
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

	[[nodiscard]] TVec Multiply(const TVec& rhs) const noexcept
	{
		TVec result;
		for (int i = 0; i < row; ++i)
		{
			result.a[i] = rows[i].Dot(rhs);
		}

		return result;
	}

	[[nodiscard]] TVec Column(int index) const noexcept
	{
		Assert(index < column, "Matrix: Index out of bounds, i = ", index);

		TVec colVec(nullptr);

		for (int i = 0; i < column; ++i)
		{
			colVec.a[i] = rows[i].a[index];
		}

		return colVec;
	}
