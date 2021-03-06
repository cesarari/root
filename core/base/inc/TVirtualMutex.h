// @(#)root/base:$Id$
// Author: Fons Rademakers   14/07/2002

/*************************************************************************
 * Copyright (C) 1995-2002, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TVirtualMutex
#define ROOT_TVirtualMutex


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TVirtualMutex                                                        //
//                                                                      //
// This class implements a mutex interface. The actual work is done via //
// TMutex which is available as soon as the thread library is loaded.   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "TObject.h"

#include <memory>

class TVirtualMutex;

// Global mutex set in TThread::Init
R__EXTERN TVirtualMutex *gGlobalMutex;

class TVirtualMutex {

public:
   struct State {
      virtual ~State();
   };

   TVirtualMutex(Bool_t /* recursive */ = kFALSE) { }
   virtual ~TVirtualMutex() { }

   virtual Int_t Lock() = 0;
   virtual Int_t TryLock() = 0;
   virtual Int_t UnLock() = 0;
   virtual Int_t CleanUp() = 0;
   Int_t Acquire() { return Lock(); }
   Int_t Release() { return UnLock(); }

   virtual TVirtualMutex *Factory(Bool_t /*recursive*/ = kFALSE) = 0;
   virtual std::unique_ptr<TVirtualMutex::State> Reset() = 0;
   virtual void Restore(std::unique_ptr<State> &&state) = 0;

   ClassDef(TVirtualMutex, 0)  // Virtual mutex lock class
};


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TLockGuard                                                           //
//                                                                      //
// This class provides mutex resource management in a guaranteed and    //
// exception safe way. Use like this:                                   //
// {                                                                    //
//    TLockGuard guard(mutex);                                          //
//    ... // do something                                               //
// }                                                                    //
// when guard goes out of scope the mutex is unlocked in the TLockGuard //
// destructor. The exception mechanism takes care of calling the dtors  //
// of local objects so it is exception safe.                            //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class TLockGuard {

private:
   TVirtualMutex *fMutex;

   TLockGuard(const TLockGuard&);             // not implemented
   TLockGuard& operator=(const TLockGuard&);  // not implemented

public:
   TLockGuard(TVirtualMutex *mutex)
     : fMutex(mutex) { if (fMutex) fMutex->Lock(); }
   Int_t UnLock() {
      if (!fMutex) return 0;
      auto tmp = fMutex;
      fMutex = 0;
      return tmp->UnLock();
   }
   ~TLockGuard() { if (fMutex) fMutex->UnLock(); }

   ClassDefNV(TLockGuard,0)  // Exception safe locking/unlocking of mutex
};

namespace ROOT {

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TLockSuspend                                                         //
//                                                                      //
// This class provides mutex supension management in a guaranteed and   //
// exception safe way. Use like this:                                   //
// {                                                                    //
//    TLockSuspend guard(mutex);                                        //
//    ... // do something                                               //
// }                                                                    //
// when guard is created, 'suspend' the lock by calling Reset on the    //
// on the mutex and when the guard goes out of scope the mutex is       //
// restored in the TLockSuspend destructor.                             //
// The exception mechanism takes care of calling the dtors              //
// of local objects so it is exception safe.                            //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class TLockSuspend {
private:
   TVirtualMutex *const fMutex;

   std::unique_ptr<TVirtualMutex::State> fState;

   TLockSuspend(const TLockSuspend &) = delete;
   TLockSuspend &operator=(const TLockSuspend &) = delete;

public:
   TLockSuspend(TVirtualMutex *mutex) : fMutex(mutex)
   {
      if (fMutex)
         fState = mutex->Reset();
   }

   ~TLockSuspend()
   {
      if (fMutex)
         fMutex->Restore(std::move(fState));
   }

   ClassDefNV(TLockSuspend, 0) // Exception safe Reset/Restore of mutex
};

} // Namespace ROOT.

// Zero overhead macros in case not compiled with thread support
#if defined (_REENTRANT) || defined (WIN32)

#define R__LOCKGUARD(mutex) TLockGuard _R__UNIQUE_(R__guard)(mutex)
#define R__LOCKGUARD2(mutex)                             \
   if (gGlobalMutex && !mutex) {                         \
      gGlobalMutex->Lock();                              \
      if (!mutex)                                        \
         mutex = gGlobalMutex->Factory(kTRUE);           \
      gGlobalMutex->UnLock();                            \
   }                                                     \
   R__LOCKGUARD(mutex)
#define R__LOCKGUARD_NAMED(name,mutex) TLockGuard _NAME2_(R__guard,name)(mutex)
#define R__LOCKGUARD_UNLOCK(name) _NAME2_(R__guard,name).UnLock()
#define R__LOCK_SUSPEND(mutex) TLockSuspend _R__UNIQUE_(R__guard)(mutex)
#else
#define R__LOCKGUARD(mutex)  (void)mutex; { }
#define R__LOCKGUARD_NAMED(name,mutex) (void)mutex; { }
#define R__LOCKGUARD2(mutex) (void)mutex; { }
#define R__LOCKGUARD_UNLOCK(name) { }
#define R__LOCK_SUSPEND(mutex) { }
#endif

#ifdef R__USE_IMT
#define R__LOCKGUARD_IMT(mutex)  if (ROOT::Internal::IsParBranchProcessingEnabled()) R__LOCKGUARD(mutex)
#define R__LOCKGUARD_IMT2(mutex) if (ROOT::Internal::IsParBranchProcessingEnabled()) R__LOCKGUARD2(mutex)
#else
#define R__LOCKGUARD_IMT(mutex)  { }
#define R__LOCKGUARD_IMT2(mutex) { }
#endif

#ifdef R__USE_IMT
#define R__RWLOCK_ACQUIRE_READ(rwlock)  if (ROOT::Internal::IsParTreeProcessingEnabled()) rwlock.ReadLock();
#define R__RWLOCK_RELEASE_READ(rwlock)  if (ROOT::Internal::IsParTreeProcessingEnabled()) rwlock.ReadUnLock();
#define R__RWLOCK_ACQUIRE_WRITE(rwlock) if (ROOT::Internal::IsParTreeProcessingEnabled()) rwlock.WriteLock();
#define R__RWLOCK_RELEASE_WRITE(rwlock) if (ROOT::Internal::IsParTreeProcessingEnabled()) rwlock.WriteUnLock();
#else
#define R__RWLOCK_ACQUIRE_READ(rwlock)  { }
#define R__RWLOCK_RELEASE_READ(rwlock)  { }
#define R__RWLOCK_ACQUIRE_WRITE(rwlock) { }
#define R__RWLOCK_RELEASE_WRITE(rwlock) { }
#endif

#endif
