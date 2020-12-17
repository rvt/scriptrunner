#pragma once
#include <stdint.h>
#include <functional>
#include <vector>

#include <optparser.hpp>

#ifndef UNIT_TEST
#include <Arduino.h>
#else
extern "C" uint32_t millis();
#endif

namespace rvt {

namespace scriptrunner {

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&& ... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}


class Context {
    typedef std::unique_ptr<OptValue> OptValuePtr;
    uint32_t m_currentTime;
    std::vector<OptValuePtr>::iterator m_currentLine;
    std::vector<OptValuePtr> m_script;
    char* m_scriptText;

    unsigned long m_requestedStart;
public:
    Context(const char* script) : m_requestedStart{0} {
        m_scriptText = strdup(script);

        OptParser::get(m_scriptText, ';', [this](OptValue f) {
            m_script.push_back(make_unique<OptValue>(f));
        });
        m_script.push_back(make_unique<OptValue>(m_script.size(), "end", ""));
        m_currentLine = m_script.begin();
    }

    Context(std::vector<OptValuePtr> p_script) : m_scriptText(nullptr)  {
        m_script = std::move(p_script);
        m_currentLine = m_script.begin();
    }

    virtual ~Context() {
        if (m_scriptText != nullptr) {
            delete m_scriptText;
        }
    }

    const OptValue& currentLine() const {
        return *(*m_currentLine).get();
    }

    /**
     * Wait a number of milli seconds
     * return true if the waiting is over, returns false if we should not advance to the next line
     */
    bool wait(unsigned long currentMillis, unsigned long millisToWait) {
        if (m_requestedStart) {
            if (currentMillis - m_requestedStart > millisToWait) {
                m_requestedStart = 0;
                return true;
            }
        } else {
            m_requestedStart = currentMillis == 0 ? 1 : currentMillis;
        }

        return false;
    }

    bool jump(const char* labelName) {
        std::vector<OptValuePtr>::iterator line = m_script.begin();

        while (
            line != m_script.end() &&
            (strcmp((*line).get()->key(), "label") != 0 ||
             strcmp((char*)(*(*line).get()), labelName) != 0)) {
            line++;
        };

        if (line != m_script.end()) {
            m_currentLine = line;
            return true;
        }

        return false;
    }

    /**
     * Advance to the next line
     * as long as the script is running, we return true
     */
    bool advance() {
        const OptValue& current = currentLine();

        if (strcmp(current.key(), "end") == 0) {
            return false;
        } else if (strcmp(current.key(), "jump") == 0) {
            jump(current);
        } else if (strcmp(current.key(), "wait") == 0) {
            if (wait(millis(), (int32_t)current)) {
                m_currentLine++;
            }
        } else {
            m_currentLine++;
        }

        return true;
    }
};

/**
 * Simpel state that gets run each time the StateMachine reaches this state
 */
template<typename ContextType>
class Command {
    //    friend class ScriptRunner;
public:
    typedef std::function<bool (const OptValue&, ContextType& context)> TRunFunction;

    TRunFunction m_run;
    const char* m_command;

private:

public:

    Command(const char* p_command, const TRunFunction& p_run) :
        m_command(p_command),
        m_run(p_run) {
    }

    ~Command() {
    }

    bool canExecute(const OptValue& execLine) const {
        return strcmp(execLine.key(), m_command) == 0;
    }

    virtual bool execute(const OptValue& execLine, ContextType& context) {
        return m_run(execLine, context);
    }

};

/**
 * StateMachine itself that will run through all states
 */
template<typename ContextType>
class ScriptRunner {
private:
    typedef Command<ContextType>* CommandContextPtr;
    std::vector<CommandContextPtr> m_commands;

public:
    ScriptRunner(std::vector<CommandContextPtr> p_commands) :
        m_commands(p_commands)  {
    }

    virtual ~ScriptRunner() {
    }

    /**
     * Keep running the script
     * Run's true as long as the script is still running
     */
    bool handle(ContextType& context) {
        const OptValue& currentLineValue = context.currentLine();

        bool advance = false;
        bool hasRan = false;

        for (auto value : m_commands) {
            if (value->canExecute(currentLineValue)) {
                advance = value->execute(currentLineValue, context);
                hasRan = true;
            }
        }

        if (advance || !hasRan) {
            return context.advance();
        } else {
            return true;
        }
    }

};
}
}

