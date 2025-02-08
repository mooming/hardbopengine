// Created by mooming.go@gmail.com

#include "AABB.h"

namespace HE
{

    template class AABB<Float2>;
    template class AABB<Float3>;

} // namespace HE

#ifdef __UNIT_TEST__

void HE::AABBTest::Prepare()
{
    static AABB3 bbox;

    AddTest("Default Constructor", [&, this](auto &ls) {
        ls << bbox << lf;

        if (bbox.IsEmpty())
        {
            return;
        }

        ls << "AABB created with default constructor should be empty." << lferr;
    });

    AddTest("Add Points", [&, this](auto &ls) {
        bbox.Add(Float3::Zero);
        bbox.Add(Float3::Forward);
        bbox.Add(Float3::Right);
        bbox.Add(Float3::Up);

        ls << bbox << lf;

        if (bbox.Diagonal() != Float3::Unity)
        {
            ls << "AABB dianoal error. " << bbox.Diagonal() << ", but expected "
               << Float3::Unity << lferr;
        }
    });

    AddTest("Self Containing", [&, this](auto &ls) {
        if (!bbox.IsContaining(bbox))
        {
            ls << "AABB should contain itself! " << bbox << lferr;
        }
    });

    AddTest("Self Equality", [&, this](auto &ls) {
        if (bbox != bbox)
        {
            ls << "AABB should match itself! " << bbox << lferr;
        }
    });

    AddTest("Self Intersection", [&, this](auto &ls) {
        if (bbox.Intersection(bbox) != bbox)
        {
            ls << "Self intersection of AABB should match itself! " << bbox
               << lferr;
        }
    });

    static AABB3 bbox2;

    AddTest("Clone", [&, this](auto &ls) {
        bbox2 = bbox;

        if (bbox != bbox2)
        {
            ls << "Clone of AABB should match itself! " << bbox << ", " << bbox2
               << lferr;
        }
    });

    AddTest("Containing", [&, this](auto &ls) {
        bbox2.max *= 2.0f;
        ls << "bbox2 = " << bbox2;

        if (bbox2.Intersection(bbox) != bbox)
        {
            ls << "Invalid Intersection of AABB. Intersection of " << bbox2
               << " and " << bbox << " = " << bbox2.Intersection(bbox) << hendl
               << lferr;
        }

        if ((bbox + bbox2) != bbox2)
        {
            ls << "Invalid union of AABB. Union of " << bbox << " and " << bbox2
               << " = " << (bbox + bbox2) << lferr;
        }
    });

    static AABB3 bbox3;

    AddTest("Intersection", [&, this](auto &ls) {
        bbox3 = bbox2;
        ls << "bbox3 = " << bbox3 << lf;

        bbox3.Translate(-Float3::Unity);
        ls << "bbox3 = " << bbox3 << lf;

        if (bbox2.Intersection(bbox3) != bbox)
        {
            ls << "Invalid Intersection of AABB. Intersection of " << bbox2
               << " and " << bbox3 << " = " << bbox2.Intersection(bbox3)
               << ", but expecting " << bbox << lferr;
        }

        bbox2.Translate(Float3::Unity);
        if (!bbox2.Intersection(bbox3).IsEmpty())
        {
            ls << "Invalid Intersection of AABB. " << hendl
               << "Intersection of " << hendl << bbox2 << hendl << " and "
               << hendl << bbox3 << hendl << "is " << bbox2.Intersection(bbox3)
               << hendl << "But expecting the empty." << lferr;
        }
    });
}

#endif // __UNIT_TEST__
