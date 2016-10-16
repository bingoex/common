#ifndef __NONCOPYABLE_H__
#define __NONCOPYABLE_H__

class noncopyable
{
protected:
    noncopyable(void) {

    }

    ~noncopyable(void) {

    }

private:  // emphasize the following members are private
    noncopyable(const noncopyable&);
    const noncopyable& operator= (const noncopyable&);
};

#endif 


