/*
    rgb2yuv - Code covered by the MIT License
    Author: mutouyun (http://darkc.at)
*/

#ifndef RGB2YUV_HPP__
#define RGB2YUV_HPP__

#include <stddef.h>  // size_t, ...
#include <stdint.h>  // uint8_t, ...
#include <assert.h>  // assert
#include <new>       // placement new, std::nothrow
#include <algorithm> // std::swap

////////////////////////////////////////////////////////////////
/// Predefine functional macros
////////////////////////////////////////////////////////////////

#pragma push_macro("GLB_")
#undef  GLB_
#define GLB_ ::

#pragma push_macro("STD_")
#undef  STD_
#define STD_ GLB_ std::

#pragma push_macro("R2Y_NAMESPACE_")
#undef  R2Y_NAMESPACE_
#define R2Y_NAMESPACE_ r2y

#pragma push_macro("R2Y_")
#undef  R2Y_
#define R2Y_ GLB_ R2Y_NAMESPACE_ ::

#pragma push_macro("R2Y_ALLOC_")
#ifndef R2Y_ALLOC_
#define R2Y_ALLOC_ R2Y_ allocator
#endif // R2Y_ALLOC_

#pragma push_macro("R2Y_UNUSED_")
#undef  R2Y_UNUSED_
#if defined(_MSC_VER)
#   define R2Y_UNUSED_ __pragma(warning(suppress:4100))
#elif defined(__GNUC__)
#   define R2Y_UNUSED_ __attribute__((__unused__))
#else
#   define R2Y_UNUSED_
#endif

namespace R2Y_NAMESPACE_ {

////////////////////////////////////////////////////////////////
/// Provide basic universal types and constants
////////////////////////////////////////////////////////////////

typedef          GLB_ uint8_t               byte_t;
typedef struct { GLB_ uint8_t b_, g_, r_; } rgb_t;
typedef struct { GLB_ uint8_t v_, u_, y_; } yuv_t;

enum supported
{
    rgb_MIN,
    rgb_888,
    rgb_565,
    rgb_555,
    rgb_444,
    rgb_888X,
    rgb_MAX,

    /*
     * YUV Formats Chapter 2. Image Formats
     * See: http://www.linuxtv.org/downloads/v4l-dvb-apis/yuv-formats.html
    */
    yuv_MIN,
    yuv_NV24,            // 444 SP
    yuv_NV42,
    yuv_YUY2,            // 422
    yuv_YUYV = yuv_YUY2,
    yuv_YVYU,
    yuv_UYVY,
    yuv_VYUY,
    yuv_422P,            // 422 P
    yuv_YV12,            // 420 P
    yuv_YU12,
    yuv_I420 = yuv_YU12,
    yuv_NV12,            // 420 SP
    yuv_NV21,
    yuv_Y41P,            // 411
    yuv_Y411 = yuv_Y41P,
    yuv_411P,            // 411 P
    yuv_YVU9,            // 410 P
    yuv_YUV9,
    yuv_MAX
};

enum plane_type
{
    plane_Y = 0,
    plane_U,
    plane_V,

    plane_R,
    plane_G,
    plane_B
};

////////////////////////////////////////////////////////////////
/// Useful tools for SFINAE
////////////////////////////////////////////////////////////////

template <bool, typename T = void> struct enable_if          {  /* Nothing */  };
template <      typename T>        struct enable_if<true, T> { typedef T type; };

template <R2Y_ supported S> struct is_rgb
{
    enum { value = ((S > rgb_MIN) && (S < rgb_MAX)) ? 1 : 0 };
};

template <R2Y_ supported S> struct is_yuv
{
    enum { value = ((S > yuv_MIN) && (S < yuv_MAX)) ? 1 : 0 };
};

template <R2Y_ plane_type P> struct is_rgb_plane               { enum { value = 0 }; };
template <>                  struct is_rgb_plane<R2Y_ plane_R> { enum { value = 1 }; };
template <>                  struct is_rgb_plane<R2Y_ plane_G> { enum { value = 1 }; };
template <>                  struct is_rgb_plane<R2Y_ plane_B> { enum { value = 1 }; };

template <R2Y_ plane_type P> struct is_yuv_plane               { enum { value = 0 }; };
template <>                  struct is_yuv_plane<R2Y_ plane_Y> { enum { value = 1 }; };
template <>                  struct is_yuv_plane<R2Y_ plane_U> { enum { value = 1 }; };
template <>                  struct is_yuv_plane<R2Y_ plane_V> { enum { value = 1 }; };

////////////////////////////////////////////////////////////////
/// Define default memory allocator
////////////////////////////////////////////////////////////////

struct allocator
{
    static void* alloc(GLB_ size_t size)
    {
        return ( (size == 0) ? NULL : GLB_ operator new(size, STD_ nothrow) );
    }

    static void free(void* ptr)
    {
        GLB_ operator delete(ptr, STD_ nothrow);
    }
};

////////////////////////////////////////////////////////////////
/// The limited garbage collection facility for memory block
////////////////////////////////////////////////////////////////

template <typename T, typename AllocT = R2Y_ALLOC_>
class scope_block
{
    template <typename U, typename A>
    friend class scope_block;

