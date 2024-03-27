#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>

#include <iostream>


template<typename T>
class RawMemory{
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity){}

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
    : buffer_(other.buffer_)
    , capacity_(other.capacity_){
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept{
        if(this != &rhs){
            Deallocate(buffer_);
            buffer_ = nullptr;
            capacity_ = 0;
            Swap(rhs);
        }
        return *this;
    }


    ~RawMemory(){
        if(buffer_ != nullptr){
            Deallocate(buffer_);
        }
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n){
        return n != 0 ? static_cast<T*>((operator new(n * sizeof(T)))) : nullptr;
    }        

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() noexcept = default;

    explicit Vector(size_t size)
    : data_(size)
    , size_(size)
    {   
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    explicit Vector(std::initializer_list<T> list)
    : data_(list.size())
    , size_(list.size())
    {
        if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>){
            std::uninitialized_move_n(list.begin(), size_, data_.GetAddress());
        } else {
            std::uninitialized_copy_n(list.begin(), size_, data_.GetAddress()); 
        }
    }

    explicit Vector(const Vector& other)
    : data_(other.size_)
    , size_(other.size_)
    {   
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector& operator=(const Vector& rhs){
        if(this != &rhs){
            if(rhs.size_ > data_.Capacity()){
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if(rhs.size_ >= size_){
                    size_t i = 0;
                    for(; i < size_; ++i){
                        data_[i] = rhs[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + i, rhs.size_ - size_, data_.GetAddress() + i);
                    size_ = rhs.size_;
                } else {
                    size_t i = 0;
                    for(; i < rhs.size_; ++i){
                        data_[i] = rhs[i];
                    }
                    std::destroy_n(data_.GetAddress() + i, size_- rhs.size_);
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }

    Vector(Vector&& other) noexcept
    : data_(std::move(other.data_))
    , size_(other.size_){
        other.size_ = 0;
    }
    
    Vector& operator=(Vector&& rhs){
        if(this != &rhs){
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }

        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity){
        if(new_capacity <= data_.Capacity()){
            return;
        }

        RawMemory<T> new_data(new_capacity);
        FillNewData(new_data, 0, 0, size_);
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size){
        if(new_size <= size_){
            size_t left_elems = size_ - new_size;
            if(left_elems != 0){
                std::destroy_n(data_.GetAddress() + new_size, left_elems);
            }
            
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value){
        EmplaceBack(value);
    }   

    void PushBack(T&& value){
        EmplaceBack(std::move(value));
    }

    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... ctor_args){
        if(size_ == data_.Capacity()){
            RawMemory<T> new_data((size_ == 0 ? 1 : size_ * 2));
            FillNewData(new_data,0,  0, size_);
            new (new_data.GetAddress() + size_) T(std::forward<Args>(ctor_args)...);
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(ctor_args)...);
        }
        ++size_; 

        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... ctor_args){
        size_t index = pos - data_.GetAddress();

        if(size_ == data_.Capacity()){
            RawMemory<T> new_data((size_ == 0 ? 1 : size_ * 2));
            new (new_data + index) T(std::forward<Args>(ctor_args)...);
            FillBehindIndex(new_data,index);
            FillAfterIndex(new_data,index);
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            
            if(size_ != 0){
                T elem(std::forward<Args>(ctor_args)...);
                new (end()) T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + index, end() - 1, end());
                data_[index] = std::move(elem);
            } else {
                new (data_.GetAddress()) T(std::forward<Args>(ctor_args)...);
            }
            
        }
        ++size_;
        return begin() + index;
    }

    iterator Erase(const_iterator pos){
        size_t index = pos - data_.GetAddress();
        std::move(begin() + index + 1, end(), begin() + index);
        data_[size_ - 1].~T();
        --size_;
        return begin() + index;
    }

    void PopBack() noexcept{
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    iterator begin() noexcept{
        return data_.GetAddress();
    }

    iterator end() noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator end() const noexcept{
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept{
        return data_.GetAddress() + size_;
    }
 
    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector(){
        if(data_.GetAddress() != nullptr){
            std::destroy_n(data_.GetAddress(), size_);
        }
    }
private:
    void FillNewData(RawMemory<T>& new_data, size_t from, size_t to, size_t count){
        if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>){
            std::uninitialized_move_n(data_.GetAddress() + from, count, new_data.GetAddress() + to);
        } else {
            std::uninitialized_copy_n(data_.GetAddress() + from, count, new_data.GetAddress() + to); 
        }
    }

    void FillBehindIndex(RawMemory<T>& new_data, size_t index){
        try {
            FillNewData(new_data, 0, 0, index);
        } catch(...){
            new_data[index].~T();
        }
    }

    void FillAfterIndex(RawMemory<T>& new_data, size_t index){
        try{
            FillNewData(new_data, index, index + 1, size_ - index);
        } catch(...){
            for(size_t i = 0; i <= index; ++i){
                new_data[i].~T();
            }
        }
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};