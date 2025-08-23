#include <iostream>
#include <memory>
#include <vector>

using namespace std;

/*
 * RAII, or Resource Acquisition Is Initialization, is a programming idiom primarily used in C++ to
 * manage resources reliably. It dictates that resource acquisition should occur during object
 * initialization (e.g., in a constructor), and resource release should happen during object
 * destruction (e.g., in a destructor).
 */

class A
{
  public:
    A()
    {
        data_ = new int[1];
        data_[0] = 1;
    }

    ~A()
    {
        delete[] data_;
    }

  private:
    int* data_ = nullptr; // A에서만 생성/삭제 가능
};

class B1
{
  public:
    // B1()
    //{
    //     // A의 객체가 실제로 만들어지기 전에 초기화 가능
    // }

    B1(A* a) : a_(a)
    {
        // a가 nullptr인지 검토
    }

    ~B1()
    {
        // 여기서 a_를 삭제 해야 하나?
    }

    void setA(A* a)
    {
        a_ = a; // 주의가 많이 필요
    }

  private:
    A* a_ = nullptr;
};

class B2
{
  public:
    B2(A& a) : a_(a)
    {
        // A의 객체 없이는 초기화 불가능
    }

    ~B2()
    {
        // a_에 대해서 고민할 필요가 없음
    }

  private:
    A& a_;
};

class B3
{
  public:
    B3()
    {
    }

    B3(shared_ptr<A> a) : a_(a)
    {
        // 내부적으로 레퍼런스 카운트 증가
    }

    B3(const A& a) : a_(make_shared<A>(a))
    {
        // 일관성이 떨어짐
    }

    void setA(shared_ptr<A> a)
    {
        a_ = a;
    }

    ~B3()
    {
        // a_를 삭제할지에 대해 고민할 필요가 없음
        // (누군가가 해결하겠지)
    }

  private:
    shared_ptr<A> a_; // a_.get()으로 간접적으로 사용
};

class Context
{
  public:
  private:
    uint32_t device_; // device 생성과 해제는 Context 담당
};

class Resource1
{
  public:
    ~Resource1()
    {
        // resource_가 제대로 삭제되었나 확인
    }

    void cleanup(uint32_t device)
    {
        // device를 이용해서 resource_ 삭제
    }

  private:
    // device_를 스스로 갖고 있지 않은 경우
    uint32_t resource_;
};

class Resource2
{
  public:
  private:
    uint32_t device_; // <- 사본을 갖고 있는 경우
};

class Resource3
{
  public:
  private:
    uint32_t& device_; // <- 참조를 갖고 있는 경우
};

class Resource4
{
  public:
  private:
    uint32_t* device_; // <- 포인터를 갖고 있는 경우
};

int main()
{
    A a;

    B1 b1(nullptr); // <- &a 대신 nullptr를 넣을 수도 있음
    B2 b2(a);       // <- a 없이는 생성 불가
}

/*
비유:
- A가 Context 클래스
- B가 Resource 클래스
- B가 스스로 자원을 해제하려면 A(또는 A의 일부)에 접근할 수 있어야 함
*/