    T *         block_;
    GLB_ size_t count_;
    bool        trust_;

public:
    scope_block(GLB_ size_t count)
        : block_(NULL), count_(0), trust_(false)
    {
        reset(count);
    }

    scope_block(T * block, GLB_ size_t count)
        : block_(NULL), count_(0), trust_(false)
    {
        reset(block, count);
    }

    /*
     * The copy constructor always taking ownership 
     * from the other scope_block object, just like std::auto_ptr.
     */
    template <typename U>
    scope_block(scope_block<U, AllocT> const & rhs)
        : block_(NULL), count_(0), trust_(false)
    {
        this->move(rhs);
    }

    ~scope_block(void)
    {
        if (trust_) AllocT::free(block_);
    }

    void reset(GLB_ size_t count)
    {
        this->~scope_block();
        count_ = count;
        block_ = static_cast<T *>( AllocT::alloc(this->size()) );
        trust_ = true;
    }

    void reset(T * block, GLB_ size_t count)
    {
        this->~scope_block();
        count_ = count;
        block_ = block;
        trust_ = false;
    }

    void trust(bool t) { trust_ = t; }

    template <typename U>
    void move(scope_block<U, AllocT> const & rhs)
    {
        this->swap( const_cast<scope_block<U, AllocT> &>(rhs) );
    }

    void swap(scope_block & rhs)
    {
        STD_ swap(this->block_, rhs.block_);
        STD_ swap(this->count_, rhs.count_);
        STD_ swap(this->trust_, rhs.trust_);
    }

    template <typename U>
    void swap(scope_block<U, AllocT> & rhs)
    {
        // swap block pointer
        void * tmp_ptr = this->block_;
        this->block_ = reinterpret_cast<T *>(rhs.block_);
        rhs  .block_ = reinterpret_cast<U *>(tmp_ptr);
        // swap block count var
        GLB_ size_t ths_size = this->size(), rhs_size = rhs.size();
        this->count_ = rhs_size / sizeof(T); // re-split the memory block
        rhs  .count_ = ths_size / sizeof(U);
        // swap the trust flag
        STD_ swap(this->trust_, rhs.trust_);
    }

    /*
     * You need to handle the datas from this scope_block object
     * by yourself before you calling this function.
     */
    T * dismiss(void)
    {
        T * data_ret = this->data();
        block_ = NULL;
        count_ = 0;
        trust_ = false;
        return data_ret;
    }

    T *         data (void) const { return block_; }
    GLB_ size_t count(void) const { return count_; }
    GLB_ size_t size (void) const { return this->count() * sizeof(T); }
    bool   is_trusted(void) const { return trust_; }

    T       & operator[](GLB_ size_t pos)       { return block_[pos]; }
    T const & operator[](GLB_ size_t pos) const { return block_[pos]; }

private:
    /*
     * Delete the copy assignment operator,
     * to make scope_block non-copyable.
     */
    scope_block & operator=(const scope_block &); // No need to implement it.
};

template <typename T, typename U, typename A>
void swap(R2Y_ scope_block<T, A> & a, R2Y_ scope_block<U, A> & b)
{
    a.swap(b);
}

////////////////////////////////////////////////////////////////
/// RGB formatting to 888
////////////////////////////////////////////////////////////////

namespace detail_rgb_format_ {

template <R2Y_ supported S> struct impl_;

template <> struct impl_<R2Y_ rgb_888>
{
    static void to_888(R2Y_UNUSED_ R2Y_ byte_t * in_rgb, R2Y_UNUSED_ GLB_ size_t in_count,
                       R2Y_UNUSED_ R2Y_ rgb_t  * ot_rgb)
    {
        assert(in_rgb == reinterpret_cast<R2Y_ byte_t *>(ot_rgb));
        /* Do nothing. */
    }
};

#pragma push_macro("R2Y_BMP_16_TRANSFORM_HELPER_")
#undef  R2Y_BMP_16_TRANSFORM_HELPER_
#define R2Y_BMP_16_TRANSFORM_HELPER_(MASK_R, MASK_G, MASK_B, SHIFT_R, SHIFT_G, SHIFT_B) do \
{                                                                                          \
    assert(in_rgb != reinterpret_cast<R2Y_ byte_t *>(ot_rgb));                             \
    assert((in_count & 1) == 0); /* in_count must be an even number */                     \
                                                                                           \
    GLB_ uint16_t * cur_pixel = reinterpret_cast<GLB_ uint16_t *>(in_rgb);                 \
    for (GLB_ size_t i = 0; i < in_count; i += 2, ++cur_pixel, ++ot_rgb)                   \
    {                                                                                      \
        ot_rgb->r_ = static_cast<GLB_ uint8_t>( ((*cur_pixel) & (MASK_R)) SHIFT_R );       \
        ot_rgb->g_ = static_cast<GLB_ uint8_t>( ((*cur_pixel) & (MASK_G)) SHIFT_G );       \
        ot_rgb->b_ = static_cast<GLB_ uint8_t>( ((*cur_pixel) & (MASK_B)) SHIFT_B );       \
    }                                                                                      \
} while(0)

template <> struct impl_<R2Y_ rgb_565>
{
    static void to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_count, R2Y_ rgb_t * ot_rgb)
    {
        R2Y_BMP_16_TRANSFORM_HELPER_(0xF800, 0x07E0, 0x001F, >> 8, >> 3, << 3);
    }
};

template <> struct impl_<R2Y_ rgb_555>
{
    static void to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_count, R2Y_ rgb_t * ot_rgb)
    {
        R2Y_BMP_16_TRANSFORM_HELPER_(0x7C00, 0x03E0, 0x001F, >> 7, >> 2, << 3);
    }
};

