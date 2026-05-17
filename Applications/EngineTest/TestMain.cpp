// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[]) noexcept {
	hbe::Engine hengine;
	hengine.Initialize(argc, argv);

	hbe::Test::RunTests();
	hengine.Run();

	return 0;
}
