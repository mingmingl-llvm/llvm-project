class Base {
 public:
  virtual int func1(int a, int b) = 0;
  virtual int func2(int a, int b) = 0;
  virtual ~Base() = default;
};

class Derived1 : public Base {
    public:
    ~Derived1() override = default;
    
    int func1(int a, int b) override;

    int func2(int a, int b) override;
};

class Derived2 : public Base {
public:
  ~Derived2() override = default;
  int func1(int a, int b) override;
  int func2(int a, int b) override;
};

__attribute__((noinline)) Base* createType(int a);

