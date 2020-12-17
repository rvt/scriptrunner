#include <catch2/catch.hpp>

#include <scriptrunner.hpp>
#include <iostream>
#include "arduinostubs.hpp"
using Catch::Matchers::Equals;

using namespace rvt::scriptrunner;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&& ... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

TEST_CASE("Should Run script till end", "[scriptrunner]") {
    std::vector<Command<Context>*> commands;

    commands.push_back(new Command<Context>("cerr", [](const OptValue & value, Context & context) {
        std::cerr << value.key() << "=" << (char*)value << "\n";
        return true;
    }));

    Context context{
        "cerr=foo;"
        "cerr=bar;"};
    auto scriptRunner = new ScriptRunner<Context>(commands);


    bool returned;
    REQUIRE_THAT((const char*)context.currentLine(), Equals("foo"));
    returned = scriptRunner->handle(context);
    REQUIRE(returned == true);

    REQUIRE_THAT((const char*)context.currentLine(), Equals("bar"));
    returned = scriptRunner->handle(context);
    REQUIRE(returned == true);

    REQUIRE_THAT((const char*)context.currentLine().key(), Equals("end"));
    returned = scriptRunner->handle(context);
    REQUIRE(returned == false);

    REQUIRE_THAT((const char*)context.currentLine().key(), Equals("end"));
    returned = scriptRunner->handle(context);
    REQUIRE(returned == false);
}

TEST_CASE("Should handle an extendedContext", "[scriptrunner]") {

    class ExtendedContext : public Context {
        uint16_t m_counter;
    public:
        ExtendedContext(const char* script) : Context(script), m_counter(0)  {

        }
        uint16_t increaseAndGet() {
            return ++m_counter;
        }
        uint16_t get() {
            return m_counter;
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const OptValue & value, ExtendedContext & context) {
        std::cerr << context.increaseAndGet() << "\n";
        return true;
    }));

    ExtendedContext context{
        "count=1;"
        "count=1;"};
    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    scriptRunner->handle(context);
    REQUIRE(context.get() == 1);
    scriptRunner->handle(context);
    REQUIRE(context.get() == 2);
    // Reached the end
    bool returned = scriptRunner->handle(context);
    REQUIRE(returned == false);
    REQUIRE(context.get() == 2);
}


TEST_CASE("Should perform jump", "[scriptrunner]") {
    class ExtendedContext : public Context {
    public:
        uint16_t counter;
        ExtendedContext(const char* script) : Context(script), counter(0)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const OptValue & value, ExtendedContext & context) {
        std::cerr << context.counter++ << "\n";
        return true;
    }));

    ExtendedContext context{
        "count=1;"
        "jump=bar;"
        "count=1;"
        "count=1;"
        "count=1;"
        "label=bar;"
        "count=1;"};
    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 100; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE(context.counter == 2);
}

TEST_CASE("Should perform jump, even as first line", "[scriptrunner]") {
    class ExtendedContext : public Context {
    public:
        char* value;
        uint8_t counter = 0;
        ExtendedContext(const char* script) : Context(script), value(nullptr), counter(0)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("test", [](const OptValue & value, ExtendedContext & context) {
        context.value = (char*)value;
        context.counter++;
        std::cerr << (value.key()) << ":" << (char*)value << "\n";
        return true;
    }));

    ExtendedContext context{
        "jump=bar;"
        "test=1;"
        "test=2;"
        "test=3;"
        "label=bar;"
        "test=This one only;"
        "jump=bas;"
        "test=This not;"
        "label=bas;"
    };

    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 50; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE_THAT((const char*)context.value, Equals("This one only"));
    REQUIRE(context.counter == 1);
}

TEST_CASE("Should handle waits", "[scriptrunner]") {
    class ExtendedContext : public Context {
    public:
        char* value;
        ExtendedContext(const char* script) : Context(script), value(nullptr)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("test", [](const OptValue & value, ExtendedContext & context) {
        context.value = (char*)value;
        std::cerr << (value.key()) << ":" << (char*)value << "\n";
        return true;
    }));

    ExtendedContext context{
        "test=before;"
        "wait=50;"
        "test=after;"
    };

    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    scriptRunner->handle(context);
    scriptRunner->handle(context);
    scriptRunner->handle(context);
    REQUIRE_THAT((const char*)context.value, Equals("before"));
    millisStubbed = 51;
    scriptRunner->handle(context);
    REQUIRE_THAT((const char*)context.value, Equals("after"));
}
