// gpp_exception.hpp
/*
  This file contains exception objects along with helper functions and macros
  for exceptions.  This library never calls throw directly. Instead, it provides
  a wrapper in optimal_learning::ThrowException(). Thus we never do:
  throw MyException(...);
  instead preferring one of:
  optimal_learning::ThrowException(MyException(...));  // uncommon
  OL_THROW_EXCEPTION(MyException, ...);  // preferred
  These are analogous to boost::throw_exception() and BOOST_THROW_EXCEPTION.

  ALL exception objects MUST inherit publicly from std::exception.

  Additionally, to use the OL_THROW_EXCEPTION macro (see its #define for details), the
  first two arguments in MyException's ctor must be char *.

  Users may define the macro OL_NO_EXCEPTIONS to *disable* exception
  handling in this library.  Defining that macro means this library will
  never call throw.  Doing so requires users to implement
  optimal_learning::ThrowException() (see comments below for #ifdef OL_NO_EXCEPTIONS).
*/

#include <exception>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "gpp_common.hpp"

#ifndef OPTIMAL_LEARNING_EPI_SRC_CPP_GPP_EXCEPTION_HPP_
#define OPTIMAL_LEARNING_EPI_SRC_CPP_GPP_EXCEPTION_HPP_

namespace optimal_learning {

/*
  Macro to stringify the expansion of a macro. For example, say we are on line 53:
  #__LINE__ --> "__LINE__"
  OL_STRINGIFY_EXPANSION(__LINE__) --> "53"

  OL_STRINGIFY_EXPANSION_INNER is not meant to be used directly;
  but we need "#x" in a macro for this expansion to work.

  This is a standard trick; see bottom of:
  http://gcc.gnu.org/onlinedocs/cpp/Stringification.html
*/
#define OL_STRINGIFY_EXPANSION_INNER(x) #x
#define OL_STRINGIFY_EXPANSION(x) OL_STRINGIFY_EXPANSION_INNER(x)

/*
  Macro to stringify and format the current file and line number. For
  example, if the macro is invoked from line 893 of file gpp_foo.cpp,
  this macro produces the compile-time string-constant:
  "(gpp_foo.cpp: 893)"
*/
#define OL_STRINGIFY_FILE_AND_LINE "(" __FILE__ ": " OL_STRINGIFY_EXPANSION(__LINE__) ")"

/*
  Users may disable exceptions so that this library NEVER invokes throw directly.
  Doing so requires the user to define ThrowException().

  This makes the most sense when paired with the compiler flag -fno-exceptions.
  Using -fno-exceptions will also require disabling Boost's exceptions; #define:
  BOOST_NO_EXCEPTIONS
  BOOST_EXCEPTION_DISABLE
  And provide a definition for:
  void throw_exception( std::exception const & e );
  e.g., the same definition as ThrowException(). See:
  http://www.boost.org/doc/libs/1_55_0/libs/exception/doc/throw_exception.html

  throw may still be called indirectly through Boost, so disable that
  library's exceptions too. (See below for details/link.)
*/
#ifdef OL_NO_EXCEPTIONS

/*
  Disabling exceptions requires the user to implement ThrowException().
  This can be as simple as calling std::abort(). A reference to the
  thrown exception is provided in case other behavior is desired.

  This function normally wraps throw. Callers are allowed to assume
  that this function NEVER returns. If the user-specified implementation
  does, the resulting behavior is UNDEFINED.

  INPUTS:
  exception: an exception object publicly deriving from std::exception

  RETURNS:
  **NEVER RETURNS**
*/
OL_NORETURN void ThrowException(const std::exception& exception);

#else

/*
  Wrapper around the "throw" keyword, making it easy to disable exceptions.
  Checks that the argument inherits from std::exception and invokes throw.

  INPUTS:
  exception: reference to exception object (publicly deriving from std::exception) to throw

  RETURNS:
  **NEVER RETURNS**
*/
template <typename ExceptionType>
OL_NORETURN inline void ThrowException(const ExceptionType& except) {
  static_assert(std::is_base_of<std::exception, ExceptionType>::value, "ExceptionType must be derived from std::exception.");

  throw except;
}

#endif


/*
  Macro for throwing exceptions that adds file/line and function name information.
  It is just for convenience, saving callers from having to type OL_STRINGIFY_FILE_AND_LINE,
  and OL_CURRENT_FUNCTION_NAME repeatedly.

  To use this macro, the argument list of ExceptionType's ctor MUST start with two
  "char const *", followed by the arguments in "Args...".
  Additionally, ExceptionType must be a complete type.

  For example, if you could write:
  throw_exception(BoundsException<double>(OL_STRINGIFY_FILE_AND_LINE, OL_CURRENT_FUNCTION_NAME, "Invalid length scale.", value, min, max));
  then you can instead write:
  OL_THROW_EXCEPTION(BoundsException<double>, "Invalid length scale.", value, min, max);
*/
#define OL_THROW_EXCEPTION(ExceptionType, Args...) ThrowException(ExceptionType(OL_STRINGIFY_FILE_AND_LINE, OL_CURRENT_FUNCTION_NAME, Args))

/*
  Exception to handle general runtime errors (e.g., not fitting into other exception types).
  Subclasses std::exception.

  This class is essentially the same as std::runtime_error but it includes a ctor with
  some extra logic for formatting the error message.

  Holds only a std::string containing the message produced by what(). Note that
  any exceptions from std::string operations (e.g., std::bad_alloc) will cause std::terminate().
*/
class RuntimeException : public std::exception {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "RuntimeException";

