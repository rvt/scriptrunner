#pragma once
#include <cstring>
#include <cstdint>
#include <functional>
#include <algorithm>
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
 * Command name and function of something that can be executed.
 */
template<typename ContextType>
class Command {
public:
    typedef std::function<bool (const char*, ContextType& context)> TRunFunction;

private:
    const TRunFunction m_run;
    const char* m_command;

public:
    Command(const char* p_command, const TRunFunction p_run) :
        m_run{p_run},
        m_command{p_command} {
    }

    inline bool canExecute(const char* requestedCommand) const {
        return strcmp(requestedCommand, m_command) == 0;
    }

    inline bool execute(const char* command, ContextType& context) {
        return m_run(command, context);
    }

};

/**
 * ScriptRunner is capable of running a script with a context
 * The class itself is stateless and on one ScriptRunner you can run multiple scripts with
 * each their own context but witj the same command list commands.
 */
template<typename ContextType>
class ScriptRunner {
public:
    typedef Command<ContextType>* CommandContextPtr;
protected:
    std::vector<CommandContextPtr> m_commands;

    virtual CommandContextPtr getCommandExecutor(const char* line) {
        auto found = std::find_if(m_commands.begin(), m_commands.end(), [&](CommandContextPtr f) {
            return f->canExecute(line);
        });

        return found != m_commands.end() ? *found : nullptr;
    }

public:
    ScriptRunner(std::vector<CommandContextPtr> const& p_commands) :
        m_commands{p_commands}  {
    }

    bool handle(ContextType& context) {
        return handle(context, false);
    }

    /**
     * Handle the current command and advance to the next command
     * Returns true when we are still running the script, returns false if the script has ended.
     */
    bool handle(ContextType& context, bool fastForeward) {
        auto startPosition = context.currentPosition();

        bool canRunNextLine;

        do {
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

            canRunNextLine = shouldAdvance;

            if (shouldAdvance) {
                // When returned false, we cannot run the next line
                canRunNextLine = context.advanceToNextLine();

                // Optimalisation to execute the next command after the wait is over to
                // keep timing as tight as possible
                if (canRunNextLine && strcmp(currentLineValue.key(), "wait") == 0) {
                    return handle(context, fastForeward);
                }
            }
        } while (fastForeward && startPosition != context.currentPosition() && canRunNextLine);


        return true;
    }
};

/**
 * Same as ScriptRunner but it will cache the commands in a hasmap for faster lookup.
 */
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
    CachedScriptRunner(std::vector<Command<ContextType>*> const& p_commands) :
        ScriptRunner<ContextType> {
        p_commands
    } {
    }

    /**
     * Find a command from the cache or look them up
     */
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


/**
 * Base context you can inherit to add your own functionality
 */
typedef std::unique_ptr<OptValue> OptValuePtr;
class Context {
protected:
    std::vector<OptValuePtr> m_script;
    std::vector<OptValuePtr>::iterator m_currentLine;
    uint32_t m_requestedStartMillis;
public:
    Context(std::vector<OptValuePtr> p_script) :
        m_script(std::move(p_script)),
        m_currentLine(m_script.begin()),
        m_requestedStartMillis(0) {
    }

    Context() :
        m_requestedStartMillis(0) {
    }

    void setScript(std::vector<OptValuePtr> p_script) {
        m_script = std::move(p_script);
        m_currentLine = m_script.begin();
        m_requestedStartMillis = 0;
    }

    const OptValue& currentLine() const {
        return (*(*m_currentLine).get());
    }

    uint16_t currentPosition() const {
        return m_currentLine - m_script.begin();
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

    /**
     * Find a location to jump into, else just advance to the next line
     */
    void jump(const char* labelName) {
        std::vector<OptValuePtr>::iterator line = m_script.begin();

        while (
            line != m_script.end() &&
            (strcmp((*line).get()->key(), "label") != 0 ||
             strcmp((char*)(*(*line).get()), labelName) != 0)) {
            line++;
        };

        if (line != m_script.end()) {
            m_currentLine = line;
        } else {
            m_currentLine++;
        }
    }

    /**
     * Process the next line
     * Returns true when we have advanced to the next line
     */
    bool advanceToNextLine() {
        if (isEnd()) {
            return false;
        }

        const OptValue& current = currentLine();

        if (current.isKey("jump")) {
            jump(current);
        } else if (current.isKey("wait")) {
            if (wait(millis(), (int32_t)current)) {
                m_currentLine++;
            } else {
                return false;
            }
        } else {
            m_currentLine++;
        }

        return true;
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
            if (strlen(f.key()) != 0) {
                scriptOpts.push_back(make_unique<OptValue>(f));
            }
        });
        setScript(std::move(scriptOpts));
    }
};



}
}