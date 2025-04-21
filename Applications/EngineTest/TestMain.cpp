// Created by mooming.go@gmail.com

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[])
{
    HE::Engine hengine;

    hengine.Initialize(argc, argv);

    auto testFunc = [](std::size_t, std::size_t)
    {
        Test::RunUnitTests();

        auto& engine = HE::Engine::Get();
        engine.Stop();
    };

    auto& taskSys = hengine.GetTaskSystem();
    auto streamIndex = taskSys.GetMainTaskStreamIndex();
    HE::StaticString taskName("TestMain");
    taskSys.DispatchTask(taskName, testFunc, streamIndex);

    hengine.Run();

    return 0;
}
