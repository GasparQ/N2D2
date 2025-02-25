/*
    (C) Copyright 2010 CEA LIST. All Rights Reserved.
    Contributor(s): Olivier BICHLER (olivier.bichler@cea.fr)
                    Damien QUERLIOZ (damien.querlioz@cea.fr)

    This software is governed by the CeCILL-C license under French law and
    abiding by the rules of distribution of free software.  You can  use,
    modify and/ or redistribute the software under the terms of the CeCILL-C
    license as circulated by CEA, CNRS and INRIA at the following URL
    "http://www.cecill.info".

    As a counterpart to the access to the source code and  rights to copy,
    modify and redistribute granted by the license, users are provided only
    with a limited warranty  and the software's author,  the holder of the
    economic rights,  and the successive licensors  have only  limited
    liability.

    The fact that you are presently reading this means that you have had
    knowledge of the CeCILL-C license and that you accept its terms.
*/

#ifndef N2D2_UTILS_H
#define N2D2_UTILS_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <sys/stat.h>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>
#include <memory>
#include <array>

#ifdef WIN32
#include <direct.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

template <class T> double asinh(T x)
{
    return std::log(x + std::sqrt(x * x + 1.0));
}

template <class T> double log2(T x)
{
    return std::log(x) / std::log(2.0);
}
#endif

#ifdef OPENCV_USE_OLD_HEADERS       //  before OpenCV 2.2.0
    #include "cv.h"
    #include "highgui.h"
#else
    #include "opencv2/core/version.hpp"
    #if CV_MAJOR_VERSION == 2
        #include "opencv2/core/core.hpp"
        #include "opencv2/imgproc/imgproc.hpp"
        #include "opencv2/highgui/highgui.hpp"
    #elif CV_MAJOR_VERSION >= 3
        #include "opencv2/core.hpp"
        #include "opencv2/imgproc.hpp"
        #include "opencv2/highgui.hpp"
    #endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace {
// This is the type that will hold all the strings. Each enumerate type will
// declare its own specialization.
template <typename T> struct EnumStrings {
    static const char* const data[];
};
}

namespace N2D2 {
/**
 * This is not a class, just a namespace containing common and useful functions.
*/
namespace Utils {
    enum Endpoints {
        ClosedInterval,
        LeftHalfOpenInterval,
        RightHalfOpenInterval,
        OpenInterval
    };
    enum AngularRange {
        MinusPiToPi,
        ZeroToTwoPi
    };
    enum Rounding {
        HalfUp,
        HalfDown,
        HalfAwayFromZero,
        HalfTowardsZero
    };

    /// Deleter function object to delete the objects, which pointers are stored
    /// in a STL container.
    struct Delete {
        template <class T> void operator()(T*& p) const
        {
            delete p;
            p = NULL;
        }

        template <class T1, class T2>
        void operator()(std::pair<T1, T2*>& p) const
        {
            delete p.second;
            p.second = NULL;
        }
    };

    template <class T>
    struct PtrLess : public std::binary_function<T, T, bool> {
        inline bool operator()(const T& left, const T& right) const
        {
            return ((*left) < (*right));
        }
    };

    template <class T>
    struct SizeCompare : public std::binary_function<T, T, bool> {
        inline bool operator()(const T& left, const T& right) const
        {
            return (left.size() < right.size());
        }
    };

    template <class T>
    struct IsZero : public std::unary_function<T, bool> {
        inline bool operator()(const T& value) const
        {
            return (value == 0);
        }
    };

    template <class T1, class T2, class Pred = std::less<T1> >
    struct PairFirstPred : public std::binary_function
                           <std::pair<T1, T2>, std::pair<T1, T2>, bool> {
        bool operator()(const std::pair<T1, T2>& left,
                        const std::pair<T1, T2>& right) const
        {
            Pred p;
            return p(left.first, right.first);
        }
    };

    template <class T1, class T2, class Pred = std::less<T2> >
    struct PairSecondPred : public std::binary_function
                            <std::pair<T1, T2>, std::pair<T1, T2>, bool> {
        bool operator()(const std::pair<T1, T2>& left,
                        const std::pair<T1, T2>& right) const
        {
            Pred p;
            return p(left.second, right.second);
        }
    };

    template <class T> struct Left : public std::binary_function<T, T, T> {
        T operator()(const T& left, const T& /*right*/) const
        {
            return left;
        }
    };

    template <class T> struct Right : public std::binary_function<T, T, T> {
        T operator()(const T& /*left*/, const T& right) const
        {
            return right;
        }
    };

