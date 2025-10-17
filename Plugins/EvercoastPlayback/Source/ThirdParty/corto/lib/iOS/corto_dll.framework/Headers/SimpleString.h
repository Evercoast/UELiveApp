//
//  SimpleString.h
//  corto
//
//  Created by Ye @ Evercoast on 30/05/25.
//

#ifndef SimpleString_h
#define SimpleString_h

#include <cstring>   // for std::strlen, std::strcpy
#include <iostream>  // for std::cout (optional, for demonstration)

namespace crt
{
class SimpleString {
private:
    
    std::size_t size_;  // length excluding null terminator
    char* data_;
    
public:
    // Constructor from const char*
    SimpleString(const char* str = "")
    : size_(std::strlen(str)), data_(new char[size_ + 1]) {
        std::strcpy(data_, str);
    }
    
    // Copy constructor
    SimpleString(const SimpleString& other)
    : size_(other.size_), data_(new char[other.size_ + 1]) {
        std::strcpy(data_, other.data_);
    }
    
    // Copy assignment
    SimpleString& operator=(const SimpleString& other) {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            data_ = new char[size_ + 1];
            std::strcpy(data_, other.data_);
        }
        return *this;
    }
    
    // Move constructor
    SimpleString(SimpleString&& other) noexcept
    : size_(other.size_), data_(other.data_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    
    // Move assignment
    SimpleString& operator=(SimpleString&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    // Destructor
    ~SimpleString() {
        delete[] data_;
    }
    
    // Size accessor
    std::size_t size() const {
        return size_;
    }
    
    // C-string accessor
    const char* c_str() const {
        return data_;
    }
    
    // Operator[] accessor
    char& operator[](std::size_t index) {
        return data_[index];
    }
    
    const char& operator[](std::size_t index) const {
        return data_[index];
    }
    
    // Comparison operators
    bool operator==(const SimpleString& other) const {
        return std::strcmp(data_, other.data_) == 0;
    }
    
    bool operator!=(const SimpleString& other) const {
        return !(*this == other);
    }
    
    bool operator<(const SimpleString& other) const {
        return std::strcmp(data_, other.data_) < 0;
    }
    
    bool operator<=(const SimpleString& other) const {
        return std::strcmp(data_, other.data_) <= 0;
    }
    
    bool operator>(const SimpleString& other) const {
        return std::strcmp(data_, other.data_) > 0;
    }
    
    bool operator>=(const SimpleString& other) const {
        return std::strcmp(data_, other.data_) >= 0;
    }
};
}

#endif /* SimpleString_h */
