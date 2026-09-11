// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AABB.h"


namespace hbe
{

	template class AABB<TFloat2>;
	template class AABB<TFloat3>;

} // namespace hbe

#ifdef __UNIT_TEST__

void hbe::AABBTest::prepare() noexcept
{
	static AABB3 bbox;

	addTest("Default Constructor", [&, this](auto& ls)
	{
		ls << bbox << lf;

		if (bbox.IsEmpty())
		{
			return;
		}

		ls << "AABB created with default constructor should be empty." << lferr;
	});

	addTest("Add Points", [&, this](auto& ls)
	{
		bbox.add(TFloat3::Zero);
		bbox.add(TFloat3::Forward);
		bbox.add(TFloat3::Right);
		bbox.add(TFloat3::Up);

		ls << bbox << lf;

		if (bbox.diagonal() != TFloat3::Unity)
		{
			ls << "AABB dianoal error. " << bbox.diagonal() << ", but expected " << TFloat3::Unity << lferr;
		}
	});

	addTest("Self Containing", [&, this](auto& ls)
	{
		if (!bbox.isContaining(bbox))
		{
			ls << "AABB should contain itself! " << bbox << lferr;
		}
	});

	addTest("Self Equality", [&, this](auto& ls)
	{
		if (bbox != bbox)
		{
			ls << "AABB should match itself! " << bbox << lferr;
		}
	});

	addTest("Self Intersection", [&, this](auto& ls)
	{
		if (bbox.intersection(bbox) != bbox)
		{
			ls << "Self intersection of AABB should match itself! " << bbox << lferr;
		}
	});

	static AABB3 bbox2;

	addTest("Clone", [&, this](auto& ls)
	{
		bbox2 = bbox;

		if (bbox != bbox2)
		{
			ls << "Clone of AABB should match itself! " << bbox << ", " << bbox2 << lferr;
		}
	});

	addTest("Containing", [&, this](auto& ls)
	{
		bbox2.max *= 2.0f;
		ls << "bbox2 = " << bbox2;

		if (bbox2.intersection(bbox) != bbox)
		{
			ls << "Invalid Intersection of AABB. Intersection of " << bbox2 << " and " << bbox << " = "
			   << bbox2.intersection(bbox) << hendl << lferr;
		}

		if ((bbox + bbox2) != bbox2)
		{
			ls << "Invalid union of AABB. Union of " << bbox << " and " << bbox2 << " = " << (bbox + bbox2) << lferr;
		}
	});

	static AABB3 bbox3;

	addTest("Intersection", [&, this](auto& ls)
	{
		bbox3 = bbox2;
		ls << "bbox3 = " << bbox3 << lf;

		bbox3.translate(-TFloat3::Unity);
		ls << "bbox3 = " << bbox3 << lf;

		if (bbox2.intersection(bbox3) != bbox)
		{
			ls << "Invalid Intersection of AABB. Intersection of " << bbox2 << " and " << bbox3 << " = "
			   << bbox2.intersection(bbox3) << ", but expecting " << bbox << lferr;
		}

		bbox2.translate(TFloat3::Unity);
		if (!bbox2.intersection(bbox3).IsEmpty())
		{
			ls << "Invalid Intersection of AABB. " << hendl << "Intersection of " << hendl << bbox2 << hendl << " and "
			   << hendl << bbox3 << hendl << "is " << bbox2.intersection(bbox3) << hendl << "But expecting the empty."
			   << lferr;
		}
	});
}

#endif // __UNIT_TEST__
