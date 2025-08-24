#include <iostream>
#include <vector>

using namespace std;

class A
{
};

class B
{
  public:
    B(A& a) : a_(a)
    {
        // data_ 초기화
    }

    // Move constructor
    B(B&& other) noexcept : a_(other.a_), data_(std::move(other.data_))
    {
        cout << "B move constructor called" << endl;
        // Reference a_ points to the same object as other.a_
        // data_ is moved from other.data_ (other.data_ becomes empty)
    }

    // Copy constructor
    // B(const B& other) : a_(other.a_) // , data_(other.data_)
    //{
    //    cout << "B copy constructor called" << endl;
    //    // Reference a_ points to the same object as other.a_
    //    // data_ is copied from other.data_
    //}

    B(const B&) = delete;

    // Delete move assignment operator
    B& operator=(B&&) = delete;

    // Delete copy operations
    // B(const B&) = delete;
    B& operator=(const B&) = delete;

  private:
    A& a_;
    vector<float> data_; // 복사되면 안되는 데이터
};

int main()
{
    A a;

    // vector<B> Bs;
    // Bs.emplace_back(a);

    // vector<B> Bs(10, a);

    vector<B> Bs;
    Bs.reserve(10);
    for (int i = 0; i < 10; ++i) {
        Bs.emplace_back(a); // Construct each B object in-place
    }

    // 대안
    // vector<unique_ptr<B>> Bs;
}
