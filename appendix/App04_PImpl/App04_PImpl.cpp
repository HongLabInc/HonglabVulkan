#include <iostream>
#include <vector>
#include <memory>
#include <string>

using namespace std;

// Forward declarations for PImpl pattern
class DataImpl;
class FloatDataImpl;
class IntDataImpl;

/*
================================================================================
PIMPL PATTERN (Pointer to Implementation) DEMONSTRATION
================================================================================
The PImpl pattern separates interface from implementation by:
1. Moving implementation details to separate classes (DataImpl, etc.)
2. Using forward declarations to hide implementation from clients
3. Storing implementation as a pointer/smart pointer

Benefits:
- Compilation firewall: Changes to impl don't require recompiling clients
- Binary compatibility: Can change implementation without breaking ABI
- Reduced header dependencies: Headers only need forward declarations
- Information hiding: Implementation details completely hidden

Drawbacks:
- Extra indirection (pointer dereference)
- Dynamic allocation overhead
- More complex code structure
================================================================================
*/

class Data
{
  public:
    Data();          // Constructor will be defined in .cpp file
    virtual ~Data(); // Virtual destructor for polymorphism
    virtual void print() = 0;

  protected:
    // PImpl: Hide implementation behind pointer
    unique_ptr<DataImpl> pImpl_;
};

class FloatData : public Data
{
  public:
    FloatData(float value);
    ~FloatData() override;
    void print() override;

  private:
    // PImpl: Hide FloatData implementation
    unique_ptr<FloatDataImpl> pFloatImpl_;
};

class IntData : public Data
{
  public:
    IntData(int value);
    ~IntData() override;
    void print() override;

  private:
    // PImpl: Hide IntData implementation
    unique_ptr<IntDataImpl> pIntImpl_;
};

// Implementation classes (would typically be in separate .cpp file)
class DataImpl
{
  public:
    virtual ~DataImpl() = default;
    virtual void print() = 0;
};

class FloatDataImpl : public DataImpl
{
  public:
    FloatDataImpl(float value) : f_(value)
    {
    }

    void print() override
    {
        cout << "Float (PImpl): " << f_ << endl;
    }

    float getValue() const
    {
        return f_;
    }
    void setValue(float value)
    {
        f_ = value;
    }

  private:
    float f_;
    // Could add more implementation details here
    // without affecting the public interface
};

class IntDataImpl : public DataImpl
{
  public:
    IntDataImpl(int value) : i_(value)
    {
    }

    void print() override
    {
        cout << "Int (PImpl): " << i_ << endl;
    }

    int getValue() const
    {
        return i_;
    }
    void setValue(int value)
    {
        i_ = value;
    }

  private:
    int i_;
    // Could add more implementation details here
    // without affecting the public interface
};

// Data class implementation
Data::Data() = default;
Data::~Data() = default;

// FloatData class implementation
FloatData::FloatData(float value) : pFloatImpl_(make_unique<FloatDataImpl>(value))
{
}

FloatData::~FloatData() = default;

void FloatData::print()
{
    if (pFloatImpl_) {
        pFloatImpl_->print();
    }
}

// IntData class implementation
IntData::IntData(int value) : pIntImpl_(make_unique<IntDataImpl>(value))
{
}

IntData::~IntData() = default;

void IntData::print()
{
    if (pIntImpl_) {
        pIntImpl_->print();
    }
}

class DataFactory
{
  public:
    static unique_ptr<Data> createFloatData(float value)
    {
        return make_unique<FloatData>(value);
    }

    static unique_ptr<Data> createIntData(int value)
    {
        return make_unique<IntData>(value);
    }
};

/*
================================================================================
활용 사례: VULKAN RESOURCE MANAGEMENT WITH PIMPL PATTERN
================================================================================
PImpl 패턴은 특히 Vulkan과 같은 복잡한 API에서 유용합니다:
- 복잡한 Vulkan 구현 세부사항을 숨김
- 헤더 파일에서 vulkan.h 의존성 제거
- 바이너리 호환성 보장
- 컴파일 시간 단축
================================================================================
*/

// Forward declarations for Vulkan resource implementations
class ResourceImpl;
class ImageResourceImpl;
class BufferResourceImpl;

// Abstract resource interface (PImpl pattern)
class Resource
{
  public:
    Resource();
    virtual ~Resource();
    virtual void cleanup() = 0;
    virtual void print() const = 0;

