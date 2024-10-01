#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <numeric>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    explicit RawMemory(size_t capacity);
    RawMemory(const RawMemory&) = delete;
    RawMemory(RawMemory&& other) noexcept;
    ~RawMemory();

    const T* GetAddress() const noexcept;
    T* GetAddress() noexcept;
    size_t Capacity() const;
    void Swap(RawMemory& other) noexcept;

    T* operator+(size_t offset) noexcept;
    const T* operator+(size_t offset) const noexcept;
    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory& operator=(RawMemory&& rhs) noexcept;
    

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n);

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept;

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    Vector() = default;
    explicit Vector(size_t size);
    Vector(const Vector& other);
    Vector(Vector&& other) noexcept;
    ~Vector();

    size_t Size() const noexcept;
    size_t Capacity() const noexcept;
    void Reserve(size_t new_capacity);
    void Swap(Vector& other);
    void Resize(size_t new_size);

    template <typename S>
    void PushBack(S&& value);

    template <typename... Args>
    T& EmplaceBack(Args&&... args);

    template <typename S>
    iterator Insert(const_iterator pos, S&& value);

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args);

    void PopBack();

    iterator Erase(const_iterator pos);

    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;
    Vector& operator=(const Vector& rhs);
    Vector& operator=(Vector&& rhs) noexcept;

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Разрушает количество элементов n по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept;

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem);

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept;
};

// ------------------RawMemory-----------------

template <typename T>
RawMemory<T>::RawMemory(size_t capacity)
    : buffer_(Allocate(capacity))
    , capacity_(capacity) {
}

template <typename T>
RawMemory<T>::RawMemory(RawMemory&& other) noexcept
    : buffer_(std::exchange(other.buffer_, nullptr))
    , capacity_(std::exchange(other.capacity_, 0)) {
}

template <typename T>
RawMemory<T>::~RawMemory() {
    Deallocate(buffer_);
}

template <typename T>
const T* RawMemory<T>::GetAddress() const noexcept {
    return buffer_;
}

template <typename T>
T* RawMemory<T>::GetAddress() noexcept {
    return buffer_;
}

template <typename T>
size_t RawMemory<T>::Capacity() const {
    return capacity_;
}

template <typename T>
void RawMemory<T>::Swap(RawMemory& other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
}

template <typename T>
T* RawMemory<T>::Allocate(size_t n) {
    return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
}

template <typename T>
void RawMemory<T>::Deallocate(T* buf) noexcept {
    operator delete(buf);
}

template <typename T>
T* RawMemory<T>::operator+(size_t offset) noexcept {
    // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
    assert(offset <= capacity_);
    return buffer_ + offset;
}

template <typename T>
const T* RawMemory<T>::operator+(size_t offset) const noexcept {
    return const_cast<RawMemory&>(*this) + offset;
}

template <typename T>
const T& RawMemory<T>::operator[](size_t index) const noexcept {
    return const_cast<RawMemory&>(*this)[index];
}

template <typename T>
T& RawMemory<T>::operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
}

template <typename T>
RawMemory<T>& RawMemory<T>::operator=(RawMemory&& rhs) noexcept {
    if (this != &rhs) {
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::move(rhs.capacity_);
    }

    return *this;
}

// -------------------Vector-------------------

template <typename T>
typename Vector<T>::iterator Vector<T>::begin() noexcept {
    return data_.GetAddress();
}

