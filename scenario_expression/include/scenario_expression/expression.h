#ifndef INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H
#define INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H

#include <boost/smart_ptr/shared_ptr.hpp>
#include <functional>
#include <ios>
#include <iostream>
#include <pluginlib/class_loader.h>
#include <ros/ros.h>
#include <scenario_api/scenario_api_core.h>
#include <scenario_conditions/condition_base.h>
#include <tuple>
#include <type_traits>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace scenario_expression
{

/* -----------------------------------------------------------------------------
 *
 * EXPRESSION
 *   <Expression> = <Literal>
 *                | <Logical>
 *                | <Procedure Call>
 *                | <Sequential>
 *                | <Parallel>
 *
 * LITERAL EXPRESSION
 *   <Literal> = <Boolean> | <Number>
 *
 *   <Number> = <Double Float>
 *
 * LOGICAL EXPRESSION
 *   <Logical> = <N-Ary Logical Operator> [ <Test>* ]
 *             | <Unary Logical Operator> { <Test> }
 *
 *   <N-Ary Logical Operator> = <And> | <Or>
 *   <Unary Logical Operator> = <Not>
 *
 *   <Test> = <Expression>
 *
 * PROCEDURE CALL
 *   <Procedure Call> = <Action Call> | <Predicate Call>
 *
 * SEQUENTIAL EXPRESSION
 *   <Sequential>
 *
 * PARALLEL EXPRESSION
 *   <Parallel>
 *
 * The value of the test is Boolean, which returns whether the return value of
 * the expression is equal to false or not. Note that the return value of the
 * expression is not necessarily Boolean.
 *
 * -------------------------------------------------------------------------- */

template <typename T>
class Literal;

using Boolean = Literal<bool>;

class And;
class Or;

template <typename PluginBase>
class Procedure;

class Predicate;
// class Action;

class Expression
{
  friend Boolean;

  friend Predicate;
  // friend Action;

  friend And;
  friend Or;

public:
  Expression()
    : data { nullptr }
    , reference_count { 0 }
  {}

  Expression(const Expression& expression)
    : data { expression.data }
    , reference_count { 0 }
  {
    if (data)
    {
      ++(data->reference_count);
    }
  }

  virtual ~Expression()
  {
    if (data && --(data->reference_count) == 0)
    {
      delete data;
    }
  }

  Expression& operator =(const Expression& rhs)
  {
    Expression e { rhs };
    swap(e);
    return *this;
  }

  virtual Expression evaluate()
  {
    return data ? data->evaluate() : *this;
  }

  template <typename T, typename... Ts>
  static auto make(Ts&&... xs)
  {
    Expression e {};
    e.remake(new T { std::forward<decltype(xs)>(xs)... });
    return e;
  }

  void swap(Expression& e) noexcept
  {
    std::swap(data, e.data);
  }

  static void define(const std::shared_ptr<ScenarioAPI>& api)
  {
    Expression::api = api;
  }

  virtual std::ostream& write(std::ostream& os) const
  {
    return os << "Nil";
  }

  friend std::ostream& operator <<(std::ostream& os, const Expression& expression)
  {
    return expression.data ? expression.data->write(os) : (os << "()");
  }

  virtual operator bool() const noexcept
  {
    return data ? static_cast<bool>(*data) : true;
  }

protected:
  Expression(std::integral_constant<decltype(0), 0>)
    : data { nullptr }
    , reference_count { 1 }
  {}

  static std::shared_ptr<ScenarioAPI> api;

private:
  void remake(Expression* e)
  {
    if (data && --(data->reference_count) == 0)
    {
      delete data;
    }

    data = e;
  }

  Expression* data;

  std::size_t reference_count;
};

std::shared_ptr<ScenarioAPI> Expression::api = nullptr;

template <typename... Ts>
decltype(auto) define(Ts&&... xs)
{
  return Expression::define(std::forward<decltype(xs)>(xs)...);
}

Expression read(const YAML::Node&);

template <typename T>
class Literal
  : public Expression
{
  friend Expression;

  using value_type = T;

  value_type value;

protected:
  Literal(const value_type& value)
    : Expression { std::integral_constant<decltype(0), 0>() }
    , value { value }
  {}

  Literal(const Literal& rhs)
    : Expression { std::integral_constant<decltype(0), 0>() }
    , value { rhs.value }
  {}

  Literal(const YAML::Node& node)
    : Expression { std::integral_constant<decltype(0), 0>() }
  {
  }

  virtual ~Literal() = default;

  std::ostream& write(std::ostream& os) const override
  {
    return os << std::boolalpha << value;
  }

  Expression evaluate() override
  {
    return *this;
  }

  operator bool() const noexcept override
  {
    return value;
  }
};

#define DEFINE_N_ARY_LOGICAL_EXPRESSION(NAME, OPERATOR)                        \
class NAME                                                                     \
  : public Expression                                                          \
{                                                                              \
  friend Expression;                                                           \
                                                                               \
  OPERATOR<Boolean> compare;                                                   \
                                                                               \
  std::vector<Expression> operands;                                            \
                                                                               \
protected:                                                                     \
  NAME(const NAME& rhs)                                                        \
    : Expression { std::integral_constant<decltype(0), 0>() }                  \
    , operands { rhs.operands }                                                \
  {}                                                                           \
                                                                               \
  NAME(const YAML::Node& node)                                                 \
    : Expression { std::integral_constant<decltype(0), 0>() }                  \
  {                                                                            \
    if (node.IsSequence())                                                     \
    {                                                                          \
      for (const auto& each : node)                                            \
      {                                                                        \
        std::cout << " ";                                                      \
        operands.push_back(read(each));                                        \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual ~NAME() = default;                                                   \
                                                                               \
  Expression evaluate() override                                               \
  {                                                                            \
    return Expression::make<Boolean>(true);                                    \
  }                                                                            \
                                                                               \
  std::ostream& write(std::ostream& os) const override                         \
  {                                                                            \
    os << "(" #NAME;                                                           \
                                                                               \
    for (const auto& each : operands)                                          \
    {                                                                          \
      os << " " << each;                                                       \
    }                                                                          \
                                                                               \
    return os << ")";                                                          \
  }                                                                            \
}

DEFINE_N_ARY_LOGICAL_EXPRESSION(And, std::logical_and);
DEFINE_N_ARY_LOGICAL_EXPRESSION(Or, std::logical_or);

template <typename PluginBase>
class Procedure
  : public Expression
{
  friend Expression;

protected:
  using plugin_type = boost::shared_ptr<PluginBase>;

  plugin_type plugin;

  Procedure()
    : Expression { std::integral_constant<decltype(0), 0>() }
  {}

  Procedure(const Procedure& proc)
    : Expression { std::integral_constant<decltype(0), 0>() }
    , plugin { proc.plugin }
  {}

  std::ostream& write(std::ostream& os) const override
  {
    return os << "(" << (plugin ? plugin->getType() : "Error") << ")";
  }

  virtual pluginlib::ClassLoader<PluginBase>& loader() const = 0;

  auto load(const std::string& name)
  {
    for (const auto& declaration : loader().getDeclaredClasses())
    {
      if (loader().getName(declaration) == name)
      {
        return loader().createInstance(declaration);
      }
    }

    ROS_ERROR_STREAM(__FILE__ << ":" << __LINE__ << ": Failed to load Procedure " << name);
    return boost::shared_ptr<PluginBase>(nullptr);
  }
};

class Predicate
  : public Procedure<scenario_conditions::ConditionBase>
{
  friend Expression;

protected:
  using Procedure::Procedure;

  Predicate(const YAML::Node& node)
    : Procedure {}
  {
    if (plugin = read(node))
    {
      ROS_ERROR_STREAM("Configure " << node["Type"] << " => " << std::boolalpha << plugin->configure(node, api));
    }
  }

  plugin_type read(const YAML::Node& node)
  {
    if (const auto type { node["Type"] })
    {
      return load(type.as<std::string>() + "Condition");
    }
    else
    {
      return { nullptr };
    }
  }

  pluginlib::ClassLoader<scenario_conditions::ConditionBase>& loader() const
  {
    static pluginlib::ClassLoader<scenario_conditions::ConditionBase> loader {
      "scenario_conditions", "scenario_conditions::ConditionBase"
    };
    return loader;
  }

  Expression evaluate() override
  {
    return Expression::make<Boolean>(false);
  }
};

// TODO MOVE INTO Expression's CONSTRUCTOR!
Expression read(const YAML::Node& node)
{
  if (node.IsScalar())
  {
    ROS_ERROR_STREAM("IsScalar " << node);
  }
  else if (node.IsSequence())
  {
    ROS_ERROR_STREAM("IsSequence " << node);
  }
  else if (node.IsMap()) // NOTE: is keyword
  {
    if (const auto node_and { node["All"] }) // NOTE: should be 'and'
    {
      return Expression::make<And>(node_and);
    }
    else if (const auto node_or { node["Any"] }) // NOTE: should be 'or'
    {
      return Expression::make<Or>(node_or);
    }
    else if (const auto node_type { node["Type"] }) // <procedure call>
    {
      if (const auto node_params { node["Params"] }) // <action call>
      {
        // std::cout << "\e[1;31m(change " << node_type.as<std::string>() << ")";
      }
      else // <predicate call>
      {
        // std::cout << "\e[1;31m(if " << node_type.as<std::string>() << ")";
        return Expression::make<Predicate>(node);
      }
    }
    else
    {
      ROS_ERROR_STREAM("ERROR!");
    }
  }
  else
  {
    return {};
  }
}

} // namespace scenario_expression

namespace std
{

template <>
void swap(scenario_expression::Expression& lhs,
          scenario_expression::Expression& rhs)
{
  lhs.swap(rhs);
}

} // namespace std

#endif // INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H