    template
        <class T1, class T2, class Op1 = Left<T1>, class Op2 = std::plus<T2> >
    struct PairOp : public std::binary_function
                    <std::pair<T1, T2>, std::pair<T1, T2>, std::pair<T1, T2> > {
        std::pair<T1, T2> operator()(const std::pair<T1, T2>& left,
                                     const std::pair<T1, T2>& right) const
        {
            Op1 p1;
            Op2 p2;
            return std::make_pair<T1, T2>(p1(left.first, right.first),
                                          p2(left.second, right.second));
        }
    };

    template <class T> struct max : public std::binary_function<T, T, T> {
        T operator()(const T& x, const T& y) const
        {
            return std::max<T>(x, y);
        }
    };

    template <class T> struct min : public std::binary_function<T, T, T> {
        T operator()(const T& x, const T& y) const
        {
            return std::min<T>(x, y);
        }
    };

    template <class T1, class T2> T1 pairFirst(const std::pair<T1, T2>& p)
    {
        return p.first;
    }
    template <class T1, class T2> T2 pairSecond(const std::pair<T1, T2>& p)
    {
        return p.second;
    }
    template <class T> void swapEndian(T& obj);
    template <class T1, class T2>
    typename std::enable_if<std::is_unsigned<T1>::value, T2&>::type
    signChecked(T2& stream);
    template <class T1, class T2>
    typename std::enable_if<!std::is_unsigned<T1>::value, T2&>::type
    signChecked(T2& stream)
    {
        return stream;
    }

    template <typename T, size_t N> T* begin(T (&a)[N])
    {
        return a;
    }
    template <typename T, size_t N> T* end(T (&a)[N])
    {
        return a + N;
    }

    template <typename T>
    typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type operator<<(
        std::ostream& os, const T& data);

    template <typename T>
    typename std::enable_if<std::is_enum<T>::value, std::istream&>::type
    operator>>(std::istream& is, T& data);

    template <typename T>
    typename std::enable_if<std::is_enum<T>::value, std::string>::type
    toString(const T& data);

    inline bool isBigEndian();

    std::tuple<double, double, double>
    hsvToHsl(double hsvH, double hsvS, double hsvV);

    /**
     * Convert HSV color to RGB color.
     *
     * @param hsvH          Hue, in degrees (>= 0.0 and < 360.0)
     * @param hsvS          Saturation (>= 0.0 and <= 1.0)
     * @param hsvV          Value (>= 0.0 and <= 1.0)
     * @return (R, G, B) tuple
     *
     * @exception std::domain_error One of the input argument is not within
     *allowed range
    */
    std::tuple<double, double, double>
    hsvToRgb(double hsvH, double hsvS, double hsvV);

    /**
     * Convert RGB color to HSV color.
     *
     * @param rgbR          Red (>= 0.0 and <= 1.0)
     * @param rgbG          Green (>= 0.0 and <= 1.0)
     * @param rgbB          Blue (>= 0.0 and <= 1.0)
     * @return (H, S, V) tuple
     *
     * @exception std::domain_error One of the input argument is not within
     *allowed range
    */
    std::tuple<double, double, double>
    rgbToHsv(double rgbR, double rgbG, double rgbB);

    /**
     * Convert RGB color to YUV color.
     *
     * @param rgbR          Red (>= 0.0 and <= 1.0)
     * @param rgbG          Green (>= 0.0 and <= 1.0)
     * @param rgbB          Blue (>= 0.0 and <= 1.0)
     * @param normalize     Normalize the components between 0 and 1
     * @return (Y, U, V) tuple
     *
     * @exception std::domain_error One of the input argument is not within
     *allowed range
    */
    std::tuple<double, double, double>
    rgbToYuv(double rgbR, double rgbG, double rgbB, bool normalize = false);

    void colorReduce(cv::Mat& img, unsigned int nbColors);
    void colorDiscretize(cv::Mat& img, unsigned int nbLevels);
    std::string cvMatDepthToString(int depth);
    double cvMatDepthUnityValue(int depth);

    std::string searchAndReplace(const std::string& value,
                                 const std::string& search,
                                 const std::string& replace);
    std::string escapeBinary(const std::string& value);
    std::vector<std::string> split(const std::string& value,
                                   const std::string& delimiters,
                                   bool trimEmpty = false);
    template<typename ForwardIt>
    std::string join(ForwardIt begin, ForwardIt end, char separator = ',');
    std::string upperCase(const std::string& str);
    std::string lowerCase(const std::string& str);
    unsigned int countSubstring(const std::string& str, const std::string& sub);
    std::string ltrim(std::string s);

