public:

    This operator+(const This& rhs) const
    {
        This result(*this);
        result.Add(rhs);
        return result;
    }

    This operator-() const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
        {
            result.a[i] = -a[i];
        }

        return result;
    }

    This operator-(const This& rhs) const
    {
        This result(*this);
        result.Sub(rhs);
        return result;
    }

    bool operator==(const This& rhs) const
    {
        return (*this - rhs).IsZero();
    }

    bool operator!=(const This& rhs) const
    {
        return !(*this == rhs);
    }

    This operator*(const Number rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * rhs;

        return result;
    }

    This operator*(const This& rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * rhs.a[i];

        return result;
    }

    This operator/(const Number rhs) const
    {
        Assert(rhs != 0);

        This result(nullptr);

        Number inverse = static_cast<Number>(1) / rhs;
        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * inverse;

        return result;
    }

    This operator/(const This& rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
        {
            Assert(!HE::IsZero(static_cast<float>(rhs.a[i])));
            result.a[i] = a[i] / rhs.a[i];
        }

        return result;
    }

    This& operator+=(const This& rhs)
    {
        Add(rhs);
        return *this;
    }

    This& operator-=(const This& rhs)
    {
        Sub(rhs);
        return *this;
    }

    This& operator*=(const Number rhs)
    {
        Multiply(rhs);
        return *this;
    }

    This& operator*=(const This& rhs)
    {
        Multiply(rhs);
        return *this;
    }

    This& operator/=(const Number rhs)
    {
        Assert(rhs != 0.0f);

        Multiply(static_cast<Number>(1) / rhs);
        return *this;
    }

    This& ToAbsolute()
    {
        for (int i = 0; i < order; ++i)
            a[i] = Abs(a[i]);

        return *this;
    }

    This GetAbsolute() const
    {
        This v(nullptr);
        for (int i = 0; i < order; ++i)
            v.a[i] = Abs(a[i]);

        return v;
    }

    bool IsZero() const
    {
        Number total(0);

        for (int i = 0; i < order; ++i)
            total += Abs(a[i]);

        return total < Epsilon;
    }

    void Multiply(Number value)
    {
        for (int i = 0; i < order; ++i)
            a[i] *= value;
    }

    void Multiply(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] *= rhs.a[i];
    }

    void Divide(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
        {
            Assert(!HE::IsZero(static_cast<float>(rhs.a[i])));
            a[i] /= rhs.a[i];
        }
    }

    void Add(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] += rhs.a[i];
    }

    void Sub(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] -= rhs.a[i];
    }

    void Negate()
    {
        for (int i = 0; i < order; ++i)
            a[i] = -a[i];
    }

    Number Dot(const This& rhs) const
    {
        Number value = 0;
        for (int i = 0; i < order; ++i)
            value += (a[i] * rhs.a[i]);

        return value;
    }

    Number SqrLength() const
    {
        return Dot(*this);
    }

    float Length() const
    {
        return sqrtf(SqrLength());
    }

    float Normalize()
    {
        auto length = Length();
        if (HE::IsZero(length))
        {
            *this = This::Forward;
            return length;
        }

        Multiply(static_cast<Number>(1) / length);

        return length;
    }

    This Normalized() const
    {
        This result(*this);
        result.Normalize();
        return result;
    }

    bool IsUnity() const
    {
        return Abs(SqrLength() - static_cast<Number>(1)) < SqrEpsilon;
    }

    This Lerp(const This& to, float t) const
    {
        return Lerp(*this, to, t);
    }

    static This Lerp(const This& from, const This& to, float t)
    {
        return from * (1.0f - t) + to * t;
    }

    This Slerp(const This& to, float t) const
    {
        return Slerp(*this, to, t);
    }

    static This Slerp(const This& from, const This& to, float t)
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
