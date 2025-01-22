#pragma once

#include <cstring>
#include <vector>
#include "felix86/common/utility.hpp"

struct SerializedFunction {
    template <typename T>
    void Push(T value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        u8* ptr = reinterpret_cast<u8*>(&value);
        data.insert(data.end(), ptr, ptr + sizeof(T));
    }

    template <typename T>
    T Pop() const {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        T value;
        u8* ptr = reinterpret_cast<u8*>(&value);
        memcpy(ptr, data.data() + index, sizeof(T));
        index += sizeof(T);
        return value;
    }

    const std::vector<u8>& GetData() const {
        return data;
    }

    std::vector<u8>& GetData() {
        return data;
    }

    bool AllPopped() const {
        return index == data.size();
    }

private:
    std::vector<u8> data;
    mutable u64 index = 0;
    friend struct BackendFunction;
};