    bool match(const std::string& first, const std::string& second);
    std::string expandEnvVars(std::string str);
    bool createDirectories(const std::string& dirName);
    std::string exec(const std::string& cmd);
    bool isAbsolutePath(const std::string& path);
    std::string dirName(const std::string& filePath, bool stripSlash = false);
    std::string baseName(const std::string& filePath);
    std::string fileBaseName(const std::string& filePath,
                             const std::string& sep = ".");
    std::string fileExtension(const std::string& filePath,
                              const std::string& sep = ".");
    bool isNotValidIdentifier(int c);
    std::string CIdentifier(const std::string& str);

    template <typename T> std::string TtoString(const T& data);

    /**
     * Symmetrical round (as the round function in C99).
     *
     * @param x             Value to round
     * @return Nearest integer (stored however in the same type as the input)
    */
    template <class T> T round(T x, Rounding rule = HalfAwayFromZero);

    /**
     * GCD (Greatest Common Divisor) that can deal with non-integers.
     *
     * @param x             First real number
     * @param y             Second real number
     * @param precision     Desired precision
     * @return Real r such as x = N*r and y = M*r, with N and M two integers
    */
    template <class T> T gcd(T x, T y, T precision = 1.0e-6);

    /**
     * Make sure 0.0 is exactly represented in the quantized range.
     * roundingRule behavior:
     * - HalfAwayFromZero: symmetrical range [-a a] with even levels
     *                     becomes [-a-delta a] (default, will match signed INT
     *                                           representation)
     * - HalfTowardsZero: symmetrical range [-a a] with even levels
     *                    becomes [-a a+delta]
    */
    template <class T>
    std::pair<T, T> zeroAlignedQuantizedRange(T minVal,
                                              T maxVal,
                                              unsigned int levels,
                                    Rounding roundingRule = HalfAwayFromZero);

    template <class T> T quantize(double x, T vmin, T vmax);

    /**
     * Numerically stable computation of the arithmetic mean of a vector.
     *
     * @param x             Input vector
     * @return Mean value of the vector (~ sum(x[i])/size(x))
    */
    template <class T> double mean(const std::vector<T>& x);
    template <class InputIt> double mean(InputIt first, InputIt last);

    /**
     * Numerically stable computation of the arithmetic mean and standard
     * deviation of a vector.
     *
     * @param x             Input vector
     * @param unbiased      If true, normalizes the result by N-1, where N is
     *the vector size. Else, normalizes the result by N.
     * @return std::pair with mean and standard deviation of the vector
    */
    template <class T>
    std::pair<double, double> meanStdDev(const std::vector<T>& x,
                                         bool unbiased = true);
    template <class InputIt>
    std::pair<double, double>
    meanStdDev(InputIt first, InputIt last, bool unbiased = true);

    /**
     * Standard deviation of a vector.
     *
     * @param x             Input vector
     * @param unbiased      If true, normalizes the result by N-1, where N is
     *the vector size. Else, normalizes the result by N.
     * @return Standard deviation of the vector
    */
    template <class T>
    double stdDev(const std::vector<T>& x, bool unbiased = true);

    /**
     * Median value of a vector.
     *
     * @param x             Input vector
     * @return Median value of the vector
    */
    template <class T> double median(const std::vector<T>& x);

    /**
     * Root mean square (RMS) of a vector.
     *
     * @param x             Input vector
     * @return RMS of the vector
    */
    template <class T> double rms(const std::vector<T>& x);

    /**
     * Lower tail quantile for standard normal distribution function.
     * This function returns an approximation of the inverse cumulative standard
     *normal distribution function.  I.e., given p,
     * it returns an approximation to the x satisfying p = Pr{z <= x} where z is
     *a random variable from the standard normal
     * distribution.
     * The algorithm uses a minimax approximation by rational functions and the
     *result has a relative error whose absolute value
     * is less than 1.15e-9.
     *
     * Author:  Peter John Acklam <jacklam@math.uio.no>
     *          http://www.math.uio.no/~jacklam
     *
     * @param p             Probability
     * @return Inverse of the normal cumulative distribution function at the
     *corresponding probabilities in p
    */
    double normalInverse(double p);

    /**
     * Compute d' ("dee-prime") = z(H) - z(F)
     * with H the hit rate, F the false-alarm rate and z() the inverse of the
     *normal cumulative distribution function.
     * To avoid infinite values, the following adjustment is made: the
     *proportions 0 and 1 are converted to 1/(2N) and
     * 1 - 1/(2N), respectively, where N is the number of trials on which the
     *proportion is based.
     * (see Neil A Macmillan and C. Douglas Creelman, "Detection Theory: A
     *User's Guide")
     *
     * @param hits          Number of hits (among the @p yesTrials)
     * @param yesTrials     Number of "yes" trials, H = @p hits / @p yesTrials
     * @param falseAlarms   Number of false-alarms (among the @p noTrials)
     * @param noTrials      Number of "no" trials, F = @p falseAlarms / @p
     *noTrials
     * @return d' sensitivity
    */
    double dPrime(unsigned int hits,
                  unsigned int yesTrials,
                  unsigned int falseAlarms,
                  unsigned int noTrials);

