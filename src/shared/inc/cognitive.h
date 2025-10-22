/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    cognitive.h

Abstract:

    This file contains the OpenCog cognitive architecture framework 
    interfaces and data structures for autogenetic WSL capabilities.

--*/

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include "defs.h"
#include "message.h"

namespace wsl::shared::cognitive {

/// <summary>
/// Represents the fundamental unit of knowledge in the cognitive architecture.
/// Similar to OpenCog's Atom concept but adapted for WSL environment.
/// </summary>
class Atom {
public:
    enum class Type : uint32_t {
        Concept = 0,
        Link,
        Process,
        Agent,
        Rule,
        Goal,
        Memory
    };

    Atom(Type type, const std::string& name, float truthValue = 1.0f, float confidence = 1.0f);
    virtual ~Atom() = default;

    // Core properties
    Type GetType() const { return m_type; }
    const std::string& GetName() const { return m_name; }
    uint64_t GetId() const { return m_id; }
    
    // Truth values for uncertainty reasoning
    float GetTruthValue() const { return m_truthValue; }
    float GetConfidence() const { return m_confidence; }
    void SetTruthValue(float truth, float confidence);
    
    // Attention and importance values for resource allocation
    float GetAttention() const { return m_attention; }
    void SetAttention(float attention) { m_attention = attention; }
    
    // Links to other atoms
    const std::vector<std::shared_ptr<Atom>>& GetIncomingLinks() const { return m_incomingLinks; }
    const std::vector<std::shared_ptr<Atom>>& GetOutgoingLinks() const { return m_outgoingLinks; }
    
    void AddIncomingLink(std::shared_ptr<Atom> atom);
    void AddOutgoingLink(std::shared_ptr<Atom> atom);

protected:
    static std::atomic<uint64_t> s_nextId;
    
    Type m_type;
    std::string m_name;
    uint64_t m_id;
    float m_truthValue;
    float m_confidence;
    float m_attention;
    std::chrono::system_clock::time_point m_creationTime;
    std::chrono::system_clock::time_point m_lastAccessed;
    
    std::vector<std::shared_ptr<Atom>> m_incomingLinks;
    std::vector<std::shared_ptr<Atom>> m_outgoingLinks;
};

/// <summary>
/// Knowledge repository similar to OpenCog's AtomSpace.
/// Manages the graph of cognitive knowledge within WSL.
/// </summary>
class AtomSpace {
public:
    AtomSpace();
    ~AtomSpace();

    // Atom management
    std::shared_ptr<Atom> CreateAtom(Atom::Type type, const std::string& name, 
                                   float truthValue = 1.0f, float confidence = 1.0f);
    std::shared_ptr<Atom> GetAtom(uint64_t id) const;
    std::shared_ptr<Atom> FindAtom(const std::string& name) const;
    std::vector<std::shared_ptr<Atom>> FindAtomsByType(Atom::Type type) const;
    
    bool RemoveAtom(uint64_t id);
    void Clear();
    
    // Query and reasoning
    std::vector<std::shared_ptr<Atom>> Query(const std::function<bool(const Atom&)>& predicate) const;
    void UpdateAttentionValues();
    
    // Statistics
    size_t GetAtomCount() const { return m_atoms.size(); }
    size_t GetAtomCountByType(Atom::Type type) const;

private:
    std::unordered_map<uint64_t, std::shared_ptr<Atom>> m_atoms;
    std::unordered_map<std::string, std::shared_ptr<Atom>> m_atomsByName;
    mutable std::shared_mutex m_mutex;
};

/// <summary>
/// Represents an autonomous cognitive agent with self-modification capabilities.
/// This implements the "autogenetic" aspect of the framework.
/// </summary>
class CognitiveAgent {
public:
    enum class State : uint32_t {
        Inactive = 0,
        Active,
        Learning,
        Planning,
        Executing,
        SelfModifying,
        Error
    };