  /*
    Constructs a RuntimeException with the specified message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
  */
  RuntimeException(char const * line_info, char const * func_info, char const * custom_message);

  /*
    Provides a C-string containing information about the conditions of the exception.
    See: http://en.cppreference.com/w/cpp/error/exception

    The message is formatted in the class ctor (capitals indicate variable information):
    R"%%(
    RuntimeException: CUSTOM_MESSAGE FUNCTION_NAME FILE_LINE_INFO
    )%%"

    RETURNS:
    C-style char string describing the exception.
  */
  virtual const char* what() const noexcept override OL_WARN_UNUSED_RESULT {
    return message_.c_str();
  }

  RuntimeException() = delete;

 private:
  std::string message_;
};

/*
  Exception to capture value < min_value OR value > max_value.
  Subclasses std::exception.

  Stores value, min, and max for debugging/logging/reacting purposes.

  Also holds a std::string containing the message produced by what(). Note that
  any exceptions from std::string operations (e.g., std::bad_alloc) will cause std::terminate().
*/
template <typename ValueType>
class BoundsException : public std::exception {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "BoundsException";

  /*
    Constructs a BoundsException object with extra fields to flesh out the what() message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    value: the value that violates its min or max bound
    min: the minimum bound for value
    max: the maximum bound for value
  */
  BoundsException(char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType min_in, ValueType max_in) : BoundsException(kName, line_info, func_info, custom_message, value_in, min_in, max_in) {
  }

  /*
    Provides a C-string containing information about the conditions of the exception.
    See: http://en.cppreference.com/w/cpp/error/exception

    The message is formatted in the class ctor (capitals indicate variable information):
    R"%%(
    BoundsException: VALUE is not in range [MIN, MAX].
    CUSTOM_MESSAGE FUNCTION_NAME FILE_LINE_INFO
    )%%"

    RETURNS:
    C-style char string describing the exception.
  */
  virtual const char* what() const noexcept override OL_WARN_UNUSED_RESULT {
    return message_.c_str();
  }

  ValueType value() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return value_;
  }

  ValueType max() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return max_;
  }

  ValueType min() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return min_;
  }

  OL_DISALLOW_DEFAULT_AND_ASSIGN(BoundsException);

 protected:
  BoundsException(char const * name_in, char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType min_in, ValueType max_in);

 private:
  ValueType value_, min_, max_;
  std::string message_;
};

// template explicit instantiation declarations, see gpp_common.hpp header comments, item 6
extern template class BoundsException<int>;
extern template class BoundsException<double>;

/*
  Exception to capture value < min_value.
  Simple subclass of BoundsException that sets the max argument to std::numeric_limits<ValueType>::max()
*/
template <typename ValueType>
class LowerBoundException : public BoundsException<ValueType> {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "LowerBoundException";

  /*
    Constructs a LowerBoundException object with extra fields to flesh out the what() message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    value: the value that violates its min or max bound
    min: the minimum bound for value
  */
  LowerBoundException(char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType min_in) : BoundsException<ValueType>(kName, line_info, func_info, custom_message, value_in, min_in, std::numeric_limits<ValueType>::max()) {
  }

  OL_DISALLOW_DEFAULT_AND_ASSIGN(LowerBoundException);
};

/*
  Exception to capture value > max_value.
  Simple subclass of BoundsException that sets the min argument to std::numeric_limits<ValueType>::lowest()
*/
template <typename ValueType>
class UpperBoundException : public BoundsException<ValueType> {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "UpperBoundException";

  /*
    Constructs an UpperBoundException object with extra fields to flesh out the what() message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    value: the value that violates its min or max bound
    max: the maximum bound for value
  */
  UpperBoundException(char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType max_in) : BoundsException<ValueType>(kName, line_info, func_info, custom_message, value_in, std::numeric_limits<ValueType>::lowest(), max_in) {
  }

  OL_DISALLOW_DEFAULT_AND_ASSIGN(UpperBoundException);
};

/*
  Exception to capture value != truth (+/- tolerance).
  The tolerance parameter is optional and only usable with floating point data types.
  Subclasses std::exception.

  Stores value and truth (and tolerance as applicable) for debugging/logging/reacting purposes.

  Also holds a std::string containing the message produced by what(). Note that
  any exceptions from std::string operations (e.g., std::bad_alloc) will cause std::terminate().
*/
template <typename ValueType>
class InvalidValueException : public std::exception {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "InvalidValueException";

