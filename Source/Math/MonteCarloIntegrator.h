//
// Created by mooming on 21/06/2025.
//

#pragma once

#include <cstdint>

namespace HE
{

template <typename TInput = double, typename TOutput = double,
    typename TReal = double, typename TInteger = uint32_t>
class MonteCarloIntegrator final
{
public:
    using TFunction = TOutput (*)(const TInput&);
    using TPDF = TReal (*)(const TInput&);
    using TRandomGen = TInput (*)();

public:
    MonteCarloIntegrator() = default;
    ~MonteCarloIntegrator() = default;

    bool operator()(TOutput& result, const TFunction f, const TPDF p,
        const TRandomGen random, const TInteger numIterations)
    {
        result = 0;

        if (numIterations <= 0)
        {
            return false;
        }

        for (TInteger i = 0; i < numIterations; ++i)
        {
            const TInput x = random();
            const TReal p_x = p(x);
            if (p_x <= 0)
            {
                return false;
            }

            const TOutput f_x = f(x);
            const TReal fx_over_px = f_x / p_x;

            result += fx_over_px;
        }

        result /= numIterations;

        return result;
    }
};

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class MonteCarloIntegrationTest : public TestCollection
{
public:
    MonteCarloIntegrationTest()
        : TestCollection("Monte Carlo Integration Test")
    {
    }

protected:
    void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
