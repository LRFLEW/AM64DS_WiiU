#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <exception>

class error : public std::exception {
public:
    error(const char *msg) noexcept : msg(msg) { }
    virtual const char *what() const noexcept override { return msg; }

private:
    const char *const msg;
};

#endif // EXCEPTION_HPP
