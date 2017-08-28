public:

    inline This operator+ (const This& rhs) const
    {
        This result(*this);
        result.Add(rhs);
        return result;
    }

    inline This operator- () const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
        {
            result.a[i] = -a[i];
        }

        return result;
    }

    inline This operator- (const This& rhs) const
    {
        This result(*this);
        result.Sub(rhs);
        return result;
    }

    inline bool operator== (const This& rhs) const
    {
        return (*this - rhs).IsZero();
    }

    inline bool operator!= (const This& rhs) const
    {
        return !(*this == rhs);
    }

    inline This operator* (const Number rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * rhs;

        return result;
    }

    inline This operator* (const This& rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * rhs.a[i];

        return result;
    }

    inline This operator/ (const Number rhs) const
    {
        Assert(rhs != 0.0f);

        This result(nullptr);

        Number inverse = 1.0f / rhs;
        for (int i = 0; i < order; ++i)
            result.a[i] = a[i] * inverse;

        return result;
    }

    inline This operator/ (const This& rhs) const
    {
        This result(nullptr);

        for (int i = 0; i < order; ++i)
        {
            Assert(!HE::IsZero(rhs.a[i]));
            result.a[i] = a[i] / rhs.a[i];
        }

        return result;
    }

    inline This& operator+= (const This& rhs)
    {
        Add(rhs);
        return *this;
    }

    inline This& operator-= (const This& rhs)
    {
        Sub(rhs);
        return *this;
    }

    inline This& operator*= (const Number rhs)
    {
        Multiply(rhs);
        return *this;
    }

    inline This& operator*= (const This& rhs)
    {
        Multiply(rhs);
        return *this;
    }

    inline This& operator/= (const Number rhs)
    {
        Assert(rhs != 0.0f);

        Multiply(1.0f / rhs);
        return *this;
    }

    inline This& ToAbsolute()
    {
        for (int i = 0; i < order; ++i)
            a[i] = Abs(a[i]);

        return *this;
    }

    inline This GetAbsolute() const
    {
        This v(nullptr);
        for (int i = 0; i < order; ++i)
            v.a[i] = Abs(a[i]);

        return v;
    }

    inline bool IsZero() const
    {
        Number total(0);

        for (int i = 0; i < order; ++i)
            total += Abs(a[i]);

        return total < epsilon;
    }

    inline void Multiply(Number value)
    {
        for (int i = 0; i < order; ++i)
            a[i] *= value;
    }

    inline void Multiply(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] *= rhs.a[i];
    }

    inline void Divide(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
        {
            Assert(!HE::IsZero(rhs.a[i]));
            a[i] /= rhs.a[i];
        }
    }

    inline void Add(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] += rhs.a[i];
    }

    inline void Sub(const This& rhs)
    {
        for (int i = 0; i < order; ++i)
            a[i] -= rhs.a[i];
    }

    inline void Negate()
    {
        for (int i = 0; i < order; ++i)
            a[i] = -a[i];
    }

    inline Number Dot(const This& rhs) const
    {
        Number value = 0.0f;
        for (int i = 0; i < order; ++i)
            value += (a[i] * rhs.a[i]);

        return value;
    }

    inline Number SqrLength() const
    {
        return Dot(*this);
    }

    inline float Length() const
    {
        return std::sqrtf(SqrLength());
    }

    float Normalize()
    {
        Assert(!IsZero());

        const float length = MaxFast(Length(), epsilon);
        Multiply(1.0f / length);

        return length;
    }

    This Normalized() const
    {
        This result(*this);
        result.Normalize();
        return result;
    }

    inline bool IsUnity() const
    {
        return Abs(SqrLength() - 1.0f) < sqrEpsilon;
    }

    inline This Lerp(const This& to, float t) const
    {
        return Lerp(*this, to, t);
    }

    inline static This Lerp(const This& from, const This& to, float t) const
    {
        return (*this) * (1.0f - t) + to * t;
    }

    inline This Slerp(const This& to, float t) const
    {
        return Slerp(*this, to, t);
    }

    inline static This Slerp(const This& from, This to, float t) const
    {
        auto lengthA = from.Length();
        auto lengthB = to.Length();
        
        cosnt auto nt = 1.0f - t;
        auto length = (lengthA * nt) + (lengthB * t);
        
        auto a = from.Normalized();
        auto b = to.Normalized();
        
        // Need to check
        auto angle = std::acos(a.Dot(b)) * nt;
        auto rotDir = a.Cross(b);
        auto rightDir = rotDir.Cross(a);
        auto dir = (a * std::cos(angle)) + (rightDir * std::sin(angle));
        dir.Normalize();
        
        return a * length;
    }
