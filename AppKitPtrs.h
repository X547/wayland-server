#ifndef _APPKITPTRS_H_
#define _APPKITPTRS_H_

#include <utility>


namespace AppKitPtrs {

template <typename Base>
class LockedPtr {
	Base *fPtr;
public:
	LockedPtr(): fPtr(NULL) {}
	LockedPtr(Base *ptr): fPtr(ptr) {if (fPtr != NULL) fPtr->LockLooper();}
	~LockedPtr() {Unset();}

	void Unset()
	{
		if (fPtr != NULL) {
			fPtr->UnlockLooper();
			fPtr = NULL;
		}
	}

	LockedPtr& operator =(Base *ptr)
	{
		if (fPtr == ptr) return *this;
		Unset();
		fPtr = ptr;
		if (fPtr != NULL) fPtr->LockLooper();
		return *this;
	}

	Base& operator*() const {return *fPtr;}
	Base* operator->() const {return fPtr;}
	operator Base*() const {return fPtr;}
};

template <typename Base>
LockedPtr<Base> MakeLocked(Base *ptr)
{
	return LockedPtr<Base>(ptr);
}

template <typename Base>
class ExternalPtr {
private:
	Base *fPtr;
	friend class AsyncReq;
	template<class OtherBase> friend class ExternalPtr;

public:
	ExternalPtr(): fPtr(NULL) {}
	ExternalPtr(Base *ptr): fPtr(ptr) {}
	template <typename OtherBase> ExternalPtr(ExternalPtr<OtherBase> other): fPtr(other.fPtr) {}
	void Unset() {fPtr = NULL;}
	ExternalPtr& operator =(Base *ptr) {fPtr = ptr; return *this;}
	ExternalPtr& operator =(const ExternalPtr<Base> &ptr) {fPtr = ptr.fPtr; return *this;}
	template <typename OtherBase> void operator=(ExternalPtr<OtherBase> other) {fPtr = other.fPtr;}
	template <typename OtherBase> void operator=(LockedPtr<OtherBase> other){fPtr = other.fPtr;}

	LockedPtr<Base> Lock() const
	{
		return LockedPtr<Base>(fPtr);
	}

	LockedPtr<Base> operator->() const {return Lock();}

	Base *Get() const {return fPtr;}
};

template<typename T, typename... Args>
ExternalPtr<T> MakeExternal(Args&&... args)
{
	ExternalPtr<T> ptr(new T(std::forward<Args>(args)...));
	return ptr;
}
}

#endif	// _APPKITPTRS_H_
