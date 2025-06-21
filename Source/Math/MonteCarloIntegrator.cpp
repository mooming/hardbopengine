//
// Created by mooming on 21/06/2025.
//

#include "MonteCarloIntegrator.h"

#ifdef __UNIT_TEST__
#include "System/Constants.h"
#include <random>

void HE::MonteCarloIntegrationTest::Prepare()
{
    AddTest("Calculate Pi", [this](auto& ls) -> void {
        struct Vec2 final
        {
            double x = 0;
            double y = 0;
        };

        auto func = [](const Vec2& x) -> double {
            double r = std::sqrt(x.x * x.x + x.y * x.y);
            return r <= 1.0 ? 1.0 : 0.0f;
        };

        // uniform pdf in (-1, -1) to (1, 1)
        auto pdf = [](const Vec2& x) -> double {
            constexpr double p = 1.0 / 4.0f;
            return p;
        };

        auto randomGen = []() -> Vec2 {
            static std::random_device rd;  // random seed
            static std::mt19937 gen(rd()); // pseudo random gen
            static std::uniform_real_distribution dist(-1.0, 1.0);

            Vec2 x;
            x.x = dist(gen);
            x.y = dist(gen);
            return x;
        };

        double result = 0;
        using MCI = MonteCarloIntegrator<Vec2, double, double, uint32_t>;
        constexpr uint32_t numIterations = 1000000;

        MCI Integrator;
        bool bOk = Integrator(result, func, pdf, randomGen, numIterations);
        if (!bOk)
        {
            ls << "failed to perform MC integration" << lferr;
            return;
        }

        ls << "MC result = " << result << ", PI " << Pi << lf;
    });
}
#endif // __UNIT_TEST__