  protected:
    unique_ptr<ResourceImpl> pImpl_;
};

// Image resource interface
class ImageResource : public Resource
{
  public:
    ImageResource(uint32_t width, uint32_t height);
    ~ImageResource() override;

    void cleanup() override;
    void print() const override;

    // Public interface - no Vulkan types exposed
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    void transitionLayout();

  private:
    unique_ptr<ImageResourceImpl> pImageImpl_;
};

// Buffer resource interface
class BufferResource : public Resource
{
  public:
    BufferResource(size_t size);
    ~BufferResource() override;

    void cleanup() override;
    void print() const override;

    // Public interface - no Vulkan types exposed
    size_t getSize() const;
    void updateData(const void* data, size_t size);

  private:
    unique_ptr<BufferResourceImpl> pBufferImpl_;
};

// Implementation classes (would be in separate .cpp files)
class ResourceImpl
{
  public:
    virtual ~ResourceImpl() = default;
    virtual void cleanup() = 0;
    virtual void print() const = 0;
};

class ImageResourceImpl : public ResourceImpl
{
  public:
    ImageResourceImpl(uint32_t width, uint32_t height) : width_(width), height_(height)
    {
        // Vulkan VkImage, VkDeviceMemory, VkImageView creation would be here
        // All Vulkan-specific code hidden in implementation
        cout << "ImageResourceImpl: Creating Vulkan image " << width << "x" << height << endl;
    }

    void cleanup() override
    {
        cout << "ImageResourceImpl: Cleaning up Vulkan image resources" << endl;
        // vkDestroyImage, vkDestroyImageView, vkFreeMemory calls here
    }

    void print() const override
    {
        cout << "Vulkan Image Resource: " << width_ << "x" << height_ << " pixels" << endl;
    }

    uint32_t getWidth() const
    {
        return width_;
    }
    uint32_t getHeight() const
    {
        return height_;
    }

    void transitionLayout()
    {
        cout << "ImageResourceImpl: Transitioning image layout using VkImageMemoryBarrier" << endl;
        // Vulkan layout transition code here
    }

  private:
    uint32_t width_, height_;
    // VkImage image_;           // Hidden Vulkan handles
    // VkDeviceMemory memory_;   // Hidden Vulkan handles
    // VkImageView imageView_;   // Hidden Vulkan handles
};

class BufferResourceImpl : public ResourceImpl
{
  public:
    BufferResourceImpl(size_t size) : size_(size)
    {
        // Vulkan VkBuffer, VkDeviceMemory creation would be here
        cout << "BufferResourceImpl: Creating Vulkan buffer of size " << size << " bytes" << endl;
    }

    void cleanup() override
    {
        cout << "BufferResourceImpl: Cleaning up Vulkan buffer resources" << endl;
        // vkDestroyBuffer, vkFreeMemory calls here
    }

    void print() const override
    {
        cout << "Vulkan Buffer Resource: " << size_ << " bytes" << endl;
    }

    size_t getSize() const
    {
        return size_;
    }

    void updateData(const void* data, size_t size)
    {
        cout << "BufferResourceImpl: Updating " << size << " bytes using vkMapMemory" << endl;
        // Vulkan memory mapping and data copy here
    }

  private:
    size_t size_;
    // VkBuffer buffer_;         // Hidden Vulkan handles
    // VkDeviceMemory memory_;   // Hidden Vulkan handles
    // void* mapped_;            // Hidden mapped memory pointer
};

// Resource class implementation
Resource::Resource() = default;
Resource::~Resource() = default;

// ImageResource class implementation
ImageResource::ImageResource(uint32_t width, uint32_t height)
    : pImageImpl_(make_unique<ImageResourceImpl>(width, height))
{
}

ImageResource::~ImageResource() = default;

void ImageResource::cleanup()
{
    if (pImageImpl_) {
        pImageImpl_->cleanup();
    }
}

void ImageResource::print() const
{
    if (pImageImpl_) {
        pImageImpl_->print();
    }
}

uint32_t ImageResource::getWidth() const
{
    return pImageImpl_ ? pImageImpl_->getWidth() : 0;
}

uint32_t ImageResource::getHeight() const
{
    return pImageImpl_ ? pImageImpl_->getHeight() : 0;
}

