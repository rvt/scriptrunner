#pragma once
#include <string.h>
#include <stdint.h>
#include <functional>
#include <vector>
#include <memory>
#include <limits>

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
public:
    typedef std::unique_ptr<OptValue> OptValuePtr;
private:
    std::vector<OptValuePtr>::iterator m_currentLine;
    std::vector<OptValuePtr> m_script;
    unsigned long m_requestedStartMillis;
    bool m_advanced;
public:
    Context(std::vector<OptValuePtr> p_script) : m_script(std::move(p_script)), m_requestedStartMillis(0),m_advanced(false) {
        m_script.push_back(make_unique<OptValue>(m_script.size(), "end", ""));
        m_currentLine = m_script.begin();
    }

    Context() : m_requestedStartMillis(0) {
    }

    void setScript(std::vector<OptValuePtr> p_script) {
        m_script = std::move(p_script);
        m_script.push_back(make_unique<OptValue>(m_script.size(), "end", ""));
        m_currentLine = m_script.begin();
        m_requestedStartMillis = 0;
    }

    const OptValue& currentLine() const {
        return *(*m_currentLine).get();
    }

    /**
     * Wait a number of milli seconds
     * return true if the waiting is over, returns false if we should not advance to the next line
     */
    bool wait(unsigned long currentMillis, unsigned long millisToWait) {
        if (m_requestedStartMillis) {
            if (currentMillis - m_requestedStartMillis > millisToWait) {
                m_requestedStartMillis = 0;
                return true;
            }
        } else {
            m_requestedStartMillis = currentMillis == 0 ? 0xFFFFFFFFUL : currentMillis;
        }

        return false;
    }

    bool jump(const char* labelName) {
        std::vector<OptValuePtr>::iterator line = m_script.begin();
        m_advanced=false;
        while (
            line != m_script.end() &&
            (strcmp((*line).get()->key(), "label") != 0 ||
             strcmp((char*)(*(*line).get()), labelName) != 0)) {
            line++;
        };

        if (line != m_script.end()) {
            m_currentLine = line;
            m_advanced=true;
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
        m_advanced=false;
        if (current.isKey("end")) {
            return false;
        } else if (current.isKey("jump")) {
            jump(current);
        } else if (current.isKey("label")) {
            m_currentLine++;
            m_advanced=true;
        } else if (current.isKey("wait")) {
            if (wait(millis(), (int32_t)current)) {
                m_currentLine++;
                m_advanced=true;
            }
        } else {
            m_currentLine++;
            m_advanced=true;
        }

        return true;
    }

    bool isAdvanced() const {
        return m_advanced;
    }
    void isAdvanced(bool advanced)  {
        m_advanced=advanced;
    }

};

template<uint16_t ScriptSize>
class PlainTextContext : public Context {
    char m_scriptText[ScriptSize] {};
public:
    // Not happy with this contraption yet.
    // I would like to copy the character array AND of the option parser point to the right data
    PlainTextContext(const char* script) : Context() {
        strncpy(m_scriptText, script, ScriptSize);
        std::vector<Context::OptValuePtr> scriptOpts;
        OptParser::get(m_scriptText, ';', [&scriptOpts](OptValue f) {
            scriptOpts.push_back(make_unique<OptValue>(f));
        });
        setScript(std::move(scriptOpts));
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
        m_run(p_run),
        m_command(p_command) {
    }

    ~Command() {
    }

    bool canExecute(const OptValue& execLine) const {
        return strcmp(execLine.key(), m_command) == 0;
    }

    bool execute(const OptValue& execLine, ContextType& context) {
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
                if (!advance) context.isAdvanced(false);
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