    /**
     * Return a normalized angular value, for an angle in radians.
     *
     * @param angle         Angle to normalize (in rad)
     * @param range         Range of the normalized angle, can be either
     *[-pi,pi[ = [-pi,pi) (default) or [0,2*pi[ = [0,2*pi)
     * @return Normalized angle
    */
    double normalizedAngle(double angle, AngularRange range = MinusPiToPi);

    /**
     * Convert an angle in degrees to an angle in radians.
     *
     * @param angle         Angle in degrees
     * @return Angle in radians
    */
    inline double degToRad(double angle);

    /**
     * Convert an angle in radians to an angle in degrees.
     *
     * @param angle         Angle in radians
     * @return Angle in degrees
    */
    inline double radToDeg(double angle);

    template <class T>
    inline const T& clamp(const T& x, const T& min, const T& max);

    bool isIntegral(double value);
    
    /**
     * Return an iterator to the largest absolute value in the range [first, last). 
     * Returns 'last' if the range is empty.
     */
    template<class ForwardIt>
    ForwardIt max_abs_element(ForwardIt first, ForwardIt last);

    template<class T>
    T max_abs(const T& a, const T& b);

    template<class RandomAccessIt>
    bool all_same(RandomAccessIt first, RandomAccessIt last);

    template <class charT, class traits>
    std::basic_ostream<charT, traits>& cwarning(std::basic_ostream
                                                <charT, traits>& stream);
    template <class charT, class traits>
    std::basic_ostream<charT, traits>& cnotice(std::basic_ostream
                                               <charT, traits>& stream);
    template <class charT, class traits>
    std::basic_ostream<charT, traits>& cdef(std::basic_ostream
                                            <charT, traits>& stream);

    class commaNumpunct : public std::numpunct<char> {
    public:
        commaNumpunct(size_t refs = 0): std::numpunct<char>(refs) {}

    protected:
        virtual char do_thousands_sep() const
        {
            return ',';
        }
        virtual std::string do_grouping() const
        {
            return "\3";
        }
    };

    const commaNumpunct commaNumpunctInst(1);
    // For an unknown reason, a segfault can occur after an exception is thrown.
    // Relevant stack trace:
    // #3  0x00007fffe131437a in malloc_printerr (ar_ptr=<optimized out>,
    //     ptr=<optimized out>,
    //     str=0x7fffe1424f78 "free(): corrupted unsorted chunks", action=3)
    //     at malloc.c:5006
    // #4  _int_free (av=<optimized out>, p=<optimized out>, have_lock=0)
    //     at malloc.c:3867
    // #5  0x00007fffe131853c in __GI___libc_free (mem=<optimized out>)
    //     at malloc.c:2968
    // #6  0x00007fffe206a8f2 in std::__cxx11::numpunct<char>::~numpunct() ()
    //    from /usr/lib/x86_64-linux-gnu/libstdc++.so.6
    // #7  0x000000000044b997 in N2D2::Utils::numpunct::~numpunct (this=0x4cc49f0,
    //     __in_chrg=<optimized out>) at N2D2/include/utils/Utils.hpp:500
    //
    // Tentative solution:
    // Pass the numpunct by reference, with refs = 1 so that delete is not
    // called by the implementation
    // See https://stackoverflow.com/questions/22647584/segfault-when-imbueing-stringstream-with-custom-locale
    const std::locale locale(std::locale(), &commaNumpunctInst);

    // streamIgnoreBase is necessary to ensure that "rc" get initialized before
    // the call to get_table() in the derived class
    // See the Base-from-Member C++ idiom
    struct streamIgnoreBase {
        streamIgnoreBase()
            : rc(std::ctype<char>::table_size, std::ctype<char>::mask())
        {
        }

        std::ctype<char>::mask const* get_table(const std::string& ignore)
        {
            for (std::string::const_iterator it = ignore.begin(),
                                             itEnd = ignore.end();
                 it != itEnd;
                 ++it)
                rc[(*it)] = std::ctype<char>::space;

            return &rc[0];
        }

        std::vector<std::ctype<char>::mask> rc;
        virtual ~streamIgnoreBase() {}
    };

