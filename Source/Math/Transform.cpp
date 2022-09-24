// Created by mooming.go@gmail.com, 2017 - 2022

#include "Transform.h"


namespace HE
{
template class Transform<float>;
template class Transform<double>;
} // HE

#ifdef __UNIT_TEST__
#include "Vector3.h"
#include "System/CommonUtil.h"


namespace HE
{

void TransformTest::Prepare()
{
    AddTest("Default Constructor", [this](auto& ls)
    {
        FTransform root;
        ls << root << lf;
    });

    
    AddTest("Hierachycal Transform", [this](auto& ls)
    {
        FTransform root;
        FTransform a;
        root.Attach(a);
        
        FTransform b;
        a.Attach(b);
        
        FTransform c;
        b.Attach(c);
        
        a.Set(Float3(0, 0, 1));
        b.Set(Float3(0, 1, 0));
        c.Set(Float3(1, 0, 0));
        
        ls << "a = " << a << lf;
        ls << "b = " << b << lf;
        ls << "c = " << c << lf;
        
        auto worldC = c.GetWorldTransform().translation;
        if (worldC != Float3(1, 1, 1))
        {
            ls << "c(" << worldC
                << ") doesn't coincide with (1, 1, 1)" << lferr;
        }
    });
    
    AddTest("Hierachycal Rotation", [this](auto& ls)
    {
        FTransform root;
        FTransform a;
        root.Attach(a);
        
        FTransform b;
        a.Attach(b);
        
        b.Set(Float3(0, 0, 1));
        a.Set(Quat(0.0f, 45.0f, 0.0f));
        
        ls << "b = " << b << lf;
        
        auto worldB = b.GetWorldTransform().translation;
        if (!worldB.IsUnity())
        {
            ls << "b " << worldB
                << " is not the unity." << lferr;
        }
        
        a.Set(Quat(0.0f, 90.0f, 0.0f));

        ls << "b = " << b << lf;
        
        worldB = b.GetWorldTransform().translation;
        if (worldB != Float3(1, 0, 0))
        {
            ls << "b " << worldB
                << " doesn't coincide with (1, 0, 0)" << lferr;
        }
    });

    AddTest("Hierachycal Rotation (2)", [this](auto& ls)
    {
        FTransform root;
        FTransform a;
        root.Attach(a);
        
        FTransform b;
        a.Attach(b);
        
        FTransform c;
        b.Attach(c);
        
        a.Set(Quat(0.0f, 45.0f, 0.0f));
        b.Set(Quat(0.0f, 45.0f, 0.0f));
        c.Set(Float3(0, 0, 1));
        
        ls << "c = " << c << lf;
        
        auto worldC = c.GetWorldTransform().translation;
        if (!worldC.IsUnity())
        {
            ls << "The world transformed c" << worldC
                << " is not unity." << lferr;
        }
        
        ls << "c = " << c << lf;
        
        worldC = c.GetWorldTransform().translation;
        if (c.GetWorldTransform().translation != Float3(1, 0, 0))
        {
            ls << "The world transformed c" << worldC
                << " doesn't coincide with (1, 0, 0)" << lferr;
        }
    });
}

} // HE

#endif //__UNIT_TEST__
