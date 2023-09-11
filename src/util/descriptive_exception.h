#ifndef DESCRIPTIVE_EXCEPTION_H
#define DESCRIPTIVE_EXCEPTION_H

#if defined(_WIN32)
#define descriptive_exception std::bad_exception
#else

#include <string>

// https://stackoverflow.com/questions/28640553/exception-class-with-a-char-constructor

class descriptive_exception : public std::exception {
public:
    descriptive_exception(std::string const &message) : msg_(message) { }
    virtual char const *what() const noexcept { return msg_.c_str(); }

private:
    std::string msg_;
};

#endif

#endif // DESCRIPTIVE_EXCEPTION_H
