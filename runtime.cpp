#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        assert(data_ != nullptr);
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        assert(data_ != nullptr);
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (!object) {
            return false;
        }
        if (auto p = object.TryAs<runtime::Number>(); p && p->GetValue() != 0) {
            return true;
        }
        if (auto p = object.TryAs<runtime::String>(); p && !p->GetValue().empty()) {
            return true;
        }
        if (auto p = object.TryAs<runtime::Bool>(); p && p->GetValue()) {
            return true;
        }
        return false;
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : class_name_(std::move(name))
        , parent_(parent) {
        for (Method& method : methods) {
            if (methods_.count(method.name) > 0) {
                throw runtime_error( class_name_ + " has duplicate methods: " + method.name);
            }
            methods_[method.name] = std::move(method);
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        if (methods_.count(name) > 0) {
            return &methods_.at(name);
        }

        if (parent_) {
            return parent_->GetMethod(name);
        }

        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return class_name_;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (HasMethod("__str__", 0)) {
            Call("__str__", {}, context)->Print(os, context);
        }
        else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        auto* m = class_.GetMethod(method);
        if (!m) return false;
        if (m->formal_params.size() != argument_count) return false;
        return true;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls)
        : class_(cls) {

    }

    ObjectHolder ClassInstance::Call(const std::string& method, const std::vector<ObjectHolder>& actual_args, Context& context) {
        if (HasMethod(method, actual_args.size())) {
            auto* m = class_.GetMethod(method);

            try {
                Closure closure = { {"self", ObjectHolder::Share(*this)} };
                for (size_t i = 0; i < actual_args.size(); ++i) {
                    closure[m->formal_params[i]] = actual_args[i];
                }
                return m->body->Execute(closure, context);
            }
            catch (ObjectHolder& value) {
                return value;
            }
            return ObjectHolder::None();
        }
        throw std::runtime_error(class_.GetName() + " does not have method " + method + "or method is incorrect");
    }

    void Class::Print(ostream& os, Context&) {
        os << "Class " << class_name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"s : "False"s);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
            return std::equal_to<int>()(lhs.TryAs<runtime::Number>()->GetValue(), rhs.TryAs<runtime::Number>()->GetValue());
        }
        if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
            return std::equal_to<string>()(lhs.TryAs<runtime::String>()->GetValue(), rhs.TryAs<runtime::String>()->GetValue());
        }
        if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
            return std::equal_to<bool>()(lhs.TryAs<runtime::Bool>()->GetValue(), rhs.TryAs<runtime::Bool>()->GetValue());
        }

        auto p = lhs.TryAs<runtime::ClassInstance>();
        if ( p != nullptr && p->HasMethod("__eq__", 1)) {
            return IsTrue(p->Call("__eq__", { rhs }, context));
        }

        throw std::runtime_error("Error in compare equal");
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

        if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
            return std::less<int>()(lhs.TryAs<runtime::Number>()->GetValue(), rhs.TryAs<runtime::Number>()->GetValue());
        }
        if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
            return std::less<string>()(lhs.TryAs<runtime::String>()->GetValue(), rhs.TryAs<runtime::String>()->GetValue());
        }
        if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
            return std::less<bool>()(lhs.TryAs<runtime::Bool>()->GetValue(), rhs.TryAs<runtime::Bool>()->GetValue());
        }

        auto p = lhs.TryAs<runtime::ClassInstance>();
        if ( p != nullptr && p->HasMethod("__lt__", 1)) {
            return IsTrue(p->Call("__lt__", { rhs }, context));
        }

        throw std::runtime_error("Error in compare less");
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Greater(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime