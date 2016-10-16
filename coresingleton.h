#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <Lock.h>
//----------------------------------------------------------------------

template <class T> struct CreateUsingNew {
    static T* Create(void) 
	{
        return new T;
    }

    static void Destroy(T* p) 
	{
        delete p;
    }
};
//----------------------------------------------------------------------

template < class T, template <class> class CreationPolicy = CreateUsingNew >
class CSingleton
{
public: 
    static T* Instance(void);
    static void Destroy(void);

private:
    CSingleton(void);
    CSingleton(const CSingleton&);
    CSingleton& operator= (const CSingleton&);

private: 
    static T*       _instance;
#ifdef _THREAD_SAFE
    static CMutex   _mutex;
#endif
};
//----------------------------------------------------------------------

#ifdef _THREAD_SAFE
template <class T, template <class> class CreationPolicy> 
CMutex CSingleton<T, CreationPolicy>::_mutex;
#endif

template <class T, template <class> class CreationPolicy> 
T* CSingleton<T, CreationPolicy>::_instance = 0;

//----------------------------------------------------------------------

template <class T, template <class> class CreationPolicy> 
T* CSingleton<T, CreationPolicy>::Instance (void)
{
    if (0 == _instance) {
#ifdef _THREAD_SAFE
        CScopedLock guard(_mutex);
#endif

        if (0 == _instance) {
            _instance = CreationPolicy<T>::Create ();
        }
    }

    return _instance;
}
//----------------------------------------------------------------------

template <class T, template <class> class CreationPolicy> 
void CSingleton<T, CreationPolicy>::Destroy (void)
{
    return CreationPolicy<T>::Destroy (_instance);
}
//----------------------------------------------------------------------

#endif //__SINGLETON_H__
