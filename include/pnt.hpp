// Copyright (c) 2013, Philippe Daouadi <p.daouadi@free.fr>
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met: 
// 
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer. 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of the FreeBSD Project.

#ifndef PNT_HPP
#define PNT_HPP

#include <cassert>
#include <limits>
#include <stdexcept>
#include <iostream>

/*
FormatString:
    FormatStringItem*
FormatStringItem:
    '%%'
    '%' Position Flags Width Precision FormatChar
    '%(' FormatString '%)'
    OtherCharacterExceptPercent
Position:
    empty
    Integer '$'
Flags:
    empty
    '-' Flags
    '+' Flags
    '#' Flags
    '0' Flags
    ' ' Flags
Width:
    empty
    Integer
    '*'
Precision:
    empty
    '.'
    '.' Integer
    '.*'
Integer:
    Digit
    Digit Integer
Digit:
    '0'|'1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9'
FormatChar:
    's'|'c'|'b'|'d'|'o'|'x'|'X'|'p'|'e'|'E'|'f'|'F'|'g'|'G'|'a'|'A'
*/

#ifdef FORMATTER_THROW_ON_ERROR
#define FORMAT_ERROR(type) \
  throw FormatError(type)
#else
#define FORMAT_ERROR(type) \
  assert(!#type)
#endif

namespace pnt
{

class FormatError : public std::exception
{
  public:
    enum Type
    {
      InvalidFormatter,
      TooFewArguments,
      TooManyArguments,
      IncompatibleType,
      NotImplemented
    };

    FormatError(Type type);

    const char* what() const noexcept;

  private:
    Type m_type;
};

inline FormatError::FormatError(Type type) :
  m_type(type)
{
}

inline const char* FormatError::what() const noexcept
{
  switch (m_type)
  {
    case InvalidFormatter: return "Invalid formatter";
    case TooFewArguments: return "Too few arguments";
    case TooManyArguments: return "Too many arguments";
    case IncompatibleType: return "Incompatible type";
    case NotImplemented: return "Not implemented";
    default: return "Unknown error";
  }
}

namespace _Formatter
{
  template <typename T>
  struct isIntegral
  {
    static constexpr bool value =
      std::is_integral<T>::value &&
      !std::is_same<T, bool>::value;
  };

  class FormatterItem
  {
    public:
      static constexpr unsigned int POSITION_NONE = -1;

      static constexpr unsigned int FLAG_LEFT_JUSTIFY  =  0x1;
      static constexpr unsigned int FLAG_SHOW_SIGN     =  0x2;
      static constexpr unsigned int FLAG_EXPLICIT_BASE =  0x4;
      static constexpr unsigned int FLAG_FILL_ZERO     =  0x8;
      static constexpr unsigned int FLAG_ADD_SPACE     = 0x10;

      static constexpr unsigned int WIDTH_EMPTY = -1;
      static constexpr unsigned int WIDTH_ARG = -2;

      unsigned int position;
      unsigned char flags;
      unsigned int width;
      unsigned int precision;
      char formatChar;

    protected:
      void fixFlags();
  };

  inline void FormatterItem::fixFlags()
  {
    // no sign or space for non decimal and non binary and non %s
    if (formatChar != 'd' && formatChar != 'b' && formatChar != 's')
      flags &= ~(FLAG_SHOW_SIGN | FLAG_ADD_SPACE);
    // no explicit base for decimal or binary
    else
      flags &= ~FLAG_EXPLICIT_BASE;

    // no space if sign
    if (flags & FLAG_SHOW_SIGN)
      flags &= ~FLAG_ADD_SPACE;

    // fill only with space on left justify
    if (flags & FLAG_LEFT_JUSTIFY)
      flags &= ~FLAG_FILL_ZERO;
  }

  template <typename Iterator>
  class StringFormatterItem : public FormatterItem
  {
    public:
      void handleFormatter(Iterator& iter);

    private:
      Iterator findIntegerEnd(Iterator iter);
      unsigned int parseInt(Iterator iter);

      void handlePosition(Iterator& iter);
      void handleFlags(Iterator& iter);
      void handleWidth(Iterator& iter);
      void handlePrecision(Iterator& iter);
      void handleFormatChar(Iterator& iter);
  };

  template <typename Iterator>
  inline Iterator StringFormatterItem<Iterator>::findIntegerEnd(
      Iterator iter)
  {
    auto end = iter;
    while (*end >= '0' && *end <= '9')
      ++end;
    return end;
  }

  template <typename Iterator>
  inline unsigned int StringFormatterItem<Iterator>::parseInt(Iterator iter)
  {
    unsigned int out = 0;
    for (; *iter >= '0' && *iter <= '9'; ++iter)
      out = out * 10 + (*iter - '0');
    return out;
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handlePosition(Iterator& iter)
  {
    auto end = findIntegerEnd(iter);

    if (iter == end || *end != '$')
    {
      position = POSITION_NONE;
      return;
    }

    position = parseInt(iter);

    iter = end+1;
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handleFlags(Iterator& iter)
  {
    flags = 0;
    while (true)
    {
      switch (*iter)
      {
        case '-': flags |= FLAG_LEFT_JUSTIFY; break;
        case '+': flags |= FLAG_SHOW_SIGN; break;
        case '#': flags |= FLAG_EXPLICIT_BASE; break;
        case '0': flags |= FLAG_FILL_ZERO; break;
        case ' ': flags |= FLAG_ADD_SPACE; break;
        default:
          return;
      }

      ++iter;
    }
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handleWidth(Iterator& iter)
  {
    if (*iter == '*')
    {
      width = WIDTH_ARG;
      ++iter;
      return;
    }

    auto end = findIntegerEnd(iter);

    if (iter == end)
    {
      width = WIDTH_EMPTY;
      return;
    }

    width = parseInt(iter);

    iter = end;
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handlePrecision(Iterator& iter)
  {
    if (*iter != '.')
    {
      precision = WIDTH_EMPTY;
      return;
    }

    ++iter;

    if (*iter == '*')
    {
      precision = WIDTH_ARG;
      ++iter;
      return;
    }

    auto end = findIntegerEnd(iter);
    if (end == iter)
    {
      precision = 0;
      return;
    }

    precision = parseInt(iter);

    iter = end;
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handleFormatChar(Iterator& iter)
  {
    switch (*iter)
    {
      case 's':
      case 'c':
      case 'b':
      case 'd':
      case 'o':
      case 'x':
      case 'X':
      case 'p':
      case 'e':
      case 'E':
      case 'f':
      case 'F':
      case 'g':
      case 'G':
      case 'a':
      case 'A':
        formatChar = *iter;
        break;
      default:
        FORMAT_ERROR(FormatError::InvalidFormatter);
    }

    ++iter;
  }

  template <typename Iterator>
  inline void StringFormatterItem<Iterator>::handleFormatter(Iterator& iter)
  {
    handlePosition(iter);
    handleFlags(iter);
    handleWidth(iter);
    handlePrecision(iter);
    handleFormatChar(iter);

    fixFlags();
  }
}

template <typename Streambuf>
class Formatter
{
  public:
    typedef typename std::remove_reference<Streambuf>::type streambuf_type;
    typedef typename streambuf_type::char_type char_type;
    typedef typename streambuf_type::traits_type traits_type;

    Formatter(Streambuf& stream);

    template <typename... Args>
    void print(const char_type* format, Args... args);

  private:
    Streambuf& m_streambuf;

    template <typename... Args>
    void printArg(const _Formatter::FormatterItem& fmt, Args... args);
    template <typename Arg1, typename... Args>
    void printArg(unsigned int item, const _Formatter::FormatterItem& fmt,
        Arg1 arg1, Args... args);
    void printArg(unsigned int item, const _Formatter::FormatterItem& fmt);

    void printPreFill(
        const _Formatter::FormatterItem& fmt, unsigned int size);
    void printPostFill(
        const _Formatter::FormatterItem& fmt, unsigned int size);

    void printByType(const _Formatter::FormatterItem&, bool arg);
    void printByType(const _Formatter::FormatterItem&, char_type arg);
    void printByType(const _Formatter::FormatterItem&, const char_type* arg);
    template <typename T>
    typename std::enable_if<
        !std::is_integral<T>::value &&
        !std::is_floating_point<T>::value &&
        !std::is_convertible<T,
          std::basic_string<char_type, traits_type>>::value &&
        !std::is_pointer<T>::value
      >::type printByType(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value>::type
      printByType(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<std::is_pointer<T>::value>::type
      printByType(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value>::type
      printByType(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<
        !std::is_floating_point<T>::value &&
        !std::is_integral<T>::value &&
        std::is_convertible<T, std::basic_string<typename Formatter::char_type,
      typename Formatter::traits_type>>::value
      >::type printByType(const _Formatter::FormatterItem& fmt, T arg);

    template <typename T>
    typename std::enable_if<std::is_convertible<T, char_type>::value>::type
      printChar(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<!std::is_convertible<T, char_type>::value>::type
      printChar(const _Formatter::FormatterItem& fmt, T arg);

    template <typename T>
    typename std::enable_if<std::is_pointer<T>::value>::type
      printPointer(const _Formatter::FormatterItem& fmt, T arg);
    template <typename T>
    typename std::enable_if<!std::is_pointer<T>::value>::type
      printPointer(const _Formatter::FormatterItem& fmt, T arg);

    template <unsigned int base, typename T>
    typename std::enable_if<
        _Formatter::isIntegral<T>::value
      >::type printIntegral(const _Formatter::FormatterItem& fmt, T value);
    template <unsigned int Tbase, typename T>
    std::size_t printIntegral(char_type* str,
        const _Formatter::FormatterItem& fmt, T value);
    template <unsigned int base, typename T>
    typename std::enable_if<
        !_Formatter::isIntegral<T>::value
      >::type printIntegral(const _Formatter::FormatterItem& fmt, T value);

    template <unsigned int base, typename T>
    typename std::enable_if<
        _Formatter::isIntegral<T>::value
      >::type printUnsigned(const _Formatter::FormatterItem& fmt, T value);
    template <unsigned int base, typename T>
    typename std::enable_if<
        !_Formatter::isIntegral<T>::value
      >::type printUnsigned(const _Formatter::FormatterItem&, T);
};

template <typename Streambuf>
inline Formatter<Streambuf>::Formatter(Streambuf& stream) :
  m_streambuf(stream)
{
}

template <typename Streambuf>
template <typename... Args>
void Formatter<Streambuf>::print(const char_type* format, Args... args)
{
  bool positional = false;
  unsigned int position = 0;
  const char_type* last = format;
  auto iter = format;
  while (true)
  {
    switch (*iter)
    {
      case '\0':
        m_streambuf.sputn(last, iter-last);
        last = iter;
        return;
      case '%':
        m_streambuf.sputn(last, iter-last);
        ++iter;
        switch (*iter)
        {
          case '%':
            ++iter;
            m_streambuf.sputc('%');
            break;
          case '(':
            FORMAT_ERROR(FormatError::NotImplemented);
            break;
          default:
            {
              _Formatter::StringFormatterItem<decltype(iter)> fmt;
              fmt.handleFormatter(iter);
              if (fmt.position == _Formatter::FormatterItem::POSITION_NONE)
                fmt.position = position;
              else
              {
                positional = true;
                position = fmt.position;
              }

              if (!positional)
                ++position;

              printArg(fmt, args...);
            }
            break;
        }
        last = iter;
        break;
      default:
        ++iter;
    }
  }
}

template <typename Streambuf>
template <typename... Args>
inline
void Formatter<Streambuf>::printArg(const _Formatter::FormatterItem& fmt,
    Args... args)
{
  printArg(fmt.position, fmt, args...);
}

template <typename Streambuf>
template <typename Arg1, typename... Args>
inline
void Formatter<Streambuf>::printArg(unsigned int item,
    const _Formatter::FormatterItem& fmt, Arg1 arg1, Args... args)
{
  if (item)
    return printArg(item-1, fmt, args...);

  switch (fmt.formatChar)
  {
    case 's':
      printByType(fmt, arg1);
      break;
    case 'c':
      printChar(fmt, arg1);
      break;
    case 'b':
      printUnsigned<2>(fmt, arg1);
      break;
    case 'd':
      printIntegral<10>(fmt, arg1);
      break;
    case 'o':
      printUnsigned<8>(fmt, arg1);
      break;
    case 'x':
    case 'X':
      printUnsigned<16>(fmt, arg1);
      break;
    case 'p':
      printPointer(fmt, arg1);
      break;
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
      FORMAT_ERROR(FormatError::NotImplemented);
      break;
    default:
      // should not be here
      assert(false);
      break;
  }
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printArg(unsigned int,
    const _Formatter::FormatterItem&)
{
  FORMAT_ERROR(FormatError::TooFewArguments);
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printPreFill(
    const _Formatter::FormatterItem& fmt, unsigned int size)
{
  if (fmt.width == _Formatter::FormatterItem::WIDTH_EMPTY)
    return;

  size = fmt.width - size;
  if (!(fmt.flags & _Formatter::FormatterItem::FLAG_LEFT_JUSTIFY))
    while (size--)
      m_streambuf.sputc(' ');
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printPostFill(
    const _Formatter::FormatterItem& fmt, unsigned int size)
{
  if (fmt.width == _Formatter::FormatterItem::WIDTH_EMPTY)
    return;

  size = fmt.width - size;
  if (fmt.flags & _Formatter::FormatterItem::FLAG_LEFT_JUSTIFY)
    while (size--)
      m_streambuf.sputc(' ');
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<
    !std::is_integral<T>::value &&
    !std::is_floating_point<T>::value &&
    !std::is_convertible<T,
      std::basic_string<typename Formatter<Streambuf>::char_type,
        typename Formatter<Streambuf>::traits_type>>::value &&
    !std::is_pointer<T>::value
  >::type Formatter<Streambuf>::printByType(
      const _Formatter::FormatterItem&, T)
{
  FORMAT_ERROR(FormatError::IncompatibleType);
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printByType(
    const _Formatter::FormatterItem& fmt, bool arg)
{
  static const char_type t[] =
    {'t', 'r', 'u', 'e'};
  static const char_type f[] =
    {'f', 'a', 'l', 's', 'e'};

  printPreFill(fmt, arg ? sizeof(t)/sizeof(*t) : sizeof(f)/sizeof(*f));

  if (arg)
    m_streambuf.sputn(t, sizeof(t)/sizeof(*t));
  else
    m_streambuf.sputn(f, sizeof(f)/sizeof(*f));

  printPostFill(fmt, arg ? sizeof(t)/sizeof(*t) : sizeof(f)/sizeof(*f));
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printByType(
    const _Formatter::FormatterItem& fmt, char_type arg)
{
  printChar(fmt, arg);
}

template <typename Streambuf>
inline
void Formatter<Streambuf>::printByType(
    const _Formatter::FormatterItem& fmt, const char_type* arg)
{
  // calculate size
  std::size_t size;
  {
    const char_type* iter;
    for (iter = arg; *iter; ++iter)
      ;
    size = iter - arg;
  }

  // print
  printPreFill(fmt, size);

  m_streambuf.sputn(arg, size);

  printPostFill(fmt, size);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<std::is_integral<T>::value>::type
  Formatter<Streambuf>::printByType(
      const _Formatter::FormatterItem& fmt, T arg)
{
  printIntegral<10>(fmt, arg);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<std::is_pointer<T>::value>::type
  Formatter<Streambuf>::printByType(
      const _Formatter::FormatterItem& fmt, T arg)
{
  printPointer(fmt, arg);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<std::is_floating_point<T>::value>::type
  Formatter<Streambuf>::printByType(const _Formatter::FormatterItem&, T)
{
  FORMAT_ERROR(FormatError::NotImplemented);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<
  !std::is_floating_point<T>::value &&
  !std::is_integral<T>::value &&
  std::is_convertible<T,
    std::basic_string<
      typename Formatter<Streambuf>::char_type,
      typename Formatter<Streambuf>::traits_type>>::value
  >::type Formatter<Streambuf>::printByType(
      const _Formatter::FormatterItem& fmt, T arg)
{
  std::basic_string<
      typename Formatter<Streambuf>::char_type,
      typename Formatter<Streambuf>::traits_type> str(arg);

  printPreFill(fmt, str.length());

  m_streambuf.sputn(str.c_str(), str.length());

  printPostFill(fmt, str.length());
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<std::is_convertible<T,
    typename Formatter<Streambuf>::char_type>::value>::type
  Formatter<Streambuf>::printChar(const _Formatter::FormatterItem& fmt, T arg)
{
  printPreFill(fmt, 1);

  m_streambuf.sputc(arg);

  printPostFill(fmt, 1);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<!std::is_convertible<T,
    typename Formatter<Streambuf>::char_type>::value>::type
  Formatter<Streambuf>::printChar(const _Formatter::FormatterItem&, T)
{
  FORMAT_ERROR(FormatError::IncompatibleType);
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<std::is_pointer<T>::value>::type
  Formatter<Streambuf>::printPointer(const _Formatter::FormatterItem& fmt, T arg)
{
  _Formatter::FormatterItem fmt2 = fmt;

  fmt2.flags = _Formatter::FormatterItem::FLAG_EXPLICIT_BASE;
  fmt2.precision = sizeof(void*) * 2;
  fmt2.formatChar = 'x';

  printIntegral<16>(fmt2, reinterpret_cast<uintptr_t>(arg));
}

template <typename Streambuf>
template <typename T>
inline
typename std::enable_if<!std::is_pointer<T>::value>::type
  Formatter<Streambuf>::printPointer(const _Formatter::FormatterItem& fmt, T arg)
{
  FORMAT_ERROR(FormatError::IncompatibleType);
}

namespace _Formatter
{
  template <typename T>
  inline
  typename std::enable_if<std::is_signed<T>::value, bool>::type
    isNegative(T value)
  {
    return value < 0;
  }

  template <typename T>
  inline
  typename std::enable_if<!std::is_signed<T>::value, bool>::type
    isNegative(T)
  {
    return false;
  }
}

template <typename Streambuf>
template <unsigned int Tbase, typename T>
typename std::enable_if<
    _Formatter::isIntegral<T>::value
  >::type Formatter<Streambuf>::printIntegral(
      const _Formatter::FormatterItem& fmt, T value)
{
  // convert the number

  char_type buf[64];
  std::size_t numsize =
      printIntegral<Tbase>(buf + sizeof(buf)/sizeof(*buf), fmt, value),
    size = numsize;

  unsigned int zerofill;

  if (fmt.precision == _Formatter::FormatterItem::WIDTH_ARG)
  {
    FORMAT_ERROR(FormatError::NotImplemented);
  }
  else if (fmt.precision == _Formatter::FormatterItem::WIDTH_EMPTY)
    zerofill = 1;
  else
    zerofill = fmt.precision;

  if (zerofill > size)
    zerofill = zerofill - size;
  else
    zerofill = 0;

  // calculate size

  if ((_Formatter::isNegative(value) && fmt.formatChar == 'd') ||
      (fmt.flags & _Formatter::FormatterItem::FLAG_SHOW_SIGN) ||
      (fmt.flags & _Formatter::FormatterItem::FLAG_ADD_SPACE))
    ++size;
  else if (fmt.flags & _Formatter::FormatterItem::FLAG_EXPLICIT_BASE)
    switch (fmt.formatChar)
    {
      case 'x': 
      case 'X':
        if (value != 0)
          size += 2;
        break;
      case 'o': 
        ++size;
        break;
      default:
        // should not be here
        assert(false);
    }

  // get width and calculate needed fill size

  unsigned int fill;
  if (fmt.width == _Formatter::FormatterItem::WIDTH_ARG)
  {
    FORMAT_ERROR(FormatError::NotImplemented);
  }
  else if (fmt.width == _Formatter::FormatterItem::WIDTH_EMPTY)
    fill = 0;
  else
    fill = fmt.width;

  if (static_cast<int>(fill) - static_cast<int>(size) -
        static_cast<int>(zerofill) >= 0)
    fill = fill - size - zerofill;
  else
    fill = 0;

  // fill before sign (with spaces)

  if (!(fmt.flags & _Formatter::FormatterItem::FLAG_FILL_ZERO) &&
      !(fmt.flags & _Formatter::FormatterItem::FLAG_LEFT_JUSTIFY))
    for (unsigned int i = 0; i < fill; ++i)
      m_streambuf.sputc(' ');

  // show sign or base

  if (fmt.flags & _Formatter::FormatterItem::FLAG_EXPLICIT_BASE)
    switch (fmt.formatChar)
    {
      case 'x':
        if (value != 0)
        {
          m_streambuf.sputc('0');
          m_streambuf.sputc('x');
        }
        break;
      case 'X':
        if (value != 0)
        {
          m_streambuf.sputc('0');
          m_streambuf.sputc('X');
        }
        break;
      case 'o':
        m_streambuf.sputc('0');
        break;
      default:
        // should not be here
        assert(false);
    }
  else if (_Formatter::isNegative(value))
    m_streambuf.sputc('-');
  else if (fmt.flags & _Formatter::FormatterItem::FLAG_SHOW_SIGN)
    m_streambuf.sputc('+');
  else if (fmt.flags & _Formatter::FormatterItem::FLAG_ADD_SPACE)
    m_streambuf.sputc(' ');

  // fill after sign (with zeros)
  if (fmt.flags & _Formatter::FormatterItem::FLAG_FILL_ZERO)
    zerofill += fill;

  for (unsigned int i = 0; i < zerofill; ++i)
    m_streambuf.sputc('0');

  // print number

  m_streambuf.sputn(buf+sizeof(buf)-numsize, numsize);

  if (fmt.flags & _Formatter::FormatterItem::FLAG_LEFT_JUSTIFY)
    for (unsigned int i = 0; i < fill; ++i)
      m_streambuf.sputc(' ');
}

template <typename Streambuf>
template <unsigned int Tbase, typename T>
std::size_t Formatter<Streambuf>::printIntegral(char_type* str,
    const _Formatter::FormatterItem& fmt, T value)
{
  static_assert(Tbase == 2 || Tbase == 8 || Tbase == 10 || Tbase == 16,
      "unsupported base");

  // cast base to same type as T to avoid forcing unsigned cast later
  const T base = Tbase;

  char_type baseLetter;
  if (fmt.formatChar == 'X')
    baseLetter = 'A';
  else
    baseLetter = 'a';

  char_type* ptr = str-1;

  while (value)
  {
    typename std::make_signed<char_type>::type digit = value % base;

    value /= base;

    if (digit < 0)
    {
      digit = -digit;
      // we can invert here without losing precision since we /= 10 above
      value = -value;
    }

    if (digit >= 10)
      digit += baseLetter - 10;
    else
      digit += '0';

    *ptr = digit;

    --ptr;
  }

  return str-ptr-1;
}

template <typename Streambuf>
template <unsigned int base, typename T>
inline
typename std::enable_if<
    !_Formatter::isIntegral<T>::value
  >::type Formatter<Streambuf>::printIntegral(
      const _Formatter::FormatterItem&, T)
{
  FORMAT_ERROR(FormatError::IncompatibleType);
}

template <typename Streambuf>
template <unsigned int base, typename T>
inline
typename std::enable_if<
    _Formatter::isIntegral<T>::value
  >::type Formatter<Streambuf>::printUnsigned(
      const _Formatter::FormatterItem& fmt, T value)
{
  printIntegral<base, typename std::make_unsigned<T>::type>(fmt, value);
}

template <typename Streambuf>
template <unsigned int base, typename T>
inline
typename std::enable_if<
    !_Formatter::isIntegral<T>::value
  >::type Formatter<Streambuf>::printUnsigned(
      const _Formatter::FormatterItem&, T)
{
  FORMAT_ERROR(FormatError::IncompatibleType);
}

template <typename Streambuf, typename... Args>
inline void writef(Streambuf& streambuf,
    const typename Streambuf::char_type* format, Args... args)
{
  Formatter<Streambuf>(streambuf).print(format, args...);
}

template <typename... Args>
inline void writef(const char* format, Args... args)
{
  Formatter<std::streambuf>(*std::cout.rdbuf()).print(format, args...);
}

template <typename... Args>
inline void writef(const wchar_t* format, Args... args)
{
  Formatter<std::wstreambuf>(*std::wcout.rdbuf()).print(format, args...);
}

}

#endif
// vim: ts=2:sw=2:sts=2:expandtab
