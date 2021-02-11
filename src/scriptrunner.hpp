#pragma once
#include <cstring>
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>
#include <map>
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


/**
 * Simpel state that gets run each time the StateMachine reaches this state
 */
template<typename ContextType>
class Command {
public:
    typedef std::function<bool (const char*, ContextType& context)> TRunFunction;

private:
    TRunFunction m_run;
    const char* m_command;

public:
    Command(const char* p_command, const TRunFunction p_run) :
        m_run{p_run},
        m_command{p_command} {
    }

    bool canExecute(const char* requestedCommand) const {
        return strcmp(requestedCommand, m_command) == 0;
    }

    inline bool execute(const char* command, ContextType& context) {
        return m_run(command, context);
    }

};

/**
 * StateMachine itself that will run through all states
 */

template<typename ContextType>
class ScriptRunner {
public:
    typedef Command<ContextType>* CommandContextPtr;
protected:
    std::vector<CommandContextPtr> m_commands;

    virtual CommandContextPtr getCommandExecutor(const char* line) {
        for (auto cmd : m_commands) {
            if (cmd->canExecute(line)) {
                return cmd;
            }
        }

        return nullptr;
    }

    bool execute(ContextType& context) {
        const OptValue& currentLineValue = context.currentLine();

        bool advanced = false;

        // For the current line, find the matching command to run
        auto cmd = getCommandExecutor(currentLineValue.key());

        if (cmd != nullptr) {
            cmd->execute((const char*)currentLineValue, context);
            advanced = context.advance();
            context.advanced(advanced);
            // if (strcmp(currentLineValue.key(), "wait") == 0) {
            //     advanced = execute(context);
            //     context.advanced(advanced);
            // }
        }

        return advanced;
    }

public:
    ScriptRunner(std::vector<CommandContextPtr> const& p_commands) :
        m_commands{p_commands}  {
    }

    /**
     * Keep running the script
     * Run's true as long as the script is still running
     */
    bool handle(ContextType& context) {
        if (context.isEnd()) {
            return false;
        }

        // Find an executor and run the command
        const OptValue& currentLineValue = context.currentLine();
        auto cmd = getCommandExecutor(currentLineValue.key());

        bool shouldAdvance = true;

        if (cmd != nullptr) {
            shouldAdvance = cmd->execute((const char*)currentLineValue, context);
        }

        // move to next line
        if (shouldAdvance) {
            bool advanced = context.advance();
            context.advanced(advanced);

            // Optimalisation to execute the next command after the wait is over to
            // keep timing as tight as possible
            if (advanced && strcmp(currentLineValue.key(), "wait") == 0) {
                return handle(context);
            }
        }

        return true;
    }
};


template<typename ContextType>
class CachedScriptRunner : public ScriptRunner<ContextType> {
    struct cmp_str {
        bool operator()(char const* a, char const* b) const {
            return std::strcmp(a, b) < 0;
        }
    };

    template<typename key, typename value>
    using mymap = std::map<key, value, cmp_str>;

    mymap<const char*, Command<ContextType>*> m_cmdMap;
public:
    CachedScriptRunner(std::vector<Command<ContextType>*> const& p_commands) : ScriptRunner<ContextType> {
        p_commands
    } {
    }

    virtual Command<ContextType>* getCommandExecutor(const char* line) {
        auto search = m_cmdMap.find(line);

        if (search != m_cmdMap.end()) {
            return search->second;
        } else {
            Command<ContextType>* unCached = ScriptRunner<ContextType>::getCommandExecutor(line);
            m_cmdMap.emplace(line, unCached);
            return unCached;
        }

    }

    uint8_t cacheSize() const {
        return m_cmdMap.size();
    }
};


typedef std::unique_ptr<OptValue> OptValuePtr;
class Context {
private:

    std::vector<OptValuePtr> m_script;
    std::vector<OptValuePtr>::iterator m_currentLine;
    uint32_t m_requestedStartMillis;
    bool m_advanced;
public:
    Context(std::vector<OptValuePtr> p_script) :
        m_script(std::move(p_script)),
        m_currentLine(m_script.begin()),
        m_requestedStartMillis(0),
        m_advanced(false) {
    }

    Context() :
        m_requestedStartMillis(0),
        m_advanced(false) {
    }

    void setScript(std::vector<OptValuePtr> p_script) {
        m_script = std::move(p_script);
        m_currentLine = m_script.begin();
        m_requestedStartMillis = 0;
        m_advanced = false;
    }

    const OptValue& currentLine() const {
        return (*(*m_currentLine).get());
    }

    bool isEnd() const {
        return m_currentLine == m_script.end();
    }

    /**
     * Wait a number of milli seconds
     * return true if the waiting is over, returns false if we should not advance to the next line
     */
    bool wait(uint32_t currentMillis, uint32_t millisToWait) {
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
        m_advanced = false;

        while (
            line != m_script.end() &&
            (strcmp((*line).get()->key(), "label") != 0 ||
             strcmp((char*)(*(*line).get()), labelName) != 0)) {
            line++;
        };

        if (line != m_script.end()) {
            m_currentLine = line;
            m_advanced = true;
            return true;
        }

        return false;
    }

    /**
     * Advance to the next line
     * as long as the script is running, we return true
     */
    bool advance() {
        if (isEnd()) {
            return false;
        }

        const OptValue& current = currentLine();
        m_advanced = true;

        if (current.isKey("jump")) {
            jump(current);
        } else if (current.isKey("label")) {
            m_currentLine++;
        } else if (current.isKey("wait")) {
            if (wait(millis(), (int32_t)current)) {
                m_currentLine++;
            } else {
                m_advanced = false;
            }
        } else {
            m_currentLine++;
        }

        return m_advanced;
    }

    bool advanced() const {
        return m_advanced;
    }
    void advanced(bool advanced)  {
        m_advanced = advanced;
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
        std::vector<OptValuePtr> scriptOpts;
        OptParser::get(m_scriptText, ';', [&scriptOpts](OptValue f) {
            if (strlen(f.key())!=0) {
                scriptOpts.push_back(make_unique<OptValue>(f));
            }
        });
        setScript(std::move(scriptOpts));
    }
};



}
}