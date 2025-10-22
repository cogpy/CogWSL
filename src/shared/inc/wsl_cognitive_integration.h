/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    wsl_cognitive_integration.h

Abstract:

    Integration layer between WSL service and OpenCog cognitive architecture.
    Provides autogenetic capabilities within the WSL framework.

--*/

#pragma once

#include "cognitive.h"
#include "message.h"
#include <memory>
#include <string>
#include <functional>

namespace wsl::shared::integration {

/// <summary>
/// Message types for WSL-cognitive integration
/// </summary>
enum class WslCognitiveMessageType : uint32_t {
    ProcessCreate = 0x2000,
    ProcessDestroy,
    DistroEvent,
    SystemEvent,
    CognitiveQuery,
    CognitiveResponse,
    AgentCommand,
    SelfModificationEvent
};

/// <summary>
/// WSL cognitive integration message structure
/// </summary>
struct WslCognitiveMessage {
    struct Header {
        WslCognitiveMessageType MessageType;
        uint32_t Size;
        uint64_t Timestamp;
        uint32_t Priority;
        char SourceId[64];
        char TargetId[64];
    } Header;
    
    char Buffer[0];
};

/// <summary>
/// Bridges WSL events and operations with the cognitive system
/// </summary>
class CognitiveIntegrationManager {
public:
    CognitiveIntegrationManager();
    ~CognitiveIntegrationManager();

    // Lifecycle management
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // WSL Event Handlers
    void OnProcessCreate(const std::string& distroId, uint32_t processId, const std::string& command);
    void OnProcessDestroy(const std::string& distroId, uint32_t processId, int exitCode);
    void OnDistroEvent(const std::string& distroId, const std::string& eventType, const std::string& data);
    void OnSystemEvent(const std::string& eventType, const std::string& data);

    // Cognitive System Interface
    std::shared_ptr<cognitive::CognitiveSystem> GetCognitiveSystem() const { return m_cognitiveSystem; }
    std::shared_ptr<cognitive::CognitiveAgent> CreateCognitiveAgent(const std::string& name, const std::string& role);
    bool DestroyCognitiveAgent(const std::string& name);

    // Query and Control
    std::string QueryCognitiveState(const std::string& query);
    bool ExecuteCognitiveCommand(const std::string& agentName, const std::string& command, const std::string& parameters);

    // Configuration
    void SetCognitiveConfiguration(const std::string& key, const std::string& value);
    std::string GetCognitiveConfiguration(const std::string& key) const;

    // Event Callbacks
    using EventCallback = std::function<void(const std::string&, const std::string&)>;
    void RegisterEventCallback(const std::string& eventType, EventCallback callback);
    void UnregisterEventCallback(const std::string& eventType);

    // Statistics and Monitoring
    struct IntegrationStats {
        size_t processesMonitored;
        size_t eventsHandled;
        size_t cognitiveAgents;
        size_t activeQueries;
        std::chrono::milliseconds averageResponseTime;
    };
    
    IntegrationStats GetStatistics() const;

private:
    void ProcessWslEvent(const std::string& eventType, const std::string& source, const std::string& data);
    void UpdateCognitiveKnowledge(const std::string& concept, const std::string& information, float confidence);
    std::string GenerateCognitiveResponse(const std::string& query);
    void HandleSelfModificationEvent(const std::string& agentName, const std::string& modification);

    std::shared_ptr<cognitive::CognitiveSystem> m_cognitiveSystem;
    std::unordered_map<std::string, EventCallback> m_eventCallbacks;
    std::unordered_map<std::string, uint32_t> m_processMapping;
    
    mutable std::shared_mutex m_mutex;
    std::atomic<bool> m_initialized;
    
    // Statistics
    mutable std::mutex m_statsMutex;
    IntegrationStats m_stats;
    std::chrono::system_clock::time_point m_startTime;
};

/// <summary>
/// WSL Cognitive Process Monitor
/// Monitors WSL processes and creates cognitive representations
/// </summary>
class CognitiveProcessMonitor {
public:
    CognitiveProcessMonitor(std::shared_ptr<CognitiveIntegrationManager> integrationManager);
    ~CognitiveProcessMonitor();

    // Monitoring control
    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const { return m_monitoring; }

    // Process tracking
    void TrackProcess(const std::string& distroId, uint32_t processId, const std::string& command);
    void UntrackProcess(const std::string& distroId, uint32_t processId);

    // Cognitive analysis
    void AnalyzeProcessBehavior(const std::string& distroId, uint32_t processId);
    void LearnFromProcessPatterns();

private:
    struct ProcessInfo {
        std::string distroId;
        uint32_t processId;
        std::string command;
        std::chrono::system_clock::time_point startTime;
        std::vector<std::string> behaviorLog;
        std::shared_ptr<cognitive::Atom> cognitiveRepresentation;
    };

    void MonitoringLoop();
    void UpdateProcessKnowledge(const ProcessInfo& process);

    std::shared_ptr<CognitiveIntegrationManager> m_integrationManager;
    std::unordered_map<std::string, ProcessInfo> m_trackedProcesses;
    
    std::atomic<bool> m_monitoring;
    std::thread m_monitoringThread;
    mutable std::shared_mutex m_processMutex;
};

/// <summary>
/// Autogenetic WSL Agent Factory
/// Creates specialized cognitive agents for different WSL tasks
/// </summary>
class AutogeneticAgentFactory {
public:
    enum class AgentType {
        ProcessOptimizer,
        ResourceManager,
        SecurityAnalyzer,
        PerformanceMonitor,
        SystemLearner,
        AdaptiveScheduler
    };

    AutogeneticAgentFactory(std::shared_ptr<CognitiveIntegrationManager> integrationManager);
    ~AutogeneticAgentFactory();

    // Agent creation
    std::shared_ptr<cognitive::CognitiveAgent> CreateAgent(AgentType type, const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreateCustomAgent(const std::string& name, const std::string& specification);

    // Agent templates
    void RegisterAgentTemplate(const std::string& templateName, AgentType baseType, const std::string& specialization);
    std::vector<std::string> GetAvailableTemplates() const;

    // Self-modification capabilities
    void EnableSelfModification(const std::string& agentName, bool enable);
    void SetSelfModificationParameters(const std::string& agentName, float probability, const std::string& constraints);

private:
    std::shared_ptr<cognitive::CognitiveAgent> CreateProcessOptimizerAgent(const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreateResourceManagerAgent(const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreateSecurityAnalyzerAgent(const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreatePerformanceMonitorAgent(const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreateSystemLearnerAgent(const std::string& name);
    std::shared_ptr<cognitive::CognitiveAgent> CreateAdaptiveSchedulerAgent(const std::string& name);

    struct AgentTemplate {
        AgentType baseType;
        std::string specialization;
        std::unordered_map<std::string, std::string> parameters;
    };

    std::shared_ptr<CognitiveIntegrationManager> m_integrationManager;
    std::unordered_map<std::string, AgentTemplate> m_agentTemplates;
    std::unordered_map<std::string, bool> m_selfModificationEnabled;
    
    mutable std::shared_mutex m_templateMutex;
};

/// <summary>
/// Global cognitive integration instance
/// </summary>
extern std::unique_ptr<CognitiveIntegrationManager> g_cognitiveIntegration;

} // namespace wsl::shared::integration