#pragma pop_macro("R2Y_BMP_16_TRANSFORM_HELPER_")

template <> struct impl_<R2Y_ rgb_444>
{
    static void to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_count, R2Y_ rgb_t * ot_rgb)
    {
        assert(in_rgb != reinterpret_cast<R2Y_ byte_t *>(ot_rgb));
        assert((in_count % 3) == 0); // 3 input bytes convert to 2 pixels (6 bytes)

        for (GLB_ size_t i = 0; i < in_count; i += 3)
        {
            ot_rgb->b_ = static_cast<GLB_ uint8_t>( ((*in_rgb) & 0x0F) << 4 );
            ot_rgb->g_ = static_cast<GLB_ uint8_t>  ((*in_rgb) & 0xF0);        ++in_rgb;
            ot_rgb->r_ = static_cast<GLB_ uint8_t>( ((*in_rgb) & 0x0F) << 4 );
            ++ot_rgb;
            ot_rgb->b_ = static_cast<GLB_ uint8_t>  ((*in_rgb) & 0xF0);        ++in_rgb;
            ot_rgb->g_ = static_cast<GLB_ uint8_t>( ((*in_rgb) & 0x0F) << 4 );
            ot_rgb->r_ = static_cast<GLB_ uint8_t>  ((*in_rgb) & 0xF0);        ++in_rgb;
        }
    }
};

template <> struct impl_<R2Y_ rgb_888X>
{
    static void to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_count, R2Y_ rgb_t * ot_rgb)
    {
        assert(in_rgb != reinterpret_cast<R2Y_ byte_t *>(ot_rgb));
        assert((in_count % 4) == 0); // 4 input bytes convert to 1 pixel (3 bytes)

        GLB_ uint32_t * cur_pixel = reinterpret_cast<GLB_ uint32_t *>(in_rgb);
        for (GLB_ size_t i = 0; i < in_count; i += 4, ++cur_pixel, ++ot_rgb)
        {
            (*ot_rgb) = ( * reinterpret_cast<R2Y_ rgb_t *>(cur_pixel) );
        }
    }
};

} // namespace detail_rgb_format_

template <R2Y_ supported S>
void rgb_format(R2Y_ byte_t * in_rgb, GLB_ size_t in_count, R2Y_ rgb_t * ot_rgb)
{
    R2Y_ detail_rgb_format_::impl_<S>::to_888(in_rgb, in_count, ot_rgb);
}

////////////////////////////////////////////////////////////////
/// Converting between RGB & YUV/YCbCr planes
////////////////////////////////////////////////////////////////

struct planes_t
{
    GLB_ uint8_t c_, b_, a_;

    template <typename P> static planes_t       & cast(P       & p) { return reinterpret_cast<planes_t       &>(p); }
    template <typename P> static planes_t const & cast(P const & p) { return reinterpret_cast<planes_t const &>(p); }
};

struct convertor
{
    GLB_ int32_t fa_, fb_, fc_, fd_, fe_;

    static GLB_ uint8_t clip(GLB_ int32_t value)
    {
        return static_cast<GLB_ uint8_t>( (value < 0  ) ? 0   : 
                                          (value > 255) ? 255 : value );
    }

    GLB_ uint8_t convert(R2Y_ planes_t const & in_p) const
    {
        return clip(((fa_ * in_p.a_ + fb_ * in_p.b_ + fc_ * in_p.c_ + fd_) >> 8) + fe_);
    }
};

inline R2Y_ convertor const * factor_matrix(void)
{
    /*
     * The factors for converting between YUV and RGB
     * See: https://msdn.microsoft.com/en-us/library/ms893078.aspx
     */
    static R2Y_ convertor const matrix[] =
    {
        {  66 ,  129,  25 ,  128  , 16  },  // Y
        { -38 , -74 ,  112,  128  , 128 },  // U/Cb
        {  112, -94 , -18 ,  128  , 128 },  // V/Cr

        {  298,  0  ,  409, -56992, 0   },  // R
        {  298, -100, -208,  34784, 0   },  // G
        {  298,  516,  0  , -70688, 0   }   // B
    };
    return matrix;
}

template <R2Y_ plane_type P>
GLB_ uint8_t pixel_convert(R2Y_ planes_t const & in_p)
{
    return factor_matrix()[P].convert(in_p);
}