    struct streamIgnore : streamIgnoreBase, std::ctype<char> {
        streamIgnore(const std::string& ignore)
            : streamIgnoreBase(), std::ctype<char>(get_table(ignore))
        {
        }
    };

    // The following is a partial implementation of C++14 std::quoted()
    template <class Char, class Traits, class Alloc>
    struct quotedProxyType {
        std::basic_string<Char, Traits, Alloc>& str;
        Char delim;
        Char escape;

        quotedProxyType(std::basic_string<Char, Traits, Alloc>& str_,
                        Char delim_,
                        Char escape_):
                            str(str_), delim(delim_), escape(escape_) {};
    };

    template <class Char, class Traits, class Alloc>
    struct quotedProxyTypeConst {
        const std::basic_string<Char, Traits, Alloc>& str;
        Char delim;
        Char escape;

        quotedProxyTypeConst(const std::basic_string<Char, Traits, Alloc>& str_,
                        Char delim_,
                        Char escape_):
                            str(str_), delim(delim_), escape(escape_) {};
    };

    template <class Char, class Traits, class Alloc>
    N2D2::Utils::quotedProxyType<Char, Traits, Alloc>
    quoted(std::basic_string<Char, Traits, Alloc>& str,
           Char delim = '\"',
           Char escape = '\\');

    template <class Char, class Traits, class Alloc>
    N2D2::Utils::quotedProxyTypeConst<Char, Traits, Alloc>
    quoted(const std::basic_string<Char, Traits, Alloc>& str,
           Char delim = '\"',
           Char escape = '\\');

    template <class Char, class Traits, class Alloc>
    std::basic_ostream<Char, Traits>&
    operator<<(std::basic_ostream<Char, Traits>& os,
                            const quotedProxyType<Char, Traits, Alloc>& proxy);

    template <class Char, class Traits, class Alloc>
    std::basic_ostream<Char, Traits>&
    operator<<(std::basic_ostream<Char, Traits>& os,
        const quotedProxyTypeConst<Char, Traits, Alloc>& proxy);

    template <class Char, class Traits, class Alloc>
    std::basic_istream<Char, Traits>&
    operator>>(std::basic_istream<Char, Traits>& is,
                            const quotedProxyType<Char, Traits, Alloc>& proxy);
    // Scaling parameters are typically "alpha" and "beta".
    // Their type must be "float" for HALF and FLOAT (default template)
    // and "double" for DOUBLE (specialized template)
    template <class T>
    struct scaling_type {
        typedef float type;
    };

    template <>
    struct scaling_type<double> {
        typedef double type;
    };

    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}

template <class T>
std::vector<T>& operator<<(std::vector<T>& vec, const std::string& data);
template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec);
template <class T>
std::istream& operator>>(std::istream& is, std::vector<T>& vec);

// I get an undefined reference error on GCC 4.8.4 if I put the definition in
// the .cpp, but it works on GCC 4.4.7!
inline std::ostream& operator<<(std::ostream& os,
                                const std::vector<std::string>& vec);
inline std::istream& operator>>(std::istream& is,
                                std::vector<std::string>& vec);
}

#if CV_MAJOR_VERSION < 2 || (CV_MAJOR_VERSION == 2 && CV_MINOR_VERSION < 2)
namespace cv {
void vconcat(const std::vector<cv::Mat>& src, cv::Mat& dst);
}
#endif

template <class T> void N2D2::Utils::swapEndian(T& obj)
{
    unsigned char* memp = reinterpret_cast<unsigned char*>(&obj);
    std::reverse(memp, memp + sizeof(T));
}