  /*
    Constructs a InvalidValueException object with extra fields to flesh out the what() message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    value: the invalid value
    truth: what "value" is supposed to be
  */
  InvalidValueException(char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType truth_in);

  /*
    Constructs a InvalidValueException object with extra fields to flesh out the what() message.
    This ctor additionally has an input for tolerance, and is only enabled for floating point types.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    value: the invalid value
    truth: what "value" is supposed to be
    tolerance: the maximum acceptable error in |value - truth|
  */
  template <typename ValueTypeIn = ValueType, class = typename std::enable_if<std::is_floating_point<ValueType>::value, ValueTypeIn>::type>
  InvalidValueException(char const * line_info, char const * func_info, char const * custom_message, ValueType value_in, ValueType truth_in, ValueType tolerance_in);

  /*
    Provides a C-string containing information about the conditions of the exception.
    See: http://en.cppreference.com/w/cpp/error/exception

    The message is formatted in the class ctor (capitals indicate variable information):
    R"%%(
    InvalidValueException: VALUE != TRUTH (value != truth).
    CUSTOM_MESSAGE FUNCTION_NAME FILE_LINE_INFO
    )%%"
    OR
    R"%%(
    InvalidValueException: VALUE != TRUTH ± TOLERANCE (value != truth ± tolerance).
    CUSTOM_MESSAGE FUNCTION_NAME FILE_LINE_INFO
    )%%"
    Depending on which ctor was used.

    RETURNS:
    C-style char string describing the exception.
  */
  virtual const char* what() const noexcept override OL_WARN_UNUSED_RESULT {
    return message_.c_str();
  }

  ValueType value() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return value_;
  }

  ValueType truth() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return truth_;
  }

  ValueType tolerance() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return tolerance_;
  }

  OL_DISALLOW_DEFAULT_AND_ASSIGN(InvalidValueException);

 private:
  ValueType value_, truth_, tolerance_;
  std::string message_;
};

// template explicit instantiation definitions, see gpp_common.hpp header comments, item 6
extern template class InvalidValueException<int>;
extern template class InvalidValueException<double>;
extern template InvalidValueException<double>::InvalidValueException(char const * line_info, char const * func_info, char const * custom_message, double value_in, double truth_in, double tolerance_in);

/*
  Exception to capture when a matrix A (\in R^{m x n}) is singular.
  Subclasses std::exception.

  Stores the matrix (in a std::vector) and its dimensions.

  Also holds a std::string containing the message produced by what(). Note that
  any exceptions from std::string operations (e.g., std::bad_alloc) will cause std::terminate().
  Similarly the std::vector<double> ctor can throw and cause std::terminate().
*/
class SingularMatrixException : public std::exception {
 public:
  // TODO(eliu): the "const" keyword is redundant (constexpr => const) but needed
  // here b/c gcc gives a warning about deprecated conversions to char* otherwise.
  // REMOVE later when gcc figures out what's up.
  constexpr static char const * const kName = "SingularMatrixException";

  /*
    Constructs a SingularMatrixException object with extra fields to flesh out the what() message.

    INPUTS:
    line_info[]: ptr to char array containing __FILE__ and __LINE__ info; e.g., from OL_STRINGIFY_FILE_AND_LINE
    func_info[]: optional ptr to char array from OL_CURRENT_FUNCTION_NAME or similar
    custom_message[]: optional ptr to char array with any additional text/info to print/log
    matrix[num_rows][num_cols]: the singular matrix
    num_rows: number of rows in the matrix
    num_cols: number of columns in the matrix
  */
  SingularMatrixException(char const * line_info, char const * func_info, char const * custom_message, double const * matrix_in, int num_rows_in, int num_cols_in);

  /*
    Provides a C-string containing information about the conditions of the exception.
    See: http://en.cppreference.com/w/cpp/error/exception

    The message is formatted in the class ctor (capitals indicate variable information):
    R"%%(
    SingularMatrixException: M x N matrix is singular.
    CUSTOM_MESSAGE FUNCTION_NAME FILE_LINE_INFO
    )%%"

    Note: this exception currently does not print the full matrix. Use a debugger
    and call PrintMatrix() (gpp_linear_algebra.hpp) or catch the exception and
    proecss the matrix.

    RETURNS:
    C-style char string describing the exception.
  */
  virtual const char* what() const noexcept override OL_WARN_UNUSED_RESULT {
    return message_.c_str();
  }

  int num_rows() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return num_rows_;
  }

  int num_cols() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return num_cols_;
  }

  const std::vector<double>& matrix() const noexcept OL_PURE_FUNCTION OL_WARN_UNUSED_RESULT {
    return matrix_;
  }

  OL_DISALLOW_DEFAULT_AND_ASSIGN(SingularMatrixException);

 private:
  int num_rows_, num_cols_;
  std::vector<double> matrix_;
  std::string message_;
};

}  // end namespace optimal_learning

#endif  // OPTIMAL_LEARNING_EPI_SRC_CPP_GPP_EXCEPTION_HPP_