template <R2Y_ plane_type P>
typename R2Y_ enable_if<R2Y_ is_yuv_plane<P>::value,
GLB_ uint8_t>::type pixel_convert(R2Y_ rgb_t const & in_p)
{
    return pixel_convert<P>(R2Y_ planes_t::cast(in_p));
}

template <R2Y_ plane_type P>
typename R2Y_ enable_if<R2Y_ is_rgb_plane<P>::value,
GLB_ uint8_t>::type pixel_convert(R2Y_ yuv_t const & in_p)
{
    return pixel_convert<P>(R2Y_ planes_t::cast(in_p));
}

////////////////////////////////////////////////////////////////
/// Create a buffer for transforming the image pixels
////////////////////////////////////////////////////////////////

namespace detail_create_buffer_ {

template <R2Y_ supported S> struct impl_;

/* Calculate RGB */

template <> struct impl_<R2Y_ rgb_888>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        return (in_w * in_h) * sizeof(R2Y_ rgb_t);
    }
};

template <> struct impl_<R2Y_ rgb_565>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        return (in_w * in_h) * sizeof(GLB_ uint16_t);
    }
};
template <> struct impl_<R2Y_ rgb_555> : impl_<R2Y_ rgb_565> {};

template <> struct impl_<R2Y_ rgb_444>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        return ( (in_w * in_h * 3 + 1) >> 1 );
    }
};

template <> struct impl_<R2Y_ rgb_888X>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        return (in_w * in_h) * sizeof(GLB_ uint32_t);
    }
};

/* Calculate YUV */

template <> struct impl_<R2Y_ yuv_NV24>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        return (in_w * in_h) * sizeof(R2Y_ yuv_t);
    }
};
template <> struct impl_<R2Y_ yuv_NV42> : impl_<R2Y_ yuv_NV24>{};

template <> struct impl_<R2Y_ yuv_YUY2>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        GLB_ size_t s = in_w * in_h;
        assert((s & 1) == 0); // s % 2 == 0
        return (s << 1);
    }
};
template <> struct impl_<R2Y_ yuv_YVYU> : impl_<R2Y_ yuv_YUY2>{};
template <> struct impl_<R2Y_ yuv_UYVY> : impl_<R2Y_ yuv_YUY2>{};
template <> struct impl_<R2Y_ yuv_VYUY> : impl_<R2Y_ yuv_YUY2>{};
template <> struct impl_<R2Y_ yuv_422P> : impl_<R2Y_ yuv_YUY2>{};

template <> struct impl_<R2Y_ yuv_Y41P>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        GLB_ size_t s = in_w * in_h;
        assert((s & 7) == 0); // s % 8 == 0
        return ( s + (s >> 1) );
    }
};

template <> struct impl_<R2Y_ yuv_411P>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        GLB_ size_t s = in_w * in_h;
        assert((s & 3) == 0); // s % 4 == 0
        return ( s + (s >> 1) );
    }
};
template <> struct impl_<R2Y_ yuv_YV12> : impl_<R2Y_ yuv_411P>{};
template <> struct impl_<R2Y_ yuv_YU12> : impl_<R2Y_ yuv_411P>{};
template <> struct impl_<R2Y_ yuv_NV12> : impl_<R2Y_ yuv_411P>{};
template <> struct impl_<R2Y_ yuv_NV21> : impl_<R2Y_ yuv_411P>{};

template <> struct impl_<R2Y_ yuv_YVU9>
{
    static GLB_ size_t size(GLB_ size_t in_w, GLB_ size_t in_h)
    {
        GLB_ size_t s = in_w * in_h;
        assert((s & 15) == 0); // s % 16 == 0
        return ( s + (s >> 3) );
    }
};
template <> struct impl_<R2Y_ yuv_YUV9> : impl_<R2Y_ yuv_YVU9>{};

} // namespace detail_create_buffer_

template <R2Y_ supported S>
GLB_ size_t calculate_size(GLB_ size_t in_w, GLB_ size_t in_h)
{
    return R2Y_ detail_create_buffer_::impl_<S>::size(in_w, in_h);
}

template <R2Y_ supported S>
R2Y_ scope_block<R2Y_ byte_t> create_buffer(GLB_ size_t in_w, GLB_ size_t in_h)
{
    return R2Y_ scope_block<R2Y_ byte_t>( calculate_size<S>(in_w, in_h) );
}

////////////////////////////////////////////////////////////////
/// Transforming between RGB & YUV/YCbCr blocks
////////////////////////////////////////////////////////////////

namespace detail_transform_ {

#pragma push_macro("R2Y_DETAIL_")
#undef  R2Y_DETAIL_
#define R2Y_DETAIL_ R2Y_ detail_transform_::

template <R2Y_ supported S, int = R2Y_ is_rgb<S>::value>
struct rgb_formatter;

template <> struct rgb_formatter<R2Y_ rgb_888, 1>
{
    static R2Y_ scope_block<R2Y_ byte_t> to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_w, GLB_ size_t in_h)
    {
        R2Y_ scope_block<R2Y_ rgb_t> ret( reinterpret_cast<R2Y_ rgb_t *>(in_rgb), in_w * in_h );
        R2Y_ rgb_format<R2Y_ rgb_888>(in_rgb, ret.size(), ret.data());
        return ret;
    }
};