void ImageResource::transitionLayout()
{
    if (pImageImpl_) {
        pImageImpl_->transitionLayout();
    }
}

// BufferResource class implementation
BufferResource::BufferResource(size_t size) : pBufferImpl_(make_unique<BufferResourceImpl>(size))
{
}

BufferResource::~BufferResource() = default;

void BufferResource::cleanup()
{
    if (pBufferImpl_) {
        pBufferImpl_->cleanup();
    }
}

void BufferResource::print() const
{
    if (pBufferImpl_) {
        pBufferImpl_->print();
    }
}

size_t BufferResource::getSize() const
{
    return pBufferImpl_ ? pBufferImpl_->getSize() : 0;
}

void BufferResource::updateData(const void* data, size_t size)
{
    if (pBufferImpl_) {
        pBufferImpl_->updateData(data, size);
    }
}

// Resource factory using PImpl pattern
class ResourceFactory
{
  public:
    static unique_ptr<Resource> createImageResource(uint32_t width, uint32_t height)
    {
        return make_unique<ImageResource>(width, height);
    }

    static unique_ptr<Resource> createBufferResource(size_t size)
    {
        return make_unique<BufferResource>(size);
    }
};

int main()
{
    cout << "=== PIMPL PATTERN DEMONSTRATION ===" << endl;

    vector<unique_ptr<Data>> pimplData;

    // Use PImpl factory to create objects
    pimplData.push_back(DataFactory::createFloatData(3.14f));
    pimplData.push_back(DataFactory::createIntData(42));
    pimplData.push_back(DataFactory::createFloatData(2.71f));
    pimplData.push_back(DataFactory::createIntData(100));

    cout << "\nPImpl Pattern Results:" << endl;
    for (const auto& item : pimplData) {
        item->print();
    }

    cout << "\n=== VULKAN RESOURCE MANAGEMENT WITH PIMPL ===" << endl;

    vector<unique_ptr<Resource>> resources;

    // Create Vulkan resources using PImpl pattern
    resources.push_back(ResourceFactory::createImageResource(1024, 768));
    resources.push_back(ResourceFactory::createBufferResource(1024 * 1024));
    resources.push_back(ResourceFactory::createImageResource(512, 512));
    resources.push_back(ResourceFactory::createBufferResource(4096));

    cout << "\nVulkan Resources:" << endl;
    for (const auto& resource : resources) {
        resource->print();
    }

    // Demonstrate specific operations
    cout << "\nVulkan Resource Operations:" << endl;
    if (auto imageRes = dynamic_cast<ImageResource*>(resources[0].get())) {
        cout << "Image dimensions: " << imageRes->getWidth() << "x" << imageRes->getHeight()
             << endl;
        imageRes->transitionLayout();
    }

    if (auto bufferRes = dynamic_cast<BufferResource*>(resources[1].get())) {
        cout << "Buffer size: " << bufferRes->getSize() << " bytes" << endl;
        int data = 42;
        bufferRes->updateData(&data, sizeof(data));
    }

    /*
    ========================================================================
    PIMPL PATTERN ANALYSIS:
    ========================================================================

    ADVANTAGES:
    1. Compilation Firewall: Changes to *Impl classes don't require
       recompiling client code
    2. Binary Compatibility: Can modify implementation without breaking ABI
    3. Reduced Dependencies: Headers only need forward declarations
    4. Information Hiding: Implementation completely hidden from clients
    5. Faster Compilation: Reduced header dependencies speed up builds

    DISADVANTAGES:
    1. Performance: Extra pointer indirection on every method call
    2. Memory: Additional heap allocation for implementation objects
    3. Complexity: More classes and indirection to maintain
    4. Debug Difficulty: Implementation details harder to inspect

    WHEN TO USE PIMPL:
    - Large classes with complex implementations
    - Library interfaces that need binary stability
    - Classes with many private dependencies
    - When compilation times are problematic
    - When hiding implementation details is critical

    WHEN NOT TO USE PIMPL:
    - Small, simple classes (overhead outweighs benefits)
    - Performance-critical code paths
    - Header-only libraries
    - Classes that change infrequently

    VULKAN USE CASE:
    - Hides complex Vulkan API details from client code
    - Prevents vulkan.h inclusion in public headers
    - Allows implementation changes without recompiling clients
    - Provides clean, simple interface for complex GPU resources
    ========================================================================
    */

    return 0;
}