// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef Platform_h
#define Platform_h

// =====================================================================================================================
// Debug Control
// =====================================================================================================================

#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#undef __DEBUG__
#define __DEBUG__
#endif // _DEBUG

// =====================================================================================================================
// Mathematics
// =====================================================================================================================

//#define __LEFT_HANDED__
#define __RIGHT_HANDED__

#endif /* Platform_h */