template <R2Y_ supported S> struct rgb_formatter<S, 1>
{
    static R2Y_ scope_block<R2Y_ byte_t> to_888(R2Y_ byte_t * in_rgb, GLB_ size_t in_w, GLB_ size_t in_h)
    {
        R2Y_ scope_block<R2Y_ rgb_t> ret( R2Y_ create_buffer<R2Y_ rgb_888>(in_w, in_h) );
        R2Y_ rgb_format<S>(in_rgb, R2Y_ calculate_size<S>(in_w, in_h), ret.data());
        return ret;
    }
};

/* Packed */

template <R2Y_ supported> struct yuv_t;
template <>               struct yuv_t<R2Y_ yuv_YUY2> { GLB_ uint8_t y0_, cb_, y1_, cr_; };
template <>               struct yuv_t<R2Y_ yuv_YVYU> { GLB_ uint8_t y0_, cr_, y1_, cb_; };
template <>               struct yuv_t<R2Y_ yuv_UYVY> { GLB_ uint8_t cb_, y0_, cr_, y1_; };
template <>               struct yuv_t<R2Y_ yuv_VYUY> { GLB_ uint8_t cr_, y0_, cb_, y1_; };

template <> struct yuv_t<R2Y_ yuv_Y41P>
{
    GLB_ uint8_t u0_, y0_, v0_, y1_;
    GLB_ uint8_t u1_, y2_, v1_, y3_;
    GLB_ uint8_t y4_, y5_, y6_, y7_;
};

/* YUV 4:2:2 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_YUY2) || (Tar == R2Y_ yuv_YVYU) || 
                        (Tar == R2Y_ yuv_UYVY) || (Tar == R2Y_ yuv_VYUY)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_DETAIL_ yuv_t<Tar> * p_yuv = reinterpret_cast<R2Y_DETAIL_ yuv_t<Tar> *>( ot_data.data() );
    GLB_ size_t s = in_w * in_h;
    GLB_ uint16_t u_k, v_k;
    for (GLB_ size_t i = 0; i < s; ++p_yuv)
    {
        {
            R2Y_ rgb_t const & rgb = in_data[i]; ++i;
            p_yuv->y0_ = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb);
            u_k        = R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
            v_k        = R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
        }
        {
            R2Y_ rgb_t const & rgb = in_data[i]; ++i;
            p_yuv->y1_ = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb);
            p_yuv->cb_ = ( u_k += R2Y_ pixel_convert<R2Y_ plane_U>(rgb) ) >> 1;
            p_yuv->cr_ = ( v_k += R2Y_ pixel_convert<R2Y_ plane_V>(rgb) ) >> 1;
        }
    }
}

/* YUV 4:1:1 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_Y41P)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_DETAIL_ yuv_t<Tar> * p_yuv = reinterpret_cast<R2Y_DETAIL_ yuv_t<Tar> *>( ot_data.data() );
    GLB_ size_t s = in_w * in_h;
    GLB_ uint16_t u_k, v_k;
    for (GLB_ size_t i = 0; i < s; ++p_yuv)
    {
#   pragma push_macro("R2Y_TRANSFORM_TO_")
#   undef  R2Y_TRANSFORM_TO_
#   define R2Y_TRANSFORM_TO_(I, OP, ...) do                         \
        {                                                           \
            R2Y_ rgb_t const & rgb = in_data[i]; ++i;               \
            p_yuv->y##I##_ = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); \
            u_k           OP R2Y_ pixel_convert<R2Y_ plane_U>(rgb); \
            v_k           OP R2Y_ pixel_convert<R2Y_ plane_V>(rgb); \
            __VA_ARGS__                                             \
        } while(0)

        R2Y_TRANSFORM_TO_(0,  =);
        R2Y_TRANSFORM_TO_(1, +=);
        R2Y_TRANSFORM_TO_(2, +=);
        R2Y_TRANSFORM_TO_(3, +=, p_yuv->u0_ = u_k >> 2; p_yuv->v0_ = v_k >> 2;);
        R2Y_TRANSFORM_TO_(4,  =);
        R2Y_TRANSFORM_TO_(5, +=);
        R2Y_TRANSFORM_TO_(6, +=);
        R2Y_TRANSFORM_TO_(7, +=, p_yuv->u1_ = u_k >> 2; p_yuv->v1_ = v_k >> 2;);

#   pragma pop_macro("R2Y_TRANSFORM_TO_")
    }
}

/* Planar */

template <R2Y_ supported S, R2Y_ plane_type P>
typename R2Y_ enable_if<(P == R2Y_ plane_Y),
R2Y_ byte_t *>::type split(R2Y_ byte_t * in_data, GLB_ size_t /*in_w*/, GLB_ size_t /*in_h*/)
{
    return in_data;
}

