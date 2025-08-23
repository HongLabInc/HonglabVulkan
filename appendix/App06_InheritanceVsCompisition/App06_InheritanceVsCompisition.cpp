#include <iostream>
#include <string>

using namespace std;

/*
===============================================================================
                    INHERITANCE VS COMPOSITION COMPARISON TABLE
===============================================================================
| Aspect              | Inheritance (IS-A)        | Composition (HAS-A)       |
|---------------------|----------------------------|---------------------------|
| Relationship        | Car IS-A Vehicle           | Car2 HAS-A Vehicle        |
| Coupling            | Tight coupling             | Loose coupling            |
| Flexibility         | Less flexible              | More flexible             |
| Code Reuse          | Through base class         | Through delegation        |
| Polymorphism        | Natural support            | Requires forwarding       |
| Memory Layout       | Single object              | Multiple objects          |
| Access to Members   | Direct access (protected)  | Through public interface  |
| Runtime Changes     | Fixed at compile time      | Can change at runtime     |
| Multiple Relations  | Single inheritance only    | Multiple compositions     |
| Dependency          | Depends on base class      | Depends on interface      |
| When to Use         | True "is-a" relationship   | "has-a" or "uses-a"       |
| Example             | Car is a Vehicle           | Car has an Engine         |
===============================================================================
*/

// ========== INHERITANCE EXAMPLE ==========
// Base class
class Vehicle
{
  public:
    Vehicle(const string& brand, int year) : brand(brand), year(year)
    {
    }

    virtual void start()
    {
        cout << brand << " (" << year << ") is starting...\n";
    }

    virtual void displayInfo()
    {
        cout << "Vehicle: " << brand << " (" << year << ")\n";
    }

    virtual ~Vehicle() = default;

  protected:
    string brand;
    int year;
};

// Derived class using inheritance
class Car : public Vehicle
{
  public:
    Car(const string& brand, int year, int doors) : Vehicle(brand, year), doors(doors)
    {
    }

    void start() override
    {
        cout << "Car " << brand << " with " << doors << " doors is starting with ignition key...\n";
    }

    void displayInfo() override
    {
        cout << "Car: " << brand << " (" << year << ") with " << doors << " doors\n";
    }

  private:
    int doors;
};

// ========== COMPOSITION EXAMPLE ==========
// Car2 class using composition instead of inheritance
class Car2
{
  public:
    Car2(const string& brand, int year, int doors) : vehicle(brand, year), doors(doors)
    {
    }

    void start()
    {
        cout << "Car2 with " << doors << " doors is starting with ignition key...\n";
        vehicle.start(); // Delegate to the composed Vehicle object
    }

    void displayInfo()
    {
        cout << "Car2: ";
        vehicle.displayInfo(); // Delegate to the composed Vehicle object
        cout << "  Doors: " << doors << "\n";
    }

  private:
    Vehicle vehicle; // Has-a relationship instead of is-a
    int doors;
};

int main()
{
    cout << "=== INHERITANCE EXAMPLE ===\n";
    cout << "Car 'is-a' Vehicle (inheritance relationship)\n\n";

    Car myCar("Toyota", 2023, 4);
    myCar.displayInfo();
    myCar.start();

    cout << "\n=== COMPOSITION EXAMPLE ===\n";
    cout << "Car2 'has-a' Vehicle (composition relationship)\n\n";

    Car2 myCar2("Honda", 2023, 2);
    myCar2.displayInfo();
    myCar2.start();

    cout << "\n=== KEY DIFFERENCES ===\n";
    cout << "Inheritance (Car): IS-A Vehicle - stronger coupling, polymorphism\n";
    cout << "Composition (Car2): HAS-A Vehicle - looser coupling, more flexible\n";

    return 0;
}