template <class T1, class T2>
typename std::enable_if<std::is_unsigned<T1>::value, T2&>::type
N2D2::Utils::signChecked(T2& stream)
{
    int ch;
    while (std::isspace(ch = stream.get()))
        ; // skip white spaces

    if (ch == '-')
        throw std::runtime_error(
            "Trying to read a negative number into an unsigned variable");
    else
        stream.unget();

    return stream;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type operator<<(
    std::ostream& os, const T& data)
{
    return (os << EnumStrings<T>::data[data]);
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::istream&>::type
operator>>(std::istream& is, T& data)
{
    std::string value;
    std::istream& r = is >> value;

    static const char* const* begin = N2D2::Utils::begin(EnumStrings<T>::data);
    static const char* const* end = N2D2::Utils::end(EnumStrings<T>::data);
    const char* const* find = std::find(begin, end, value);

    if (find != end)
        data = T(static_cast<T>(std::distance(begin, find)));
    else
        throw std::runtime_error("Value \"" + value + "\" is not part of enum "
                                 + typeid(T).name());

    return r;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::string>::type
N2D2::Utils::toString(const T& data)
{
    return EnumStrings<T>::data[data];
}

template <typename T> std::string N2D2::Utils::TtoString(const T& data)
{
    std::stringstream ss;
    ss << data;
    return ss.str();
}

bool N2D2::Utils::isBigEndian()
{
    const union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1;
}

template <class T> T N2D2::Utils::round(T x, Rounding rule)
{
    using namespace std;    // allow using half_float too

    switch (rule) {
    case HalfUp:
        return floor(x + T(0.5f));
    case HalfDown:
        return ceil(x - T(0.5f));
    case HalfTowardsZero:
        return (x < T(0.0f)) ? floor(x + T(0.5f)) : ceil(x - T(0.5f));
    case HalfAwayFromZero:
    default:
        return (x < T(0.0f)) ? ceil(x - T(0.5f)) : floor(x + T(0.5f));
    }
}

template <class T> T N2D2::Utils::gcd(T x, T y, T precision)
{
    T a = std::min(x, y);
    T b = std::max(x, y);

    do
        std::tie(a, b) = std::make_pair(std::fmod(b, a), a);
    while (std::fabs(a) > precision);

    return b;
}

template <class T> std::pair<T, T>
N2D2::Utils::zeroAlignedQuantizedRange(T minVal,
                                       T maxVal,
                                       unsigned int levels,
                                       Rounding roundingRule)
{
    if (maxVal <= minVal) {
        throw std::domain_error("Utils::zeroAlignedQuantizedRange(): "
                                "maxVal must be > minVal.");
    }

    if (levels < 2) {
        throw std::domain_error("Utils::zeroAlignedQuantizedRange(): "
                                "levels must be > 1.");
    }

    // Scaling with base range
    double scaling = (maxVal - minVal) / (double)(levels - 1);
    // zero: position of 0.0 in the quantized scale. If zero is not an INT,
    // zero alignment is necessary.
    const double zero = (0.0 - minVal) / scaling;
    // quantizedZero: target position of 0.0 in the quantized scale.
    const int quantizedZero = (int)round(zero, roundingRule);

    // New range so that 0.0 is exactly representable as an INT
    // Meaning zero == quantizedZero
    const int zeroLevel = quantizedZero - (levels - 1);

    // When 0.0 is outside the initial range, the current behavior is to extend
    // the range to include 0.0.
    if (quantizedZero < 0) {
        // 0.0 is below range => minVal is decreased to 0.0
        minVal = 0.0;
    }
    else if (zeroLevel > 0) {
        // 0.0 is above range => maxVal is increased to 0.0
        maxVal = 0.0;
    }
    else {
        // else: this should be the normal condition when calling this function
        if (quantizedZero <= zero) {
            // Increase maxVal
            if (quantizedZero > 0)
                maxVal = zeroLevel * minVal / (double)quantizedZero;
            else
                minVal = 0.0;
        }
        else {
            // Decrease minVal
            if (zeroLevel < 0)
                minVal = quantizedZero * maxVal / (double)zeroLevel;
            else
                maxVal = 0.0;
        }
    }

    return std::make_pair(minVal, maxVal);
}

template <class T> T N2D2::Utils::quantize(double x, T vmin, T vmax)
{
    if (x < 0.0 || x > 1.0)
        throw std::domain_error(
            "Utils::quantize(): x is out of range (must be >= 0.0 and <= 1.0)");

    return std::min(vmax, (T)std::floor(vmin + x * (vmax - vmin + 1)));
}

template <class T> double N2D2::Utils::mean(const std::vector<T>& x)
{
    return mean(x.begin(), x.end());
}

template <class InputIt> double N2D2::Utils::mean(InputIt first, InputIt last)
{
    if (last == first)
        throw std::runtime_error(
            "Utils::mean(): number of elements must be > 0.");

    double mean = 0.0;

    for (size_t k = 1; first != last; ++first, ++k) {
        const double delta = ((*first) - mean);
        mean += delta / k;
    }

    return mean;
}

template <class T>
std::pair<double, double> N2D2::Utils::meanStdDev(const std::vector<T>& x,
                                                  bool unbiased)
{
    return meanStdDev(x.begin(), x.end(), unbiased);
}

template <class InputIt>
std::pair<double, double>
N2D2::Utils::meanStdDev(InputIt first, InputIt last, bool unbiased)
{
    if (last == first)
        throw std::runtime_error(
            "Utils::meanStdDev(): number of elements must be > 0.");

    const size_t size = std::distance(first, last);

    if (size == 1 && unbiased)
        throw std::runtime_error(
            "Utils::meanStdDev(): number of elements must be > 1 for unbiased.");

    // Use Welford's method to compute std. dev. in one pass
    double mean = 0.0;
    double M2 = 0.0;

    for (size_t k = 1; first != last; ++first, ++k) {
        const double delta = ((*first) - mean);
        mean += delta / k;
        const double delta2 = ((*first) - mean);
        M2 += delta * delta2;
    }

    const double stdDev = (unbiased) ? std::sqrt(M2 / (size - 1))
                                     : std::sqrt(M2 / size);
    return std::make_pair(mean, stdDev);
}

template <class T>
double N2D2::Utils::stdDev(const std::vector<T>& x, bool unbiased)
{
    return meanStdDev(x, unbiased).second;
}

template <class T> double N2D2::Utils::median(const std::vector<T>& x)
{
    std::vector<T> mx(x);
    const size_t n = mx.size() / 2;

    if (mx.size() % 2 == 1) {
        std::nth_element(mx.begin(), mx.begin() + n, mx.end());
        return mx[n];
    } else {
        std::nth_element(mx.begin(), mx.begin() + n, mx.end());
        std::partial_sort(
            mx.begin(), mx.begin() + 2, mx.begin() + n + 1, std::greater<T>());
        return (mx[0] + mx[1]) / 2.0;
    }
}

template <class T> double N2D2::Utils::rms(const std::vector<T>& x)
{
    if (x.size() > 0)
        return std::sqrt(std::inner_product(x.begin(), x.end(), x.begin(), 0.0)
                         / x.size());
    else
        throw std::runtime_error("Utils::rms(): vector size must be > 0.");
}

double N2D2::Utils::degToRad(double angle)
{
    return angle / 180.0 * M_PI;
}

double N2D2::Utils::radToDeg(double angle)
{
    return angle / M_PI * 180.0;
}

template <class T>
const T& N2D2::Utils::clamp(const T& x, const T& min, const T& max)
{
    return (x < min) ? min : (x > max) ? max : x;
}

template<class ForwardIt>
inline ForwardIt N2D2::Utils::max_abs_element(ForwardIt first, ForwardIt last) {
    return std::max_element(first, last, 
                            [](const typename ForwardIt::value_type& lhs, 
                               const typename ForwardIt::value_type& rhs) 
                            {
                                return std::abs(lhs) < std::abs(rhs);
                            });
}

template<class T>
inline T N2D2::Utils::max_abs(const T& a, const T& b) {
    return std::max(std::abs(a), std::abs(b));
}

template<class RandomAccessIt>
inline bool N2D2::Utils::all_same(RandomAccessIt first, RandomAccessIt last) {
    if(first == last) {
        return true;
    }

    return std::equal(first + 1, last, first);
}

template <class charT, class traits>
std::basic_ostream<charT, traits>&
N2D2::Utils::cwarning(std::basic_ostream<charT, traits>& stream)
{
#ifndef WIN32
    stream << "\033[31m";
#else
    stream << "*** ";
#endif
    return stream;
}

template <class charT, class traits>
std::basic_ostream<charT, traits>& N2D2::Utils::cnotice(std::basic_ostream
                                                        <charT, traits>& stream)
{
#ifndef WIN32
    stream << "\033[34m";
#endif
    return stream;
}

template <class charT, class traits>
std::basic_ostream<charT, traits>& N2D2::Utils::cdef(std::basic_ostream
                                                     <charT, traits>& stream)
{
#ifndef WIN32
    stream << "\033[39m";
#endif
    return stream;
}

template <class Char, class Traits, class Alloc>
N2D2::Utils::quotedProxyType<Char, Traits, Alloc>
N2D2::Utils::quoted(std::basic_string<Char, Traits, Alloc>& str,
                    Char delim,
                    Char escape)
{
    return quotedProxyType<Char, Traits, Alloc>(str, delim, escape);
}

template <class Char, class Traits, class Alloc>
N2D2::Utils::quotedProxyTypeConst<Char, Traits, Alloc>
N2D2::Utils::quoted(const std::basic_string<Char, Traits, Alloc>& str,
                    Char delim,
                    Char escape)
{
    return quotedProxyTypeConst<Char, Traits, Alloc>(str, delim, escape);
}

template <class Char, class Traits, class Alloc>
std::basic_ostream<Char, Traits>&
N2D2::Utils::operator<<(std::basic_ostream<Char, Traits>& os,
                        const quotedProxyType<Char, Traits, Alloc>& proxy)
{
    os << "\"";

    for (typename std::basic_string<Char, Traits, Alloc>::const_iterator it
         = proxy.str.begin(), itEnd = proxy.str.end(); it != itEnd; ++it)
    {
        if ((*it) == proxy.delim || (*it) == proxy.escape)
            os << "\\";

        os << (*it);
    }

    os << "\"";
    return os;
}

template <class Char, class Traits, class Alloc>
std::basic_ostream<Char, Traits>&
N2D2::Utils::operator<<(std::basic_ostream<Char, Traits>& os,
                        const quotedProxyTypeConst<Char, Traits, Alloc>& proxy)
{
    os << "\"";

    for (typename std::basic_string<Char, Traits, Alloc>::const_iterator it
         = proxy.str.begin(), itEnd = proxy.str.end(); it != itEnd; ++it)
    {
        if ((*it) == proxy.delim || (*it) == proxy.escape)
            os << "\\";

        os << (*it);
    }

    os << "\"";
    return os;
}

template <class Char, class Traits, class Alloc>
std::basic_istream<Char, Traits>&
N2D2::Utils::operator>>(std::basic_istream<Char, Traits>& is,
                        const quotedProxyType<Char, Traits, Alloc>& proxy)
{
    // Save flags
    const std::ios::fmtflags savedFlags = is.flags();

    if (savedFlags & is.skipws) {
        while (std::isspace(is.peek()))
            is.get(); // discard whitespace
    }

    if (is.peek() == proxy.delim) {
        is.get(); // discard delim
        // Turn off the skipws flag
        is.unsetf(std::ios_base::skipws);
        proxy.str.clear();

        int c = is.get();
        bool escaped = false;

        while (c != proxy.delim || escaped) {
            escaped = (!escaped && c == proxy.escape);

            if (!escaped)
                proxy.str.push_back(c);

            c = is.get();

            if (!is.good())
                throw std::runtime_error("Error reading quoted string");
        }

        // Restore the skipws flag to its original value
        is.flags(savedFlags);
    }
    else
        is >> proxy.str;

    return is;
}

namespace N2D2 {
template <class T>
std::vector<T>& operator<<(std::vector<T>& vec, const std::string& data)
{
    vec.clear();

    std::stringstream dataStr(data);
    dataStr >> vec;

#if defined(__GNUC__)                                                          \
    && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    // Bug in libstdc++: "complex type operator>> does not set eofbit for input
    // streams"
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59568
    // Replicated on GCC 4.6.3
    if (!std::is_same<T, std::complex<float> >::value
        && !std::is_same<T, std::complex<double> >::value
        && !std::is_same<T, std::complex<long double> >::value) {
        if (!dataStr.eof())
            throw std::runtime_error("Unreadable data before end of line: \""
                                     + data + "\"");
    }
#else
    if (!dataStr.eof())
        throw std::runtime_error("Unreadable data before end of line: \"" + data
                                 + "\"");
#endif

    return vec;
}

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec)
{
    std::copy(vec.begin(), vec.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

template <class T>
std::istream& operator>>(std::istream& is, std::vector<T>& vec)
{
    vec.clear();
    std::copy(std::istream_iterator<T>(is),
              std::istream_iterator<T>(),
              std::back_inserter(vec));

    // Because of the std::copy() behavior, the failbit is always set.
    // But if eof is also set, it means that the read was successful.
    // In this case, only set the eofbit and clear the failbit, so that this
    // operator can be used with higher-level generatic parameter read routines.
    if (is.eof())
        is.clear(is.eofbit);

    return is;
}

std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& vec)
{
    for (std::vector<std::string>::const_iterator it = vec.begin(),
         itEnd = vec.end();
         it != itEnd;
         ++it)
    {
        os << Utils::quoted(*it) << " ";
    }

    return os;
}

std::istream& operator>>(std::istream& is, std::vector<std::string>& vec)
{
    vec.clear();

    std::string word;

    while (is >> std::skipws >> Utils::quoted(word))
        vec.push_back(word);

    // The failbit is necessarily set when the while loop stops.
    // But if eof is also set, it means that the read was successful.
    // In this case, only set the eofbit and clear the failbit, so that this
    // operator can be used with higher-level generatic parameter read routines.
    if (is.eof())
        is.clear(is.eofbit);

    return is;
}
}


template<typename ForwardIt>
inline std::string N2D2::Utils::join(ForwardIt begin, ForwardIt end, char separator) {
    if(begin == end) {
        return "";
    }

    std::stringstream str;

    str << *begin;
    ++begin;

    for(; begin != end; ++begin) {
        str << separator << " " << *begin;
    }

    return str.str();
}

#endif // N2D2_UTILS_H
