#include <iostream>
#include <vector>
#include <memory>
#include <string>

using namespace std;

class Data
{
  public:
    virtual void print() = 0;
    virtual ~Data() = default;
};

class FloatData : public Data
{
  public:
    FloatData(float value) : f_(value)
    {
    }

    void print() override
    {
        cout << "Float: " << f_ << endl;
    }

  private:
    float f_;
};

class IntData : public Data
{
  public:
    IntData(int value) : i_(value)
    {
    }

    void print() override
    {
        cout << "Int: " << i_ << endl;
    }

  private:
    int i_;
};

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
활용 사례:
class Resource
{}

class ImageResource : public Resource
{
  ImageHandle handle_;
}

class BufferResource : public Resource
{
  BufferHandle handle_;
}
*/

int main()
{
    vector<unique_ptr<Data>> data;

    // Use factory to create objects
    data.push_back(DataFactory::createFloatData(3.14f));
    data.push_back(DataFactory::createIntData(42));
    data.push_back(DataFactory::createFloatData(2.71f));
    data.push_back(DataFactory::createIntData(100));

    // Print all data
    cout << "Factory Pattern (Polymorphic approach):" << endl;
    for (const auto& item : data) {
        item->print();
    }

    return 0;
}
