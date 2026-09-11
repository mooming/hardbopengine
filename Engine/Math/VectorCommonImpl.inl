public:

	[[nodiscard]] This operator+(const This& rhs) const noexcept
	{
		This result(*this);
		result.add(rhs);

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
		result.sub(rhs);

		return result;
	}

	[[nodiscard]] bool operator==(const This& rhs) const noexcept
	{
		return (*this - rhs).isZero();
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
			Assert(!hbe::isZero(static_cast<float>(rhs.a[i])));
			result.a[i] = a[i] / rhs.a[i];
		}

		return result;
	}

	This& operator+=(const This& rhs) noexcept
	{
		add(rhs);

		return *this;
	}

	This& operator-=(const This& rhs) noexcept
	{
		sub(rhs);

		return *this;
	}

	This& operator*=(const TNumber rhs) noexcept
	{
		multiply(rhs);

		return *this;
	}

	This& operator*=(const This& rhs) noexcept
	{
		multiply(rhs);

		return *this;
	}

	This& operator/=(const TNumber rhs) noexcept
	{
		Assert(rhs != 0.0f);

		multiply(static_cast<TNumber>(1) / rhs);

		return *this;
	}

	This& toAbsolute() noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] = Abs(a[i]);

		return *this;
	}

	[[nodiscard]] This getAbsolute() const noexcept
	{
		This v(nullptr);
		for (int i = 0; i < order; ++i)
			v.a[i] = Abs(a[i]);

		return v;
	}

	[[nodiscard]] bool isZero() const noexcept
	{
		TNumber total(0);

		for (int i = 0; i < order; ++i)
			total += Abs(a[i]);

		return total < Epsilon;
	}

	void multiply(TNumber value) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] *= value;
	}

	void multiply(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] *= rhs.a[i];
	}

	void divide(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
		{
			Assert(!hbe::isZero(static_cast<float>(rhs.a[i])));
			a[i] /= rhs.a[i];
		}
	}

	void add(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] += rhs.a[i];
	}

	void sub(const This& rhs) noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] -= rhs.a[i];
	}

	void negate() noexcept
	{
		for (int i = 0; i < order; ++i)
			a[i] = -a[i];
	}

	[[nodiscard]] TNumber dot(const This& rhs) const noexcept
	{
		TNumber value = 0;
		for (int i = 0; i < order; ++i)
			value += (a[i] * rhs.a[i]);

		return value;
	}

	[[nodiscard]] TNumber sqrLength() const noexcept
	{
		return dot(*this);
	}

	[[nodiscard]] float Length() const noexcept
	{
		return sqrtf(sqrLength());
	}

	float normalize() noexcept
	{
		auto length = Length();
		if (hbe::isZero(length))
		{
			*this = This::Forward;

			return length;
		}

		multiply(static_cast<TNumber>(1) / length);

		return length;
	}

	[[nodiscard]] This normalized() const noexcept
	{
		This result(*this);
		result.normalize();

		return result;
	}

	[[nodiscard]] bool isUnity() const noexcept
	{
		return Abs(sqrLength() - static_cast<TNumber>(1)) < SqrEpsilon;
	}

	[[nodiscard]] This lerp(const This& to, float t) const noexcept
	{
		return lerp(*this, to, t);
	}

	[[nodiscard]] static This lerp(const This& from, const This& to, float t) noexcept
	{
		return from * (1.0f - t) + to * t;
	}

	[[nodiscard]] This slerp(const This& to, float t) const noexcept
	{
		return slerp(*this, to, t);
	}

	[[nodiscard]] static This slerp(const This& from, const This& to, float t) noexcept
	{
		auto a = from.normalized();
		auto b = to.normalized();

		auto angle = acosf(static_cast<float>(a.dot(b)));

		if (angle < Epsilon)
			return lerp(from, to, t).normalized();

		const auto nt = 1.0f - t;
		const auto sinA = sin(nt * angle);
		const auto sinB = sin(t * angle);
		auto dir = (a * sinA + b * sinB) / sin(angle);

		auto lengthA = from.Length();
		auto lengthB = to.Length();

		auto length = (lengthA * nt) + (lengthB * t);

		return dir * length;
	}
