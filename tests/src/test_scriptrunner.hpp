#include <catch2/catch.hpp>

#include <scriptrunner.hpp>
#include <iostream>
#include "arduinostubs.hpp"
using Catch::Matchers::Equals;

using namespace rvt::scriptrunner;

typedef PlainTextContext<512> PlainTextContext512;

TEST_CASE("Can run in fastForeward mode escape wait", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t m_counter = 0;
        ExtendedContext(const char* script) : PlainTextContext512(script), m_counter(0)  {

        }

    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
        context.m_counter++;
        std::cerr << context.m_counter << "\n";
        REQUIRE(context.m_counter < 20);
        return context.m_counter < 4;
    }));

    ExtendedContext context{
        "label=start;"
        "count=1;"
        "count=1;"
        "wait=1;"
        "count=1;"
        "jump=start;"
        "count=1;"};
    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);
    scriptRunner->handle(context, true);
    REQUIRE(context.m_counter == 2);
    scriptRunner->handle(context, true);
    REQUIRE(context.m_counter == 2);
    millisStubbed = 2;
    scriptRunner->handle(context, true);
    REQUIRE(context.m_counter == 4);
}

TEST_CASE("Should advance to next line if jump location is not found", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t counter;
        ExtendedContext(const char* script) : PlainTextContext512(script), counter(0)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
        std::cerr << context.counter++ << "\n";
        return true;
    }));

    ExtendedContext context{
        "count=1;"
        "jump=bar;"
        "count=1;"
        "count=1;"
    };
    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 100; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE(context.counter == 3);
}

TEST_CASE("Should Run script till end", "[scriptrunner]") {
    std::vector<Command<Context>*> commands;

    commands.push_back(new Command<Context>("cerr", [](const char* value, Context & context) {
        std::cerr << "cerr" << "=" << value << "\n";
        return true;
    }));

    PlainTextContext<32> context {
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

    returned = scriptRunner->handle(context);
    REQUIRE(returned == false);

    returned = scriptRunner->handle(context);
    REQUIRE(returned == false);
}

TEST_CASE("Should advance to next line with unknown commands", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        const char* value;
        uint8_t counter = 0;
        ExtendedContext(const char* script) : PlainTextContext512(script), value(nullptr), counter(0)  {
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("test", [](const char* value, ExtendedContext & context) {
        context.value = value;
        context.counter++;
        std::cerr << "test" << ":" << value << "\n";
        return true;
    }));

    ExtendedContext context{
        "test=bar;"
        "unknown=1;"
        "test=bas;"
    };

    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 10; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE_THAT((const char*)context.value, Equals("bas"));
    REQUIRE(context.counter == 2);
}

TEST_CASE("Should handle an extendedContext", "[scriptrunner]") {

    class ExtendedContext : public PlainTextContext512 {
        uint16_t m_counter;
    public:
        ExtendedContext(const char* script) : PlainTextContext512(script), m_counter(0)  {

        }
        uint16_t increaseAndGet() {
            return ++m_counter;
        }
        uint16_t get() {
            return m_counter;
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
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
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t counter;
        ExtendedContext(const char* script) : PlainTextContext512(script), counter(0)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
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
    class ExtendedContext : public PlainTextContext512 {
    public:
        const char* value;
        uint8_t counter = 0;
        ExtendedContext(const char* script) : PlainTextContext512(script), value(nullptr), counter(0)  {

        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("test", [](const char* value, ExtendedContext & context) {
        context.value = value;
        context.counter++;
        std::cerr << "test" << ":" << value << "\n";
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
    millisStubbed = 0;
    class ExtendedContext : public PlainTextContext512 {
    public:
        const char* value;
        uint8_t counter = 0;
        ExtendedContext(const char* script) : PlainTextContext512(script), value(nullptr)  {
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("test", [](const char* value, ExtendedContext & context) {
        context.value = value;
        context.counter++;
        std::cerr << "test" << ":" << value << "\n";
        return true;
    }));

    ExtendedContext context{
        "test=before;"
        "wait=50;"
        "test=after1;"
        "test=after2;"
    };

    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    scriptRunner->handle(context);
    //REQUIRE(context.advanced() == true);
    scriptRunner->handle(context);
    //REQUIRE(context.advanced() == false);
    scriptRunner->handle(context);
    //REQUIRE(context.advanced() == false);
    REQUIRE_THAT((const char*)context.value, Equals("before"));
    millisStubbed = 51;
    scriptRunner->handle(context);
    scriptRunner->handle(context);
    //REQUIRE(context.advanced() == true);
    REQUIRE_THAT((const char*)context.value, Equals("after2"));
    REQUIRE(context.counter == 3);
}

TEST_CASE("Should Use cached runner", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t counter;
        ExtendedContext(const char* script) : PlainTextContext512(script), counter(0)  {
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
        std::cerr << context.counter++ << "\n";
        std::cerr << value << "\n";
        return true;
    }));
    commands.push_back(new Command<ExtendedContext>("uncount", [](const char* value, ExtendedContext & context) {
        std::cerr << context.counter++ << "\n";
        return true;
    }));

    ExtendedContext context{
        "count=1;"
        "count=2;"
        "count=3;"
        ""
        "uncount=4;"
        "count=5;"
        "count=6;"};
    auto scriptRunner = new CachedScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 10; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE(context.counter == 6);
    REQUIRE(scriptRunner->cacheSize() == 2);
}

TEST_CASE("Should not advance when not requested", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t counter;
        ExtendedContext(const char* script) : PlainTextContext512(script), counter(0)  {
        }
    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
        std::cerr << context.counter++ << "\n";
        return context.counter >= 10;
    }));

    ExtendedContext context{
        "count=1;"
    };
    auto scriptRunner = new CachedScriptRunner<ExtendedContext>(commands);

    for (int i = 0; i < 20; i++) {
        scriptRunner->handle(context);
    }

    REQUIRE(context.counter == 10);
}

TEST_CASE("Can run in fastForeward mode", "[scriptrunner]") {
    class ExtendedContext : public PlainTextContext512 {
    public:
        uint16_t m_counter;
        ExtendedContext(const char* script) : PlainTextContext512(script), m_counter(0)  {

        }

    };

    std::vector<Command<ExtendedContext>*> commands;
    commands.push_back(new Command<ExtendedContext>("count", [](const char* value, ExtendedContext & context) {
        context.m_counter++;
        return true;
    }));

    ExtendedContext context{
        "label=start;"
        "count=1;"
        "count=1;"
        "count=1;"
        "jump=start;"
        "count=1;"};
    auto scriptRunner = new ScriptRunner<ExtendedContext>(commands);

    scriptRunner->handle(context, true);
    REQUIRE(context.m_counter == 3);
    scriptRunner->handle(context, true);
    REQUIRE(context.m_counter == 6);
}

