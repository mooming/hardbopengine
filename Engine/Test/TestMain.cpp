// Created by mooming.go@gmail.com

#include "../Engine/Engine.h"
#include "UnitTestCollection.h"

int main(int argc, const char* argv[])
{
	hbe::Engine hengine;

	hengine.Initialize(argc, argv);

#ifdef __UNIT_TEST__
	Test::RunUnitTests();
#endif // __UNIT_TEST__

	hengine.Run();

	return 0;
}
