#pragma once

#include "Context.h"
#include "MappedBuffer.h"
#include <vulkan/vulkan.h>

namespace hlab {

template <typename T_DATA>
class UniformBuffer : public MappedBuffer
{
  public:
    UniformBuffer(Context& ctx, T_DATA& cpuData) : MappedBuffer(ctx), cpuData_(cpuData)
    {
        static_assert(std::is_trivially_copyable_v<T_DATA>,
                      "UniformBuffer data type must be trivially copyable");
        // 안내: vector 같이 동적 메모리를 사용하는 컨테이너가 가 포함되어 있으면
        //      memcpy()로 간단히 복사할 수 없습니다.

        createUniformBuffer(sizeof(T_DATA), &cpuData);
    }

    UniformBuffer(UniformBuffer&& other) noexcept
        : MappedBuffer(std::move(other)), cpuData_(other.cpuData_)
    {
    }

    ~UniformBuffer() = default; // MappedBuffer handles cleanup

    void updateData()
    {
        MappedBuffer::updateData(&cpuData_, sizeof(cpuData_), 0);
    }

    T_DATA& cpuData()
    {
        return cpuData_;
    }

    // Inherit all MappedBuffer functionality
    // No need to provide wrappers anymore since we inherit directly

  private:
    T_DATA& cpuData_;
};

} // namespace hlab