    CognitiveAgent(const std::string& name, std::shared_ptr<AtomSpace> atomSpace);
    virtual ~CognitiveAgent();

    // Agent lifecycle
    void Start();
    void Stop();
    void Pause();
    void Resume();
    
    State GetState() const { return m_state; }
    const std::string& GetName() const { return m_name; }
    
    // Cognitive processes
    virtual void Perceive();
    virtual void Reason();
    virtual void Plan();
    virtual void Act();
    virtual void Learn();
    virtual void SelfModify();
    
    // Goal and task management
    void AddGoal(std::shared_ptr<Atom> goal);
    void RemoveGoal(uint64_t goalId);
    const std::vector<std::shared_ptr<Atom>>& GetGoals() const { return m_goals; }
    
    // Agent communication
    void SendMessage(const std::string& targetAgent, const std::string& message);
    void ReceiveMessage(const std::string& fromAgent, const std::string& message);

protected:
    virtual void ProcessingLoop();
    virtual bool ShouldContinueProcessing() const;
    
    std::string m_name;
    std::shared_ptr<AtomSpace> m_atomSpace;
    State m_state;
    std::atomic<bool> m_shouldStop;
    std::thread m_processingThread;
    
    std::vector<std::shared_ptr<Atom>> m_goals;
    std::vector<std::shared_ptr<Atom>> m_memories;
    
    mutable std::mutex m_stateMutex;
    std::condition_variable m_stateCondition;
};

/// <summary>
/// Manages multiple cognitive agents and their interactions.
/// Provides the autogenetic multi-agent system capabilities.
/// </summary>
class CognitiveSystem {
public:
    CognitiveSystem();
    ~CognitiveSystem();

    // System lifecycle
    void Initialize();
    void Shutdown();
    
    // Agent management
    std::shared_ptr<CognitiveAgent> CreateAgent(const std::string& name);
    std::shared_ptr<CognitiveAgent> GetAgent(const std::string& name) const;
    bool RemoveAgent(const std::string& name);
    
    std::vector<std::string> GetAgentNames() const;
    size_t GetAgentCount() const { return m_agents.size(); }
    
    // System-wide operations
    void BroadcastMessage(const std::string& message);
    void UpdateSystem();
    
    // AtomSpace access
    std::shared_ptr<AtomSpace> GetGlobalAtomSpace() const { return m_globalAtomSpace; }
    
    // Configuration and monitoring
    void SetConfiguration(const std::string& key, const std::string& value);
    std::string GetConfiguration(const std::string& key) const;
    
    struct SystemStats {
        size_t totalAgents;
        size_t activeAgents;
        size_t totalAtoms;
        double averageAttention;
        std::chrono::milliseconds uptime;
    };
    
    SystemStats GetStatistics() const;

private:
    std::shared_ptr<AtomSpace> m_globalAtomSpace;
    std::unordered_map<std::string, std::shared_ptr<CognitiveAgent>> m_agents;
    std::unordered_map<std::string, std::string> m_configuration;
    
    mutable std::shared_mutex m_agentsMutex;
    mutable std::shared_mutex m_configMutex;
    
    std::chrono::system_clock::time_point m_startTime;
    std::atomic<bool> m_initialized;
};

/// <summary>
/// Message types for cognitive system communication
/// </summary>
enum class CognitiveMessageType : uint32_t {
    AgentCreated = 0x1000,
    AgentDestroyed,
    AtomCreated,
    AtomModified,
    GoalAdded,
    GoalCompleted,
    SystemEvent,
    SelfModification
};

/// <summary>
/// Cognitive system message structure
/// </summary>
struct CognitiveMessage {
    struct Header {
        CognitiveMessageType MessageType;
        uint32_t Size;
        uint64_t Timestamp;
        uint32_t Priority;
    } Header;
    
    char Buffer[0];
};

/// <summary>
/// Global cognitive system instance for WSL integration
/// </summary>
extern std::unique_ptr<CognitiveSystem> g_cognitiveSystem;

} // namespace wsl::shared::cognitive