template <typename T>
typename Vector<T>::iterator Vector<T>::end() noexcept {
    return data_.GetAddress() + size_;
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const noexcept {
    return cbegin();
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::end() const noexcept {
    return cend();
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::cbegin() const noexcept {
    return data_.GetAddress();
}

template <typename T>
typename Vector<T>::const_iterator Vector<T>::cend() const noexcept {
    return data_.GetAddress() + size_;
}

template <typename T>
Vector<T>::Vector(size_t size)
    : data_(size)
    , size_(size)
{
    size_t i = 0;
    try {
        for (; i != size; ++i) {
            new (data_.GetAddress() + i) T();
        }
    } catch (...) {
        // В переменной i содержится количество созданных элементов, которые надо разрушить
        DestroyN(data_.GetAddress(), i);
        // Перевыбрасываем пойманное исключение, чтобы сообщить об ошибке создания объекта
        throw;
    }
}

template <typename T>
Vector<T>::Vector(const Vector& other)
    : data_(other.size_)
    , size_(other.size_)
{
    size_t i = 0;
    try {
        for (; i != other.size_; ++i) {
            CopyConstruct(data_ + i, other.data_[i]);
        }
    } catch (...) {
        DestroyN(data_.GetAddress(), i);
        throw;
    }
}

template <typename T>
Vector<T>::Vector(Vector&& other) noexcept
    : data_(std::move(other.data_))
    , size_(std::exchange(other.size_, 0)) {
}

template <typename T>
Vector<T>::~Vector() {
    DestroyN(data_.GetAddress(), size_);
}

template <typename T>
size_t Vector<T>::Size() const noexcept {
    return size_;
}

template <typename T>
size_t Vector<T>::Capacity() const noexcept {
    return data_.Capacity();
}

template <typename T>
void Vector<T>::Reserve(size_t new_capacity) {
    if (new_capacity <= data_.Capacity()) {
        return;
    }

    RawMemory<T> new_data(new_capacity);

    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
    } else {
        std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
}

template <typename T>
void Vector<T>::Swap(Vector& other) {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
}

template <typename T>
void Vector<T>::Resize(size_t new_size) {
    if (new_size < size_) {
        DestroyN(data_.GetAddress() + new_size, size_ - new_size);
    } else if (Capacity() < new_size) {
        Reserve(std::max(new_size, Capacity() * 2));
        for (size_t i = size_; i < new_size; ++i) {
            new (data_ + i) T();
        }
    }
    size_ = new_size;
}

template <typename T>
template <typename S>
void Vector<T>::PushBack(S&& value) {
    EmplaceBack(std::forward<S&&>(value));
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
    if (size_ < Capacity()) {
        new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
    } else {
        size_t new_capacity = (Capacity() == 0 ? 1 : Capacity() * 2);
        RawMemory<T> new_data(new_capacity);

        new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    return data_[size_++];
}

template <typename T>
template <typename S>
typename Vector<T>::iterator Vector<T>::Insert(const_iterator pos, S&& value) {
    return Emplace(pos, std::forward<S&&>(value));
}

template <typename T>
template <typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    if (pos == end()) return &EmplaceBack(std::forward<Args&&>(args)...);

    size_t p_distance = pos - begin();
    //try {

        if (size_ < Capacity()) {            
            T temp(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(data_[size_ - 1]));
            std::move_backward(begin() + p_distance, end() - 1, end());
            data_[p_distance] = std::move(temp);
        } else {
            size_t new_capacity = (Capacity() == 0 ? 1 : Capacity() * 2);
            RawMemory<T> new_data(new_capacity);

            new (new_data.GetAddress() + p_distance) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), p_distance, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + p_distance, size_ - p_distance, new_data.GetAddress() + p_distance + 1);                    
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), p_distance, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + p_distance, size_ - p_distance, new_data.GetAddress() + p_distance + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }

    //} catch (...) {
        //operator delete (end());
        //throw;
    //}    

    ++size_;

    return &data_[p_distance];
}

template <typename T>
void Vector<T>::PopBack() {
    Destroy(data_.GetAddress() + size_ - 1);
    --size_;
}

template <typename T>
typename Vector<T>::iterator Vector<T>::Erase(const_iterator pos) {
    assert(pos >= begin() && pos <= end());
    auto p_distance = pos - begin();
    std::move(begin() + p_distance + 1, end(), begin() + p_distance);
    PopBack();
    return &data_[p_distance];
}

template <typename T>
void Vector<T>::DestroyN(T* buf, size_t n) noexcept {
    for (size_t i = 0; i != n; ++i) {
        Destroy(buf + i);
    }
}

template <typename T>
void Vector<T>::CopyConstruct(T* buf, const T& elem) {
    new (buf) T(elem);
}

template <typename T>
void Vector<T>::Destroy(T* buf) noexcept {
    buf->~T();
}

template <typename T>
const T& Vector<T>::operator[](size_t index) const noexcept {
    return const_cast<Vector&>(*this)[index];
}

template <typename T>
T& Vector<T>::operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& rhs) {
    if (Capacity() < rhs.size_) {
        Vector temp(rhs);
        Swap(temp);
    } else if (rhs.size_ < size_) {        
        std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
        DestroyN(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
    } else {
        std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
        std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
    }
    size_ = rhs.size_;
    return *this;
}

template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& rhs) noexcept {
    if (this != &rhs) {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
    }

    return *this;
}