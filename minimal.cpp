
#include <emscripten.h>
#define BOOST_DISABLE_ASSERTS
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>

namespace boost
{
const boost::detail::sp_nothrow_tag NO_THROW_TAG;
template<class T> class enable_shared_from_this_no_throw
{
protected:

    enable_shared_from_this_no_throw()
    {
    }

    enable_shared_from_this_no_throw(enable_shared_from_this_no_throw const &)
    {
    }

    enable_shared_from_this_no_throw & operator=(enable_shared_from_this_no_throw const &)
    {
        return *this;
    }

    ~enable_shared_from_this_no_throw()
    {
    }

public:

    shared_ptr<T> shared_from_this()
    {
        shared_ptr<T> p( weak_this_, NO_THROW_TAG);
        //BOOST_ASSERT( p.get() == this );
        return p;
    }

    shared_ptr<T const> shared_from_this() const
    {
        shared_ptr<T const> p( weak_this_, NO_THROW_TAG );
        //BOOST_ASSERT( p.get() == this );
        return p;
    }

public: // actually private, but avoids compiler template friendship issues

    // Note: invoked automatically by shared_ptr; do not call
    template<class X, class Y> void _internal_accept_owner( shared_ptr<X> const * ppx, Y * py ) const
    {
        if( weak_this_.expired() )
        {
            weak_this_ = shared_ptr<T>( *ppx, py );
        }
    }

private:

    mutable weak_ptr<T> weak_this_;
};

} // namespace boost




struct SmallObject : boost::enable_shared_from_this_no_throw<SmallObject> {
    
};

EMSCRIPTEN_KEEPALIVE
int using_shared_ptr()
{
    //std::recursive_mutex bees;
    //bees.lock();
    //int wow = 0;
    //bees.unlock();
    //boost::weak_ptr<SmallObject> weak_this_;
    //weak_this_.reset();
    //if (weak_this_.empty()) {
        //boost::shared_ptr<SmallObject> bees(weak_this_, );
        //weak_this_.reset();
        //dynamic v = weak_this_.px;
    
    //}
    
    //boost::shared_ptr<SmallObject> bees2(weak_this_);
    //;bees2.get();
    
    SmallObject* wow = new SmallObject();
    boost::shared_ptr <SmallObject> ptr_name = wow->shared_from_this();

    //weak_this_.get();
}