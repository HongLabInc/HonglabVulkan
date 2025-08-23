#include <iostream>
#include <vector>

using namespace std;

class ResourcePool
{
  public:
    ResourcePool()
    {
        resources_.reserve(10); // <- 메모리 미리 배정
    }

    ~ResourcePool()
    {
        // 여기서는 별도 해제 불필요
    }

    uint32_t allocate(float f)
    {
        resources_.push_back(f);
        return uint32_t(resources_.size() - 1);
    }

    float get(uint32_t handle)
    {
        return resources_[handle];
    }

  private:
    vector<float> resources_; // <- 모든 리소스를 한 곳에서 생성/삭제
};

class Resource
{
  public:
    Resource(uint32_t handle) : handle_(handle)
    {
    }

    uint32_t handle()
    {
        return handle_;
    }

  private:
    uint32_t handle_;
};

int main()
{
    ResourcePool pool;

    Resource res(pool.allocate(3.14f));

    cout << pool.get(res.handle()) << endl;

    return 0;
}
