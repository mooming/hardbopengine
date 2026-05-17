public:

	[[nodiscard]] This operator+(const This& rhs) const noexcept
	{
		This result(*this);
		result.Add(rhs);

		return result;
	}

	[[nodiscard]] This operator-() const noexcept
	{
		This result(nullptr);

		for (int i = 0; i < order; ++i)
		{
			result.a[i] = -a[i];
		}

		return result;
	}

	[[nodiscard]] This operator-(const This& rhs) const noexcept
	{
		This result(*this);
		result.Sub(rhs);

		return result;
	}

	[[nodiscard]] bool operator==(const This& rhs) const noexcept
	{
		return (*this - rhs).IsZero();
	}

	[[nodiscard]] bool operator!=(const This& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	[[nodiscard]] This operator*(const TNumber rhs) const noexcept
	{
		This result(nullptr);

		for (int i = 0; i < order; ++i)
			result.a[i] = a[i] * rhs;

		return result;
	}

	[[nodiscard]] This operator*(const This& rhs) const noexcept
	{
		This result(nullptr);

		for (int i = 0; i < order; ++i)
			result.a[i] = a[i] * rhs.a[i];

		return result;
	}

	[[nodiscard]] This operator/(const TNumber rhs) const noexcept
	{
		Assert(rhs != 0);

		This result(nullptr);

		TNumber inverse = static_cast<TNumber>(1) / rhs;
		for (int i = 0; i < order; ++i)
			result.a[i] = a[i] * inverse;

		return result;
	}

	[[nodiscard]] This operator/(const This& rhs) const noexcept
	{
		This result(nullptr);

		for (int i = 0; i < order; ++i)
		{
			Assert(!hbe::IsZero(static_cast<float>(rhs.a[i])));
			result.a[i] = a[i] / rhs.a[i];
		}

		return result;
	}

	This& operator+=(const This& rhs) noexcept
	{
		Add(rhs);

		return *this;
	}

	This& operator-=(const This& rhs) noexcept
	{
		Sub(rhs);

		return *this;
	}

	This& operator*=(const TNumber rhs) noexcept
	{
		Multiply(rhs);

		return *this;
	}

	This& operator*=(const This& rhs) noexcept
	{
		Multiply(rhs);

		return *this;
	}

	This& operator/=(const TNumber rhs) noexcept
	{
		Assert(rhs != 0.0f);

		Multiply(static_cast<TNumber>(1) / rhs);

		return *this;
	}

	This& ToAbsolute() noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] = Abs(a[i]);

		return *this;
	}

	[[nodiscard]] This GetAbsolute() const noexcept
	{
		This v(nullptr);
		for (int i = 0; i < order; ++i)
			v.a[i] = Abs(a[i]);

		return v;
	}

	[[nodiscard]] bool IsZero() const noexcept
	{
		TNumber total(0);

		for (int i = 0; i < order; ++i)
			total += Abs(a[i]);

		return total < Epsilon;
	}

	void Multiply(TNumber value) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] *= value;
	}

	void Multiply(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] *= rhs.a[i];
	}

	void Divide(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
		{
			Assert(!hbe::IsZero(static_cast<float>(rhs.a[i])));
			a[i] /= rhs.a[i];
		}
	}

	void Add(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] += rhs.a[i];
	}

	void Sub(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] -= rhs.a[i];
	}

	void Negate() noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] = -a[i];
	}

	[[nodiscard]] TNumber Dot(const This& rhs) const noexcept
	{
		TNumber value = 0;
		for (int i = 0; i < order; ++i)
			value += (a[i] * rhs.a[i]);

		return value;
	}

	[[nodiscard]] TNumber SqrLength() const noexcept
	{
		return Dot(*this);
	}

	[[nodiscard]] float Length() const noexcept
	{
		return sqrtf(SqrLength());
	}

	float Normalize() noexcept
	{
		auto length = Length();
		if (hbe::IsZero(length))
		{
			*this = This::Forward;

			return length;
		}

		Multiply(static_cast<TNumber>(1) / length);

		return length;
	}

	[[nodiscard]] This Normalized() const noexcept
	{
		This result(*this);
		result.Normalize();

		return result;
	}

	[[nodiscard]] bool IsUnity() const noexcept
	{
		return Abs(SqrLength() - static_cast<TNumber>(1)) < SqrEpsilon;
	}

	[[nodiscard]] This Lerp(const This& to, float t) const noexcept
	{
		return Lerp(*this, to, t);
	}

	[[nodiscard]] static This Lerp(const This& from, const This& to, float t) noexcept
	{
		return from * (1.0f - t) + to * t;
	}

	[[nodiscard]] This Slerp(const This& to, float t) const noexcept
	{
		return Slerp(*this, to, t);
	}

	[[nodiscard]] static This Slerp(const This& from, const This& to, float t) noexcept
	{
		auto a = from.Normalized();
		auto b = to.Normalized();

		auto angle = acosf(static_cast<float>(a.Dot(b)));

		if (angle < Epsilon)
			return Lerp(from, to, t).Normalized();

		const auto nt = 1.0f - t;
		const auto sinA = sin(nt * angle);
		const auto sinB = sin(t * angle);
		auto dir = (a * sinA + b * sinB) / sin(angle);

		auto lengthA = from.Length();
		auto lengthB = to.Length();

		auto length = (lengthA * nt) + (lengthB * t);

		return dir * length;
	}
