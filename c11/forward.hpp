#pragma once
#include "../headers/logger.h"

#define MARCO_TYPE(type, name)                                                 \
    struct name {                                                              \
        type value_;                                                           \
        type get() const { return value_; }                                    \
        void set(const type &value) { value_ = value; }                        \
    };

template <typename T, const char *name> class NamedValue {
  public:
    explicit NamedValue(const T &value) : value_(value) {}
    T get() const { return value_; }
    void set(const T &value) { value_ = value; }

  private:
    T value_;
};

#define TEMPLATE_TYPE(type, name)                                              \
    constexpr char name##_[] = #name;                                          \
    using name = NamedValue<type, name##_>;

namespace Forward {
constexpr auto TAG{"C11"};

MARCO_TYPE(std::string, Name)
MARCO_TYPE(uint, Age)
MARCO_TYPE(std::string, Title)
MARCO_TYPE(uint, Salary)

TEMPLATE_TYPE(std::string, Department)
TEMPLATE_TYPE(std::string, Birth)

struct Worker {
  private:
    std::string name_;
    uint age_;
    std::string title_;
    uint salary_;
    std::string department_;
    std::string birth_;

  public:
    void setProperty(const Name &x) { name_ = x.get(); };
    void setProperty(const Age &x) { age_ = x.get(); };
    void setProperty(const Title &x) { title_ = x.get(); };
    void setProperty(const Salary &x) { salary_ = x.get(); };
    void setProperty(const Department &x) { department_ = x.get(); };
    void setProperty(const Birth &x) { birth_ = x.get(); };

    template <typename... Ts> auto set(Ts &&... ts) {
        std::initializer_list<int> ignore = {
            (this->setProperty(std::forward<Ts>(ts)), 0)...};
        (void)ignore;
    }
    friend std::ostream &operator<<(std::ostream &os, const Worker &n) {
        os << "{" << CR(n.name_) << CB(n.age_) << CG(n.title_) << CY(n.salary_)
           << CP(n.department_) << CC(n.birth_) << "}" << std::endl;
        return os;
    }
};

namespace Test {
void test() {
    Worker w1, w2;
    w1.set(Name{"Zhang San"}, Age{20}, Salary{2000}, Department{"Tech"},
           Birth{"2010-10-10"});
    w2.set(Title{"Manager"}, Department{"Tech"}, Birth{"1970-10-10"},
           Name{"Li Si"}, Age{60}, Salary{2000});
    ARGS(w1, w2)
}
void what() {
    std::cout
        << "\n-------------\n"
        << "完美转发是指在函数模板中使用通用引用（或称为万能引用）以及 "
           "`std::forward` "
           "函数，将参数按原有类型和值类别（左值或右值）转发给另一个函数，"
           "以达到避免不必要的值拷贝和保持参数类型和值类别不变的目的。通用"
           "引用是 C++11 中引入的一种特殊的引用类型，其语法为 "
           "`T&&`"
           "，可以同时绑定到左值和右值，因此也被称为万能引用。在函数模板中"
           "，使用通用引用作为函数参数类型，可以让函数接受任意类型的参数，"
           "并保留参数传递时的值类别（左值或右值）。"
        << "\n-------------\n";
}
} // namespace Test
} // namespace Forward