template <R2Y_ supported S, R2Y_ plane_type P>
typename R2Y_ enable_if<( (P == R2Y_ plane_U) && (S == R2Y_ yuv_YU12 || S == R2Y_ yuv_411P || 
                                                  S == R2Y_ yuv_422P || S == R2Y_ yuv_YUV9) ) ||
                        ( (P == R2Y_ plane_V) && (S == R2Y_ yuv_YV12 || S == R2Y_ yuv_YVU9) ),
R2Y_ byte_t *>::type split(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    return in_data + (in_w * in_h);
}

template <R2Y_ supported S, R2Y_ plane_type P>
typename R2Y_ enable_if<( (P == R2Y_ plane_V) && (S == R2Y_ yuv_422P) ),
R2Y_ byte_t *>::type split(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    GLB_ size_t s = in_w * in_h;
    return in_data + s + (s >> 1);
}

template <R2Y_ supported S, R2Y_ plane_type P>
typename R2Y_ enable_if<( (P == R2Y_ plane_V) && (S == R2Y_ yuv_YU12 || S == R2Y_ yuv_411P) ) ||
                        ( (P == R2Y_ plane_U) && (S == R2Y_ yuv_YV12) ),
R2Y_ byte_t *>::type split(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    GLB_ size_t s = in_w * in_h;
    return in_data + s + (s >> 2);
}

template <R2Y_ supported S, R2Y_ plane_type P>
typename R2Y_ enable_if<( (P == R2Y_ plane_V) && (S == R2Y_ yuv_YUV9) ) ||
                        ( (P == R2Y_ plane_U) && (S == R2Y_ yuv_YVU9) ),
R2Y_ byte_t *>::type split(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    GLB_ size_t s = in_w * in_h;
    return in_data + s + (s >> 4);
}

template <R2Y_ supported> struct uv_t                { GLB_ uint8_t * cb_, * cr_; };
template <>               struct uv_t<R2Y_ yuv_NV24> { struct inner { GLB_ uint8_t cb_, cr_; } * uv_; };
template <>               struct uv_t<R2Y_ yuv_NV42> { struct inner { GLB_ uint8_t cr_, cb_; } * uv_; };
template <>               struct uv_t<R2Y_ yuv_NV12> { struct inner { GLB_ uint8_t cb_, cr_; } * uv_; };
template <>               struct uv_t<R2Y_ yuv_NV21> { struct inner { GLB_ uint8_t cr_, cb_; } * uv_; };

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_YV12 || S == R2Y_ yuv_YU12 || 
                         S == R2Y_ yuv_411P || S == R2Y_ yuv_422P ||
                         S == R2Y_ yuv_YUV9 || S == R2Y_ yuv_YVU9),
R2Y_DETAIL_ uv_t<S> >::type fill(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    R2Y_DETAIL_ uv_t<S> ret = 
    {
        R2Y_DETAIL_ split<S, R2Y_ plane_U>(in_data, in_w, in_h),
        R2Y_DETAIL_ split<S, R2Y_ plane_V>(in_data, in_w, in_h)
    };
    return ret;
}

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_NV24 || S == R2Y_ yuv_NV42 || 
                         S == R2Y_ yuv_NV12 || S == R2Y_ yuv_NV21),
R2Y_DETAIL_ uv_t<S> >::type fill(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h)
{
    R2Y_DETAIL_ uv_t<S> ret = 
    {
        reinterpret_cast<typename R2Y_DETAIL_ uv_t<S>::inner *>(
                                  R2Y_DETAIL_ split<R2Y_ yuv_YU12, R2Y_ plane_U>(in_data, in_w, in_h))
    };
    return ret;
}

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_422P)
>::type set_and_next(GLB_ uint16_t in_u, GLB_ uint16_t in_v, R2Y_DETAIL_ uv_t<S> & ot_uv)
{
    (*(ot_uv.cb_)) = in_u >> 1; ++(ot_uv.cb_);
    (*(ot_uv.cr_)) = in_v >> 1; ++(ot_uv.cr_);
}

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_YV12 || S == R2Y_ yuv_YU12 || S == R2Y_ yuv_411P)
>::type set_and_next(GLB_ uint16_t in_u, GLB_ uint16_t in_v, R2Y_DETAIL_ uv_t<S> & ot_uv)
{
    (*(ot_uv.cb_)) = in_u >> 2; ++(ot_uv.cb_);
    (*(ot_uv.cr_)) = in_v >> 2; ++(ot_uv.cr_);
}

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_NV12 || S == R2Y_ yuv_NV21)
>::type set_and_next(GLB_ uint16_t in_u, GLB_ uint16_t in_v, R2Y_DETAIL_ uv_t<S> & ot_uv)
{
    ot_uv.uv_->cb_ = in_u >> 2;
    ot_uv.uv_->cr_ = in_v >> 2;
    ++(ot_uv.uv_);
}

template <R2Y_ supported S>
typename R2Y_ enable_if<(S == R2Y_ yuv_YUV9 || S == R2Y_ yuv_YVU9)
>::type set_and_next(GLB_ uint16_t in_u, GLB_ uint16_t in_v, R2Y_DETAIL_ uv_t<S> & ot_uv)
{
    (*(ot_uv.cb_)) = in_u >> 4; ++(ot_uv.cb_);
    (*(ot_uv.cr_)) = in_v >> 4; ++(ot_uv.cr_);
}

