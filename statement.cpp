#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;

    }  // namespace


    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        : var_(std::move(var))
        , rv_(std::move(rv)) {
    }

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        return closure[var_] = rv_->Execute(closure, context);
    }

    VariableValue::VariableValue(const std::string& var_name)
        : dotted_ids_{ var_name } {
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context&) {
        auto* current_closure = &closure;

        for (size_t i = 0; i + 1 < dotted_ids_.size(); ++i) {
            if (current_closure->count(dotted_ids_[i]) > 0) {
                auto p = current_closure->at(dotted_ids_[i]).TryAs<runtime::ClassInstance>();
                if (p != nullptr) {
                    current_closure = &p->Fields();
                    continue;
                }
                throw std::runtime_error(dotted_ids_[i] + "can't access fields");
            }
            throw std::runtime_error(dotted_ids_[i] + " not found");
        }

        if (current_closure->count(dotted_ids_.back()) > 0) {
            return current_closure->at(dotted_ids_.back());
        }

        throw std::runtime_error(dotted_ids_.back() + " not found in closure");
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        ObjectHolder holder = object_.Execute(closure, context);
        auto* p = holder.TryAs<runtime::ClassInstance>();
        if (p != nullptr) {
            return p->Fields()[field_name_] = rv_->Execute(closure, context);
        }
        return ObjectHolder::None();
    }

    NewInstance::NewInstance(const runtime::Class& class_type)
        : class_(class_type) {
    }

    NewInstance::NewInstance(const runtime::Class& class_type, std::vector<std::unique_ptr<Statement>> args)
        : class_(class_type)
        , args_(std::move(args)) {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        if (class_.HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> current_args;
            for (const auto& arg : args_) {
                current_args.push_back(arg->Execute(closure, context));
            }
            class_.Call(INIT_METHOD, current_args, context);
        }
        return ObjectHolder::Share(class_);
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args)
        : args_(std::move(args)) {
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        bool first = true;
        ostream& out = context.GetOutputStream();
        for (const auto& arg : args_) {
            if (!first) out << ' ';
            first = false;

            if (ObjectHolder holder = arg->Execute(closure, context)) {
                holder->Print(out, context);
            }
            else {
                out << "None";
            }
        }
        out << '\n';
        return ObjectHolder::None();
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object))
        , method_(std::move(method))
        , args_(std::move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<ObjectHolder> current_args;
        for (auto& arg : args_) {
            current_args.push_back(arg->Execute(closure, context));
        }

        ObjectHolder holder = object_->Execute(closure, context);
        auto instance = holder.TryAs<runtime::ClassInstance>();
        if (instance != nullptr) {
            return instance->Call(method_, current_args, context);
        }
        return ObjectHolder::None();
    }


    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ObjectHolder holder = argument_->Execute(closure, context);

        std::string result = "None";
        if (holder) {
            std::ostringstream out;
            holder->Print(out, context);
            result = out.str();
        }

        return ObjectHolder::Own(runtime::String(result));
    }



    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
            auto result = lhs.TryAs<runtime::Number>()->GetValue() + rhs.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number(result));
        }

        if (lhs.TryAs<runtime::String>() != nullptr && rhs.TryAs<runtime::String>() != nullptr) {
            auto result = lhs.TryAs<runtime::String>()->GetValue() + rhs.TryAs<runtime::String>()->GetValue();
            return ObjectHolder::Own(runtime::String(result));
        }

        if (auto pointer = lhs.TryAs<runtime::ClassInstance>()) {
            return pointer->Call(ADD_METHOD, { rhs }, context);
        }

        throw std::runtime_error("Error in add"s);
    }


    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
            auto result = lhs.TryAs<runtime::Number>()->GetValue() - rhs.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number( result ));
        }
        throw std::runtime_error("Error in sub"s);
       
    }


    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
            auto result = lhs.TryAs<runtime::Number>()->GetValue() * rhs.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number( result ));  
        }
        
        throw std::runtime_error("Error in mult"s);
      
    }



    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
            auto result = lhs.TryAs<runtime::Number>()->GetValue() / rhs.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number( result ));
        }
        else if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>()->GetValue() == 0) {
            throw std::runtime_error("Division by zero");
        }

        throw std::runtime_error("Error in division"s); 
    }



    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (auto& state : statements_) {
            state->Execute(closure, context);
        }
        return ObjectHolder::None();
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {

        if (
            runtime::IsTrue(lhs_->Execute(closure, context))
            || runtime::IsTrue(rhs_->Execute(closure, context))
            )
        {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        return ObjectHolder::Own(runtime::Bool(false));
    }


    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (
            runtime::IsTrue(lhs_->Execute(closure, context))
            && runtime::IsTrue(rhs_->Execute(closure, context))
            )
        {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        auto result = IsTrue(argument_->Execute(closure, context));
        return ObjectHolder::Own(runtime::Bool(!result));
    }



    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , comparator_(std::move(cmp)) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
         return ObjectHolder::Own(runtime::Bool(
             comparator_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context)
         ));  
    }



    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw ReturnException(statement_->Execute(closure, context));
    }


    MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
        : body_(std::move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
        }
        catch (ReturnException& ret) {
            return ret.GetResult();
        }
        return ObjectHolder::None();
    }

 

    ClassDefinition::ClassDefinition(ObjectHolder cls)
        : cls_(std::move(cls))
        , class_name_(cls_.TryAs<runtime::Class>()->GetName())
    {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context&) {
        closure[class_name_] = cls_;
        return ObjectHolder::None();
    }



    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        }
        else if (else_body_) {
            return else_body_->Execute(closure, context);
        }
        return ObjectHolder::None();
    }

}  // namespace ast