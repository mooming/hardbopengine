// Copyright, all rights are reserved by Hansol Park, mooming.go@gmail.compl

#include "AABB.h"

namespace HE
{
template class AABB<Float2>;
template class AABB<Float3>;
} // HE

#ifdef __UNIT_TEST__
#include "Log/Logger.h"


bool HE::AABBTest::DoTest()
{
    using std::cout;
    using std::cerr;
    using std::endl;

    auto log = Logger::Get(GetName());

    {
        AABB3 bbox;

        log.Out([&bbox](auto& ls)
        {
            ls << bbox;
        });

        if (!bbox.IsEmpty())
        {

            log.OutError([](auto& ls)
            {
                ls << "AABB created with default constructor should be empty.";
            });

            return false;
        }

        bbox.Add(Float3::Zero);
        bbox.Add(Float3::Forward);
        bbox.Add(Float3::Right);
        bbox.Add(Float3::Up);

        log.Out([&bbox](auto& ls)
        {
            ls << "bbox = " << bbox;
        });

        if (bbox.Diagonal() != Float3::Unity)
        {
            log.OutError([&bbox](auto& ls)
            {
                ls << "AABB dianoal error. " << bbox.Diagonal()
                    << ", but expected " << Float3::Unity;
            });

            return false;
        }

        if (!bbox.IsContaining(bbox))
        {
            log.OutError([&bbox](auto& ls)
            {
                ls << "AABB should contain itself! " << bbox;
            });

            return false;
        }

        if (bbox != bbox)
        {
            log.OutError([&bbox](auto& ls)
            {
                ls << "AABB should match itself! " << bbox;
            });

            return false;
        }

        if (bbox.Intersection(bbox) != bbox)
        {
            log.OutError([&bbox](auto& ls)
            {
                ls << "Self intersection of AABB should match itself! "
                    << bbox;
            });

            return false;
        }

        auto bbox2 = bbox;
        if (bbox != bbox2)
        {
            log.OutError([&bbox, &bbox2](auto& ls)
            {
                ls << "Clone of AABB should match itself! "
                    << bbox << ", " << bbox2;
            });

            return false;
        }

        bbox2.max *= 2.0f;
        log.Out([&bbox2](auto& ls)
        {
            ls << "bbox2 = " << bbox2;
        });

        if (bbox2.Intersection(bbox) != bbox)
        {
            log.OutError([&bbox, &bbox2](auto& ls)
            {
                ls << "Invalid Intersection of AABB. Intersection of "
                    << bbox2 << " and " << bbox << " = "
                    << bbox2.Intersection(bbox) << endl;
            });

            return false;
        }

        if ((bbox + bbox2) != bbox2)
        {
            log.OutError([&bbox, &bbox2](auto& ls)
            {
                ls << "Invalid union of AABB. Union of "
                    << bbox << " and " << bbox2 << " = "
                    << (bbox + bbox2);
            });

            return false;
        }

        auto bbox3 = bbox2;
        bbox3.Translate(-Float3::Unity);

        log.Out([&bbox3](auto& ls)
        {
            ls << "bbox3 = " << bbox3;
        });

        if (bbox2.Intersection(bbox3) != bbox)
        {
            log.OutError([&bbox, &bbox2, &bbox3](auto& ls)
            {
                ls << "Invalid Intersection of AABB. Intersection of "
                    << bbox2 << " and " << bbox3 << " = "
                    << bbox2.Intersection(bbox3)
                    << ", but expecting " << bbox;
            });

            return false;
        }

        bbox2.Translate(Float3::Unity);
        if (!bbox2.Intersection(bbox3).IsEmpty())
        {
            log.OutError([&bbox2, &bbox3](auto& ls)
            {
                ls << "Invalid Intersection of AABB. " << endl
                    << "Intersection of " << endl
                    << bbox2 << endl
                    << " and " << endl
                    << bbox3 << endl
                    << "is " << bbox2.Intersection(bbox3) << endl
                    << "But expecting the empty.";
            });

            return false;
        }
    }

    return true;
}

#endif  // __UNIT_TEST__