/* YUV 4:4:4 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_NV24 || Tar == R2Y_ yuv_NV42)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_ byte_t * p_y = R2Y_DETAIL_ split<Tar, R2Y_ plane_Y>(ot_data.data(), in_w, in_h);
    R2Y_DETAIL_ uv_t<Tar> p_uv = R2Y_DETAIL_ fill<Tar>(ot_data.data(), in_w, in_h);
    GLB_ size_t s = in_w * in_h;
    for (GLB_ size_t i = 0; i < s; ++i, ++p_y, ++(p_uv.uv_))
    {
        R2Y_ rgb_t const & rgb = in_data[i];
        (*p_y)        = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb);
        p_uv.uv_->cb_ = R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
        p_uv.uv_->cr_ = R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
    }
}

/* YUV 4:2:2 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_422P)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_ byte_t * p_y = R2Y_DETAIL_ split<Tar, R2Y_ plane_Y>(ot_data.data(), in_w, in_h);
    R2Y_DETAIL_ uv_t<Tar> p_uv = R2Y_DETAIL_ fill<Tar>(ot_data.data(), in_w, in_h);
    GLB_ size_t s = in_w * in_h;
    GLB_ uint16_t u_k, v_k;
    for (GLB_ size_t i = 0; i < s;)
    {
#   pragma push_macro("R2Y_TRANSFORM_TO_")
#   undef  R2Y_TRANSFORM_TO_
#   define R2Y_TRANSFORM_TO_(OP, ...) do                           \
        {                                                          \
            R2Y_ rgb_t const & rgb = in_data[i]; ++i;              \
            (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y; \
            u_k   OP R2Y_ pixel_convert<R2Y_ plane_U>(rgb);        \
            v_k   OP R2Y_ pixel_convert<R2Y_ plane_V>(rgb);        \
            __VA_ARGS__                                            \
        } while(0)

        R2Y_TRANSFORM_TO_( =);
        R2Y_TRANSFORM_TO_(+=, R2Y_DETAIL_ set_and_next<Tar>(u_k, v_k, p_uv););

#   pragma pop_macro("R2Y_TRANSFORM_TO_")
    }
}

/* YUV 4:2:0 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_YV12 || Tar == R2Y_ yuv_YU12) ||
                        (Tar == R2Y_ yuv_NV12 || Tar == R2Y_ yuv_NV21)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_ byte_t * p_y = R2Y_DETAIL_ split<Tar, R2Y_ plane_Y>(ot_data.data(), in_w, in_h);
    R2Y_DETAIL_ uv_t<Tar> p_uv = R2Y_DETAIL_ fill<Tar>(ot_data.data(), in_w, in_h);
    R2Y_ scope_block<GLB_ uint16_t> u_tmp(in_w >> 1), v_tmp(in_w >> 1);
    bool is_h_even = false;
    for (GLB_ size_t i = 0, index = 0; i < in_h; ++i, is_h_even = !is_h_even)
    {
        bool is_w_even = false;
        if (is_h_even) for (GLB_ size_t j = 0; j < in_w; ++j, ++index, is_w_even = !is_w_even)
        {
            R2Y_ rgb_t const & rgb = in_data[index];
            (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y;
            GLB_ size_t k = j >> 1;
            if (is_w_even)
            {
                R2Y_DETAIL_ set_and_next<Tar>(
                    u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb),
                    v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb), p_uv);
            }
            else
            {
                u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
            }
        }
        else for (GLB_ size_t j = 0; j < in_w; ++j, ++index, is_w_even = !is_w_even)
        {
            R2Y_ rgb_t const & rgb = in_data[index];
            (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y;
            GLB_ size_t k = j >> 1;
            if (is_w_even)
            {
                u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
            }
            else
            {
                u_tmp[k] = R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                v_tmp[k] = R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
            }
        }
    }
}

/* YUV 4:1:1 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_411P)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_ byte_t * p_y = R2Y_DETAIL_ split<Tar, R2Y_ plane_Y>(ot_data.data(), in_w, in_h);
    R2Y_DETAIL_ uv_t<Tar> p_uv = R2Y_DETAIL_ fill<Tar>(ot_data.data(), in_w, in_h);
    GLB_ size_t s = in_w * in_h;
    GLB_ uint16_t u_k, v_k;
    for (GLB_ size_t i = 0; i < s;)
    {
#   pragma push_macro("R2Y_TRANSFORM_TO_")
#   undef  R2Y_TRANSFORM_TO_
#   define R2Y_TRANSFORM_TO_(OP, ...) do                           \
        {                                                          \
            R2Y_ rgb_t const & rgb = in_data[i]; ++i;              \
            (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y; \
            u_k   OP R2Y_ pixel_convert<R2Y_ plane_U>(rgb);        \
            v_k   OP R2Y_ pixel_convert<R2Y_ plane_V>(rgb);        \
            __VA_ARGS__                                            \
        } while(0)

        R2Y_TRANSFORM_TO_( =);
        R2Y_TRANSFORM_TO_(+=);
        R2Y_TRANSFORM_TO_(+=);
        R2Y_TRANSFORM_TO_(+=, R2Y_DETAIL_ set_and_next<Tar>(u_k, v_k, p_uv););

#   pragma pop_macro("R2Y_TRANSFORM_TO_")
    }
}

/* YUV 4:1:0 */

