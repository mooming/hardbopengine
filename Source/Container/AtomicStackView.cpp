// Created by mooming.go@gmail.com, 2022

#include "AtomicStackView.h"

#ifdef __UNIT_TEST__
#include "HSTL/HVector.h"
#include "Log/Logger.h"
#include <thread>

namespace HE
{

    namespace
    {

        template <typename T>
        struct TNode final
        {
            T value;
            TNode* next = nullptr;

            TNode(const T& value)
                : value(value)
            {
            }
        };

    } // namespace

    void AtomicStackViewTest::Prepare()
    {
        AddTest("Default Constructor", [this](auto& ls) {
            {
                AtomicStackView<TNode<bool>> stack;
                ls << "Bool Stack: [Done]" << lf;
            }

            {
                AtomicStackView<TNode<char>> stack;
                ls << "Char Stack: [Done]" << lf;
            }

            {
                AtomicStackView<TNode<int>> stack;
                ls << "Int Stack: [Done]" << lf;
            }

            {
                AtomicStackView<TNode<float>> stack;
                ls << "Float Stack: [Done]" << lf;
            }

            {
                AtomicStackView<TNode<double>> stack;
                ls << "Double Stack: [Done]" << lf;
            }
        });

        AddTest("Push", [this](auto& ls) {
            AtomicStackView<TNode<bool>> stack;

            TNode<bool> boolValues[] = {true, true, true, false, false, false};

            for (auto& node : boolValues)
            {
                auto& value = node.value;

                ls << "Push Input = " << value << lf;
                stack.Push(node);
            }

            while (auto node = stack.Pop())
            {
                if (node == nullptr)
                {
                    ls << "Encounters a null node!" << lferr;
                    continue;
                }

                auto& value = node->value;
                ls << "Pop Output = " << value << lf;
            }
        });

        AddTest("Thread-Safety", [this](auto& ls) {
            constexpr int NumItem = 1000;
            constexpr int NumLoop = 500;

            HSTL::HVector<TNode<int>> values;
            values.reserve(NumItem);

            for (int i = 0; i < NumItem; ++i)
            {
                values.push_back(i);
            }

            AtomicStackView<TNode<int>> stack;

            std::atomic<int> pushCount = 0;
            auto PushFunc = [&]() {
                for (auto& node : values)
                {
                    stack.Push(node);
                    pushCount.fetch_add(1, std::memory_order_relaxed);
                }
            };

            std::atomic<int> popCount = 0;
            auto PopFunc = [&]() {
                int count = 0;
                while (!stack.IsEmpty())
                {
                    if (stack.Pop())
                    {
                        ++count;
                    }
                }

                popCount.fetch_add(count, std::memory_order_relaxed);
                auto log = HE::Logger::Get(GetName());
                log.Out([count](auto& ls) { ls << "Num Pop = " << count; });
            };

            HSTL::HVector<std::thread> threads;
            values.reserve(5);

            for (int j = 0; j < NumLoop; ++j)
            {
                threads.emplace_back(PushFunc);
                threads.emplace_back(PopFunc);
                threads.emplace_back(PopFunc);
                threads.emplace_back(PopFunc);
                threads.emplace_back(PopFunc);

                for (auto& thread : threads)
                {
                    thread.join();
                }

                threads.clear();

                PopFunc();

                ls << "Iteration: " << j << ", push count = "
                   << pushCount.load(std::memory_order_relaxed)
                   << ", pop count = "
                   << popCount.load(std::memory_order_relaxed) << lf;
            }

            ls << "Multithreaded push & pop test done!" << lf;
        });
    }

} // namespace HE
#endif //__UNIT_TEST__