template <R2Y_ supported Tar>
typename R2Y_ enable_if<(Tar == R2Y_ yuv_YUV9 || Tar == R2Y_ yuv_YVU9)
>::type transform_to(R2Y_ scope_block<R2Y_ rgb_t> const & in_data, GLB_ size_t in_w, GLB_ size_t in_h,
                     R2Y_ scope_block<R2Y_ byte_t>      & ot_data)
{
    R2Y_ byte_t * p_y = R2Y_DETAIL_ split<Tar, R2Y_ plane_Y>(ot_data.data(), in_w, in_h);
    R2Y_DETAIL_ uv_t<Tar> p_uv = R2Y_DETAIL_ fill<Tar>(ot_data.data(), in_w, in_h);
    R2Y_ scope_block<GLB_ uint16_t> u_tmp(in_w >> 2), v_tmp(in_w >> 2);
    GLB_ uint8_t h_flag = 1;
    for (GLB_ size_t i = 0, index = 0; i < in_h; ++i, ++h_flag)
    {
        GLB_ uint8_t w_flag = 1;
        switch (h_flag)
        {
        case 1:
            for (GLB_ size_t j = 0; j < in_w; ++j, ++index, ++w_flag)
            {
                R2Y_ rgb_t const & rgb = in_data[index];
                (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y;
                GLB_ size_t k = j >> 2;
                switch (w_flag)
                {
                case 1:
                    u_tmp[k] = R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                    v_tmp[k] = R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
                    break;
                case 4:
                    w_flag = 0;
                default:
                    u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                    v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
                    break;
                }
            }
            break;
        case 2:
        case 3:
            for (GLB_ size_t j = 0; j < in_w; ++j, ++index)
            {
                R2Y_ rgb_t const & rgb = in_data[index];
                (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y;
                GLB_ size_t k = j >> 2;
                u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
            }
            break;
        case 4:
            for (GLB_ size_t j = 0; j < in_w; ++j, ++index, ++w_flag)
            {
                R2Y_ rgb_t const & rgb = in_data[index];
                (*p_y) = R2Y_ pixel_convert<R2Y_ plane_Y>(rgb); ++p_y;
                GLB_ size_t k = j >> 2;
                switch (w_flag)
                {
                case 4:
                    R2Y_DETAIL_ set_and_next<Tar>(
                        u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb),
                        v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb), p_uv);
                    w_flag = 0;
                    break;
                default:
                    u_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_U>(rgb);
                    v_tmp[k] += R2Y_ pixel_convert<R2Y_ plane_V>(rgb);
                    break;
                }
            }
            h_flag = 0;
            break;
        }
    }
}

/* Implementing */

enum
{
    rgb = 2, // 10
    yuv = 1  // 01
};

template <R2Y_ supported In, R2Y_ supported Ot, int = (R2Y_ is_rgb<In>::value << 1) | R2Y_ is_yuv<In>::value
                                              , int = (R2Y_ is_rgb<Ot>::value << 1) | R2Y_ is_yuv<Ot>::value>
struct impl_;

template <R2Y_ supported In, R2Y_ supported Ot>
struct impl_<In, Ot, R2Y_DETAIL_ rgb, R2Y_DETAIL_ yuv>
{
    static void transform(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h, 
                          R2Y_ scope_block<R2Y_ byte_t> & ot_data)
    {
        assert(ot_data.size() == R2Y_ calculate_size<Ot>(in_w, in_h));
        R2Y_ scope_block<R2Y_ rgb_t> rgb_data( R2Y_DETAIL_ rgb_formatter<In>::to_888(in_data, in_w, in_h) );
        R2Y_DETAIL_ transform_to<Ot>(rgb_data, in_w, in_h, ot_data);
    }
};

#pragma pop_macro("R2Y_DETAIL_")

} // namespace detail_transform_

template <R2Y_ supported In, R2Y_ supported Ot>
void transform(R2Y_ byte_t * in_data, GLB_ size_t in_w, GLB_ size_t in_h,
               R2Y_ scope_block<R2Y_ byte_t> * ot_data)
{
    assert(ot_data != NULL);
    R2Y_ detail_transform_::impl_<In, Ot>::transform(in_data, in_w, in_h, *ot_data);
}

} // namespace R2Y_NAMESPACE_

////////////////////////////////////////////////////////////////
/// Pop previous definition of used macros
////////////////////////////////////////////////////////////////

#pragma pop_macro("R2Y_UNUSED_")
#pragma pop_macro("R2Y_ALLOC_")
#pragma pop_macro("R2Y_")
#pragma pop_macro("R2Y_NAMESPACE_")
#pragma pop_macro("STD_")
#pragma pop_macro("GLB_")

#endif // RGB2YUV_HPP__